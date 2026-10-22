#include "GameContentManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QHash>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QProcess>
#include <QSettings>
#include <QCryptographicHash>
#include <QJsonArray>

namespace {
constexpr qint64 MaxImportBytes = 2LL * 1024 * 1024 * 1024;
constexpr int MaxImportFiles = 100000;
bool copyTree(const QString& source, const QString& destination, qint64& total, int& files) {
    QDir src(source);
    if (!src.exists() || QFileInfo(source).isSymLink() || !QDir().mkpath(destination)) return false;
    const auto entries = src.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
    for (const auto& entry : entries) {
        if (entry.isSymLink()) return false;
        const auto target = QDir(destination).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyTree(entry.absoluteFilePath(), target, total, files)) return false;
        } else if (entry.isFile()) {
            if (++files > MaxImportFiles || entry.size() < 0 || (total += entry.size()) > MaxImportBytes) return false;
            QFile input(entry.absoluteFilePath()); QSaveFile output(target);
            if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) return false;
            while (!input.atEnd()) {
                const auto bytes = input.read(1024 * 1024);
                if (bytes.isEmpty() || output.write(bytes) != bytes.size()) return false;
            }
            if (input.error() != QFile::NoError || !output.commit()) return false;
        } else return false;
    }
    return true;
}
bool looksLikeSuyuMod(const QString& path) {
    for (const auto& name : {QStringLiteral("romfs"), QStringLiteral("exefs"), QStringLiteral("cheats")})
        if (QFileInfo(QDir(path).filePath(name)).isDir()) return true;
    return false;
}
bool supportedCheatFileName(const QString& fileName) {
    static const QRegularExpression buildFile(QStringLiteral("^[0-9A-Fa-f]{40}\\.txt$"));
    return buildFile.match(fileName).hasMatch();
}
QString titlePath(const QString& root, const QString& title) { return QDir(root).filePath(title.toUpper()); }
}

GameContentManager::GameContentManager(QString dataDir, QObject* parent)
    : QObject(parent), dataDir_(QDir::cleanPath(QFileInfo(dataDir).absoluteFilePath())) {}

bool GameContentManager::validTitleId(const QString& titleId) const {
    static const QRegularExpression id(QStringLiteral("^[0-9A-Fa-f]{16}$"));
    return id.match(titleId).hasMatch() && titleId.toULongLong(nullptr, 16) != 0;
}

QJsonArray GameContentManager::inspectCheat(const QString& filePath) const {
    const QFileInfo info(filePath);
    if (!info.isFile() || info.isSymLink() || !info.isReadable() || info.size() <= 0 || info.size() > 1024 * 1024) return {};
    const auto canonical = info.canonicalFilePath();
    if (canonical.isEmpty()) return {};
    const auto id = QString::fromLatin1(QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha256).toHex());
    const auto stamp = canonical + ":" + QString::number(info.size()) + ":" + QString::number(info.lastModified().toMSecsSinceEpoch());
    QSettings settings;
    const auto cacheKey = "library/cheatValidation/" + id;
    const auto cached = settings.value(cacheKey).toMap();
    if (cached.value("stamp").toString() == stamp) return cached.value("entries").toJsonArray();
    QProcess validator;
    validator.setProgram(QCoreApplication::applicationDirPath() + "/ShireiaCore.exe");
    validator.setArguments({"--validate-cheat",canonical,"--data-dir",dataDir_});
    validator.start();
    if (!validator.waitForStarted(3000) || !validator.waitForFinished(10000) || validator.exitStatus() != QProcess::NormalExit || validator.exitCode() != 0) return {};
    const auto bytes = validator.readAllStandardOutput();
    if (bytes.size() > 64 * 1024) return {};
    const auto entries = QJsonDocument::fromJson(bytes).array();
    if (entries.isEmpty()) return {};
    settings.setValue(cacheKey, QVariantMap{{"stamp",stamp},{"entries",entries.toVariantList()}});
    settings.sync();
    return entries;
}
void GameContentManager::setStatus(QString status) {
    if (status_ == status) return;
    status_ = std::move(status); emit statusChanged();
}
QVariantList GameContentManager::mods(const QString& titleId) const {
    QVariantList result;
    if (!validTitleId(titleId)) return result;
    const auto activeRoot = titlePath(QDir(dataDir_).filePath("load"), titleId);
    const auto disabledRoot = titlePath(QDir(dataDir_).filePath("mods-disabled"), titleId);
    QHash<QString, QStringList> pathOwners;
    const QDir activeMods(activeRoot);
    for (const auto& mod : activeMods.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Hidden, QDir::Name)) {
        if (!looksLikeSuyuMod(mod.absoluteFilePath())) continue;
        for (const auto& layer : {QStringLiteral("romfs"), QStringLiteral("exefs")}) {
            const auto layerRoot = QDir(mod.absoluteFilePath()).filePath(layer);
            if (!QFileInfo(layerRoot).isDir()) continue;
            QDirIterator files(layerRoot, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
            while (files.hasNext()) {
                files.next();
                const auto relative = QDir(layerRoot).relativeFilePath(files.filePath()).toCaseFolded();
                pathOwners[layer + "/" + relative].append(mod.fileName());
            }
        }
    }
    auto append = [&](const QString& root, bool enabled) {
        QDir dir(root);
        const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Hidden, QDir::Name);
        for (const auto& entry : entries) {
            if (!looksLikeSuyuMod(entry.absoluteFilePath())) continue;
            QString name = entry.fileName(), version, description;
            QFile manifest(QDir(entry.absoluteFilePath()).filePath("mod.json"));
            if (manifest.open(QIODevice::ReadOnly) && manifest.size() <= 64 * 1024) {
                const auto json = QJsonDocument::fromJson(manifest.readAll()).object();
                if (json.value("name").isString()) name = json.value("name").toString().left(100);
                version = json.value("version").toString().left(50);
                description = json.value("description").toString().left(500);
            }
            QStringList conflicts;
            if (enabled) {
                for (auto it = pathOwners.cbegin(); it != pathOwners.cend(); ++it)
                    if (it.value().size() > 1 && it.value().contains(entry.fileName())) conflicts.append(it.key());
            }
            result.append(QVariantMap{{"name",name},{"folder",entry.fileName()},{"version",version},
                {"description",description},{"enabled",enabled},{"path",entry.absoluteFilePath()},
                {"conflicts",conflicts}});
            if (enabled) {
                const QDir cheats(QDir(entry.absoluteFilePath()).filePath("cheats"));
                for (const auto& cheat : cheats.entryInfoList(QDir::Files | QDir::Readable | QDir::NoSymLinks, QDir::Name)) {
                    if (!supportedCheatFileName(cheat.fileName()) || cheat.size() <= 0 || cheat.size() > 1024 * 1024) continue;
                    const auto parsed = inspectCheat(cheat.absoluteFilePath());
                    QStringList cheatNames;
                    for (const auto& entry : parsed) cheatNames.append(entry.toObject().value("name").toString());
                    result.append(QVariantMap{{"kind","cheat"},{"name",cheat.completeBaseName()},
                        {"folder",entry.fileName()},{"file",cheat.fileName()},
                        {"version",QStringLiteral("Build ID %1").arg(cheat.completeBaseName())},
                        {"description",parsed.isEmpty() ? tr("Formato rejeitado pelo parser Suyu.") : cheatNames.join(" · ")},
                        {"names",cheatNames},{"valid",!parsed.isEmpty()},
                        {"enabled",true},{"path",cheat.absoluteFilePath()}});
                }
            }
        }
    };
    append(activeRoot, true); append(disabledRoot, false);
    const auto disabledCheatRoot = titlePath(QDir(dataDir_).filePath("cheats-disabled"), titleId);
    const QDir disabledCheats(disabledCheatRoot);
    for (const auto& mod : disabledCheats.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name)) {
        for (const auto& cheat : QDir(mod.absoluteFilePath()).entryInfoList(QDir::Files | QDir::Readable | QDir::NoSymLinks, QDir::Name)) {
            if (!supportedCheatFileName(cheat.fileName()) || cheat.size() <= 0 || cheat.size() > 1024 * 1024) continue;
            const auto parsed = inspectCheat(cheat.absoluteFilePath());
            QStringList cheatNames;
            for (const auto& entry : parsed) cheatNames.append(entry.toObject().value("name").toString());
            result.append(QVariantMap{{"kind","cheat"},{"name",cheat.completeBaseName()},
                {"folder",mod.fileName()},{"file",cheat.fileName()},
                {"version",QStringLiteral("Build ID %1").arg(cheat.completeBaseName())},
                {"description",parsed.isEmpty() ? tr("Formato rejeitado pelo parser Suyu.") : cheatNames.join(" · ")},
                {"names",cheatNames},{"valid",!parsed.isEmpty()},
                {"enabled",false},{"path",cheat.absoluteFilePath()}});
        }
    }
    return result;
}
bool GameContentManager::importMod(const QString& titleId, const QString& folder) {
    if (!validTitleId(titleId)) { setStatus(tr("Title ID indisponível; atualize os metadados do jogo.")); return false; }
    const QFileInfo source(folder);
    if (!source.isAbsolute() || !source.isDir() || source.isSymLink() || !looksLikeSuyuMod(source.absoluteFilePath())) {
        setStatus(tr("Pasta inválida. Esperava uma estrutura Suyu com romfs, exefs ou cheats.")); return false;
    }
    const auto name = source.fileName();
    static const QRegularExpression safeName(QStringLiteral("^[\\p{L}\\p{N} ._()\\[\\]-]{1,100}$"));
    if (!safeName.match(name).hasMatch() || name == "." || name == "..") { setStatus(tr("Nome de pasta do mod inválido.")); return false; }
    const auto activeRoot = titlePath(QDir(dataDir_).filePath("load"), titleId);
    const auto disabledRoot = titlePath(QDir(dataDir_).filePath("mods-disabled"), titleId);
    if (!QDir().mkpath(activeRoot) || !QDir().mkpath(disabledRoot) || QFileInfo(activeRoot).isSymLink() || QFileInfo(disabledRoot).isSymLink()) {
        setStatus(tr("Não foi possível preparar a pasta privada de mods.")); return false;
    }
    if (QFileInfo(QDir(activeRoot).filePath(name)).exists() || QFileInfo(QDir(disabledRoot).filePath(name)).exists()) {
        setStatus(tr("Já existe um mod com esse nome para este jogo.")); return false;
    }
    const auto staging = QDir(activeRoot).filePath(".import-" + QString::number(QCoreApplication::applicationPid()));
    QDir(staging).removeRecursively();
    qint64 total = 0; int files = 0;
    if (!copyTree(source.absoluteFilePath(), staging, total, files) || !looksLikeSuyuMod(staging)) {
        QDir(staging).removeRecursively(); setStatus(tr("Importação recusada: mod inválido, link simbólico, arquivo especial ou limite excedido.")); return false;
    }
    const QDir cheatsDir(QDir(staging).filePath("cheats"));
    for (const auto& cheat : cheatsDir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name)) {
        if (supportedCheatFileName(cheat.fileName()) && inspectCheat(cheat.absoluteFilePath()).isEmpty()) {
            QDir(staging).removeRecursively();
            setStatus(tr("Importação recusada: um cheat Build ID .txt foi rejeitado pelo parser original do Suyu."));
            return false;
        }
    }
    if (!QDir(activeRoot).rename(QFileInfo(staging).fileName(), name)) {
        QDir(staging).removeRecursively(); setStatus(tr("Não foi possível ativar a pasta importada.")); return false;
    }
    setStatus(tr("Mod importado e ativado. A alteração será aplicada ao reiniciar o jogo.")); return true;
}
bool GameContentManager::importCheat(const QString& titleId, const QString& filePath) {
    if (!validTitleId(titleId)) { setStatus(tr("Title ID indisponível; atualize os metadados do jogo.")); return false; }
    const QFileInfo source(filePath);
    if (!source.isAbsolute() || !source.isFile() || source.isSymLink() || !supportedCheatFileName(source.fileName()) || inspectCheat(source.absoluteFilePath()).isEmpty()) {
        setStatus(tr("Cheat recusado: selecione um .txt com Build ID aceito pelo parser original do Suyu.")); return false;
    }
    const auto activeRoot = titlePath(QDir(dataDir_).filePath("load"), titleId);
    const auto modPath = QDir(activeRoot).filePath("Cheats importados");
    const auto cheatDir = QDir(modPath).filePath("cheats");
    if (!QDir().mkpath(activeRoot) || QFileInfo(activeRoot).isSymLink() ||
        (QFileInfo(modPath).exists() && !looksLikeSuyuMod(modPath)) || !QDir().mkpath(cheatDir) ||
        QFileInfo(cheatDir).isSymLink()) {
        setStatus(tr("Não foi possível preparar a pasta privada de cheats.")); return false;
    }
    const auto destinationName = source.completeBaseName().toUpper() + ".txt";
    const auto destination = QDir(cheatDir).filePath(destinationName);
    if (QFileInfo(destination).exists()) { setStatus(tr("Já existe um cheat para esse Build ID.")); return false; }
    QFile input(source.absoluteFilePath()); QSaveFile output(destination);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        setStatus(tr("Não foi possível ler ou gravar o cheat.")); return false;
    }
    const auto bytes = input.readAll();
    if (input.error() != QFile::NoError || bytes.isEmpty() || bytes.size() > 1024 * 1024 ||
        output.write(bytes) != bytes.size() || !output.commit() || inspectCheat(destination).isEmpty()) {
        output.cancelWriting(); QFile::remove(destination);
        setStatus(tr("Cheat recusado: arquivo inválido ou alterado durante a importação.")); return false;
    }
    setStatus(tr("Cheat importado e ativado. O Suyu verifica o Build ID ao iniciar; reinicie o jogo para aplicar.")); return true;
}
bool GameContentManager::setModEnabled(const QString& titleId, const QString& name, bool enabled) {
    if (!validTitleId(titleId) || name.isEmpty() || name.contains('/') || name.contains('\\') || name == "." || name == "..") return false;
    const auto activeRoot = titlePath(QDir(dataDir_).filePath("load"), titleId);
    const auto disabledRoot = titlePath(QDir(dataDir_).filePath("mods-disabled"), titleId);
    const auto from = QDir(enabled ? disabledRoot : activeRoot).filePath(name);
    const auto to = QDir(enabled ? activeRoot : disabledRoot).filePath(name);
    if (!looksLikeSuyuMod(from) || QFileInfo(to).exists() || !QDir().mkpath(QFileInfo(to).absolutePath()) || QFileInfo(QFileInfo(to).absolutePath()).isSymLink()) {
        setStatus(tr("Não foi possível alterar o estado deste mod.")); return false;
    }
    if (!QDir().rename(from, to)) { setStatus(tr("Falha ao mover o mod entre ativo e desativado.")); return false; }
    setStatus(tr("Estado alterado. Reinicie o jogo para aplicar.")); return true;
}
bool GameContentManager::setCheatEnabled(const QString& titleId, const QString& modName, const QString& fileName, bool enabled) {
    static const QRegularExpression safeModName(QStringLiteral("^[\\p{L}\\p{N} ._()\\[\\]-]{1,100}$"));
    if (!validTitleId(titleId) || !safeModName.match(modName).hasMatch() || !supportedCheatFileName(fileName)) return false;
    const auto activePath = QDir(QDir(titlePath(QDir(dataDir_).filePath("load"), titleId)).filePath(modName)).filePath("cheats/" + fileName);
    const auto disabledRoot = titlePath(QDir(dataDir_).filePath("cheats-disabled"), titleId);
    const auto disabledPath = QDir(QDir(disabledRoot).filePath(modName)).filePath(fileName);
    const auto source = enabled ? disabledPath : activePath;
    const auto destination = enabled ? activePath : disabledPath;
    if (!QFileInfo(source).isFile() || QFileInfo(source).isSymLink() || inspectCheat(source).isEmpty() || QFileInfo(destination).exists() ||
        QFileInfo(source).size() > 1024 * 1024 || !QDir().mkpath(QFileInfo(destination).absolutePath()) ||
        QFileInfo(QFileInfo(destination).absolutePath()).isSymLink() || !QDir().rename(source, destination)) {
        setStatus(tr("Não foi possível alterar esse cheat. Confirme que o mod está ativo e contém o arquivo pelo Build ID.")); return false;
    }
    setStatus(tr("Cheat alterado. O Suyu valida o Build ID e o formato ao iniciar; reinicie o jogo para aplicar.")); return true;
}
bool GameContentManager::removeMod(const QString& titleId, const QString& name) {
    if (!validTitleId(titleId) || name.isEmpty() || name.contains('/') || name.contains('\\') || name == "." || name == "..") return false;
    bool removed = false;
    for (const auto& root : {titlePath(QDir(dataDir_).filePath("load"), titleId), titlePath(QDir(dataDir_).filePath("mods-disabled"), titleId)}) {
        const auto path = QDir(root).filePath(name);
        if (looksLikeSuyuMod(path) && !QFileInfo(path).isSymLink()) removed = QDir(path).removeRecursively() || removed;
    }
    const auto disabledCheatDir = QDir(titlePath(QDir(dataDir_).filePath("cheats-disabled"), titleId)).filePath(name);
    if (QFileInfo(disabledCheatDir).isDir() && !QFileInfo(disabledCheatDir).isSymLink())
        QDir(disabledCheatDir).removeRecursively();
    setStatus(removed ? tr("Mod removido. Saves e arquivos do jogo não foram alterados.") : tr("Mod não encontrado."));
    return removed;
}
bool GameContentManager::openModsFolder(const QString& titleId) {
    if (!validTitleId(titleId)) return false;
    const auto path = titlePath(QDir(dataDir_).filePath("load"), titleId);
    if (!QDir().mkpath(path) || QFileInfo(path).isSymLink()) return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
