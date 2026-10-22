#include "LibraryResources.h"
#include "AppModel.h"
#include "../settings/SettingsStore.h"
#include "../emulation/EmulationManager.h"
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QSaveFile>
#include <QScopeGuard>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QImageReader>
#include <QImage>
#include <QBuffer>
#include <QUrl>

LibraryResources::LibraryResources(AppModel& model, SettingsStore& settings, EmulationManager& session, QString dataDir, bool enabled)
    : model_(model), settings_(settings), session_(session), dataDir_(std::move(dataDir)), enabled_(enabled) {
    const auto cachedProfiles = QSettings().value("library/cachedProfiles").toList();
    if (!cachedProfiles.isEmpty()) profiles_ = cachedProfiles;
    debounce_.setSingleShot(true); debounce_.setInterval(300);
    timeout_.setSingleShot(true); timeout_.setInterval(10 * 60 * 1000);
    connect(&debounce_, &QTimer::timeout, this, [this] { start(); emit statusChanged(); });
    connect(&timeout_, &QTimer::timeout, this, [this] { status_ = tr("A an\u00e1lise excedeu o prazo. Tente novamente; a biblioteca continua dispon\u00edvel."); process_.kill(); emit statusChanged(); });
    connect(&settings_, &SettingsStore::applied, this, &LibraryResources::schedule);
    connect(&model_, &AppModel::scanCompleted, this, [this] {
        if (!applyingMetadata_) schedule();
    });
    connect(&session_, &EmulationManager::stateChanged, this, [this] {
        // Resource jobs need the core's exclusive data lock. Recheck deferred
        // work after it exits, but don't queue another scan just because the
        // idle core entered Ready; that used to leave a stale "waiting" status.
        if (!session_.active()) schedule();
    });
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, [this] { schedule(); });
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this] { schedule(); });
    connect(&process_, &QProcess::readyReadStandardOutput, this, [this] { process_.readAllStandardOutput(); });
    connect(&process_, &QProcess::readyReadStandardError, this, [this] { process_.readAllStandardError(); });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { timeout_.stop(); keysLoaded_ = false; firmwareReady_ = false; validationComplete_ = true; firmwareResult_.clear(); status_ = tr("Não foi possível iniciar o leitor de metadados do núcleo."); emit statusChanged(); }
    });
    connect(&process_, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, &LibraryResources::finish);
    schedule();
}
LibraryResources::~LibraryResources() { process_.disconnect(this); if (busy()) { process_.kill(); process_.waitForFinished(2000); } }
void LibraryResources::setEnabled(bool enabled) {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    if (enabled_) schedule();
    else debounce_.stop();
}
void LibraryResources::refresh() { force_ = true; schedule(); }
bool LibraryResources::selectProfile(const QString& uuid) {
    if (session_.active()) return false;
    for (const auto& item : profiles_)
        if (item.toMap().value("uuid").toString().compare(uuid, Qt::CaseInsensitive) == 0)
            return settings_.selectActiveUser(uuid);
    return false;
}
bool LibraryResources::createProfile(const QString& name) {
    if (busy() || session_.active() || !profileAction_.isEmpty()) return false;
    profileAction_ = {{"type", "create"}, {"name", name}}; refresh(); return true;
}
bool LibraryResources::renameProfile(const QString& uuid, const QString& name) {
    if (busy() || session_.active() || !profileAction_.isEmpty()) return false;
    profileAction_ = {{"type", "rename"}, {"uuid", uuid}, {"name", name}}; refresh(); return true;
}
bool LibraryResources::setProfilePhoto(const QString& uuid, const QUrl& photo) {
    if (busy() || session_.active() || !profileAction_.isEmpty() || !photo.isLocalFile()) return false;
    QImageReader reader(photo.toLocalFile());
    const auto size = reader.size();
    if (!reader.canRead() || size.width() < 1 || size.height() < 1 || size.width() > 4096 || size.height() > 4096) return false;
    const QImage image = reader.read().scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (image.isNull()) return false;
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPEG", 90) || bytes.size() > 1024 * 1024) return false;
    profileAction_ = {{"type", "photo"}, {"uuid", uuid}, {"jpeg", QString::fromLatin1(bytes.toBase64())}};
    refresh(); return true;
}
bool LibraryResources::installContent(const QUrl& url) {
    const QFileInfo file(url.toLocalFile());
    if (!enabled_ || busy() || session_.gameLoaded() || !url.isLocalFile() || !file.isFile() ||
        !file.isReadable() || file.suffix().compare("nsp", Qt::CaseInsensitive) != 0) return false;
    contentPath_ = file.canonicalFilePath();
    status_ = tr("Preparando instalação de update/DLC na NAND privada…");
    emit statusChanged();
    if (session_.active()) session_.stopGame();
    refresh();
    return true;
}
void LibraryResources::schedule() {
    if (!enabled_) return;
    if (process_.state() != QProcess::NotRunning) {
        rescanPending_ = true;
        return;
    }
    debounce_.start();
    emit statusChanged();
}
void LibraryResources::start() {
    if (!enabled_) return;
    if (process_.state() != QProcess::NotRunning) return; // The current job's completion checks for pending changes.
    if (!session_.coreAvailable()) { status_ = tr("Leitura de metadados requer ShireiaCore."); emit statusChanged(); return; }
    if (session_.active()) { status_ = tr("Aguardando o núcleo encerrar para importar recursos com segurança."); emit statusChanged(); return; }
    const auto values = settings_.values();
    const auto keys = values.value("system/prodKeys").toString();
    const auto firmware = values.value("system/firmwareFolder").toString();
    QDir().mkpath(dataDir_ + "/keys");
    QJsonArray games;
    QString signature;
    auto addStamp = [&](const QString& path) { QFileInfo f(path); signature += path + ":" + QString::number(f.size()) + ":" + QString::number(f.lastModified().toMSecsSinceEpoch()) + "\n"; };
    addStamp(QFileInfo(keys).isFile() ? keys : dataDir_ + "/keys/prod.keys");
    addStamp(firmware);
    if (!firmware.isEmpty() && QFileInfo(firmware).isDir())
        for (const auto& file : QDir(firmware).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name))
            if (file.suffix().compare("nca", Qt::CaseInsensitive) == 0) addStamp(file.absoluteFilePath());
    QStringList gamePaths;
    for (const auto& value : model_.games()) gamePaths.append(value.toMap().value("path").toString());
    gamePaths.sort(Qt::CaseSensitive);
    for (const auto& path : gamePaths) { games.append(path); addStamp(path); }
    const auto oldFiles = watcher_.files(); if (!oldFiles.isEmpty()) watcher_.removePaths(oldFiles);
    const auto oldDirs = watcher_.directories(); if (!oldDirs.isEmpty()) watcher_.removePaths(oldDirs);
    for (const auto& path : QStringList{keys, dataDir_ + "/keys/prod.keys", dataDir_ + "/keys", firmware}) if (!path.isEmpty() && QFileInfo(path).exists()) watcher_.addPath(path);
    if (!keys.isEmpty() && QFileInfo(keys).absoluteDir().exists()) watcher_.addPath(QFileInfo(keys).absolutePath());
    if (signature == lastSignature_ && !force_) return;
    if (signature == QSettings().value("library/resourceSignature").toString() && !force_) {
        lastSignature_ = signature;
        const QSettings cached;
        keysLoaded_ = cached.value("library/keysValidated", false).toBool();
        firmwareReady_ = cached.value("library/firmwareValidated", false).toBool();
        firmwareResult_ = cached.value("library/firmwareValidationResult").toString();
        validationComplete_ = true;
        status_ = tr("Biblioteca e recursos j\u00e1 verificados.");
        emit statusChanged();
        return;
    }
    force_ = false; lastSignature_ = signature; validationComplete_ = false;
    if (!temporary_.isValid() || !QDir().mkpath(dataDir_)) { status_ = tr("Não foi possível criar a área de análise."); emit statusChanged(); return; }
    requestFile_ = temporary_.path() + "/library-job.json";
    QFile::remove(requestFile_ + ".result");
    QSaveFile file(requestFile_);
    QJsonObject request{{"keys",keys},{"firmware",firmware},{"games",games}};
    if (!profileAction_.isEmpty()) request["profileAction"] = profileAction_;
    if (!contentPath_.isEmpty()) request["installContent"] = contentPath_;
    const auto bytes = QJsonDocument(request).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { status_ = tr("Falha ao preparar análise."); emit statusChanged(); return; }
    status_ = tr("Analisando recursos e metadados dos jogos...");
    process_.setProgram(QCoreApplication::applicationDirPath() + "/ShireiaCore.exe");
    process_.setArguments({"--library-job",requestFile_,"--data-dir",dataDir_});
    rescanPending_ = false;
    process_.start(); timeout_.start(); emit statusChanged();
    profileAction_ = {};
    contentPath_.clear();
}
void LibraryResources::finish(int code, QProcess::ExitStatus exit) {
    timeout_.stop();
    const auto pendingRescan = qScopeGuard([this] {
        if (rescanPending_) {
            rescanPending_ = false;
            schedule();
        }
    });
    keysLoaded_ = false; firmwareReady_ = false; firmwareResult_.clear(); validationComplete_ = true;
    QFile file(requestFile_ + ".result");
    if (exit != QProcess::NormalExit || !file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024) {
        status_ = tr("A análise falhou (código %1). A biblioteca continua disponível.").arg(code); emit statusChanged(); return;
    }
    const auto result = QJsonDocument::fromJson(file.readAll()).object();
    keysLoaded_ = result.value("keysLoaded").toBool();
    firmwareReady_ = keysLoaded_ && result.contains("firmware") && !result.contains("firmwareError");
    firmwareResult_ = firmwareReady_ ? result.value("firmware").toString() : result.value("firmwareError").toString();
    validationComplete_ = true;
    if (result.isEmpty() || result.contains("error")) { status_ = tr("Falha na análise: %1").arg(result["error"].toString()); emit statusChanged(); return; }
    profiles_ = result["profiles"].toArray().toVariantList();
    QSettings cache;
    cache.setValue("library/cachedProfiles", profiles_);
    cache.setValue("library/resourceSignature", lastSignature_);
    cache.setValue("library/keysValidated", keysLoaded_);
    cache.setValue("library/firmwareValidated", firmwareReady_);
    cache.setValue("library/firmwareValidationResult", firmwareResult_);
    cache.sync();
    emit profilesChanged();
    QVariantList entries; int named = 0, covered = 0;
    const auto iconRoot = QDir::cleanPath(dataDir_ + "/cache/icons") + "/";
    for (const auto& item : result["games"].toArray()) {
        auto entry = item.toObject().toVariantMap();
        const auto cover = entry.value("cover").toString();
        if (!cover.isEmpty()) {
            QImageReader reader(cover); const auto size = reader.size();
            if (!QFileInfo(cover).canonicalFilePath().startsWith(iconRoot) || !reader.canRead() || size.width() <= 0 || size.height() <= 0 || size.width() > 4096 || size.height() > 4096) entry.remove("cover");
            else { entry["cover"] = QUrl::fromLocalFile(cover).toString(); ++covered; }
        }
        if (!entry.value("title").toString().trimmed().isEmpty()) ++named;
        entries.append(entry);
    }
    applyingMetadata_ = true;
    model_.updateMetadata(entries);
    applyingMetadata_ = false;
    status_ = tr("Metadados: %1 nome(s), %2 capa(s). Arquivos sem metadados legíveis mantêm o nome local e o placeholder.").arg(named).arg(covered);
    if (result.contains("firmwareError")) status_ += "\n" + tr("Firmware não importado: %1").arg(result["firmwareError"].toString());
    else if (result.contains("firmware")) status_ += "\n" + result["firmware"].toString();
    if (result.contains("profileError")) status_ += "\n" + tr("Conta: %1").arg(result["profileError"].toString());
    else if (result.contains("profileAction")) status_ += "\n" + result["profileAction"].toString();
    if (result.contains("contentError")) status_ += "\n" + tr("Update/DLC: %1").arg(result["contentError"].toString());
    else if (result.contains("contentInstall")) status_ += "\n" + result["contentInstall"].toString();
    emit statusChanged();
}
