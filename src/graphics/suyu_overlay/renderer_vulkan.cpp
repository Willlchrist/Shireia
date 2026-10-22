// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <chrono>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>
#include "../HeadlessFrameSink.h"

namespace {
std::unordered_map<const VideoCore::RendererBase*, Shireia::HeadlessFrameSink> frame_sinks;
}
void Shireia::SetHeadlessFrameSink(const VideoCore::RendererBase* renderer, HeadlessFrameSink sink) {
    frame_sinks[renderer] = std::move(sink);
}

#include <fmt/ranges.h>

#include "common/logging.h"
#include <ranges>
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core_timing.h"
#include "core/frontend/graphics_context.h"
#include "video_core/capture.h"
#include "video_core/gpu.h"
#include "video_core/present.h"
#include "video_core/renderer_vulkan/present/util.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/renderer_vulkan/vk_blit_screen.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_stall_probe.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"
#include "video_core/textures/decoders.h"
#include "video_core/vulkan_common/vulkan_debug_callback.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_library.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_surface.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"
#ifdef __ANDROID__
#include <jni.h>
#endif
namespace Vulkan {
namespace {

struct HeadlessReadbackResources {
    Frame frame{};
    vk::Buffer buffer;
    VkExtent2D extent{};
    VkFormat format{};
    u64 allocation_micros{};
    bool initialized{};
};
std::unordered_map<const RendererVulkan*, HeadlessReadbackResources> headless_readback_resources;

u64 ElapsedMicros(std::chrono::steady_clock::time_point start);
void RecordHeadlessReadbackMetrics(u64 allocation, u64 command_recording, u64 finish_wait, u64 invalidate);
extern std::atomic<u64> headless_frame_sequence;

void ReportHeadlessStallProbeFrame() {
    if (!StallProbe::Enabled()) {
        return;
    }
    static thread_local u64 previous_ns{};
    const u64 now = StallProbe::Now();
    const u64 interval_ns = previous_ns == 0 ? 0 : now - previous_ns;
    previous_ns = now;

    const u64 build_wait = StallProbe::build_wait_ns.exchange(0, std::memory_order_relaxed);
    const u64 build_count = StallProbe::build_wait_count.exchange(0, std::memory_order_relaxed);
    const u64 draw = StallProbe::draw_ns.exchange(0, std::memory_order_relaxed);
    const u64 draw_count = StallProbe::draw_count.exchange(0, std::memory_order_relaxed);
    const u64 scheduler_wait = StallProbe::sched_wait_ns.exchange(0, std::memory_order_relaxed);
    const u64 gpu_idle = StallProbe::gpu_idle_ns.exchange(0, std::memory_order_relaxed);
    const u64 translate = StallProbe::shader_translate_ns.exchange(0, std::memory_order_relaxed);
    const u64 emit = StallProbe::shader_emit_ns.exchange(0, std::memory_order_relaxed);
    const u64 shader_count = StallProbe::shader_count.exchange(0, std::memory_order_relaxed);
    const u64 pipeline_build = StallProbe::pipeline_build_ns.exchange(0, std::memory_order_relaxed);
    const u64 pipeline_count = StallProbe::pipeline_build_count.exchange(0, std::memory_order_relaxed);
    const u64 lookup = StallProbe::pipeline_lookup_ns.exchange(0, std::memory_order_relaxed);
    const u64 configure = StallProbe::pipeline_configure_ns.exchange(0, std::memory_order_relaxed);
    const u64 draw_flush = StallProbe::draw_flush_ns.exchange(0, std::memory_order_relaxed);
    const u64 draw_record = StallProbe::draw_record_ns.exchange(0, std::memory_order_relaxed);
    if (interval_ns < 16'700'000ULL) {
        return;
    }

    const auto ms = [](u64 ns) { return static_cast<double>(ns) * 1.0e-6; };
    LOG_INFO(Render_Vulkan,
             "SHIREIA_HEADLESS_PROBE steady_ms={} frame_ms={:.2f} overlapping_scope_ms: "
             "gpu_idle={:.2f} draw={:.2f}(n={}) sched_wait={:.2f} build_wait={:.2f}(n={}) "
             "shader_translate={:.2f} shader_emit={:.2f}(n={}) pipeline_build={:.2f}(n={}) "
             "pipeline_lookup={:.2f} configure={:.2f} draw_flush={:.2f} draw_record={:.2f}",
             now / 1'000'000ULL, ms(interval_ns), ms(gpu_idle), ms(draw), draw_count,
             ms(scheduler_wait), ms(build_wait), build_count, ms(translate), ms(emit),
             shader_count, ms(pipeline_build), pipeline_count, ms(lookup), ms(configure),
             ms(draw_flush), ms(draw_record));
}

constexpr VkExtent2D CaptureImageSize{
    .width = VideoCore::Capture::LinearWidth,
    .height = VideoCore::Capture::LinearHeight,
};

constexpr VkExtent3D CaptureImageExtent{
    .width = VideoCore::Capture::LinearWidth,
    .height = VideoCore::Capture::LinearHeight,
    .depth = VideoCore::Capture::LinearDepth,
};

constexpr VkFormat CaptureFormat = VK_FORMAT_A8B8G8R8_UNORM_PACK32;

std::string GetReadableVersion(u32 version) {
    return fmt::format("{}.{}.{}", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version),
                       VK_VERSION_PATCH(version));
}

std::string GetDriverVersion(const Device& device) {
    // Extracted from
    // https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/5dddea46ea1120b0df14eef8f15ff8e318e35462/functions.php#L308-L314
    const u32 version = device.GetDriverVersion();

    if (device.GetDriverID() == VK_DRIVER_ID_NVIDIA_PROPRIETARY) {
        const u32 major = (version >> 22) & 0x3ff;
        const u32 minor = (version >> 14) & 0x0ff;
        const u32 secondary = (version >> 6) & 0x0ff;
        const u32 tertiary = version & 0x003f;
        return fmt::format("{}.{}.{}.{}", major, minor, secondary, tertiary);
    }
    if (device.GetDriverID() == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) {
        const u32 major = version >> 14;
        const u32 minor = version & 0x3fff;
        return fmt::format("{}.{}", major, minor);
    }
    return GetReadableVersion(version);
}

std::string BuildCommaSeparatedExtensions(
    const std::set<std::string, std::less<>>& available_extensions) {
    return fmt::format("{}", fmt::join(available_extensions, ","));
}

} // Anonymous namespace

Device CreateDevice(const vk::Instance& instance, const vk::InstanceDispatch& dld, VkSurfaceKHR surface) {
    const std::vector<VkPhysicalDevice> devices = instance.EnumeratePhysicalDevices();
    const u32 device_index = Settings::values.vulkan_device.GetValue();
    if (device_index >= u32(devices.size())) {
        LOG_ERROR(Render_Vulkan, "Invalid device index {}!", device_index);
        throw vk::Exception(VK_ERROR_INITIALIZATION_FAILED);
    }
    const vk::PhysicalDevice physical_device(devices[device_index], dld);
    return Device(*instance, physical_device, surface, dld);
}

RendererVulkan::RendererVulkan(Core::Frontend::EmuWindow& emu_window,
                               Tegra::MaxwellDeviceMemoryManager& device_memory_,
                               Tegra::GPU& gpu_,
                               std::unique_ptr<Core::Frontend::GraphicsContext> context_)
try
    : RendererBase(emu_window, std::move(context_))
    , device_memory(device_memory_)
    , gpu(gpu_)
    , library(OpenLibrary(context.get()))
    , dld()
    // Create raw Vulkan instance first
    , instance(CreateInstance(*library,
                            dld,
                            VK_API_VERSION_1_1,
                            render_window.GetWindowInfo().type,
                            Settings::values.renderer_debug.GetValue()))
    // Create debug messenger if debug is enabled
    , debug_messenger(Settings::values.renderer_debug ? CreateDebugUtilsCallback(instance)
                                                    : vk::DebugUtilsMessenger{})
    // Create surface
    , surface(CreateSurface(instance, render_window.GetWindowInfo()))
    , device(CreateDevice(instance, dld, *surface))
    , memory_allocator(device)
    , state_tracker()
    , scheduler(device, state_tracker)
    , swapchain(*surface,
                device,
                scheduler,
               render_window.GetFramebufferLayout().width,
               render_window.GetFramebufferLayout().height)
    , present_manager(instance,
                      render_window,
                      device,
                      memory_allocator,
                      scheduler,
                      swapchain,
                      surface)
    , blit_swapchain(device_memory,
                   device,
                   memory_allocator,
                   present_manager,
                   scheduler,
                   PresentFiltersForDisplay)
    , blit_capture(device_memory,
                   device,
                   memory_allocator,
                   present_manager,
                   scheduler,
                   PresentFiltersForDisplay)
    , blit_applet(device_memory,
                  device,
                  memory_allocator,
                  present_manager,
                  scheduler,
                  PresentFiltersForAppletCapture)
    , rasterizer(render_window, gpu, device_memory, device, memory_allocator, state_tracker, scheduler) {

    is_headless = (render_window.GetWindowInfo().type == Core::Frontend::WindowSystemType::Headless);

    if (Settings::values.renderer_force_max_clock.GetValue() && device.ShouldBoostClocks()) {
        turbo_mode.emplace(instance, dld);
        scheduler.RegisterOnSubmit([this] { turbo_mode->QueueSubmitted(); });
    }

    Report();
} catch (const vk::Exception& exception) {
    LOG_ERROR(Render_Vulkan, "Vulkan initialization failed with error: {}", exception.what());
    throw std::runtime_error{fmt::format("Vulkan initialization error {}", exception.what())};
}

RendererVulkan::~RendererVulkan() {
    scheduler.RegisterOnSubmit([] {});
    scheduler.WaitWorker();
    void(device.GetLogical().WaitIdle());
    headless_readback_resources.erase(this);
    frame_sinks.erase(this);
}

void RendererVulkan::Composite(std::span<const Tegra::FramebufferConfig> framebuffers) {
    std::span<const std::uint8_t> completed_readback;
    SCOPE_EXIT {
        if (!completed_readback.empty()) {
            frame_sinks.at(this)(completed_readback);
        }
        render_window.OnFrameDisplayed();
    };

    RenderAppletCaptureLayer(framebuffers);

    if (is_headless) {
        static unsigned headless_composite_count = 0;
        ++headless_composite_count;
        if (headless_composite_count <= 5 || (headless_composite_count % 60) == 0) {
            LOG_INFO(Render_Vulkan, "Headless Composite #{}, {} framebuffer layers",
                     headless_composite_count, framebuffers.size());
            fprintf(stderr, "[suyu-vulkan] Headless Composite #%u, %zu layers\n",
                    headless_composite_count, framebuffers.size());
            fflush(stderr);
        }

        RenderScreenshot(framebuffers);

        if (!framebuffers.empty()) {
            const Layout::FramebufferLayout layout{render_window.GetFramebufferLayout()};
            headless_width = layout.width;
            headless_height = layout.height;
            const VkDeviceSize buffer_size = headless_width * headless_height * 4;
            constexpr VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
            const VkExtent2D extent{layout.width, layout.height};
            auto& readback = headless_readback_resources[this];
            if (!readback.initialized || readback.extent.width != extent.width ||
                readback.extent.height != extent.height || readback.format != format) {
                if (readback.initialized) scheduler.Finish();
                readback = {};
                readback.extent = extent;
                readback.format = format;
                const auto allocation_start = std::chrono::steady_clock::now();
                readback.frame.width = layout.width;
                readback.frame.height = layout.height;
                readback.frame.image = CreateWrappedImage(memory_allocator, extent, format);
                readback.frame.image_view = CreateWrappedImageView(device, readback.frame.image, format);
                readback.frame.framebuffer = blit_capture.CreateFramebuffer(device, layout,
                    *readback.frame.image_view, format);
                readback.buffer = CreateWrappedBuffer(memory_allocator, buffer_size, MemoryUsage::Download);
                readback.initialized = true;
                readback.allocation_micros = ElapsedMicros(allocation_start);
            } else {
                readback.allocation_micros = 0;
            }

            const auto recording_start = std::chrono::steady_clock::now();
            blit_capture.DrawToFrame(device, rasterizer, &readback.frame, framebuffers, layout, 1, format);
            scheduler.RequestOutsideRenderPassOperationContext();
            scheduler.Record([&](vk::CommandBuffer cmdbuf) {
                DownloadColorImage(cmdbuf, *readback.frame.image, *readback.buffer,
                    VkExtent3D{layout.width, layout.height, 1});
            });
            const auto recording_micros = ElapsedMicros(recording_start);
            const auto finish_start = std::chrono::steady_clock::now();
            scheduler.Finish();
            const auto finish_micros = ElapsedMicros(finish_start);
            const auto invalidate_start = std::chrono::steady_clock::now();
            readback.buffer.Invalidate();
            RecordHeadlessReadbackMetrics(readback.allocation_micros, recording_micros,
                finish_micros, ElapsedMicros(invalidate_start));
            if (frame_sinks.contains(this)) {
                // GPU completion and host invalidation above remain mandatory.
                // Consume before returning/reusing this allocation on the next frame.
                completed_readback = {readback.buffer.Mapped().data(), std::size_t(buffer_size)};
            } else {
                headless_frame_data.resize(buffer_size);
                std::memcpy(headless_frame_data.data(), readback.buffer.Mapped().data(), buffer_size);
            }
            headless_frame_sequence.fetch_add(1, std::memory_order_release);
        }

        gpu.RendererFrameEndNotify();
        rasterizer.TickFrame();
        ReportHeadlessStallProbeFrame();
        return;
    }

    if (!render_window.IsShown()) {
        return;
    }

    RenderScreenshot(framebuffers);
    Frame* frame = present_manager.GetRenderFrame();

    scheduler.RequestOutsideRenderPassOperationContext();
    blit_swapchain.DrawToFrame(device, rasterizer, frame, framebuffers,
                               render_window.GetFramebufferLayout(), swapchain.GetImageCount(),
                               swapchain.GetImageViewFormat());
    scheduler.Flush(*frame->render_ready);

    present_manager.Present(frame);

    gpu.RendererFrameEndNotify();
    rasterizer.TickFrame();
}

void RendererVulkan::Report() const {
    using namespace Common::Literals;
    const std::string vendor_name{device.GetVendorName()};
    const std::string model_name{device.GetModelName()};
    const std::string driver_version = GetDriverVersion(device);
    const std::string driver_name = fmt::format("{} {}", vendor_name, driver_version);

    const std::string api_version = GetReadableVersion(device.ApiVersion());

    const std::string extensions = BuildCommaSeparatedExtensions(device.GetAvailableExtensions());

    const auto available_vram = static_cast<f64>(device.GetDeviceLocalMemory()) / f64{1_GiB};

    LOG_INFO(Render_Vulkan, "Driver: {}", driver_name);
    LOG_INFO(Render_Vulkan, "Device: {}", model_name);
    LOG_INFO(Render_Vulkan, "Vulkan: {}", api_version);
    LOG_INFO(Render_Vulkan, "Available VRAM: {:.2f} GiB", available_vram);
}

namespace {
struct HeadlessFinishMetrics {
    static constexpr std::size_t SampleCount = 120;
    std::array<u64, SampleCount> allocation{};
    std::array<u64, SampleCount> command_recording{};
    std::array<u64, SampleCount> finish_wait{};
    std::array<u64, SampleCount> invalidate{};
    std::size_t count{};
    std::size_t next{};
};
HeadlessFinishMetrics headless_finish_metrics;
std::atomic<u64> headless_finish_total{};
std::atomic<u64> headless_finish_samples{};
std::atomic<u64> headless_allocation_total{};
std::atomic<u64> headless_record_total{};
std::atomic<u64> headless_invalidate_total{};
std::atomic<u64> headless_frame_sequence{};
u64 ElapsedMicros(std::chrono::steady_clock::time_point start) {
    return static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
}
void RecordHeadlessReadbackMetrics(u64 allocation, u64 command_recording, u64 finish_wait, u64 invalidate) {
    auto& m = headless_finish_metrics; const auto i = m.next;
    headless_finish_total.fetch_add(finish_wait, std::memory_order_relaxed);
    headless_finish_samples.fetch_add(1, std::memory_order_relaxed);
    headless_allocation_total.fetch_add(allocation, std::memory_order_relaxed);
    headless_record_total.fetch_add(command_recording, std::memory_order_relaxed);
    headless_invalidate_total.fetch_add(invalidate, std::memory_order_relaxed);
    m.allocation[i] = allocation; m.command_recording[i] = command_recording; m.finish_wait[i] = finish_wait; m.invalidate[i] = invalidate;
    m.next = (i + 1) % HeadlessFinishMetrics::SampleCount; m.count = std::min(m.count + 1, HeadlessFinishMetrics::SampleCount);
    if (m.count != HeadlessFinishMetrics::SampleCount || m.next != 0) return;
    auto stats = [](auto v) { std::sort(v.begin(), v.end()); const auto sum = std::accumulate(v.begin(), v.end(), u64{}); return std::array<double,5>{double(sum)/v.size()/1000.0,v[59]/1000.0,v[113]/1000.0,v[118]/1000.0,v.back()/1000.0}; };
    const auto a=stats(m.allocation), r=stats(m.command_recording), f=stats(m.finish_wait), n=stats(m.invalidate);
    LOG_INFO(Render_Vulkan, "Shireia readback 120-frame avg/p50/p95/p99/max ms: alloc {:.3f}/{:.3f}/{:.3f}/{:.3f}/{:.3f}; record {:.3f}/{:.3f}/{:.3f}/{:.3f}/{:.3f}; Finish {:.3f}/{:.3f}/{:.3f}/{:.3f}/{:.3f}; invalidate {:.3f}/{:.3f}/{:.3f}/{:.3f}/{:.3f}", a[0],a[1],a[2],a[3],a[4],r[0],r[1],r[2],r[3],r[4],f[0],f[1],f[2],f[3],f[4],n[0],n[1],n[2],n[3],n[4]);
}
extern "C" u64 ShireiaHeadlessFinishMicros() {
    return headless_finish_total.load(std::memory_order_relaxed);
}
extern "C" u64 ShireiaHeadlessFinishSamples() {
    return headless_finish_samples.load(std::memory_order_relaxed);
}
extern "C" u64 ShireiaHeadlessAllocationMicros() {
    return headless_allocation_total.load(std::memory_order_relaxed);
}
extern "C" u64 ShireiaHeadlessRecordMicros() {
    return headless_record_total.load(std::memory_order_relaxed);
}
extern "C" u64 ShireiaHeadlessInvalidateMicros() {
    return headless_invalidate_total.load(std::memory_order_relaxed);
}
extern "C" u64 ShireiaHeadlessFrameSequence() {
    return headless_frame_sequence.load(std::memory_order_acquire);
}
}
vk::Buffer RendererVulkan::RenderToBuffer(std::span<const Tegra::FramebufferConfig> framebuffers,
                                          const Layout::FramebufferLayout& layout, VkFormat format,
                                          VkDeviceSize buffer_size) {
    const auto allocation_start = std::chrono::steady_clock::now();
    Frame frame{};
    frame.width = layout.width;
    frame.height = layout.height;
    frame.image = CreateWrappedImage(memory_allocator, VkExtent2D{layout.width, layout.height}, format);
    frame.image_view = CreateWrappedImageView(device, frame.image, format);
    frame.framebuffer = blit_capture.CreateFramebuffer(device, layout, *frame.image_view, format);

    auto dst_buffer = CreateWrappedBuffer(memory_allocator, buffer_size, MemoryUsage::Download);
    const auto allocation_micros = ElapsedMicros(allocation_start);
    const auto recording_start = std::chrono::steady_clock::now();
    blit_capture.DrawToFrame(device, rasterizer, &frame, framebuffers, layout, 1, format);

    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([&](vk::CommandBuffer cmdbuf) {
        DownloadColorImage(cmdbuf, *frame.image, *dst_buffer,
                           VkExtent3D{layout.width, layout.height, 1});
    });

    const auto recording_micros = ElapsedMicros(recording_start);

    // Ensure the copy is fully completed before saving the capture
    const auto finish_start = std::chrono::steady_clock::now();
    scheduler.Finish();
    const auto finish_micros = ElapsedMicros(finish_start);
    const auto invalidate_start = std::chrono::steady_clock::now();;

    // Copy backing image data to the capture buffer
    dst_buffer.Invalidate();
    if (is_headless) RecordHeadlessReadbackMetrics(allocation_micros, recording_micros, finish_micros, ElapsedMicros(invalidate_start));
    return dst_buffer;
}

void RendererVulkan::RenderScreenshot(std::span<const Tegra::FramebufferConfig> framebuffers) {
    if (!renderer_settings.screenshot_requested) {
        return;
    }

    const auto& layout{renderer_settings.screenshot_framebuffer_layout};
    const auto dst_buffer = RenderToBuffer(framebuffers, layout, VK_FORMAT_B8G8R8A8_UNORM,
                                           layout.width * layout.height * 4);

    std::memcpy(renderer_settings.screenshot_bits, dst_buffer.Mapped().data(),
                dst_buffer.Mapped().size());
    renderer_settings.screenshot_complete_callback(false);
    renderer_settings.screenshot_requested = false;
}

std::vector<u8> RendererVulkan::GetAppletCaptureBuffer() {
    using namespace VideoCore::Capture;

    std::vector<u8> out(VideoCore::Capture::TiledSize);

    if (!applet_frame.image) {
        return out;
    }

    const auto dst_buffer =
        CreateWrappedBuffer(memory_allocator, VideoCore::Capture::TiledSize, MemoryUsage::Download);

    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([&](vk::CommandBuffer cmdbuf) {
        DownloadColorImage(cmdbuf, *applet_frame.image, *dst_buffer, CaptureImageExtent);
    });

    // Ensure the copy is fully completed before writing the capture
    scheduler.Finish();

    // Swizzle image data to the capture buffer
    dst_buffer.Invalidate();
    Tegra::Texture::SwizzleTexture(out, dst_buffer.Mapped(), BytesPerPixel, LinearWidth,
                                   LinearHeight, LinearDepth, BlockHeight, BlockDepth);

    return out;
}

void RendererVulkan::RenderAppletCaptureLayer(
    std::span<const Tegra::FramebufferConfig> framebuffers) {
    if (!applet_frame.image) {
        applet_frame.image = CreateWrappedImage(memory_allocator, CaptureImageSize, CaptureFormat);
        applet_frame.image_view = CreateWrappedImageView(device, applet_frame.image, CaptureFormat);
        applet_frame.framebuffer = blit_applet.CreateFramebuffer(device,
            VideoCore::Capture::Layout, *applet_frame.image_view, CaptureFormat);
    }

    scheduler.RequestOutsideRenderPassOperationContext();
    blit_applet.DrawToFrame(device, rasterizer, &applet_frame, framebuffers, VideoCore::Capture::Layout, 1,
                            CaptureFormat);
}

} // namespace Vulkan
