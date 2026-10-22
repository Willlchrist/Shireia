// Adapter around the original Suyu APIs; no CPU/GPU implementation is replaced.
#include "SuyuAdapter.h"
#include "../graphics/FrameSize.h"
#include "../graphics/PixelConvert.h"
#include "../graphics/HeadlessFrameSink.h"
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <stdexcept>
#include <algorithm>
#include "core/core.h"
#include "core/arm/debug.h"
#include "core/loader/loader.h"
#include "core/loader/nro.h"
#include "core/loader/nso.h"
#include "core/file_sys/vfs/vfs_vector.h"
#include "common/logging.h"
#include "common/settings.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/graphics_context.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/window_system.h"
#include "core/hle/service/am/applet.h"
#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_event.h"
#include "core/cpu_manager.h"
#include "core/memory.h"
#include "core/memory/cheat_engine.h"
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"
#include "video_core/rasterizer_interface.h"
#include <chrono>
#include <QtEndian>
#include <QDebug>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QDir>
#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>
#include <QGuiApplication>
#include <QScreen>
#include <QMessageBox>
#include "suyu_keyboard/qt_software_keyboard.h"
#include "common/fs/path_util.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/registered_cache.h"
#include "frontend_common/content_manager.h"
#include "audio_core/audio_core.h"

extern "C" u64 ShireiaHeadlessFinishMicros();
extern "C" u64 ShireiaHeadlessFinishSamples();
extern "C" u64 ShireiaHeadlessAllocationMicros();
extern "C" u64 ShireiaHeadlessRecordMicros();
extern "C" u64 ShireiaHeadlessInvalidateMicros();
extern "C" u64 ShireiaHeadlessFrameSequence();
#include "audio_core/sink/sink.h"
#include "audio_core/sink/sink_details.h"
#include "common/dynamic_library.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_library.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "input_common/drivers/virtual_gamepad.h"
#include "input_common/input_poller.h"
#include "hid_core/hid_core.h"
#include "hid_core/frontend/emulated_controller.h"
#include <functional>
#include <mutex>

class ShireiaSoftwareKeyboard final : public QObject, public Core::Frontend::SoftwareKeyboardApplet {
public:
    explicit ShireiaSoftwareKeyboard(Core::System& system) : system_(system) {}
    void Close() const override { ExitKeyboard(); }
    void InitializeKeyboard(bool isInline, Core::Frontend::KeyboardInitializeParameters parameters,
                            SubmitNormalCallback normalCallback, SubmitInlineCallback inlineCallback) override {
        { std::lock_guard lock(mutex_); normalCallback_ = std::move(normalCallback); inlineCallback_ = std::move(inlineCallback); }
        const QPointer<ShireiaSoftwareKeyboard> self(const_cast<ShireiaSoftwareKeyboard*>(this));
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, isInline, parameters = std::move(parameters)]() mutable {
            if (!self) return;
            if (self->dialog_) self->dialog_->ExitKeyboard();
            auto* dialog = new QtSoftwareKeyboardDialog(nullptr, self->system_, isInline, std::move(parameters));
            self->dialog_ = dialog;
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(dialog, &QtSoftwareKeyboardDialog::SubmitNormalText, self,
                [self](Service::AM::Frontend::SwkbdResult result, std::u16string text, bool confirmed) {
                    if (!self) return;
                    SubmitNormalCallback callback;
                    { std::lock_guard lock(self->mutex_); callback = std::move(self->normalCallback_); }
                    if (callback) callback(result, std::move(text), confirmed);
                }, Qt::DirectConnection);
            QObject::connect(dialog, &QtSoftwareKeyboardDialog::SubmitInlineText, self,
                [self](Service::AM::Frontend::SwkbdReplyType result, std::u16string text, s32 cursor) {
                    if (!self) return;
                    SubmitInlineCallback callback;
                    { std::lock_guard lock(self->mutex_); callback = self->inlineCallback_; }
                    if (callback) callback(result, std::move(text), cursor);
                }, Qt::DirectConnection);
        }, Qt::QueuedConnection);
    }
    void ShowNormalKeyboard() const override {
        const QPointer<ShireiaSoftwareKeyboard> self(const_cast<ShireiaSoftwareKeyboard*>(this));
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self] {
            if (!self || !self->dialog_) return;
            const auto* screen = QGuiApplication::primaryScreen();
            const QRect area = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
            self->dialog_->ShowNormalKeyboard(area.topLeft(), area.size());
        }, Qt::QueuedConnection);
    }
    void ShowTextCheckDialog(Service::AM::Frontend::SwkbdTextCheckResult result, std::u16string text) const override {
        const QPointer<ShireiaSoftwareKeyboard> self(const_cast<ShireiaSoftwareKeyboard*>(this));
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, result, text = std::move(text)]() mutable {
            if (self && self->dialog_) self->dialog_->ShowTextCheckDialog(result, std::move(text));
        }, Qt::QueuedConnection);
    }
    void ShowInlineKeyboard(Core::Frontend::InlineAppearParameters parameters) const override {
        const QPointer<ShireiaSoftwareKeyboard> self(const_cast<ShireiaSoftwareKeyboard*>(this));
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, parameters] {
            if (!self || !self->dialog_) return;
            const auto* screen = QGuiApplication::primaryScreen();
            const QRect area = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
            self->dialog_->ShowInlineKeyboard(parameters, area.topLeft(), area.size());
        }, Qt::QueuedConnection);
    }
    void HideInlineKeyboard() const override {
        const QPointer<ShireiaSoftwareKeyboard> self(const_cast<ShireiaSoftwareKeyboard*>(this));
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self] { if (self && self->dialog_) self->dialog_->HideInlineKeyboard(); }, Qt::QueuedConnection);
    }
    void InlineTextChanged(Core::Frontend::InlineTextParameters parameters) const override {
        const QPointer<ShireiaSoftwareKeyboard> self(const_cast<ShireiaSoftwareKeyboard*>(this));
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, parameters = std::move(parameters)]() mutable {
            if (self && self->dialog_) self->dialog_->InlineTextChanged(std::move(parameters));
        }, Qt::QueuedConnection);
    }
    void ExitKeyboard() const override {
        const QPointer<ShireiaSoftwareKeyboard> self(const_cast<ShireiaSoftwareKeyboard*>(this));
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self] { if (self && self->dialog_) self->dialog_->ExitKeyboard(); }, Qt::QueuedConnection);
    }
private:
    Core::System& system_;
    mutable std::mutex mutex_;
    SubmitNormalCallback normalCallback_;
    SubmitInlineCallback inlineCallback_;
    QPointer<QtSoftwareKeyboardDialog> dialog_;
};

class SuyuAdapter::InputBridge {
public:
    std::shared_ptr<InputCommon::VirtualGamepad> pad = std::make_shared<InputCommon::VirtualGamepad>("virtual_gamepad");
    InputBridge() { Common::Input::RegisterInputFactory("virtual_gamepad", std::make_shared<InputCommon::InputFactory>(pad)); }
    ~InputBridge() { Common::Input::UnregisterInputFactory("virtual_gamepad"); }
};

class SuyuAdapter::Window final : public Core::Frontend::EmuWindow {
public:
    std::function<void()> frameReady;
    explicit Window(bool docked) { window_info.type = Core::Frontend::WindowSystemType::Headless; UpdateCurrentFramebufferLayout(docked ? 1920 : 1280, docked ? 1080 : 720); }
    void OnFrameDisplayed() override { if (frameReady) frameReady(); }
    bool IsShown() const override { return true; }
    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override { return std::make_unique<Core::Frontend::GraphicsContext>(); }
};

SuyuAdapter::SuyuAdapter() = default;
SuyuAdapter::~SuyuAdapter() { shutdown(); }
void SuyuAdapter::configureGraphics(int resolution, bool shaderCache, bool docked, int vsync, bool asynchronousShaders,
                                    bool asynchronousGpu, bool pipelineCache, int vulkanDevice,
                                    int gpuAccuracy, int astcDecode, int anisotropy, int scalingFilter,
                                    int antiAliasing, int vramUsage, int astcRecompression,
                                    int aspectRatio, int fsrSharpness, int nvdec, bool reactiveFlushing,
                                    int dmaAccuracy, int gpuFenceBehavior) {
    static constexpr Settings::ResolutionSetup scales[]{Settings::ResolutionSetup::Res1_4X,
        Settings::ResolutionSetup::Res1_2X, Settings::ResolutionSetup::Res3_4X, Settings::ResolutionSetup::Res1X,
        Settings::ResolutionSetup::Res5_4X, Settings::ResolutionSetup::Res3_2X, Settings::ResolutionSetup::Res2X,
        Settings::ResolutionSetup::Res3X, Settings::ResolutionSetup::Res4X, Settings::ResolutionSetup::Res5X,
        Settings::ResolutionSetup::Res6X, Settings::ResolutionSetup::Res7X, Settings::ResolutionSetup::Res8X};
    if (resolution < 0 || resolution >= int(std::size(scales))) throw std::runtime_error("Invalid resolution scale");
    Settings::values.resolution_setup.SetValue(scales[resolution]);
    Settings::values.use_disk_shader_cache.SetValue(shaderCache);
    Settings::values.use_docked_mode.SetValue(docked ? Settings::ConsoleMode::Docked : Settings::ConsoleMode::Handheld);
    if (vsync < 0 || vsync > 3) throw std::runtime_error("Invalid V-Sync mode");
    static constexpr Settings::VSyncMode modes[]{Settings::VSyncMode::Immediate, Settings::VSyncMode::Mailbox,
        Settings::VSyncMode::Fifo, Settings::VSyncMode::FifoRelaxed};
    Settings::values.vsync_mode.SetValue(modes[vsync]);
    Settings::values.use_asynchronous_shaders.SetValue(asynchronousShaders);
    const auto availableDevices = vulkanDevices();
    if (vulkanDevice < 0 || vulkanDevice >= availableDevices.size())
        throw std::runtime_error("Selected Vulkan device is not available");
    Settings::values.use_asynchronous_gpu_emulation.SetValue(asynchronousGpu);
    Settings::values.use_vulkan_driver_pipeline_cache.SetValue(pipelineCache);
    Settings::values.vulkan_device.SetValue(u32(vulkanDevice));
    if (gpuAccuracy < 0 || gpuAccuracy > 1 || astcDecode < 0 || astcDecode > 2 ||
        anisotropy < 0 || anisotropy > 8 || scalingFilter < 0 || scalingFilter > 14 ||
        antiAliasing < 0 || antiAliasing > 2 || vramUsage < 0 || vramUsage > 1 ||
        astcRecompression < 0 || astcRecompression > 2 || aspectRatio < 0 || aspectRatio > 5 ||
        fsrSharpness < 0 || fsrSharpness > 200 || nvdec < 0 || nvdec > 2 ||
        dmaAccuracy < 0 || dmaAccuracy > 2 || gpuFenceBehavior < 0 || gpuFenceBehavior > 4)
        throw std::runtime_error("Invalid advanced graphics setting");
    static constexpr Settings::GpuAccuracy accuracies[]{Settings::GpuAccuracy::Low, Settings::GpuAccuracy::High};
    static constexpr Settings::AstcDecodeMode astcModes[]{Settings::AstcDecodeMode::Cpu,
        Settings::AstcDecodeMode::Gpu, Settings::AstcDecodeMode::CpuAsynchronous};
    static constexpr Settings::AnisotropyMode anisotropyModes[]{Settings::AnisotropyMode::Automatic,
        Settings::AnisotropyMode::Default, Settings::AnisotropyMode::X2, Settings::AnisotropyMode::X4,
        Settings::AnisotropyMode::X8, Settings::AnisotropyMode::X16, Settings::AnisotropyMode::X32,
        Settings::AnisotropyMode::X64, Settings::AnisotropyMode::None};
    static constexpr Settings::ScalingFilter filters[]{Settings::ScalingFilter::NearestNeighbor,
        Settings::ScalingFilter::Bilinear, Settings::ScalingFilter::Bicubic, Settings::ScalingFilter::Gaussian,
        Settings::ScalingFilter::Lanczos, Settings::ScalingFilter::ScaleForce, Settings::ScalingFilter::Fsr,
        Settings::ScalingFilter::Area, Settings::ScalingFilter::ZeroTangent, Settings::ScalingFilter::BSpline,
        Settings::ScalingFilter::Mitchell, Settings::ScalingFilter::Spline1, Settings::ScalingFilter::Mmpx,
        Settings::ScalingFilter::Sgsr, Settings::ScalingFilter::SgsrEdge};
    static constexpr Settings::AntiAliasing aaModes[]{Settings::AntiAliasing::None,
        Settings::AntiAliasing::Fxaa, Settings::AntiAliasing::Smaa};
    static constexpr Settings::VramUsageMode vramModes[]{Settings::VramUsageMode::Conservative,
        Settings::VramUsageMode::Aggressive};
    static constexpr Settings::AstcRecompression recompressionModes[]{Settings::AstcRecompression::Uncompressed,
        Settings::AstcRecompression::Bc1, Settings::AstcRecompression::Bc3};
    Settings::values.gpu_accuracy.SetValue(accuracies[gpuAccuracy]);
    Settings::values.accelerate_astc.SetValue(astcModes[astcDecode]);
    Settings::values.max_anisotropy.SetValue(anisotropyModes[anisotropy]);
    Settings::values.scaling_filter.SetValue(filters[scalingFilter]);
    Settings::values.anti_aliasing.SetValue(aaModes[antiAliasing]);
    Settings::values.vram_usage_mode.SetValue(vramModes[vramUsage]);
    Settings::values.astc_recompression.SetValue(recompressionModes[astcRecompression]);
    static constexpr Settings::AspectRatio aspectRatios[]{Settings::AspectRatio::R16_9,
        Settings::AspectRatio::R4_3, Settings::AspectRatio::R21_9, Settings::AspectRatio::R16_10,
        Settings::AspectRatio::R32_9, Settings::AspectRatio::Stretch};
    Settings::values.aspect_ratio.SetValue(aspectRatios[aspectRatio]);
    Settings::values.fsr_sharpening_slider.SetValue(fsrSharpness);
    static constexpr Settings::NvdecEmulation nvdecModes[]{Settings::NvdecEmulation::Off,
        Settings::NvdecEmulation::Cpu, Settings::NvdecEmulation::Gpu};
    Settings::values.nvdec_emulation.SetValue(nvdecModes[nvdec]);
    Settings::values.use_reactive_flushing.SetValue(reactiveFlushing);
    static constexpr Settings::DmaAccuracy dmaModes[]{Settings::DmaAccuracy::Default,
        Settings::DmaAccuracy::Unsafe, Settings::DmaAccuracy::Safe};
    static constexpr Settings::GpuFenceBehavior fenceModes[]{Settings::GpuFenceBehavior::Default,
        Settings::GpuFenceBehavior::Immediate, Settings::GpuFenceBehavior::Balanced,
        Settings::GpuFenceBehavior::Accurate, Settings::GpuFenceBehavior::Strict};
    Settings::values.dma_accuracy.SetValue(dmaModes[dmaAccuracy]);
    Settings::values.gpu_fence_behavior.SetValue(fenceModes[gpuFenceBehavior]);
    qInfo() << "Original renderer settings: resolution" << resolution << "docked" << docked << "vsync" << vsync
            << "disk shader cache" << shaderCache << "async shaders" << asynchronousShaders
            << "async GPU" << asynchronousGpu << "pipeline cache" << pipelineCache << "Vulkan device" << vulkanDevice
            << "accuracy" << gpuAccuracy << "ASTC" << astcDecode << "anisotropy" << anisotropy
            << "scaling" << scalingFilter << "AA" << antiAliasing << "VRAM" << vramUsage
            << "ASTC recompression" << astcRecompression;
}
void SuyuAdapter::configureCore(int language, int region, bool multicore, int cpuAccuracy, int memoryLayout,
                                int cpuOverclock, bool customCpuTicks, int cpuTicks) {
    if (language < 0 || language > 19 || region < 0 || region > 6 || cpuAccuracy < 0 || cpuAccuracy > 3 ||
        memoryLayout < 0 || memoryLayout > 4 || cpuOverclock < 0 || cpuOverclock > 2 || cpuTicks < 77 || cpuTicks > 65535)
        throw std::runtime_error("Invalid core or system setting");
    Settings::values.language_index.SetValue(static_cast<Settings::Language>(language));
    Settings::values.region_index.SetValue(static_cast<Settings::Region>(region));
    Settings::values.use_multi_core.SetValue(multicore);
    static constexpr Settings::CpuAccuracy cpuModes[]{Settings::CpuAccuracy::Auto, Settings::CpuAccuracy::Accurate,
        Settings::CpuAccuracy::Unsafe, Settings::CpuAccuracy::Paranoid};
    static constexpr Settings::MemoryLayout memoryLayouts[]{Settings::MemoryLayout::Memory_4Gb,
        Settings::MemoryLayout::Memory_6Gb, Settings::MemoryLayout::Memory_8Gb,
        Settings::MemoryLayout::Memory_10Gb, Settings::MemoryLayout::Memory_12Gb};
    Settings::values.cpu_accuracy.SetValue(cpuModes[cpuAccuracy]);
    Settings::values.memory_layout_mode.SetValue(memoryLayouts[memoryLayout]);
    static constexpr Settings::CpuClock cpuClocks[]{Settings::CpuClock::Off, Settings::CpuClock::Boost, Settings::CpuClock::Fast};
    Settings::values.fast_cpu_time.SetValue(cpuClocks[cpuOverclock]);
    Settings::values.use_custom_cpu_ticks.SetValue(customCpuTicks);
    Settings::values.cpu_ticks.SetValue(u32(cpuTicks));
}
void SuyuAdapter::configureGraphicsHacks(bool fastGpuTime, int fastGpuDivisor, bool skipCpuInvalidation) {
    if (fastGpuDivisor < 0 || fastGpuDivisor > 2) throw std::runtime_error("Invalid GPU time divisor");
    static constexpr Settings::GpuOverclock divisors[]{Settings::GpuOverclock::Normal,
        Settings::GpuOverclock::Medium, Settings::GpuOverclock::High};
    Settings::values.use_fast_gpu_time.SetValue(fastGpuTime);
    Settings::values.fast_gpu_time.SetValue(divisors[fastGpuDivisor]);
    Settings::values.skip_cpu_inner_invalidation.SetValue(skipCpuInvalidation);
}

void SuyuAdapter::configurePresentation(int scalingFilter, int antiAliasing, int fsrSharpness) {
    if (scalingFilter < 0 || scalingFilter > 14 || antiAliasing < 0 || antiAliasing > 2 ||
        fsrSharpness < 0 || fsrSharpness > 200)
        throw std::runtime_error("Invalid presentation setting");
    static constexpr Settings::ScalingFilter filters[]{Settings::ScalingFilter::NearestNeighbor,
        Settings::ScalingFilter::Bilinear, Settings::ScalingFilter::Bicubic, Settings::ScalingFilter::Gaussian,
        Settings::ScalingFilter::Lanczos, Settings::ScalingFilter::ScaleForce, Settings::ScalingFilter::Fsr,
        Settings::ScalingFilter::Area, Settings::ScalingFilter::ZeroTangent, Settings::ScalingFilter::BSpline,
        Settings::ScalingFilter::Mitchell, Settings::ScalingFilter::Spline1, Settings::ScalingFilter::Mmpx,
        Settings::ScalingFilter::Sgsr, Settings::ScalingFilter::SgsrEdge};
    static constexpr Settings::AntiAliasing aaModes[]{Settings::AntiAliasing::None,
        Settings::AntiAliasing::Fxaa, Settings::AntiAliasing::Smaa};
    Settings::values.scaling_filter.SetValue(filters[scalingFilter]);
    Settings::values.anti_aliasing.SetValue(aaModes[antiAliasing]);
    Settings::values.fsr_sharpening_slider.SetValue(fsrSharpness);
}
void SuyuAdapter::selectUser(const QString& uuid) {
    initialize();
    auto& manager = system_->GetProfileManager();
    if (uuid.isEmpty()) {
        if (!manager.UserExistsIndex(0)) throw std::runtime_error("No emulated user is available");
        Settings::values.current_user = 0;
        return;
    }
    for (std::size_t index = 0; index < manager.GetUserCount(); ++index) {
        const auto candidate = manager.GetUser(index);
        if (candidate && QString::fromStdString(candidate->FormattedString()).compare(uuid, Qt::CaseInsensitive) == 0) {
            Settings::values.current_user = s32(index);
            manager.OpenUser(*candidate);
            qInfo() << "Selected original emulated user" << index << uuid;
            return;
        }
    }
    throw std::runtime_error("Selected emulated user no longer exists; choose an available profile");
}
void SuyuAdapter::configurePaths(const QString& dataDir) {
    const auto root = QFileInfo(dataDir).absoluteFilePath();
    using Path = Common::FS::SuyuPath;
    const std::pair<Path,const char*> paths[]{
        {Path::EdenDir,""},{Path::KeysDir,"keys"},{Path::NANDDir,"nand"},{Path::SDMCDir,"sdmc"},
        {Path::SaveDir,"save"},{Path::LoadDir,"load"},{Path::ShaderDir,"shader"},
        {Path::ConfigDir,"config"},{Path::CacheDir,"cache"},{Path::LogDir,"log"},
        {Path::DumpDir,"dump"},{Path::ScreenshotsDir,"screenshots"},{Path::PlayTimeDir,"playtime"}};
    for (const auto& [id,name] : paths) {
        const auto path=QDir::cleanPath(root+"/"+name);
        if (!QDir().mkpath(path)) throw std::runtime_error("Cannot create private system directories");
        Common::FS::SetSuyuPath(id,std::filesystem::path(path.toStdWString()));
    }
}
QJsonObject SuyuAdapter::metadata(const QString& path) {
    initialize();
    const auto file = system_->GetFilesystem()->OpenFile(path.toStdString(), FileSys::OpenMode::Read);
    if (!file) throw std::runtime_error("Cannot open library file");
    auto loader = Loader::GetLoader(*system_, file);
    if (!loader) throw std::runtime_error("No loader for library file");
    QJsonObject result;
    u64 programId = 0;
    if (loader->ReadProgramId(programId) == Loader::ResultStatus::Success && programId != 0)
        result["titleId"] = QString::number(programId, 16).rightJustified(16, QChar('0')).toUpper();
    std::string title;
    const auto titleStatus = loader->ReadTitle(title);
    result["titleStatus"] = int(titleStatus);
    if (titleStatus == Loader::ResultStatus::Success && !title.empty()) result["title"] = QString::fromStdString(title).left(1024);
    std::vector<u8> icon;
    const auto iconStatus = loader->ReadIcon(icon);
    result["iconStatus"] = int(iconStatus);
    if (iconStatus == Loader::ResultStatus::Success && !icon.empty() && icon.size() <= 4 * 1024 * 1024)
        result["icon"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(icon.data()), int(icon.size())).toBase64());
    return result;
}
QJsonArray SuyuAdapter::parseCheatText(const QByteArray& text) {
    if (text.isEmpty() || text.size() > 1024 * 1024) return {};
    const Core::Memory::TextCheatParser parser;
    const auto parsed = parser.Parse(std::string_view(text.constData(), std::size_t(text.size())));
    QJsonArray result;
    for (const auto& cheat : parsed) {
        if (cheat.definition.num_opcodes == 0) continue;
        const auto end = std::find(cheat.definition.readable_name.begin(), cheat.definition.readable_name.end(), '\0');
        const QByteArray name(cheat.definition.readable_name.data(), int(std::distance(cheat.definition.readable_name.begin(), end)));
        result.append(QJsonObject{{"name",QString::fromUtf8(name).left(128)},
            {"id",int(cheat.cheat_id)},{"opcodes",int(cheat.definition.num_opcodes)}});
    }
    return result;
}
QJsonArray SuyuAdapter::profiles(const QString& dataDir) {
    initialize();
    QJsonArray entries;
    auto& manager = system_->GetProfileManager();
    for (std::size_t index = 0; index < manager.GetUserCount(); ++index) {
        Service::Account::ProfileBase profile{};
        if (!manager.GetProfileBase(index, profile) || !profile.user_uuid.IsValid()) continue;
        const auto end = std::find(profile.username.begin(), profile.username.end(), u8{0});
        const QByteArray name(reinterpret_cast<const char*>(profile.username.data()),
                              int(std::distance(profile.username.begin(), end)));
        const auto uuid = QString::fromStdString(profile.user_uuid.FormattedString());
        const auto photo = QDir(dataDir).filePath("nand/system/save/8000000000000010/su/avators/" + uuid + ".jpg");
        entries.append(QJsonObject{{"index", int(index)},
            {"uuid", QString::fromStdString(profile.user_uuid.FormattedString())},
            {"name", QString::fromUtf8(name)}, {"photo", QFileInfo::exists(photo) ? photo : QString{}}});
    }
    return entries;
}
void SuyuAdapter::editProfile(const QJsonObject& action, const QString& dataDir) {
    initialize();
    auto& manager = system_->GetProfileManager();
    const auto type = action.value("type").toString();
    const auto uuidText = action.value("uuid").toString();
    std::optional<Common::UUID> uuid;
    for (const auto& entry : manager.GetAllUsers())
        if (entry.IsValid() && QString::fromStdString(entry.FormattedString()).compare(uuidText, Qt::CaseInsensitive) == 0) { uuid = entry; break; }
    if (type == "create" || type == "rename") {
        const auto name = action.value("name").toString().trimmed().toUtf8();
        if (name.isEmpty() || name.size() >= int(Service::Account::profile_username_size) || name.contains('\n'))
            throw std::runtime_error("Profile name must contain 1 to 31 UTF-8 bytes");
        if (type == "create") {
            if (manager.GetUserCount() >= Service::Account::MAX_USERS)
                throw std::runtime_error("Maximum of eight profiles reached");
            const auto newUuid = Common::UUID::MakeRandom();
            if (manager.CreateNewUser(newUuid, name.toStdString()).IsFailure())
                throw std::runtime_error("Could not create profile");
            manager.WriteUserSaveFile();
        } else {
            if (!uuid) throw std::runtime_error("Profile was not found");
            Service::Account::ProfileBase base{};
            if (!manager.GetProfileBase(*uuid, base)) throw std::runtime_error("Profile was not found");
            base.username.fill(0);
            std::copy(name.begin(), name.end(), base.username.begin());
            if (!manager.SetProfileBase(*uuid, base)) throw std::runtime_error("Could not rename profile");
            manager.WriteUserSaveFile();
        }
    } else if (type == "photo") {
        if (!uuid) throw std::runtime_error("Profile was not found");
        const auto bytes = QByteArray::fromBase64(action.value("jpeg").toString().toLatin1());
        if (bytes.size() < 4 || bytes.size() > 1024 * 1024 || !bytes.startsWith(QByteArray::fromHex("ffd8ff")))
            throw std::runtime_error("Invalid profile JPEG");
        const auto folder = dataDir + "/nand/system/save/8000000000000010/su/avators";
        if (QFileInfo(folder).isSymLink() || !QDir().mkpath(folder)) throw std::runtime_error("Cannot create private avatar folder");
        QSaveFile file(folder + "/" + QString::fromStdString(uuid->FormattedString()) + ".jpg");
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            throw std::runtime_error("Could not save profile photo");
    } else throw std::runtime_error("Unsupported profile action");
}
bool SuyuAdapter::validateFirmware(const QString& path) {
    FileSys::RealVfsFilesystem vfs;
    const auto file = vfs.OpenFile(path.toStdString(), FileSys::OpenMode::Read);
    if (!file) throw std::runtime_error("Cannot open firmware NCA");
    const FileSys::NCA nca(file);
    if (nca.GetStatus() != Loader::ResultStatus::Success)
        throw std::runtime_error("Firmware NCA cannot be read with the available keys (loader status " + std::to_string(int(nca.GetStatus())) + ")");
    return (nca.GetTitleId() & 0xffffffffffff0000ULL) == 0x0100000000000000ULL;
}
QJsonArray SuyuAdapter::audioDevices() {
    QJsonArray devices{"auto"};
    for (const auto& name : AudioCore::Sink::GetDeviceListForSink(Settings::AudioEngine::Cubeb, false)) {
        const auto device = QString::fromStdString(name);
        if (!device.isEmpty() && !devices.contains(device)) devices.append(device);
    }
    return devices;
}
QJsonArray SuyuAdapter::vulkanDevices() {
    QJsonArray devices;
    try {
        const auto library = Vulkan::OpenLibrary();
        Vulkan::vk::InstanceDispatch dispatch;
        const auto instance = Vulkan::CreateInstance(*library, dispatch, VK_API_VERSION_1_1);
        for (const auto raw : instance.EnumeratePhysicalDevices()) {
            const Vulkan::vk::PhysicalDevice device(raw, dispatch);
            devices.append(QString::fromUtf8(device.GetProperties().deviceName));
        }
    } catch (const std::exception& error) {
        qWarning() << "Could not enumerate Vulkan devices:" << error.what();
    }
    return devices;
}
void SuyuAdapter::configureAudio(int volume, const QString& device) {
    if (volume < 0 || volume > 100 || device.size() > 256 || device.contains(QChar(0)))
        throw std::runtime_error("Invalid audio settings");
    Settings::values.volume.SetValue(u8(volume));
    Settings::values.audio_output_device_id.SetValue(device.isEmpty() ? std::string{"auto"} : device.toStdString());
}
void SuyuAdapter::setAudioVolume(int volume) {
    if (!loaded_ || volume < 0 || volume > 100) throw std::runtime_error("Invalid live volume");
    Settings::values.volume.SetValue(u8(volume));
    const float gain = float(volume) / 100.0f;
    auto& sink = system_->AudioCore().GetOutputSink();
    sink.SetSystemVolume(gain);
    sink.SetDeviceVolume(gain);
}
void SuyuAdapter::setSpeedLimit(int percent) {
    if (percent < 50 || percent > 200) throw std::runtime_error("Invalid speed limit");
    Settings::values.use_speed_limit.SetValue(true);
    Settings::values.speed_limit.SetValue(u16(percent));
}
QJsonObject SuyuAdapter::installContent(const QString& path) {
    const QFileInfo info(path);
    if (!info.isAbsolute() || !info.isFile() || !info.isReadable() || info.suffix().compare("nsp", Qt::CaseInsensitive) != 0)
        throw std::runtime_error("A readable absolute NSP is required");
    initialize();
    if (!system_->GetFilesystem()->OpenFile(info.absoluteFilePath().toStdString(), FileSys::OpenMode::Read))
        throw std::runtime_error("The NSP cannot be opened by the original virtual filesystem");
    const auto result = ContentManager::InstallNSP(*system_, *system_->GetFilesystem(),
                                                    info.absoluteFilePath().toStdString(),
                                                    [](size_t, size_t) { return false; });
    switch (result) {
    case ContentManager::InstallResult::Success: return {{"result", "success"}};
    case ContentManager::InstallResult::Overwrite: return {{"result", "overwrite"}};
    case ContentManager::InstallResult::BaseInstallAttempted: return {{"result", "base-install-rejected"}};
    default: return {{"result", "failure"}};
    }
}
void SuyuAdapter::initialize() {
    if (system_) return;
    auto system = std::make_unique<Core::System>();
    system->Initialize();
    Service::AM::Frontend::FrontendAppletSet applets;
    applets.software_keyboard = std::make_unique<ShireiaSoftwareKeyboard>(*system);
    system->SetFrontendAppletSet(std::move(applets));
    auto manual = std::make_unique<FileSys::ManualContentProvider>();
    system->RegisterContentProvider(FileSys::ContentProviderUnionSlot::FrontendManual, manual.get());
    auto filesystem = system->GetFilesystem();
    if (!filesystem) throw std::runtime_error("Original virtual filesystem is unavailable");
    system->GetFileSystemController().CreateFactories(*filesystem);
    if (!system->GetFileSystemController().GetSystemNANDContents() ||
        !system->GetFileSystemController().GetUserNANDContents() ||
        !system->GetFileSystemController().GetSDMCContents())
        throw std::runtime_error("Original NAND/SDMC content factories did not initialize");
    manualContent_ = std::move(manual);
    system_ = std::move(system);
}
void SuyuAdapter::shutdown() {
    if (system_ && loaded_) { system_->Pause(); system_->ShutdownMainProcess(); loaded_ = false; }
    system_.reset();
    manualContent_.reset();
    window_.reset();
    input_.reset();
    QMutexLocker lock(&frameMutex_); frame_.clear(); reusableFrame_.clear();
}
void SuyuAdapter::load(const QString& path, bool nullRenderer) {
    if (loaded_) throw std::runtime_error("A program is already loaded");
    // Delegate supported container formats to the original loader; apply NRO bounds checks locally.
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || !QFileInfo(path).isAbsolute()) throw std::runtime_error("A readable absolute game path is required");
    const auto suffix = QFileInfo(path).suffix().toLower();
    if (!QStringList{"nro","nso","nsp","xci"}.contains(suffix)) throw std::runtime_error("Unsupported program extension");
    if (suffix == "nro") {
    if (file.size() < 128 || file.size() > 256 * 1024 * 1024 || identify(path) != "NRO") throw std::runtime_error("Invalid NRO image");
    const auto header = file.read(128);
    const auto size = qFromLittleEndian<quint32>(header.constData() + 0x18);
    if (size < 128 || size > file.size() || (size & 4095) != 0) throw std::runtime_error("NRO image must be complete and page aligned for this prototype");
    // The original loader assumes segments and MOD0 offsets fit in the image.
    const auto mod = qFromLittleEndian<quint32>(header.constData() + 4);
    if (quint64(mod) + 28 > size) throw std::runtime_error("Invalid NRO module offset");
    for (int i = 0; i < 3; ++i) {
        const auto offset = qFromLittleEndian<quint32>(header.constData() + 0x20 + i * 8);
        const auto length = qFromLittleEndian<quint32>(header.constData() + 0x24 + i * 8);
        if ((offset & 4095) || (length & 4095) || quint64(offset) + length > size) throw std::runtime_error("Invalid NRO segments");
    }
    }
    Settings::values.renderer_backend = nullRenderer ? Settings::RendererBackend::Null : Settings::RendererBackend::Vulkan;
    Settings::values.sink_id = nullRenderer ? Settings::AudioEngine::Null : Settings::AudioEngine::Cubeb;
    Settings::values.program_args = std::string{};
    static bool logging = false;
    if (!logging) { Common::Log::Initialize(); Common::Log::Start(); logging = true; }
    input_ = std::make_unique<InputBridge>();
    initialize();
    if (suffix == "nsp" || suffix == "xci") {
        manualContent_->ClearAllEntries();
        const auto container = system_->GetFilesystem()->OpenFile(path.toStdString(), FileSys::OpenMode::Read);
        if (!container || !manualContent_->AddEntriesFromContainer(container))
            qWarning() << "Original content provider could not register container entries; Load will report the actual loader result";
    }
    for (std::size_t player = 0; player < 2; ++player) {
        auto* controller = system_->HIDCore().GetEmulatedControllerByIndex(player);
        controller->SetNpadStyleIndex(Core::HID::NpadStyleIndex::Fullkey);
        controller->Connect(); controller->ReloadInput();
    }
    system_->RegisterHostThread();
    qInfo() << "Original system initialized; preparing Load";
    window_ = std::make_unique<Window>(Settings::values.use_docked_mode.GetValue() == Settings::ConsoleMode::Docked);
    auto deliverFrame = [this](std::span<const std::uint8_t> data) {
        ++compositorFrames_;
        const auto& renderer = system_->GPU().Renderer();
        const auto layout = window_->GetFramebufferLayout();
        if (renderer.GetHeadlessWidth() != layout.width || renderer.GetHeadlessHeight() != layout.height || data.size() != std::size_t(layout.width) * layout.height * 4) return;
        // Temporary readback bridge: real headless compositor output, not screenshot APIs.
        QElapsedTimer copyTimer; copyTimer.start();
        QByteArray pixels;
        {
            QMutexLocker lock(&frameMutex_);
            pixels = std::move(reusableFrame_);
        }
        if (pixels.capacity() >= qsizetype(data.size())) ++reusedFrames_;
        pixels.resize(int(data.size()));
        bgraToRgba(data.data(), reinterpret_cast<u8*>(pixels.data()), data.size());
        frameCopyMicros_ += quint64(copyTimer.nsecsElapsed() / 1000);
        ++copiedFrames_;
        {
            QMutexLocker lock(&frameMutex_);
            if (!frame_.isEmpty() && reusableFrame_.isEmpty()) reusableFrame_ = std::move(frame_);
            frame_ = std::move(pixels);
        }
        if (frameAvailable_) frameAvailable_();
    };
    Service::AM::FrontendAppletParameters parameters{};
    parameters.applet_id = Service::AM::AppletId::Application;
    parameters.applet_type = Service::AM::AppletType::Application;
    parameters.launch_type = Service::AM::LaunchType::FrontendInitiated;
    const auto result = system_->Load(*window_, path.toStdString(), parameters);
    qInfo() << "Original Load returned" << int(result);
    if (result != Core::SystemResultStatus::Success) {
        std::string reason = "Original Core::System::Load failed: " + std::to_string(int(result));
        if (result > Core::SystemResultStatus::ErrorLoader)
            reason += " (" + Loader::GetResultStatusString(Loader::ResultStatus(u32(result) - u32(Core::SystemResultStatus::ErrorLoader))) + ")";
        throw std::runtime_error(reason);
    }
    // Keep the original bridge available for controlled A/B and troubleshooting.
    if (Settings::values.renderer_backend.GetValue() != Settings::RendererBackend::Vulkan ||
        qEnvironmentVariableIntValue("SHIREIA_LEGACY_FRAME_COPY") == 1) {
        window_->frameReady = [this, deliverFrame] {
            deliverFrame(system_->GPU().Renderer().GetLastRenderedFrame());
        };
    } else {
        Shireia::SetHeadlessFrameSink(&system_->GPU().Renderer(), std::move(deliverFrame));
    }
    if (Settings::values.use_disk_shader_cache.GetValue()) {
        auto& gpu = system_->GPU();
        gpu.ObtainContext();
        try {
            system_->Renderer().ReadRasterizer()->LoadDiskResources(
                system_->GetApplicationProcessProgramID(), std::stop_token{},
                [](VideoCore::LoadCallbackStage, std::size_t, std::size_t) {});
        } catch (const std::exception& error) {
            qWarning() << "Disk shader cache loading failed; continuing without cached pipelines:" << error.what();
        } catch (...) {
            qWarning() << "Disk shader cache loading failed; continuing without cached pipelines";
        }
        gpu.ReleaseContext();
    }
    loaded_ = true;
    guestExited_ = false;
    system_->RegisterExitCallback([this] { guestExited_ = true; });
    system_->GPU().Start();
    system_->GetCpuManager().OnGpuReady();
    qInfo() << "Original GPU/CPU managers ready";
}
void SuyuAdapter::run() { if (!loaded_) throw std::runtime_error("No program loaded"); system_->Run(); }
void SuyuAdapter::pause() { if (!loaded_) throw std::runtime_error("No program loaded"); system_->Pause(); }
QByteArray SuyuAdapter::takeFrame() { QMutexLocker lock(&frameMutex_); return std::exchange(frame_,{}); }
void SuyuAdapter::recycleFrame(QByteArray frame) {
    if (frame.isEmpty()) return;
    QMutexLocker lock(&frameMutex_);
    if (reusableFrame_.isEmpty()) reusableFrame_ = std::move(frame);
}
quint32 SuyuAdapter::observedButtons() const { return loaded_ ? quint32(system_->HIDCore().GetEmulatedControllerByIndex(0)->GetNpadButtons().raw) : 0; }
QJsonObject SuyuAdapter::threadSnapshot() const {
    QJsonObject result;
    result["selectedUserIndex"] = s32(Settings::values.current_user);
    if (!loaded_ || !system_->ApplicationProcess()) return result;
    int initialized=0, waiting=0, runnable=0, terminated=0;
    int sleep=0, ipc=0, synchronization=0, condition=0, arbitration=0, other=0;
    QJsonArray details;
    std::shared_ptr<Service::AM::Applet> applet;
    if (auto* windowSystem = system_->GetAppletManager().GetWindowSystem()) applet = windowSystem->GetMainApplet();
    for (auto& thread : system_->ApplicationProcess()->GetThreadList()) {
        const auto& context = thread.GetContext();
        QJsonArray stack;
        QJsonArray matchingEvents;
        if (applet) {
            for (auto* object : thread.GetSynchronizationObjectBuffer()) {
                if (object == static_cast<Kernel::KSynchronizationObject*>(applet->health_warning_disappeared_system_event.GetHandle())) matchingEvents.append("healthWarningDisappeared");
                if (object == static_cast<Kernel::KSynchronizationObject*>(applet->sleep_lock_event.GetHandle())) matchingEvents.append("sleepLock");
                if (object == static_cast<Kernel::KSynchronizationObject*>(applet->lifecycle_manager.GetSystemEvent().GetHandle())) matchingEvents.append("systemMessage");
            }
        }
        for (const auto& frame : Core::GetBacktrace(&thread)) {
            if (stack.size() >= 16) break;
            stack.append(QJsonObject{{"module",QString::fromStdString(frame.module)},
                {"name",QString::fromStdString(frame.name)},
                {"address",QString::number(frame.address,16)},
                {"offset",QString::number(frame.offset,16)}});
        }
        details.append(QJsonObject{{"id",QString::number(thread.GetThreadId())},
            {"pc",QString::number(context.pc,16)},{"lr",QString::number(context.lr,16)},
            {"state",int(thread.GetState())},{"waitReason",int(thread.GetWaitReasonForDebugging())},
            {"lastScheduledTick",QString::number(thread.GetLastScheduledTick())},{"stack",stack},
            {"matchingEvents",matchingEvents}});
        switch (thread.GetState()) {
        case Kernel::ThreadState::Initialized: ++initialized; break;
        case Kernel::ThreadState::Waiting: ++waiting; break;
        case Kernel::ThreadState::Runnable: ++runnable; break;
        case Kernel::ThreadState::Terminated: ++terminated; break;
        default: ++other; break;
        }
        switch (thread.GetWaitReasonForDebugging()) {
        case Kernel::ThreadWaitReasonForDebugging::Sleep: ++sleep; break;
        case Kernel::ThreadWaitReasonForDebugging::IPC: ++ipc; break;
        case Kernel::ThreadWaitReasonForDebugging::Synchronization: ++synchronization; break;
        case Kernel::ThreadWaitReasonForDebugging::ConditionVar: ++condition; break;
        case Kernel::ThreadWaitReasonForDebugging::Arbitration: ++arbitration; break;
        default: break;
        }
    }
    result["initialized"]=initialized; result["waiting"]=waiting; result["runnable"]=runnable;
    result["terminated"]=terminated; result["sleep"]=sleep; result["ipc"]=ipc;
    result["synchronization"]=synchronization; result["conditionVar"]=condition;
    result["arbitration"]=arbitration; result["other"]=other;
    result["details"]=details;
    return result;
}
void SuyuAdapter::input(int player, quint32 buttons, float lx, float ly, float rx, float ry) {
    if (!loaded_ || !input_ || player < 0 || player > 1) return;
    for (int i=0;i<20;++i) input_->pad->SetButtonState(std::size_t(player),i,(buttons & (1u<<i)) != 0);
    input_->pad->SetStickPosition(std::size_t(player),0,lx,ly); input_->pad->SetStickPosition(std::size_t(player),1,rx,ry);
}
QJsonObject SuyuAdapter::performanceSnapshot() const {
    return {{"timestampMs", qint64(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count())},
            {"compositorFrames", qint64(compositorFrames())},
            {"frameCopyMicros", qint64(frameCopyMicros())},
            {"copiedFrames", qint64(copiedFrames())},
            {"reusedFrames", qint64(reusedFrames())},
            {"vulkanFinishMicros", qint64(ShireiaHeadlessFinishMicros())},
            {"vulkanFinishSamples", qint64(ShireiaHeadlessFinishSamples())},
            {"readbackAllocationMicros", qint64(ShireiaHeadlessAllocationMicros())},
            {"readbackCommandMicros", qint64(ShireiaHeadlessRecordMicros())},
            {"readbackInvalidateMicros", qint64(ShireiaHeadlessInvalidateMicros())},
            {"headlessFrameSequence", qint64(ShireiaHeadlessFrameSequence())}};
}
quint32 SuyuAdapter::testCounter() const {
    if (!loaded_ || !system_->ApplicationProcess()) return 0;
    auto* process = system_->ApplicationProcess();
    return process->GetMemory().Read32(Common::ProcessAddress{GetInteger(process->GetEntryPoint()) + 0x2000});
}
QByteArray SuyuAdapter::counterProgram() {
    // Authored AArch64 program: increment a word in its own data segment forever.
    // No ROM, firmware, service SDK or copyrighted game content is used.
    QByteArray image(0x3000, '\0');
    auto word = [&](int offset, quint32 value) { qToLittleEndian(value, image.data() + offset); };
    word(0, 0x14000020); image.replace(0x10, 4, "NRO0"); word(0x18, image.size());
    for (int i = 0; i < 3; ++i) { word(0x20 + i * 8, i * 0x1000); word(0x24 + i * 8, 0x1000); }
    word(0x80, 0x1000fc02); // adr x2, data at 0x2000
    word(0x84, 0xb9400041); // ldr w1, [x2]
    word(0x88, 0x11000421); // add w1, w1, #1
    word(0x8c, 0xb9000041); // str w1, [x2]
    word(0x90, 0xd284e200); // mov x0, #10000 (nanoseconds)
    word(0x94, 0xd4000161); // svc #0x0b: SleepThread(10000), timed cooperative wait
    word(0x98, 0x17fffffa); // b 0x80
    return image;
}
QString SuyuAdapter::identify(const QString& path) {
    FileSys::RealVfsFilesystem vfs;
    const auto file = vfs.OpenFile(path.toStdString(), FileSys::OpenMode::Read);
    if (!file) throw std::runtime_error("Cannot read program");
    // Identify from the real file. Container probes need offsets outside the first 4 KiB.
    // This does not claim that decryption, Load, execution or rendering succeeded.
    const auto type = Loader::IdentifyFile(file);
    return QString::fromStdString(Loader::GetFileTypeString(type));
}
