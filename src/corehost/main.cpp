#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QTimer>
#include <QTemporaryDir>
#include <QDebug>
#include <QThread>
#include <QElapsedTimer>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>
#include <atomic>
#include "SuyuAdapter.h"
#include "../ipc/JsonChannel.h"
#include "../graphics/GpuProducer.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("ShireiaCore");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({{"server", "Local server", "name"}, {"shireia-session", "Session ID", "id"},
        {"data-dir", "Private core data directory", "path"}, {"self-test", "Initialize and destroy the real Core::System"},
        {"execution-self-test", "Run authored AArch64 counter program; no graphical output"}, {"vulkan-test", "Use the original headless Vulkan renderer in execution self-test"},
        {"library-job", "Import system resources and read real game metadata", "request"},
        {"validate-cheat", "Validate a Suyu text cheat file with the original parser", "path"},
        {"program-test", "Execute a provided authorized program and verify real frames", "path"},
        {"user-uuid", "Select an existing emulated user for --program-test", "uuid"},
        {"test-seconds", "Measured duration for --program-test (1-120 seconds)", "seconds", "10"},
        {"test-warmup-seconds", "Unmeasured warm-up before --program-test sampling (0-120 seconds)", "seconds", "10"},
        {"test-shireia-defaults", "Use the default Shireia graphics/core settings for --program-test"},
        {"test-shireia-docked", "Use the Shireia 1x docked graphics/core settings for --program-test"},
        {"test-no-input", "Do not inject controller input during --program-test"},
        {"test-wonder-start", "Press Mario Wonder L+R after boot to leave the title screen"}});
    parser.process(app);
    QTemporaryDir selfTestData;
    const auto data = parser.isSet("data-dir") ? parser.value("data-dir") :
        (parser.isSet("self-test") || parser.isSet("execution-self-test") || parser.isSet("program-test") ? selfTestData.path() : QString{});
    if (data.isEmpty() || !QFileInfo(data).isAbsolute() || !QDir().mkpath(data + "/roaming")) return 64;
    // The original path manager migrates legacy directories; isolate its APPDATA
    // before any original API, so user installations and keys are never touched.
    qputenv("APPDATA", QDir::toNativeSeparators(data + "/roaming").toUtf8());
    try { SuyuAdapter::configurePaths(data); }
    catch (const std::exception& error) { qCritical() << error.what(); return 1; }
    if (parser.isSet("validate-cheat")) {
        QFile file(parser.value("validate-cheat"));
        if (!QFileInfo(file.fileName()).isAbsolute() || !file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > 1024 * 1024) return 64;
        const auto parsed = SuyuAdapter::parseCheatText(file.readAll());
        QTextStream(stdout) << QJsonDocument(parsed).toJson(QJsonDocument::Compact) << Qt::endl;
        return parsed.isEmpty() ? 2 : 0;
    }
    if (parser.isSet("library-job")) return runLibraryJob(parser.value("library-job"), data);
    SuyuAdapter adapter;
    GpuProducer producer;
    if (parser.isSet("program-test")) {
        try {
            bool durationOk = false, warmupOk = false;
            const int durationSeconds = parser.value("test-seconds").toInt(&durationOk);
            const int warmupSeconds = parser.value("test-warmup-seconds").toInt(&warmupOk);
            if (!durationOk || durationSeconds < 1 || durationSeconds > 120 ||
                !warmupOk || warmupSeconds < 0 || warmupSeconds > 120) return 64;
            if (parser.isSet("test-shireia-defaults")) {
                // Match SettingsStore::initial() for a reproducible Shireia-default run.
                adapter.configureGraphics(3, true, false, 2, true, true, true, 0,
                    1, 1, 0, 1, 0, 0, 0, 0, 25, 2, true, 0, 0);
                adapter.configureCore(1, 1, true, 0, 0, 0, false, 16000);
                adapter.configureGraphicsHacks(false, 1, false);
            } else if (parser.isSet("test-shireia-docked")) {
                // Match the 1x docked settings captured from the user's Shireia session.
                adapter.configureGraphics(3, true, true, 2, true, true, true, 0,
                    1, 1, 0, 1, 0, 1, 0, 0, 25, 2, true, 0, 0);
                adapter.configureCore(1, 1, true, 0, 0, 0, false, 16000);
                adapter.configureGraphicsHacks(false, 1, false);
            }            adapter.selectUser(parser.value("user-uuid"));
            adapter.load(QFileInfo(parser.value("program-test")).absoluteFilePath()); adapter.run();
            QElapsedTimer timer; timer.start(); int sampledFrames = 0, measuredFrames = 0;
            QByteArray first, last;
            quint64 measuredFrameBase = 0, measuredCopyBase = 0;
            bool measuring = false;
            while (timer.elapsed() < (warmupSeconds + durationSeconds) * 1000) {
                const auto elapsed = timer.elapsed();
                if (!measuring && elapsed >= warmupSeconds * 1000) {
                    measuring = true;
                    measuredFrameBase = adapter.compositorFrames();
                    measuredCopyBase = adapter.frameCopyMicros();
                }
                auto frame = adapter.takeFrame();
                if (!frame.isEmpty() && measuring) {
                    if (first.isEmpty()) first = frame;
                    last = frame;
                    ++sampledFrames;
                }
                const int inputStartMs = parser.isSet("test-wonder-start") ? 8000 : 2500;
                const quint32 startButtons = parser.isSet("test-wonder-start") ? ((1u << 6) | (1u << 7) | (1u << 0)) : (1u << 3);
                if (!parser.isSet("test-no-input") && elapsed >= inputStartMs && elapsed < inputStartMs + 500) adapter.input(0,startButtons,0,0,0,0);
                else adapter.input(0,0,0,0,0,0);
                QThread::msleep(20);
            }
            const int measuredMs = qMax(1, timer.elapsed() - warmupSeconds * 1000);
            measuredFrames = int(adapter.compositorFrames() - measuredFrameBase);
            const auto measuredCopyMicros = adapter.frameCopyMicros() - measuredCopyBase;
            qInfo() << "Program test warm-up ms:" << warmupSeconds * 1000
                    << "measurement ms:" << measuredMs
                    << "measured compositor frames:" << measuredFrames
                    << "sampled frames:" << sampledFrames
                    << "steady compositor FPS:" << double(measuredFrames) * 1000.0 / measuredMs
                    << "copy ms/frame:" << double(measuredCopyMicros) / qMax(1, measuredFrames) / 1000.0;
            QFile before(data + "/program-before.rgba");
            if (before.open(QIODevice::WriteOnly)) before.write(first);
            QFile after(data + "/program-after.rgba");
            if (after.open(QIODevice::WriteOnly)) after.write(last);
            adapter.pause();
            QFile diagnostic(data + "/program-diagnostic.json");
            if (diagnostic.open(QIODevice::WriteOnly))
                diagnostic.write(QJsonDocument(QJsonObject{{"warmupMs",warmupSeconds * 1000},
                    {"measurementMs",measuredMs},{"sampledFrames",sampledFrames},
                    {"measuredCompositorFrames",measuredFrames},
                    {"compositorFramesTotal",qint64(adapter.compositorFrames())},
                    {"measuredFrameCopyMicros",qint64(measuredCopyMicros)},
                    {"frameCopyMicrosTotal",qint64(adapter.frameCopyMicros())},
                    {"width",last.size() == 1920 * 1080 * 4 ? 1920 : 1280},
                    {"height",last.size() == 1920 * 1080 * 4 ? 1080 : 720},
                    {"threads",adapter.threadSnapshot()}}).toJson());
            adapter.run(); QThread::msleep(100); adapter.shutdown();
            return measuredFrames > 5 ? 0 : 2;
        } catch (const std::exception& error) { qCritical() << error.what(); return 1; }
    }
    if (parser.isSet("execution-self-test")) {
        try {
            QFile file(data + "/authored-counter.nro");
            if (!file.open(QIODevice::WriteOnly)) return 1;
            file.write(SuyuAdapter::counterProgram()); file.close();
            adapter.load(file.fileName(), !parser.isSet("vulkan-test"));
            qInfo() << "Execution self-test: run";
            adapter.run();
            QThread::msleep(200);
            qInfo() << "Execution self-test: pause"; adapter.pause();
            const auto first = adapter.testCounter();
            QThread::msleep(50);
            const auto paused = adapter.testCounter();
            adapter.run(); QThread::msleep(100); adapter.pause();
            const auto resumed = adapter.testCounter();
            adapter.shutdown();
            qInfo() << "Guest counter (run/paused/resumed):" << first << paused << resumed;
            if (!first || first != paused || first == resumed) return 2;
            qInfo() << "PASS: actual AArch64 execution, pause, resume and shutdown; no game frames claimed";
            return 0;
        } catch (const std::exception& error) { qCritical() << error.what(); return 1; }
    }
    if (parser.isSet("self-test")) {
        try { adapter.initialize(); adapter.shutdown(); qInfo() << "PASS: original Core::System initialize/destruct"; return 0; }
        catch (const std::exception& error) { qCritical() << error.what(); return 1; }
    }
    const auto session = parser.value("shireia-session");
    const auto token = qEnvironmentVariable("SHIREIA_CORE_TOKEN");
    qunsetenv("SHIREIA_CORE_TOKEN");
    if (session.size() != 36 || token.size() != 64 || parser.value("server").isEmpty()) return 64;
    QLocalSocket socket;
    ShireiaIpc::JsonChannel channel(&socket);
    QString state = "Idle";
    int exitCode = 0;
    int lastId = 0;
    QTimer frameTimer;
    frameTimer.setInterval(33);
    QTimer exitWatchdog;
    exitWatchdog.setInterval(250);
    std::atomic_bool publishQueued{false};
    quint64 ipcSendMicros = 0, ipcSendCount = 0;
    bool perfCaptureActive = false;
    QFile perfCapture;
    QElapsedTimer perfCaptureClock;
    QElapsedTimer perfSampleClock;
    QJsonObject perfPrevious;
    QJsonObject perfConfig;
    quint64 perfPreviousIpcMicros = 0, perfPreviousIpcCount = 0;
    quint64 perfTransferMicros = 0, perfTransferCount = 0;
    quint64 perfPreviousTransferMicros = 0, perfPreviousTransferCount = 0;
    quint64 perfPublishedFrames = 0, perfPreviousPublishedFrames = 0;
    auto writePerfSample = [&] {
        if (!perfCaptureActive || !perfCapture.isOpen() || !perfSampleClock.isValid() || perfSampleClock.elapsed() < 1000) return;
        const auto current = adapter.performanceSnapshot();
        const qint64 elapsed = qMax<qint64>(1, current["timestampMs"].toInteger() - perfPrevious["timestampMs"].toInteger());
        const qint64 frames = current["compositorFrames"].toInteger() - perfPrevious["compositorFrames"].toInteger();
        const qint64 copied = current["copiedFrames"].toInteger() - perfPrevious["copiedFrames"].toInteger();
        const qint64 copyUs = current["frameCopyMicros"].toInteger() - perfPrevious["frameCopyMicros"].toInteger();
        const qint64 finishUs = current["vulkanFinishMicros"].toInteger() - perfPrevious["vulkanFinishMicros"].toInteger();
        const qint64 allocationUs = current["readbackAllocationMicros"].toInteger() - perfPrevious["readbackAllocationMicros"].toInteger();
        const qint64 commandUs = current["readbackCommandMicros"].toInteger() - perfPrevious["readbackCommandMicros"].toInteger();
        const qint64 invalidateUs = current["readbackInvalidateMicros"].toInteger() - perfPrevious["readbackInvalidateMicros"].toInteger();
        const qint64 finishCount = current["vulkanFinishSamples"].toInteger() - perfPrevious["vulkanFinishSamples"].toInteger();
        const quint64 transferMicros = perfTransferMicros - perfPreviousTransferMicros;
        const quint64 transferCount = perfTransferCount - perfPreviousTransferCount;
        const double seconds = elapsed / 1000.0;
        const QJsonObject sample{{"elapsedMs", perfCaptureClock.elapsed()},
            {"intervalMs", elapsed}, {"compositorFrames", frames},
            {"compositorFps", frames / seconds}, {"cpuCopyFrames", copied},
            {"cpuCopyMsPerFrame", copied ? double(copyUs) / copied / 1000.0 : 0.0},
            {"vulkanFinishSamples", finishCount},
            {"vulkanFinishMsPerFrame", finishCount ? double(finishUs) / finishCount / 1000.0 : 0.0},
            {"readbackAllocationMsPerFrame", finishCount ? double(allocationUs) / finishCount / 1000.0 : 0.0},
            {"readbackCommandMsPerFrame", finishCount ? double(commandUs) / finishCount / 1000.0 : 0.0},
            {"readbackInvalidateMsPerFrame", finishCount ? double(invalidateUs) / finishCount / 1000.0 : 0.0},
            {"completedReadbacks", current["headlessFrameSequence"].toInteger() - perfPrevious["headlessFrameSequence"].toInteger()},
            {"publishedFrames", qint64(perfPublishedFrames - perfPreviousPublishedFrames)},
            {"publishedFps", double(perfPublishedFrames - perfPreviousPublishedFrames) / seconds},
            {"d3dTransferSamples", qint64(transferCount)},
            {"d3dTransferMsPerFrame", transferCount ? double(transferMicros) / transferCount / 1000.0 : 0.0},
            {"ipcEvents", qint64(ipcSendCount - perfPreviousIpcCount)},
            {"ipcSendMsPerEvent", ipcSendCount > perfPreviousIpcCount
                ? double(ipcSendMicros - perfPreviousIpcMicros) / (ipcSendCount - perfPreviousIpcCount) / 1000.0 : 0.0}};
        perfCapture.write(QJsonDocument(sample).toJson(QJsonDocument::Compact) + '\n');
        perfCapture.flush();
        perfPrevious = current; perfPreviousIpcMicros = ipcSendMicros; perfPreviousIpcCount = ipcSendCount;
        perfPreviousTransferMicros = perfTransferMicros; perfPreviousTransferCount = perfTransferCount;
        perfPreviousPublishedFrames = perfPublishedFrames;
        perfSampleClock.restart();
    };
    auto publishFrame = [&] {
        try {
            writePerfSample();
            if ((state == "Running" || state == "Paused") && adapter.guestExited()) {
                frameTimer.stop(); exitWatchdog.stop(); auto event = ShireiaIpc::envelope("event",0,session); event["event"]="guest-exited";
                channel.send(event); return;
            }
            auto pixels = (state == "Running" || state == "Paused") ? adapter.takeFrame() : QByteArray{};
            const auto frame = producer.tick(pixels);
            adapter.recycleFrame(std::move(pixels));
            if (frame.isEmpty()) return;
            ++perfPublishedFrames;
            perfTransferMicros += quint64(qMax(0, frame["transferUs"].toInt()));
            ++perfTransferCount;
            auto message = ShireiaIpc::envelope("event", 0, session);
            message["event"] = "gpu-frame"; message["slot"] = frame["slot"]; message["sequence"] = frame["sequence"];
            message["generation"] = frame["generation"];
            message["coreFrames"] = qint64(adapter.compositorFrames());
            message["transferUs"] = frame["transferUs"];
            message["copyUsTotal"] = qint64(adapter.frameCopyMicros());
            message["copiedFrames"] = qint64(adapter.copiedFrames());
            message["reusedFrames"] = qint64(adapter.reusedFrames());
            message["ipcSendUsTotal"] = qint64(ipcSendMicros);
            message["ipcSendCount"] = qint64(ipcSendCount);
            QElapsedTimer sendTimer; sendTimer.start();
            if (!channel.send(message)) app.exit(68);
            ipcSendMicros += quint64(sendTimer.nsecsElapsed() / 1000);
            ++ipcSendCount;
        } catch (const std::exception& error) { qCritical() << error.what(); app.exit(69); }
    };
    adapter.setFrameAvailableCallback([&] {
        if (!publishQueued.exchange(true)) QMetaObject::invokeMethod(&app, [&] {
            publishQueued.store(false);
            if (state == "Running" || state == "Paused") publishFrame();
        }, Qt::QueuedConnection);
    });
    QObject::connect(&frameTimer, &QTimer::timeout, &app, publishFrame);
    QObject::connect(&exitWatchdog, &QTimer::timeout, &app, [&] {
        if ((state == "Running" || state == "Paused") && adapter.guestExited()) {
            exitWatchdog.stop();
            auto event = ShireiaIpc::envelope("event", 0, session); event["event"] = "guest-exited";
            channel.send(event);
        }
    });
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &app, [&] { app.exit(65); });
    deadline.start(10000);
    QObject::connect(&socket, &QLocalSocket::connected, &app, [&] {
        auto hello = ShireiaIpc::envelope("hello", 0, session);
        hello["token"] = token;
        hello["backend"] = "suyu-original-prebuilt";
        hello["capabilities"] = QJsonObject{{"initialize", true}, {"identify", true}, {"load", true},
            {"execute", true}, {"pause", true}, {"frames", true}, {"gameInput", true}, {"gpuDiagnostic", true}, {"nativeGpuFrames",false}};
        channel.send(hello);
    });
    QObject::connect(&socket, &QLocalSocket::disconnected, &app, [&] { adapter.shutdown(); app.exit(exitCode); });
    QObject::connect(&socket, &QLocalSocket::errorOccurred, &app, [&](QLocalSocket::LocalSocketError error) {
        if (error == QLocalSocket::PeerClosedError) { adapter.shutdown(); app.exit(exitCode); } else { exitCode = 66; app.exit(exitCode); }
    });
    QObject::connect(&channel, &ShireiaIpc::JsonChannel::invalid, &app, [&](const QString&) { exitCode = 67; app.exit(exitCode); });
    QObject::connect(&channel, &ShireiaIpc::JsonChannel::message, &app, [&](const QJsonObject& request) {
        if (!ShireiaIpc::validEnvelope(request, session) || request["type"] != "request" || !ShireiaIpc::validId(request["id"]) || request["id"].toInt() <= lastId || !request["command"].isString()) { socket.abort(); app.exit(67); return; }
        deadline.stop();
        lastId = request["id"].toInt();
        const auto command = request["command"].toString();
        auto response = ShireiaIpc::envelope("response", lastId, session);
        response["command"] = command;
        response["ok"] = true;
        response["state"] = state;
        try {
            if (command == "initialize") { adapter.initialize(); state = "Ready"; qInfo() << "Core::System original initialized; no application loaded"; }
            else if (command == "status") {}
            else if (command == "performance-capture-start") {
                if (state != "Running") throw std::runtime_error("Start the game before capturing performance");
                if (perfCaptureActive) throw std::runtime_error("A performance capture is already active");
                const QString stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss");
                const QString capturePath = QDir(data).filePath("performance-capture-" + stamp + ".jsonl");
                perfCapture.setFileName(capturePath);
                if (!perfCapture.open(QIODevice::WriteOnly | QIODevice::Append)) throw std::runtime_error("Cannot open performance capture file");
                perfCaptureActive = true; perfCaptureClock.start(); perfSampleClock.start();
                perfPrevious = adapter.performanceSnapshot(); perfPreviousIpcMicros = ipcSendMicros; perfPreviousIpcCount = ipcSendCount;
                perfPreviousTransferMicros = perfTransferMicros; perfPreviousTransferCount = perfTransferCount;
                perfPreviousPublishedFrames = perfPublishedFrames;
                perfCapture.write(QJsonDocument(QJsonObject{{"type","metadata"},{"startTimestampMs",perfPrevious["timestampMs"]},
                    {"readbackMode","sync-per-frame"},
                    {"stallProbeEnabled", qEnvironmentVariableIsSet("SUYU_STALL_PROBE") && qEnvironmentVariable("SUYU_STALL_PROBE") != "0"},
                    {"pipelineTimingEnabled", qEnvironmentVariableIsSet("SUYU_VK_PIPELINE_TIMING") && qEnvironmentVariable("SUYU_VK_PIPELINE_TIMING") != "0"},
                    {"graphicsSettings",perfConfig}}).toJson(QJsonDocument::Compact) + '\n');
                perfCapture.flush(); response["path"] = capturePath;
            }
            else if (command == "performance-capture-stop") {
                if (!perfCaptureActive) throw std::runtime_error("No performance capture is active");
                if (perfSampleClock.elapsed() >= 250) writePerfSample(); perfCaptureActive = false;
                const QString capturePath = perfCapture.fileName(); perfCapture.close(); response["path"] = capturePath;
                response["durationMs"] = perfCaptureClock.elapsed(); perfCaptureClock.invalidate(); perfSampleClock.invalidate();
            }
            else if (command == "audio-devices") { response["devices"] = SuyuAdapter::audioDevices(); }
            else if (command == "vulkan-devices") { response["devices"] = SuyuAdapter::vulkanDevices(); }
            else if (command == "volume") {
                if (state != "Running" && state != "Paused") throw std::runtime_error("No active game for volume change");
                if (!request["volume"].isDouble()) throw std::runtime_error("Invalid volume");
                adapter.setAudioVolume(request["volume"].toInt(-1));
            }
            else if (command == "speed-limit") {
                if (state != "Running" && state != "Paused") throw std::runtime_error("No active game for speed change");
                if (!request["percent"].isDouble()) throw std::runtime_error("Invalid speed limit");
                adapter.setSpeedLimit(request["percent"].toInt(-1));
            }
            else if (command == "presentation-settings") {
                if (state != "Running" && state != "Paused") throw std::runtime_error("No active game for presentation change");
                if (!request["scalingFilter"].isDouble() || !request["antiAliasing"].isDouble() ||
                    !request["fsrSharpness"].isDouble()) throw std::runtime_error("Invalid presentation settings");
                SuyuAdapter::configurePresentation(request["scalingFilter"].toInt(-1),
                    request["antiAliasing"].toInt(-1), request["fsrSharpness"].toInt(-1));
            }
            else if (command == "load") {
                const auto path = request["path"].toString();
                if (state != "Ready" || path.size() > 4096 || !QFileInfo(path).isAbsolute() || !QFileInfo(path).isFile()) throw std::runtime_error("Invalid program path or session already loaded");
                if (!request["resolution"].isDouble() || !request["shaderCache"].isBool() || !request["docked"].isBool()
                    || !request["vsync"].isDouble() || !request["asynchronousShaders"].isBool()
                    || !request["asynchronousGpu"].isBool() || !request["pipelineCache"].isBool()
                    || !request["vulkanDevice"].isDouble() || !request["gpuAccuracy"].isDouble()
                    || !request["astcDecode"].isDouble() || !request["anisotropy"].isDouble()
                    || !request["scalingFilter"].isDouble() || !request["antiAliasing"].isDouble()
                    || !request["vramUsage"].isDouble() || !request["astcRecompression"].isDouble()
                    || !request["aspectRatio"].isDouble() || !request["fsrSharpness"].isDouble()
                    || !request["nvdec"].isDouble() || !request["reactiveFlushing"].isBool()
                    || !request["dmaAccuracy"].isDouble() || !request["gpuFence"].isDouble()
                    || !request["language"].isDouble() || !request["region"].isDouble()
                    || !request["multicore"].isBool() || !request["cpuAccuracy"].isDouble()
                    || !request["memoryLayout"].isDouble()
                    || !request["cpuOverclock"].isDouble() || !request["customCpuTicks"].isBool()
                    || !request["cpuTicks"].isDouble() || !request["fastGpuTime"].isBool()
                    || !request["fastGpuDivisor"].isDouble() || !request["skipCpuInvalidation"].isBool()
                    || !request["volume"].isDouble() || !request["audioDevice"].isString()
                    || !request["speedLimit"].isDouble()) throw std::runtime_error("Invalid graphics, audio or speed settings");
                if (!request["userUuid"].isString()) throw std::runtime_error("Invalid emulated user selection");
                perfConfig = QJsonObject{{"resolution",request["resolution"]},{"docked",request["docked"]},
                    {"vsync",request["vsync"]},{"asynchronousShaders",request["asynchronousShaders"]},
                    {"asynchronousGpu",request["asynchronousGpu"]},{"pipelineCache",request["pipelineCache"]},
                    {"vulkanDevice",request["vulkanDevice"]},{"gpuAccuracy",request["gpuAccuracy"]},
                    {"astcDecode",request["astcDecode"]},{"anisotropy",request["anisotropy"]},
                    {"scalingFilter",request["scalingFilter"]},{"antiAliasing",request["antiAliasing"]},
                    {"vramUsage",request["vramUsage"]},{"fastGpuTime",request["fastGpuTime"]},
                    {"fastGpuDivisor",request["fastGpuDivisor"]},{"skipCpuInvalidation",request["skipCpuInvalidation"]},
                    {"multicore",request["multicore"]},{"cpuAccuracy",request["cpuAccuracy"]},
                    {"cpuOverclock",request["cpuOverclock"]},{"speedLimit",request["speedLimit"]}};
                SuyuAdapter::configureGraphics(request["resolution"].toInt(-1), request["shaderCache"].toBool(),
                    request["docked"].toBool(), request["vsync"].toInt(-1), request["asynchronousShaders"].toBool(),
                    request["asynchronousGpu"].toBool(), request["pipelineCache"].toBool(), request["vulkanDevice"].toInt(-1),
                    request["gpuAccuracy"].toInt(-1), request["astcDecode"].toInt(-1), request["anisotropy"].toInt(-1),
                    request["scalingFilter"].toInt(-1), request["antiAliasing"].toInt(-1), request["vramUsage"].toInt(-1),
                    request["astcRecompression"].toInt(-1), request["aspectRatio"].toInt(-1), request["fsrSharpness"].toInt(-1),
                    request["nvdec"].toInt(-1), request["reactiveFlushing"].toBool(),
                    request["dmaAccuracy"].toInt(-1), request["gpuFence"].toInt(-1));
                SuyuAdapter::configureCore(request["language"].toInt(-1), request["region"].toInt(-1),
                    request["multicore"].toBool(), request["cpuAccuracy"].toInt(-1), request["memoryLayout"].toInt(-1),
                    request["cpuOverclock"].toInt(-1), request["customCpuTicks"].toBool(), request["cpuTicks"].toInt(-1));
                SuyuAdapter::configureGraphicsHacks(request["fastGpuTime"].toBool(),
                    request["fastGpuDivisor"].toInt(-1), request["skipCpuInvalidation"].toBool());
                adapter.configureAudio(request["volume"].toInt(-1), request["audioDevice"].toString());
                adapter.setSpeedLimit(request["speedLimit"].toInt(-1));
                adapter.selectUser(request["userUuid"].toString());
                frameTimer.stop(); producer.stop();
                response["texture"] = producer.start(session,true,request["docked"].toBool() ? 1920 : 1280,
                    request["docked"].toBool() ? 1080 : 720);
                adapter.load(path); adapter.run(); state = "Running";
                response["loaded"] = true; response["runRequested"] = true;
                exitWatchdog.start(); // Real frames publish on compositor notification.
            }
            else if (command == "pause") { if (state != "Running") throw std::runtime_error("Session is not running"); adapter.input(0,0,0,0,0,0); adapter.input(1,0,0,0,0,0); adapter.pause(); state="Paused"; }
            else if (command == "resume") { if (state != "Paused") throw std::runtime_error("Session is not paused"); adapter.run(); state="Running"; }
            else if (command == "input") {
                const int player = request["player"].toInt(-1);
                const auto buttons = request["buttons"].toInt(-1);
                if (state != "Running" || player < 0 || player > 1 || buttons < 0 || buttons >= (1<<20)) throw std::runtime_error("Invalid game input state");
                float axes[4]; int i=0;
                for (const auto& name : {"lx","ly","rx","ry"}) {
                    if (!request[name].isDouble() || request[name].toDouble() < -1 || request[name].toDouble() > 1) throw std::runtime_error("Invalid stick coordinate");
                    axes[i++]=float(request[name].toDouble());
                }
                adapter.input(player, quint32(buttons), axes[0],axes[1],axes[2],axes[3]);
                if (player == 0) response["hidButtons"] = int(adapter.observedButtons());
            }
            else if (command == "stop") { if (perfCaptureActive) { if (perfSampleClock.elapsed() >= 250) writePerfSample(); perfCapture.close(); perfCaptureActive = false; } frameTimer.stop(); exitWatchdog.stop(); producer.stop(); adapter.shutdown(); state = "Stopped"; }
            else if (command == "gpu-diagnostic") {
                if (state != "Ready") throw std::runtime_error("Initialize core first");
                frameTimer.stop(); response["texture"] = producer.start(session); frameTimer.start();
            }
            else if (command == "identify") {
                const auto path = request["path"].toString();
                const auto suffix = QFileInfo(path).suffix().toLower();
                if (state != "Ready" || !request["path"].isString() || path.size() > 4096 || !QFileInfo(path).isAbsolute() || !QFileInfo(path).isFile()
                    || !QStringList{"nro","nso","nsp","xci"}.contains(suffix)) throw std::runtime_error("Only readable local NRO/NSO/NSP/XCI identification is supported");
                response["format"] = adapter.identify(path);
                response["loaded"] = false;
            } else { response["ok"] = false; response["error"] = "Unsupported operation"; }
        } catch (const std::exception& error) {
            response["ok"] = false; response["error"] = QString::fromUtf8(error.what()).left(512);
            // Failed loads may leave original subsystems partially initialized; exit this child.
            if (command == "load") { channel.send(response); socket.flush(); app.exit(70); return; }
        }
        response["state"] = state;
        if (!channel.send(response)) { app.exit(68); return; }
        if (command == "stop") {
            socket.flush();
            socket.disconnectFromServer();
            QTimer::singleShot(1000, &app, &QCoreApplication::quit);
        }
    });
    socket.connectToServer(parser.value("server"));
    return app.exec();
}
