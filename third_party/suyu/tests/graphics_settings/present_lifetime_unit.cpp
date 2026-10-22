// SPDX-License-Identifier: GPL-2.0-or-later
// Runs the production layer methods with a simulated GPU timeline. Resource
// destruction is invalid until every frame using the resource has completed.
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <variant>
#include <vector>

using u64 = std::uint64_t;
struct VkExtent2D { unsigned width, height; };
struct Device {};
namespace Tegra {
struct FramebufferConfig { unsigned width, height, pixel_format; };
}
namespace Settings {
enum class AntiAliasing { None, Fxaa, Smaa };
struct ResolutionInfo {
    unsigned ScaleUp(unsigned value) const { return value; }
};
struct { ResolutionInfo resolution_info; } values;
}

static u64 completed_tick{};
static u64 last_use_tick{};
static unsigned unsafe_destructions{};
static unsigned waits{};
struct Scheduler {
    void Wait(u64 tick) {
        ++waits;
        completed_tick = std::max(completed_tick, tick);
    }
};
struct Pass {
    Pass(const Device&, int&, unsigned, VkExtent2D) {}
    ~Pass() {
        if (completed_tick < last_use_tick) {
            ++unsafe_destructions;
        }
    }
};
struct FXAA : Pass { using Pass::Pass; };
struct SMAA : Pass { using Pass::Pass; };
struct Filters {
    Settings::AntiAliasing selected{Settings::AntiAliasing::None};
    Settings::AntiAliasing get_anti_aliasing() const { return selected; }
};
struct Layer {
    void SetAntiAliasPass(const Device&);
    void RefreshResources(const Device&, const Tegra::FramebufferConfig&);
    void ReleaseRawImages() {
        for (auto tick : resource_ticks) {
            scheduler.Wait(tick);
        }
        raw_images.clear();
    }
    void CreateStagingBuffer(const Device&, const Tegra::FramebufferConfig&) {}
    void CreateRawImages(const Device&, const Tegra::FramebufferConfig&) { raw_images.push_back(1); }
    int memory_allocator{};
    unsigned image_count{3};
    Scheduler scheduler;
    Filters filters;
    unsigned raw_width{1280}, raw_height{720}, pixel_format{};
    std::vector<int> raw_images{1};
    std::vector<u64> resource_ticks;
    Settings::AntiAliasing anti_alias_setting{};
    std::variant<std::monostate, FXAA, SMAA> anti_alias;
};

#include "present_methods.inc"

int main() {
    Device device;
    Layer layer;
    layer.filters.selected = Settings::AntiAliasing::Fxaa;
    layer.SetAntiAliasPass(device);
    // Three queued frames still own the FXAA resources.
    layer.resource_ticks = {4, 6, 5};
    last_use_tick = 6;
    layer.filters.selected = Settings::AntiAliasing::Smaa;
    layer.SetAntiAliasPass(device);
    if (unsafe_destructions) {
        std::cerr << "AA switched before in-flight frames completed\n";
        return 1;
    }
    layer.resource_ticks = {7, 9, 8};
    last_use_tick = 9;
    layer.filters.selected = Settings::AntiAliasing::None;
    layer.SetAntiAliasPass(device);
    if (unsafe_destructions) {
        std::cerr << "AA disabled before in-flight frames completed\n";
        return 1;
    }
    const auto prior_waits = waits;
    layer.SetAntiAliasPass(device);
    if (waits != prior_waits) {
        std::cerr << "Disabled AA introduced an unnecessary per-frame wait\n";
        return 1;
    }
    layer.filters.selected = Settings::AntiAliasing::Fxaa;
    layer.SetAntiAliasPass(device);
    layer.resource_ticks = {10, 12, 11};
    last_use_tick = 12;
    layer.RefreshResources(device, {1920, 1080, 0});
    if (unsafe_destructions) {
        std::cerr << "Framebuffer resized before AA frames completed\n";
        return 1;
    }
    std::cout << "PASS: switch, disable, resize, and steady disabled AA lifetime\n";
}
