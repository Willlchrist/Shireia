#pragma once
#include <QObject>
#include <QProcess>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>
#include <QTimer>
#include <QPointer>
#include <QVariantList>
#include <QElapsedTimer>
#include <QLockFile>
#include <memory>
#include "../ipc/JsonChannel.h"

class EmulationManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool coreAvailable READ coreAvailable CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(qint64 processId READ processId NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap capabilities READ capabilities NOTIFY stateChanged)
    Q_PROPERTY(QStringList log READ log NOTIFY logChanged)
    Q_PROPERTY(QVariantList exits READ exits NOTIFY exitsChanged)
    Q_PROPERTY(bool gpuDiagnostic READ gpuDiagnostic NOTIFY gpuDescriptorChanged)
    Q_PROPERTY(bool gameLoaded READ gameLoaded NOTIFY gpuDescriptorChanged)
    Q_PROPERTY(QString gamePath READ gamePath NOTIFY stateChanged)
    Q_PROPERTY(int lastHidButtons READ lastHidButtons NOTIFY hidChanged)
    Q_PROPERTY(QStringList audioDevices READ audioDevices NOTIFY audioDevicesChanged)
    Q_PROPERTY(QStringList vulkanDevices READ vulkanDevices NOTIFY vulkanDevicesChanged)
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(int speedLimit READ speedLimit NOTIFY speedLimitChanged)
    Q_PROPERTY(bool performanceCaptureActive READ performanceCaptureActive NOTIFY performanceCaptureChanged)
    Q_PROPERTY(QString performanceCapturePath READ performanceCapturePath NOTIFY performanceCaptureChanged)
    Q_PROPERTY(QVariantMap performanceMetrics READ performanceMetrics NOTIFY performanceMetricsChanged)
    Q_PROPERTY(QString graphicsError READ graphicsError NOTIFY graphicsErrorChanged)
    Q_PROPERTY(QString diagnosticsExportPath READ diagnosticsExportPath NOTIFY diagnosticsExported)
    Q_PROPERTY(bool stallProbeEnabled READ stallProbeEnabled WRITE setStallProbeEnabled NOTIFY stallProbeEnabledChanged)
public:
    explicit EmulationManager(QObject* parent = nullptr, QString executable = {}, QString dataDir = {});
    ~EmulationManager() override;
    QString state() const { return state_; }
    QString message() const { return message_; }
    bool coreAvailable() const;
    bool active() const { return process_.state() != QProcess::NotRunning; }
    qint64 processId() const { return process_.processId(); }
    QVariantMap capabilities() const { return capabilities_.toVariantMap(); }
    QStringList log() const { return log_; }
    QVariantList exits() const { return exits_; }
    bool gpuDiagnostic() const { return !gpuDescriptor_.isEmpty(); }
    bool gameLoaded() const { return gpuDescriptor_["content"] == "emulation-headless-readback"; }
    QString gamePath() const { return currentGamePath_; }
    int lastHidButtons() const { return lastHidButtons_; }
    QStringList audioDevices() const { return audioDevices_; }
    QStringList vulkanDevices() const { return vulkanDevices_; }
    int volume() const { return volume_; }
    int speedLimit() const { return speedLimit_; }
    bool performanceCaptureActive() const { return performanceCaptureActive_; }
    QString performanceCapturePath() const { return performanceCapturePath_; }
    QVariantMap performanceMetrics() const { return performanceMetrics_; }
    QString graphicsError() const { return graphicsError_; }
    QString diagnosticsExportPath() const { return diagnosticsExportPath_; }
    bool stallProbeEnabled() const { return stallProbeEnabled_; }
    void setStallProbeEnabled(bool enabled);
    QJsonObject gpuDescriptor() const { return gpuDescriptor_; }
    Q_INVOKABLE void startCore();
    Q_INVOKABLE bool startGame(const QString& path);
    Q_INVOKABLE bool identifyFile(const QString& path);
    void sendInput(quint32 buttons, float lx, float ly, float rx, float ry);
    void sendPlayerInput(int player, quint32 buttons, float lx, float ly, float rx, float ry);
    Q_INVOKABLE void stopGame();
    Q_INVOKABLE void pauseGame();
    Q_INVOKABLE void resumeGame();
    Q_INVOKABLE void queryState();
    Q_INVOKABLE void startGpuDiagnostic();
    Q_INVOKABLE void startPerformanceCapture();
    Q_INVOKABLE void stopPerformanceCapture();
    Q_INVOKABLE bool exportDiagnostics();
    Q_INVOKABLE void recordGraphicsError(const QString& error);
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void setSpeedLimit(int percent);
    Q_INVOKABLE bool setPresentationSettings(int scalingFilter, int antiAliasing, int fsrSharpness);
signals:
    void stateChanged();
    void messageChanged();
    void logChanged();
    void exitsChanged();
    void failed();
    void identified(const QString& format);
    void gpuDescriptorChanged();
    void gpuFrame(int slot, int sequence);
    void hidChanged();
    void audioDevicesChanged();
    void vulkanDevicesChanged();
    void volumeChanged();
    void speedLimitChanged();
    void performanceCaptureChanged();
    void performanceMetricsChanged();
    void frameIntervalReceived(double milliseconds);
    void graphicsErrorChanged();
    void diagnosticsExported();
    void stallProbeEnabledChanged();
private:
    struct Request { QString command; qint64 deadline; };
    QProcess process_;
    QLocalServer server_;
    QPointer<QLocalSocket> socket_;
    QPointer<ShireiaIpc::JsonChannel> channel_;
    QTimer startup_, shutdown_, requestsTimer_;
    QMap<int, Request> pending_;
    QString executable_, dataDir_, state_ = "Idle", message_, session_, token_, pendingPath_;
    QString currentGamePath_;
    QString lastLoadError_;
    QJsonObject capabilities_;
    QJsonObject gpuDescriptor_;
    int gpuSequence_ = 0;
    qint64 metricsStartMs_ = 0, lastCoreFrames_ = 0, transferMicros_ = 0;
    qint64 lastCopyUs_ = 0, lastCopiedFrames_ = 0, lastIpcSendUs_ = 0, lastIpcSendCount_ = 0;
    int lastTransportSequence_ = 0, transferSamples_ = 0;
    QStringList log_;
    QVariantList exits_;
    int sequence_ = 0;
    bool authenticated_ = false, expectedExit_ = false, failed_ = false;
    bool pendingLoad_ = false;
    std::unique_ptr<QLockFile> resourceLock_;
    int lastHidButtons_ = 0;
    int volume_ = 100;
    int speedLimit_ = 100;
    bool performanceCaptureActive_ = false;
    bool stallProbeEnabled_ = false;
    bool restartCoreAfterStop_ = false;
    QString performanceCapturePath_;
    QVariantMap performanceMetrics_{{"compositorFps",0.0},{"publishedFps",0.0},{"intervalMs",0.0},
        {"internalScale","—"},{"outputResolution","—"},{"backend","Vulkan → CPU readback → D3D11"}};
    QString graphicsError_;
    QString diagnosticsExportPath_;
    QElapsedTimer frameArrivalClock_;
    double metricsWindowMs_ = 0;
    qint64 metricsWindowCoreFrames_ = 0;
    int metricsWindowSequence_ = 0;
    QStringList audioDevices_{"auto"};
    QStringList vulkanDevices_{"Detectando GPUs Vulkan..."};
    void accept();
    void receive(const QJsonObject& message);
    int request(const QString& command, QJsonObject fields = {});
    void setState(const QString& state);
    void setMessage(const QString& message);
    void appendLog(const QString& message);
    void fail(const QString& message);
    void probePending();
    bool queueProgram(const QString& path, bool load);
};
