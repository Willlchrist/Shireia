#include "EmulationManager.h"
#include "../graphics/FrameSize.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QDateTime>
#include <QProcessEnvironment>
#include <QSettings>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QDebug>
#include <QSaveFile>
#include <QSysInfo>
#include <utility>

EmulationManager::EmulationManager(QObject* parent, QString executable, QString dataDir) : QObject(parent),
    executable_(executable.isEmpty() ? QCoreApplication::applicationDirPath() + "/ShireiaCore.exe" : executable),
    dataDir_(dataDir.isEmpty() ? QCoreApplication::applicationDirPath() + "/core-data" : dataDir) {
    volume_ = qBound(0, QSettings().value("audio/volume", 100).toInt(), 100);
    speedLimit_ = qBound(50, QSettings().value("core/speedLimit", 100).toInt(), 200);
    stallProbeEnabled_ = QSettings().value("diagnostics/vulkanStallProbe", false).toBool();
    startup_.setSingleShot(true); shutdown_.setSingleShot(true);
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&server_, &QLocalServer::newConnection, this, &EmulationManager::accept);
    connect(&startup_, &QTimer::timeout, this, [this] { fail(tr("O núcleo não concluiu a conexão no prazo. Isso não comprova um crash.")); });
    connect(&shutdown_, &QTimer::timeout, this, [this] {
        if (active()) { appendLog(tr("Prazo de encerramento excedido; finalizando somente o processo supervisionado.")); process_.kill(); }
    });
    connect(&process_, &QProcess::started, this, [this] { emit stateChanged(); });
    connect(&process_, &QProcess::readyReadStandardOutput, this, [this] { appendLog(QString::fromUtf8(process_.readAllStandardOutput()).left(8192)); });
    connect(&process_, &QProcess::readyReadStandardError, this, [this] { appendLog(QString::fromUtf8(process_.readAllStandardError()).left(8192)); });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { resourceLock_.reset(); startup_.stop(); fail(tr("Falha ao iniciar ShireiaCore: %1").arg(process_.errorString())); }
    });
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        const bool restartCore = restartCoreAfterStop_;
        restartCoreAfterStop_ = false;
        startup_.stop(); shutdown_.stop(); requestsTimer_.stop(); pending_.clear(); pendingPath_.clear();
        resourceLock_.reset();
        server_.close();
        gpuDescriptor_ = {}; gpuSequence_ = 0; emit gpuDescriptorChanged();
        currentGamePath_.clear();
        exits_.prepend(QVariantMap{{"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}, {"code", code},
            {"status", status == QProcess::CrashExit ? "CrashExit" : "NormalExit"}, {"expected", expectedExit_ && !failed_}});
        if (exits_.size() > 50) exits_.removeLast();
        emit exitsChanged();
        if (!expectedExit_ || failed_ || status != QProcess::NormalExit || code != 0) {
            const auto processError = tr("O processo do núcleo terminou (código %1, %2). A interface permanece aberta.")
                .arg(code).arg(status == QProcess::CrashExit ? "CrashExit" : "NormalExit");
            fail(lastLoadError_.isEmpty() ? processError : lastLoadError_ + " " + processError);
        } else {
            setState("Stopped"); setMessage(tr("Núcleo encerrado de forma controlada."));
            if (restartCore) QTimer::singleShot(0, this, [this] { startCore(); });
        }
        if (socket_) socket_->deleteLater();
        if (channel_) channel_->deleteLater();
        socket_ = nullptr; channel_ = nullptr; authenticated_ = false; capabilities_ = {}; emit stateChanged();
    });
    requestsTimer_.setInterval(250);
    connect(&requestsTimer_, &QTimer::timeout, this, [this] {
        const auto now = QDateTime::currentMSecsSinceEpoch();
        for (const auto& request : std::as_const(pending_)) if (now > request.deadline) { fail(tr("Tempo de resposta excedido. Encerramento solicitado; travamento não confirmado.")); return; }
    });
    if (!coreAvailable()) state_ = "Unavailable";
}
EmulationManager::~EmulationManager() {
    // Closing the UI must never leave a detached core process behind.
    process_.disconnect(this); server_.close();
    if (active()) {
        if (authenticated_ && channel_) { request("stop"); if (socket_) socket_->flush(); }
        if (!process_.waitForFinished(1500)) { process_.kill(); process_.waitForFinished(1500); }
    }
}
bool EmulationManager::coreAvailable() const {
#if SHIREIA_CORE_ENABLED
    return QFileInfo(executable_).isFile();
#else
    return false;
#endif
}
void EmulationManager::setStallProbeEnabled(bool enabled) {
    if (stallProbeEnabled_ == enabled) return;
    stallProbeEnabled_ = enabled;
    QSettings().setValue("diagnostics/vulkanStallProbe", enabled);
    emit stallProbeEnabledChanged();
    if (state_ == "Ready" && active() && authenticated_) {
        restartCoreAfterStop_ = true;
        expectedExit_ = true;
        setState("Stopping");
        setMessage(tr("Reiniciando o núcleo ocioso para aplicar a sonda Vulkan…"));
        request("stop");
        shutdown_.start(5000);
    }
}
void EmulationManager::setState(const QString& value) { state_ = value; emit stateChanged(); }
void EmulationManager::setMessage(const QString& value) { message_ = value; emit messageChanged(); appendLog(value); }
void EmulationManager::appendLog(const QString& value) {
    if (value.trimmed().isEmpty()) return;
    log_.append(QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + " " + value.left(8192));
    while (log_.size() > 100) log_.removeFirst();
    emit logChanged();
}
void EmulationManager::recordGraphicsError(const QString& error) {
    const auto clean = error.trimmed().left(2048);
    if (clean.isEmpty() || clean == graphicsError_) return;
    graphicsError_ = clean;
    emit graphicsErrorChanged();
    appendLog(tr("Erro gráfico: %1").arg(clean));
}

bool EmulationManager::exportDiagnostics() {
    const auto directory = QDir(dataDir_).filePath("diagnostics");
    if (!QDir().mkpath(directory)) { setMessage(tr("Não foi possível criar a pasta de diagnósticos.")); return false; }
    const auto stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss");
    diagnosticsExportPath_ = QDir(directory).filePath("Shireia-diagnostic-" + stamp + ".json");
    static const QRegularExpression pathPattern(QStringLiteral("(?:[A-Za-z]:[\\\\/][^\\r\\n\\\"']*|\\\\\\\\[^\\\\]+\\\\[^\\r\\n\\\"']*)"));
    static const QRegularExpression tokenPattern(QStringLiteral("\\b[A-Fa-f0-9]{48,}\\b"));
    QJsonArray safeLog;
    for (const auto& line : log_) safeLog.append(QString(line).replace(pathPattern, "[caminho ocultado]").replace(tokenPattern, "[token ocultado]"));
    QJsonArray safeExits;
    for (const auto& entry : exits_) {
        const auto value = entry.toMap();
        safeExits.append(QJsonObject{{"time", value.value("time").toString()}, {"code", value.value("code").toInt()},
            {"status", value.value("status").toString()}, {"expected", value.value("expected").toBool()}});
    }
    const auto safeError = QString(graphicsError_).replace(pathPattern, "[caminho ocultado]").replace(tokenPattern, "[token ocultado]");
    const QJsonObject report{{"formatVersion", 1}, {"createdUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"application", QCoreApplication::applicationName()}, {"platform", QSysInfo::prettyProductName()},
        {"state", state_}, {"performance", QJsonObject::fromVariantMap(performanceMetrics_)},
        {"graphicsError", safeError},
        {"recentLog", safeLog}, {"processExits", safeExits}};
    QSaveFile output(diagnosticsExportPath_);
    const auto data = QJsonDocument(report).toJson(QJsonDocument::Indented);
    if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit()) {
        diagnosticsExportPath_.clear(); setMessage(tr("Falha ao salvar o relatório de diagnóstico.")); return false;
    }
    emit diagnosticsExported();
    setMessage(tr("Diagnóstico exportado: %1").arg(QFileInfo(diagnosticsExportPath_).fileName()));
    return true;
}
void EmulationManager::fail(const QString& value) {
    failed_ = true; setState("Failed"); setMessage(value); pending_.clear();
    requestsTimer_.stop(); startup_.stop(); server_.close();
    if (active()) { process_.terminate(); shutdown_.start(2000); }
    else resourceLock_.reset();
    emit failed();
}
void EmulationManager::startCore() {
    if (active()) return;
    if (!coreAvailable()) { setMessage(tr("ShireiaCore.exe não está disponível neste build.")); setState("Unavailable"); return; }
    if (!QDir().mkpath(dataDir_ + "/roaming")) { fail(tr("Não foi possível criar o diretório privado do núcleo.")); return; }
    auto lock=std::make_unique<QLockFile>(dataDir_+"/resources.lock");
    if (!lock->tryLock(0)) { setMessage(tr("Aguarde a análise/importação de recursos terminar antes de iniciar a sessão.")); return; }
    resourceLock_=std::move(lock);
    if (socket_) { socket_->abort(); socket_->deleteLater(); }
    if (channel_) channel_->deleteLater();
    socket_ = nullptr; channel_ = nullptr;
    session_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    token_ = QUuid::createUuid().toString(QUuid::Id128) + QUuid::createUuid().toString(QUuid::Id128);
    authenticated_ = false; expectedExit_ = false; failed_ = false; capabilities_ = {}; sequence_ = 0; pending_.clear();
    lastLoadError_.clear();
    server_.close();
    if (!server_.listen("shireia-" + session_)) { fail(tr("Falha ao abrir o canal IPC local.")); return; }
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("SHIREIA_CORE_TOKEN", token_);
    env.insert("APPDATA", QDir::toNativeSeparators(dataDir_ + "/roaming"));
    // Override inherited shell settings too, so the UI switch reliably turns
    // the probe both on and off in the child process.
    env.insert("SUYU_STALL_PROBE", stallProbeEnabled_ ? "1" : "0");
    process_.setProcessEnvironment(env);
    process_.setWorkingDirectory(QFileInfo(executable_).absolutePath());
    process_.setProgram(executable_);
    process_.setArguments({"--server", server_.serverName(), "--shireia-session", session_, "--data-dir", QDir(dataDir_).absolutePath()});
    setState("Starting"); setMessage(tr("Iniciando processo isolado do núcleo original…"));
    process_.start(); startup_.start(10000); requestsTimer_.start();
}
void EmulationManager::accept() {
    while (server_.hasPendingConnections()) {
        auto* candidate = server_.nextPendingConnection();
        if (socket_) { candidate->abort(); candidate->deleteLater(); continue; }
        socket_ = candidate;
        channel_ = new ShireiaIpc::JsonChannel(candidate, this);
        const auto connectionSession = session_;
        connect(channel_, &ShireiaIpc::JsonChannel::message, this, [this,connectionSession](const QJsonObject& message) { if (connectionSession == session_) receive(message); });
        connect(channel_, &ShireiaIpc::JsonChannel::invalid, this, [this,connectionSession](const QString& reason) { if (connectionSession == session_) fail(tr("Mensagem IPC inválida: %1").arg(reason)); });
        connect(candidate, &QLocalSocket::disconnected, this, [this,connectionSession] {
            if (connectionSession == session_ && active() && !expectedExit_ && !failed_) QTimer::singleShot(250, this, [this,connectionSession] { if (connectionSession == session_ && active() && !expectedExit_ && !failed_) fail(tr("Canal IPC perdido com o processo ainda ativo.")); });
        });
    }
}
int EmulationManager::request(const QString& command, QJsonObject fields) {
    if (!authenticated_ || !channel_) return 0;
    const int id = ++sequence_;
    auto message = ShireiaIpc::envelope("request", id, session_);
    message["command"] = command;
    for (auto it = fields.begin(); it != fields.end(); ++it) message[it.key()] = it.value();
    pending_[id] = {command, QDateTime::currentMSecsSinceEpoch() + 30000};
    if (!channel_->send(message)) { pending_.remove(id); fail(tr("Falha ao transmitir comando IPC.")); return 0; }
    return id;
}
void EmulationManager::receive(const QJsonObject& message) {
    if (!ShireiaIpc::validEnvelope(message, session_)) { fail(tr("Envelope IPC incompatível.")); return; }
    if (authenticated_ && message["type"] == "event") {
        if (message["event"] == "guest-exited" && gameLoaded() && message["id"].toInt() == 0) {
            setMessage(tr("O programa solicitou encerramento. Finalizando a sessão.")); stopGame(); return;
        }
        if (message["generation"].isString() && message["generation"].toString() != gpuDescriptor_["generation"].toString()) return;
        if (!gpuDiagnostic() || message["id"].toInt() != 0 || message["event"] != "gpu-frame"
            || message["generation"].toString() != gpuDescriptor_["generation"].toString()
            || !ShireiaIpc::validId(message["slot"], true) || message["slot"].toInt() > 2
            || !ShireiaIpc::validId(message["sequence"]) || message["sequence"].toInt() <= gpuSequence_) { fail(tr("Evento de textura inválido.")); return; }
        gpuSequence_ = message["sequence"].toInt();
        if (gameLoaded()) {
            double interval = 0.0;
            if (frameArrivalClock_.isValid()) interval = frameArrivalClock_.restart();
            else frameArrivalClock_.start();
            if (interval > 0.0) {
                performanceMetrics_["intervalMs"] = interval;
                emit frameIntervalReceived(interval);
            }
        }
        if (gameLoaded() && message["coreFrames"].isDouble() && message["transferUs"].isDouble()) {
            const auto now = QDateTime::currentMSecsSinceEpoch();
            const auto coreFrames = qint64(message["coreFrames"].toDouble());
            transferMicros_ += qMax(0, message["transferUs"].toInt()); ++transferSamples_;
            const auto copyUs = qint64(message["copyUsTotal"].toDouble());
            const auto copiedFrames = qint64(message["copiedFrames"].toDouble());
            const auto ipcUs = qint64(message["ipcSendUsTotal"].toDouble());
            const auto ipcCount = qint64(message["ipcSendCount"].toDouble());
            if (!metricsStartMs_) {
                metricsStartMs_ = now; lastCoreFrames_ = coreFrames; lastTransportSequence_ = gpuSequence_;
                metricsWindowMs_ = now; metricsWindowCoreFrames_ = coreFrames; metricsWindowSequence_ = gpuSequence_;
                lastCopyUs_ = copyUs; lastCopiedFrames_ = copiedFrames;
                lastIpcSendUs_ = ipcUs; lastIpcSendCount_ = ipcCount;
            }
            if (metricsWindowMs_ <= 0) { metricsWindowMs_ = now; metricsWindowCoreFrames_ = coreFrames; metricsWindowSequence_ = gpuSequence_; }
            else if (now - qint64(metricsWindowMs_) >= 1000) {
                const auto elapsed = now - qint64(metricsWindowMs_);
                performanceMetrics_["compositorFps"] = (coreFrames - metricsWindowCoreFrames_) * 1000.0 / qMax<qint64>(1, elapsed);
                performanceMetrics_["publishedFps"] = (gpuSequence_ - metricsWindowSequence_) * 1000.0 / qMax<qint64>(1, elapsed);
                emit performanceMetricsChanged();
                metricsWindowMs_ = now; metricsWindowCoreFrames_ = coreFrames; metricsWindowSequence_ = gpuSequence_;
            }
            else if (now - metricsStartMs_ >= 2000) {
                const auto elapsed = now - metricsStartMs_;
                const auto metrics = tr("Gráficos: núcleo %1 FPS · transporte %2 FPS · D3D %3 ms/quadro · cópia CPU %4 ms/quadro · envio IPC %5 ms/evento")
                    .arg((coreFrames - lastCoreFrames_) * 1000.0 / elapsed, 0, 'f', 1)
                    .arg((gpuSequence_ - lastTransportSequence_) * 1000.0 / elapsed, 0, 'f', 1)
                    .arg(double(transferMicros_) / qMax(1, transferSamples_) / 1000.0, 0, 'f', 2)
                    .arg(double(copyUs - lastCopyUs_) / qMax<qint64>(1, copiedFrames - lastCopiedFrames_) / 1000.0, 0, 'f', 2)
                    .arg(double(ipcUs - lastIpcSendUs_) / qMax<qint64>(1, ipcCount - lastIpcSendCount_) / 1000.0, 0, 'f', 3);
                appendLog(metrics);
                qInfo().noquote() << metrics;
                metricsStartMs_ = now; lastCoreFrames_ = coreFrames; lastTransportSequence_ = gpuSequence_;
                lastCopyUs_ = copyUs; lastCopiedFrames_ = copiedFrames;
                lastIpcSendUs_ = ipcUs; lastIpcSendCount_ = ipcCount;
                transferMicros_ = 0; transferSamples_ = 0;
            }
        }
        if (gameLoaded() && state_ == "AwaitingFrame") { setState("Running"); setMessage(tr("Jogo em execução.")); }
        emit gpuFrame(message["slot"].toInt(), gpuSequence_);
        return;
    }
    if (!authenticated_) {
        if (message["type"] != "hello" || message["id"].toInt() != 0 || message["token"].toString() != token_ || message["backend"] != "suyu-original-prebuilt" || !message["capabilities"].isObject()) { fail(tr("Handshake do núcleo rejeitado.")); return; }
        capabilities_ = message["capabilities"].toObject();
        for (const auto& name : {"initialize", "identify", "load", "execute", "pause", "frames", "gameInput"})
            if (!capabilities_[name].isBool()) { fail(tr("Capacidades IPC inválidas.")); return; }
        authenticated_ = true; startup_.stop(); server_.close();
        if (expectedExit_) { setState("Stopping"); request("stop"); }
        else {
            setState("Initializing");
            setMessage(tr("Core::System original: inicializa\u00e7\u00e3o em andamento..."));
            request("initialize");
        }
        return;
    }
    const auto id = message["id"].toInt();
    if (message["type"] != "response" || !pending_.contains(id) || !message["ok"].isBool()
        || message["command"].toString() != pending_[id].command || !message["state"].isString()) { fail(tr("Resposta IPC inválida ou inesperada.")); return; }
    const auto command = pending_.take(id).command;
    if (!QStringList{"Idle", "Ready", "Stopped", "Running", "Paused"}.contains(message["state"].toString())) { fail(tr("Estado de núcleo não reconhecido.")); return; }
    if (!message["ok"].toBool()) {
        if (command == "initialize") { fail(tr("Falha ao inicializar o núcleo: %1").arg(message["error"].toString())); return; }
        const auto error = tr("Operação rejeitada pelo núcleo: %1").arg(message["error"].toString().left(512));
        if (command == "load") lastLoadError_ = error;
        setMessage(error); return;
    }
    if (command == "initialize") {
        if (!expectedExit_) { setState("Ready"); setMessage(tr("Core::System original inicializado. Nenhum programa carregado ou em execução.")); request("audio-devices"); probePending(); }
    }
    else if (command == "audio-devices") {
        const auto entries = message["devices"].toArray();
        QStringList devices{"auto"};
        for (const auto& entry : entries) if (entry.isString() && entry.toString().size() <= 256 && !devices.contains(entry.toString())) devices.append(entry.toString());
        audioDevices_ = devices; emit audioDevicesChanged(); request("vulkan-devices");
    }
    else if (command == "vulkan-devices") {
        QStringList devices;
        for (const auto& entry : message["devices"].toArray())
            if (entry.isString() && !entry.toString().trimmed().isEmpty() && !devices.contains(entry.toString())) devices.append(entry.toString());
        vulkanDevices_ = devices.isEmpty() ? QStringList{tr("Nenhuma GPU Vulkan detectada")} : devices;
        emit vulkanDevicesChanged();
    }
    else if (command == "identify") {
        if (!message["format"].isString() || message["format"].toString().size() > 32 || message["loaded"] != false) { fail(tr("Resultado de identificação inválido.")); return; }
        const auto format = message["format"].toString();
        setMessage(tr("Identificação pelo loader original: %1. Isso não valida o programa; carga e execução ainda indisponíveis.").arg(format));
        emit identified(format);
    } else if (command == "gpu-diagnostic" || command == "load") {
        const auto descriptor = message["texture"].toObject();
        const auto buffers = descriptor["buffers"].toArray();
        const bool validSize = (descriptor["width"].toInt() == 1280 && descriptor["height"].toInt() == 720)
            || (command == "load" && descriptor["width"].toInt() == 1920 && descriptor["height"].toInt() == 1080);
        if (buffers.size() != 3 || !validSize
            || descriptor["generation"].toString().size() != 32 || descriptor["luid"].toString().size() != 16 || descriptor["content"] != (command == "load" ? "emulation-headless-readback" : "diagnostic-not-emulation")) { fail(tr("Descritor GPU inválido.")); return; }
        for (const auto& slot : buffers) if (!slot.isString() || !slot.toString().startsWith("Local\\shireia-" + session_ + "-") || slot.toString().size() > 128) { fail(tr("Nome de textura inválido.")); return; }
        gpuDescriptor_ = descriptor; gpuSequence_ = 0; metricsStartMs_ = 0; metricsWindowMs_ = 0; frameArrivalClock_.invalidate(); transferMicros_ = 0; transferSamples_ = 0;
        emit gpuDescriptorChanged();
        if (command == "load") {
            if (message["loaded"] != true || message["runRequested"] != true || message["state"] != "Running") { fail(tr("Núcleo não confirmou Load/Run.")); return; }
            setState("AwaitingFrame"); setMessage(tr("Preparando imagem do jogo…"));
        } else setMessage(tr("Teste técnico: textura GPU entre processos. O padrão de cores não é emulação."));
    } else if (command == "input") { lastHidButtons_ = message["hidButtons"].toInt(); emit hidChanged(); }
    else if (command == "performance-capture-start") {
        if (message["ok"].toBool()) {
            performanceCaptureActive_ = true; performanceCapturePath_ = message["path"].toString();
            emit performanceCaptureChanged(); setMessage(tr("Captura de desempenho iniciada: %1").arg(performanceCapturePath_));
        } else setMessage(tr("Falha na captura: %1").arg(message["error"].toString()));
    } else if (command == "performance-capture-stop") {
        if (message["ok"].toBool()) {
            performanceCaptureActive_ = false; performanceCapturePath_ = message["path"].toString();
            emit performanceCaptureChanged(); setMessage(tr("Captura salva: %1 (%2 ms)").arg(performanceCapturePath_).arg(message["durationMs"].toInt()));
        } else setMessage(tr("Falha ao encerrar captura: %1").arg(message["error"].toString()));
    }
    else if (command == "pause" || command == "resume") { setState(message["state"].toString()); setMessage(tr("Estado confirmado pelo núcleo: %1.").arg(state_)); }
    else if (command == "status") { setMessage(tr("Estado confirmado pelo processo: %1.").arg(message["state"].toString())); }
}
void EmulationManager::startGpuDiagnostic() {
    if (state_ != "Ready") { setMessage(tr("Inicialize o núcleo antes de testar o transporte GPU.")); return; }
    for (const auto& pending : std::as_const(pending_)) if (pending.command == "gpu-diagnostic") return;
    if (!gpuDiagnostic()) request("gpu-diagnostic");
}
void EmulationManager::startPerformanceCapture() {
    if (state_ != "Running" || performanceCaptureActive_) return;
    request("performance-capture-start");
}
void EmulationManager::stopPerformanceCapture() {
    if (!performanceCaptureActive_) return;
    request("performance-capture-stop");
}
bool EmulationManager::startGame(const QString& path) {
    if (pendingLoad_ || gameLoaded() || state_ == "Loading" || state_ == "AwaitingFrame") { setMessage(tr("Aguarde a sessão atual ou encerre o jogo antes de iniciar outro.")); return false; }
    return queueProgram(path,true);
}
bool EmulationManager::identifyFile(const QString& path) {
    return queueProgram(path,false);
}
bool EmulationManager::queueProgram(const QString& path, bool load) {
    const auto info = QFileInfo(path);
    if (!info.isFile() || !info.isReadable()) { pendingLoad_=false; setMessage(tr("Arquivo de programa inexistente ou sem permissão de leitura.")); return false; }
    if (state_ == "Stopping") { setMessage(tr("Aguarde o encerramento do núcleo.")); return false; }
    if (pendingLoad_ || gameLoaded() || state_=="Loading" || state_=="AwaitingFrame") { setMessage(tr("Aguarde a sessão atual antes de analisar outro arquivo.")); return false; }
    pendingLoad_=load;
    pendingPath_ = info.canonicalFilePath();
    if (load) { currentGamePath_ = pendingPath_; emit stateChanged(); }
    if (!active()) startCore(); else if (state_ == "Ready") probePending();
    return true;
}
void EmulationManager::probePending() {
    if (pendingPath_.isEmpty()) return;
    const auto path = std::exchange(pendingPath_, {});
    const bool load = std::exchange(pendingLoad_,false);
    if (load) { setState("Loading"); setMessage(tr("Carregando programa pelo núcleo original…")); }
    QJsonObject fields{{"path", path}};
    if (load) {
        QSettings settings;
        const auto gameId = QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex());
        const auto metadata = settings.value("library/metadata/" + gameId).toMap();
        const auto titleId = metadata.value("titleId").toString().toUpper();
        static const QRegularExpression titlePattern(QStringLiteral("^[0-9A-F]{16}$"));
        const auto overridePrefix = titlePattern.match(titleId).hasMatch()
            ? "library/gameOverrides/title/" + titleId + "/"
            : "library/gameOverrides/path/" + gameId + "/";
        const auto legacyOverridePrefix = "library/gameOverrides/" + gameId + "/";
        const auto setting = [&](const QString& key, const QVariant& defaultValue) {
            const auto overrideKey = overridePrefix + key;
            if (settings.contains(overrideKey)) return settings.value(overrideKey);
            const auto legacyKey = legacyOverridePrefix + key;
            return settings.contains(legacyKey) ? settings.value(legacyKey) : settings.value(key, defaultValue);
        };
        fields["resolution"] = qBound(0, setting("graphics/resolution", 3).toInt(), 12);
        fields["shaderCache"] = setting("graphics/shaderCache", true).toBool();
        fields["docked"] = setting("graphics/docked", false).toBool();
        static const QStringList scales{"0.25×", "0.5×", "0.75×", "1×", "1.25×", "1.5×", "2×", "3×", "4×", "5×", "6×", "7×", "8×"};
        performanceMetrics_["internalScale"] = scales.at(fields["resolution"].toInt());
        performanceMetrics_["outputResolution"] = fields["docked"].toBool() ? "1920 × 1080" : "1280 × 720";
        performanceMetrics_["backend"] = "Vulkan · readback síncrono · D3D11";
        emit performanceMetricsChanged();
        fields["vsync"] = qBound(0, setting("graphics/vsync", 2).toInt(), 3);
        fields["asynchronousShaders"] = setting("graphics/asynchronousShaders", true).toBool();
        fields["asynchronousGpu"] = setting("graphics/asynchronousGpu", true).toBool();
        fields["pipelineCache"] = setting("graphics/pipelineCache", true).toBool();
        fields["vulkanDevice"] = qBound(0, setting("graphics/vulkanDevice", settings.value("graphics/vulkanDevice", 0)).toInt(), 31);
        fields["gpuAccuracy"] = qBound(0, setting("graphics/gpuAccuracy", 1).toInt(), 1);
        fields["astcDecode"] = qBound(0, setting("graphics/astcDecode", 1).toInt(), 2);
        fields["anisotropy"] = qBound(0, setting("graphics/anisotropy", 0).toInt(), 8);
        fields["scalingFilter"] = qBound(0, setting("graphics/scalingFilter", 1).toInt(), 14);
        fields["antiAliasing"] = qBound(0, setting("graphics/antiAliasing", 0).toInt(), 2);
        fields["vramUsage"] = qBound(0, setting("graphics/vramUsage", 0).toInt(), 1);
        fields["astcRecompression"] = qBound(0, setting("graphics/astcRecompression", 0).toInt(), 2);
        fields["aspectRatio"] = qBound(0, setting("graphics/aspectRatio", 0).toInt(), 5);
        fields["fsrSharpness"] = qBound(0, setting("graphics/fsrSharpness", 25).toInt(), 200);
        fields["nvdec"] = qBound(0, setting("graphics/nvdec", 2).toInt(), 2);
        fields["reactiveFlushing"] = setting("graphics/reactiveFlushing", true).toBool();
        fields["dmaAccuracy"] = qBound(0, setting("graphics/dmaAccuracy", 0).toInt(), 2);
        fields["gpuFence"] = qBound(0, setting("graphics/gpuFence", 0).toInt(), 4);
        fields["language"] = qBound(0, setting("core/language", settings.value("system/language", 1)).toInt(), 19);
        fields["region"] = qBound(0, setting("core/region", settings.value("system/region", 1)).toInt(), 6);
        fields["multicore"] = setting("core/multicore", true).toBool();
        fields["cpuAccuracy"] = qBound(0, setting("core/cpuAccuracy", 0).toInt(), 3);
        fields["memoryLayout"] = qBound(0, setting("core/memoryLayout", 0).toInt(), 4);
        fields["cpuOverclock"] = qBound(0, setting("core/cpuOverclock", 0).toInt(), 2);
        fields["customCpuTicks"] = setting("core/customCpuTicks", false).toBool();
        fields["cpuTicks"] = qBound(77, setting("core/cpuTicks", 16000).toInt(), 65535);
        fields["fastGpuTime"] = setting("graphics/fastGpuTime", false).toBool();
        fields["fastGpuDivisor"] = qBound(0, setting("graphics/fastGpuDivisor", 1).toInt(), 2);
        fields["skipCpuInvalidation"] = setting("graphics/skipCpuInvalidation", false).toBool();
        fields["volume"] = qBound(0, setting("audio/volume", 100).toInt(), 100);
        fields["speedLimit"] = qBound(50, setting("core/speedLimit", 100).toInt(), 200);
        fields["audioDevice"] = setting("audio/outputDevice", "auto").toString();
        fields["userUuid"] = settings.value("system/userUuid", QString{}).toString();
    }
    request(load ? "load" : "identify", fields);
}
void EmulationManager::setVolume(int volume) {
    volume = qBound(0, volume, 100);
    if (volume_ == volume) return;
    volume_ = volume;
    QSettings settings; settings.setValue("audio/volume", volume); settings.sync();
    emit volumeChanged();
    if (gameLoaded() && (state_ == "Running" || state_ == "Paused")) request("volume", {{"volume", volume}});
}
void EmulationManager::setSpeedLimit(int percent) {
    percent = qBound(50, percent, 200);
    if (speedLimit_ == percent) return;
    speedLimit_ = percent;
    QSettings settings; settings.setValue("core/speedLimit", percent); settings.sync();
    emit speedLimitChanged();
    if (gameLoaded() && (state_ == "Running" || state_ == "Paused")) request("speed-limit", {{"percent", percent}});
}
bool EmulationManager::setPresentationSettings(int scalingFilter, int antiAliasing, int fsrSharpness) {
    if (scalingFilter < 0 || scalingFilter > 14 || antiAliasing < 0 || antiAliasing > 2 ||
        fsrSharpness < 0 || fsrSharpness > 200 || !gameLoaded() ||
        (state_ != "Running" && state_ != "Paused")) return false;
    return request("presentation-settings", {{"scalingFilter", scalingFilter},
        {"antiAliasing", antiAliasing}, {"fsrSharpness", fsrSharpness}}) != 0;
}
void EmulationManager::stopGame() {
    // A game request can still be waiting for the core handshake. Cancel its
    // queued path and flag even when the child is not running yet, otherwise
    // a later session can remain permanently blocked as "already loading".
    pendingPath_.clear();
    pendingLoad_ = false;
    if (!active()) return;
    if (expectedExit_) return;
    expectedExit_ = true; startup_.stop(); setState("Stopping");
    if (authenticated_) request("stop");
    shutdown_.start(5000);
}
void EmulationManager::pauseGame() { if (gameLoaded() && state_ == "Running") request("pause"); }
void EmulationManager::resumeGame() { if (gameLoaded() && state_ == "Paused") request("resume"); }
void EmulationManager::sendInput(quint32 buttons, float lx, float ly, float rx, float ry) {
    sendPlayerInput(0, buttons, lx, ly, rx, ry);
}
void EmulationManager::sendPlayerInput(int player, quint32 buttons, float lx, float ly, float rx, float ry) {
    if (player < 0 || player > 1 || !gameLoaded() || state_ != "Running" || expectedExit_) return;
    request("input", {{"player",player},{"buttons",int(buttons)},{"lx",lx},{"ly",ly},{"rx",rx},{"ry",ry}});
}
void EmulationManager::queryState() { if (authenticated_ && active()) request("status"); }
