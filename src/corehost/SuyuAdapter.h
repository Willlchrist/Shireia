#pragma once
#include <memory>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <atomic>
#include <functional>
namespace Core { class System; }
namespace Core::Frontend { class EmuWindow; }
namespace FileSys { class ManualContentProvider; }
class SuyuAdapter final {
public:
    SuyuAdapter();
    ~SuyuAdapter();
    void initialize();
    void shutdown();
    QString identify(const QString& path);
    void load(const QString& path, bool nullRenderer = false);
    static void configureGraphics(int resolution, bool shaderCache, bool docked, int vsync, bool asynchronousShaders,
                                  bool asynchronousGpu, bool pipelineCache, int vulkanDevice,
                                  int gpuAccuracy, int astcDecode, int anisotropy, int scalingFilter,
                                  int antiAliasing, int vramUsage, int astcRecompression,
                                  int aspectRatio, int fsrSharpness, int nvdec, bool reactiveFlushing,
                                  int dmaAccuracy, int gpuFenceBehavior);
    static void configureCore(int language, int region, bool multicore, int cpuAccuracy, int memoryLayout,
                              int cpuOverclock, bool customCpuTicks, int cpuTicks);
    static void configureGraphicsHacks(bool fastGpuTime, int fastGpuDivisor, bool skipCpuInvalidation);
    static void configurePresentation(int scalingFilter, int antiAliasing, int fsrSharpness);
    void selectUser(const QString& uuid);
    void run();
    void pause();
    quint32 testCounter() const;
    static QByteArray counterProgram();
    static void configurePaths(const QString& dataDir);
    QJsonObject metadata(const QString& path);
    static QJsonArray parseCheatText(const QByteArray& text);
    QJsonArray profiles(const QString& dataDir);
    void editProfile(const QJsonObject& action, const QString& dataDir);
    static bool validateFirmware(const QString& path);
    QJsonObject installContent(const QString& path);
    static QJsonArray audioDevices();
    static QJsonArray vulkanDevices();
    void configureAudio(int volume, const QString& device);
    void setAudioVolume(int volume);
    void setSpeedLimit(int percent);
    QByteArray takeFrame();
    void recycleFrame(QByteArray frame);
    void setFrameAvailableCallback(std::function<void()> callback) { frameAvailable_ = std::move(callback); }
    quint64 compositorFrames() const { return compositorFrames_.load(); }
    quint64 frameCopyMicros() const { return frameCopyMicros_.load(); }
    quint64 copiedFrames() const { return copiedFrames_.load(); }
    quint64 reusedFrames() const { return reusedFrames_.load(); }
    QJsonObject performanceSnapshot() const;
    void input(int player, quint32 buttons, float lx, float ly, float rx, float ry);
    quint32 observedButtons() const;
    QJsonObject threadSnapshot() const;
    bool guestExited() const { return guestExited_.load(); }
private:
    std::unique_ptr<Core::System> system_;
    std::unique_ptr<FileSys::ManualContentProvider> manualContent_;
    class Window;
    std::unique_ptr<Window> window_;
    bool loaded_ = false;
    class InputBridge;
    std::unique_ptr<InputBridge> input_;
    QMutex frameMutex_;
    QByteArray frame_;
    QByteArray reusableFrame_;
    std::function<void()> frameAvailable_;
    std::atomic<quint64> compositorFrames_{0}, frameCopyMicros_{0}, copiedFrames_{0}, reusedFrames_{0};
    std::atomic_bool guestExited_{false};
};
int runLibraryJob(const QString& requestFile, const QString& dataDir);
