#include "SuyuAdapter.h"
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QLockFile>
#include <QUuid>
#include <QRegularExpression>
#include <QDirIterator>
#include <archive.h>
#include <archive_entry.h>
#include <memory>
#include <set>
#include <functional>
#include <stdexcept>
#include "core/crypto/key_manager.h"

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void save(const QString& path, const QByteArray& bytes) {
    QSaveFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit(), "Cannot commit private resource file");
}
QByteArray readSmall(const QString& path, qint64 limit) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly) && file.size() <= limit, "Cannot read resource file or file exceeds limit");
    return file.readAll();
}
QString stamp(const QFileInfo& file) {
    return file.canonicalFilePath() + ":" + QString::number(file.size()) + ":" + QString::number(file.lastModified().toMSecsSinceEpoch());
}
void copyFirmware(const QString& source, const QString& destination) {
    QFile input(source); QSaveFile output(destination);
    require(input.open(QIODevice::ReadOnly) && output.open(QIODevice::WriteOnly), "Cannot open firmware for copying");
    const auto size = input.size(); qint64 copied = 0;
    while (!input.atEnd()) {
        const auto bytes = input.read(1024 * 1024);
        require(!bytes.isEmpty() && output.write(bytes) == bytes.size(), "Firmware copy failed");
        copied += bytes.size();
    }
    require(copied == size && input.error() == QFile::NoError && output.commit(), "Incomplete firmware copy");
}
bool sameFirmwareFile(const QString& left, const QString& right) {
    QFileInfo leftInfo(left), rightInfo(right);
    if (!leftInfo.isFile() || !rightInfo.isFile() || leftInfo.size() != rightInfo.size()) return false;
    auto digest = [](const QString& path) {
        QFile file(path);
        require(file.open(QIODevice::ReadOnly), "Cannot verify an existing firmware file");
        QCryptographicHash hash(QCryptographicHash::Sha256);
        while (!file.atEnd()) {
            const auto bytes = file.read(1024 * 1024);
            require(!bytes.isEmpty() || file.error() == QFile::NoError, "Cannot read an existing firmware file");
            hash.addData(bytes);
        }
        require(file.error() == QFile::NoError, "Cannot finish verifying an existing firmware file");
        return hash.result();
    };
    return digest(left) == digest(right);
}
void copyDirectoryTree(const QString& sourceRoot, const QString& destinationRoot) {
    const auto sourceCanonical = QDir::cleanPath(QFileInfo(sourceRoot).canonicalFilePath());
    require(!sourceCanonical.isEmpty() && QDir().mkpath(destinationRoot), "Cannot create a firmware backup folder");
    int visited = 0;
    std::function<void(const QString&, int)> copy = [&](const QString& directory, int depth) {
        require(depth <= 8, "Firmware backup folder is too deeply nested");
        const auto entries = QDir(directory).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto& entry : entries) {
            require(++visited <= 4096 && !entry.isSymLink(), "Firmware backup contains too many entries or a symbolic link");
            const auto source = QDir::fromNativeSeparators(entry.canonicalFilePath());
            require(!source.isEmpty() && source.startsWith(QDir::fromNativeSeparators(sourceCanonical) + "/"),
                    "Firmware backup entry resolves outside the current installation");
            const auto relative = QDir(sourceRoot).relativeFilePath(entry.absoluteFilePath());
            const auto target = QDir(destinationRoot).filePath(relative);
            if (entry.isDir()) {
                require(QDir().mkpath(target), "Cannot create a firmware backup subfolder");
                copy(entry.absoluteFilePath(), depth + 1);
            } else {
                require(entry.isFile() && entry.isReadable(), "Cannot read the current firmware file for backup");
                copyFirmware(entry.absoluteFilePath(), target);
                require(QFileInfo(target).size() == entry.size(), "Firmware backup verification failed");
            }
        }
    };
    copy(sourceRoot, 0);
}
void syncFlatFirmwareFiles(const QString& sourceRoot, const QString& destinationRoot) {
    const auto staged = QDir(sourceRoot).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    require(!staged.isEmpty(), "Validated firmware staging folder is empty");
    for (const auto& file : staged) {
        const auto destination = destinationRoot + "/" + file.fileName();
        if (!sameFirmwareFile(file.absoluteFilePath(), destination))
            copyFirmware(file.absoluteFilePath(), destination);
    }
    // Firmware content is content-addressed and the original core can retain older
    // registered NCAs. Never delete entries during activation: open handles and
    // Windows file sharing rules can make removals fail after a partial update.
}
bool validFirmwareName(const QString& name) {
    static const QRegularExpression pattern("^[0-9a-f]{32}(?:\\.cnmt)?\\.nca$", QRegularExpression::CaseInsensitiveOption);
    return pattern.match(name).hasMatch();
}
QFileInfoList collectFirmwareNcas(const QString& rootPath) {
    const QString root = QDir::fromNativeSeparators(QFileInfo(rootPath).canonicalFilePath());
    require(!root.isEmpty(), "Cannot resolve extracted firmware folder");
    QFileInfoList files;
    std::set<QString> names;
    int visited = 0;
    std::function<void(const QString&, int)> scan = [&](const QString& directory, int depth) {
        require(depth <= 8 && visited <= 4096, "Extracted firmware folder is too deeply nested or contains too many entries");
        const auto entries = QDir(directory).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
        for (const auto& entry : entries) {
            require(++visited <= 4096, "Extracted firmware folder contains too many entries");
            if (entry.isSymLink()) continue;
            if (entry.isDir()) { scan(entry.absoluteFilePath(), depth + 1); continue; }
            if (!entry.isFile() || entry.suffix().compare("nca", Qt::CaseInsensitive) != 0) continue;
            const auto canonical = QDir::fromNativeSeparators(entry.canonicalFilePath());
            require(!canonical.isEmpty() && canonical.startsWith(root + "/"), "Firmware NCA resolves outside the selected folder");
            require(validFirmwareName(entry.fileName()) && entry.size() > 0 && entry.size() <= 512LL * 1024 * 1024,
                    "Firmware filenames or sizes are not valid");
            require(files.size() < 512 && names.insert(entry.fileName().toLower()).second,
                    "Duplicate or excessive firmware NCA files");
            files.append(entry);
        }
    };
    scan(root, 0);
    return files;
}
QStringList extractFirmwareZip(const QString& source, const QString& staging) {
    using ArchivePtr = std::unique_ptr<archive, decltype(&archive_read_free)>;
    ArchivePtr reader(archive_read_new(), archive_read_free);
    require(reader != nullptr, "Cannot create ZIP reader");
    require(archive_read_support_format_zip(reader.get()) == ARCHIVE_OK, "ZIP format is unavailable");
    require(archive_read_open_filename_w(reader.get(), reinterpret_cast<const wchar_t*>(source.utf16()), 1024 * 1024) == ARCHIVE_OK,
            "Cannot open firmware ZIP");
    QStringList names;
    std::set<QString> seen;
    qint64 total = 0;
    archive_entry* entry = nullptr;
    int status;
    while ((status = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
        const QString path = QString::fromUtf8(archive_entry_pathname(entry));
        if (archive_entry_filetype(entry) == AE_IFDIR) { archive_read_data_skip(reader.get()); continue; }
        require(archive_entry_filetype(entry) == AE_IFREG, "Firmware ZIP contains a nonregular entry");
        const auto name = path.section('/', -1);
        if (!name.endsWith(".nca", Qt::CaseInsensitive)) { archive_read_data_skip(reader.get()); continue; }
        require(validFirmwareName(name) && !path.contains('\\') && !path.startsWith('/') && !path.contains("../") &&
                !path.contains("./") && !path.contains(':') && !seen.contains(name.toLower()), "Unsafe or duplicate firmware ZIP entry");
        const auto declaredSize = archive_entry_size(entry);
        require(declaredSize > 0 && declaredSize <= 512LL * 1024 * 1024 && names.size() < 512,
                "Firmware ZIP entry exceeds size or count limit");
        QSaveFile output(staging + "/" + name);
        require(output.open(QIODevice::WriteOnly), "Cannot stage firmware ZIP entry");
        char buffer[1024 * 1024];
        qint64 written = 0;
        while (true) {
            const auto count = archive_read_data(reader.get(), buffer, sizeof(buffer));
            require(count >= 0, "Firmware ZIP entry is damaged");
            if (count == 0) break;
            written += count; total += count;
            require(written <= declaredSize && total <= 4LL * 1024 * 1024 * 1024,
                    "Firmware ZIP exceeds extraction limit");
            require(output.write(buffer, count) == count, "Cannot write firmware ZIP entry");
        }
        require(written == declaredSize && output.commit(), "Incomplete firmware ZIP entry");
        if (SuyuAdapter::validateFirmware(staging + "/" + name)) names.append(name);
        else QFile::remove(staging + "/" + name);
        seen.insert(name.toLower());
    }
    require(status == ARCHIVE_EOF && !names.isEmpty(), "Firmware ZIP has no complete NCA set");
    return names;
}
}
int runLibraryJob(const QString& requestFile, const QString& dataDir) {
    QJsonObject result;
    try {
        const auto request = QJsonDocument::fromJson(readSmall(requestFile, 8 * 1024 * 1024)).object();
        require(!request.isEmpty(), "Invalid library job");
        QLockFile lock(dataDir + "/resources.lock");
        require(lock.tryLock(0), "Another system resource import is active");
        const auto keysSource = request["keys"].toString();
        const auto keysDestination = dataDir + "/keys/prod.keys";
        if (!keysSource.isEmpty() && QFileInfo(keysSource).exists()) {
            const auto bytes = readSmall(keysSource, 4 * 1024 * 1024);
            require(!bytes.trimmed().isEmpty(), "Empty prod.keys file");
            // Check using the real key manager before replacing an existing installation.
            QTemporaryDir candidate(dataDir + "/keys-candidate-XXXXXX");
            require(candidate.isValid(), "Cannot stage keys");
            SuyuAdapter::configurePaths(candidate.path());
            // configurePaths uses a keys subdirectory.
            save(candidate.path() + "/keys/prod.keys", bytes);
            Core::Crypto::KeyManager::Instance().ReloadKeys();
            require(Core::Crypto::KeyManager::Instance().AreKeysLoaded(), "No recognized production keys were loaded");
            if (QFileInfo(keysDestination).canonicalFilePath() != QFileInfo(keysSource).canonicalFilePath()) save(keysDestination, bytes);
        }
        SuyuAdapter::configurePaths(dataDir);
        Core::Crypto::KeyManager::Instance().ReloadKeys();
        const bool keysLoaded = Core::Crypto::KeyManager::Instance().AreKeysLoaded();
        result["keysLoaded"] = keysLoaded;
        const auto firmwareSource = request["firmware"].toString();
        if (!firmwareSource.isEmpty()) {
            try {
                require(keysLoaded, "Select readable production keys before importing firmware");
                const QFileInfo sourceInfo(firmwareSource);
                const bool zipped = sourceInfo.isFile() && sourceInfo.suffix().compare("zip", Qt::CaseInsensitive) == 0;
                require(sourceInfo.isDir() || zipped, "Firmware source must be a directory or ZIP");
                QFileInfoList ncas; QByteArray signature;
                if (zipped) signature = stamp(sourceInfo).toUtf8();
                else {
                    ncas = collectFirmwareNcas(firmwareSource);
                    for (const auto& file : ncas) signature += stamp(file).toUtf8() + '\n';
                    require(!ncas.isEmpty(), "No firmware NCA files found");
                }
                const auto fingerprint = QString::fromLatin1(QCryptographicHash::hash(signature, QCryptographicHash::Sha256).toHex());
                const auto contents = dataDir + "/nand/system/Contents";
                const auto registered = contents + "/registered";
                require(QDir().mkpath(contents), "Cannot create private NAND");
                // All moves stay in this private Contents directory; reject redirected targets.
                require(QFileInfo(contents).canonicalFilePath() == QDir::cleanPath(QFileInfo(contents).absoluteFilePath()), "Private NAND must not be redirected");
                require(!QFileInfo(registered).isSymLink(), "Registered NAND directory must not be a link");
                QString previousFingerprint;
                int previousCount = 0;
                QFile previous(registered + "/shireia-import.json");
                if (previous.open(QIODevice::ReadOnly)) {
                    const auto prior = QJsonDocument::fromJson(previous.readAll()).object();
                    previousFingerprint = prior["fingerprint"].toString();
                    previousCount = prior["files"].toInt();
                }
                bool complete = QDir(registered).exists();
                if (!zipped) for (const auto& file : ncas) if (QFileInfo(registered + "/" + file.fileName()).size() != file.size()) complete = false;
                int installedCount = ncas.size();
                if (zipped && complete) installedCount = previousCount;
                if (previousFingerprint != fingerprint || !complete) {
                    QTemporaryDir staging(contents + "/.shireia-firmware-XXXXXX");
                    require(staging.isValid(), "Cannot stage firmware installation");
                    if (zipped) installedCount = extractFirmwareZip(firmwareSource, staging.path()).size();
                    else {
                        installedCount = 0;
                        for (const auto& file : ncas) {
                        const auto target = staging.path() + "/" + file.fileName();
                        copyFirmware(file.absoluteFilePath(), target);
                        if (SuyuAdapter::validateFirmware(target)) ++installedCount;
                        else QFile::remove(target);
                        }
                    }
                    require(installedCount > 0, "No system firmware NCA files found");
                    save(staging.path() + "/shireia-import.json", QJsonDocument(QJsonObject{{"fingerprint",fingerprint},{"files",installedCount}}).toJson());
                    QDir parent(contents);
                    const auto backup = "registered-backup-" + QUuid::createUuid().toString(QUuid::Id128);
                    const bool hadOld = QDir(registered).exists();
                    const auto backupPath = contents + "/" + backup;
                    const bool oldMoved = hadOld && parent.rename("registered", backup);
                    const bool inPlaceBackup = hadOld && !oldMoved;
                    if (inPlaceBackup) {
                        try { copyDirectoryTree(registered, backupPath); }
                        catch (...) {
                            QDir(backupPath).removeRecursively();
                            throw std::runtime_error("Could not verify a backup of the current firmware; the existing installation was left untouched.");
                        }
                    }
                    bool activated = false;
                    if (inPlaceBackup) {
                        try {
                            syncFlatFirmwareFiles(staging.path(), registered);
                            activated = true;
                        } catch (const std::exception& updateError) {
                            const auto updateMessage = QString::fromUtf8(updateError.what());
                            try { syncFlatFirmwareFiles(backupPath, registered); }
                            catch (const std::exception& restoreError) {
                                throw std::runtime_error(QString("Firmware update failed (%1); restore also failed (%2). The previous firmware remains in its verified backup folder.")
                                    .arg(updateMessage, QString::fromUtf8(restoreError.what())).toUtf8().constData());
                            }
                            throw std::runtime_error(QString("Could not activate the validated firmware (%1); the previous installation was restored.")
                                .arg(updateMessage).toUtf8().constData());
                        }
                    } else {
                        activated = parent.rename(QFileInfo(staging.path()).fileName(), "registered");
                        if (!activated && QDir().mkpath(registered)) {
                            // Windows can refuse a directory rename even when all staged
                            // files are valid. Copy the verified stage as a fallback.
                            try { syncFlatFirmwareFiles(staging.path(), registered); activated = true; }
                            catch (const std::exception&) { QDir(registered).removeRecursively(); }
                        }
                        if (!activated) {
                            const bool restored = !oldMoved || parent.rename(backup, "registered");
                            if (!restored) throw std::runtime_error("Firmware activation failed; the previous firmware is preserved in its backup folder.");
                            throw std::runtime_error("Could not activate the validated firmware; the previous installation was restored.");
                        }
                    }
                    if (!QFileInfo(staging.path()).exists()) staging.setAutoRemove(false);
                }
                result["firmware"] = QString("Firmware importado: %1 arquivo(s), em %2").arg(installedCount).arg(registered);
                if (!zipped && installedCount < ncas.size()) result["firmware"] = result["firmware"].toString() +
                    QString("; %1 NCA(s) fora da faixa de sistema ignorados").arg(ncas.size() - installedCount);
            } catch (const std::exception& error) { result["firmwareError"] = QString::fromUtf8(error.what()); }
        }
        SuyuAdapter adapter;
        if (request.contains("installContent")) {
            try {
                require(keysLoaded, "Production keys are required to install NSP content");
                const auto installed = adapter.installContent(request["installContent"].toString());
                const auto outcome = installed["result"].toString();
                if (outcome == "success") result["contentInstall"] = "Update/DLC instalado na NAND privada.";
                else if (outcome == "overwrite") result["contentInstall"] = "Update/DLC existente substituído na NAND privada.";
                else if (outcome == "base-install-rejected") result["contentError"] = "O arquivo é um jogo base, não update/DLC.";
                else result["contentError"] = "O Suyu não conseguiu instalar o NSP.";
            } catch (const std::exception& error) { result["contentError"] = QString::fromUtf8(error.what()).left(512); }
        }
        if (request.contains("profileAction")) {
            try { adapter.editProfile(request["profileAction"].toObject(), dataDir); result["profileAction"] = "Conta atualizada."; }
            catch (const std::exception& error) { result["profileError"] = QString::fromUtf8(error.what()).left(512); }
        }
        result["profiles"] = adapter.profiles(dataDir);
        QJsonArray games;
        for (const auto& item : request["games"].toArray()) {
            const auto path = item.toString();
            QJsonObject entry{{"path",path}};
            try {
                require(QFileInfo(path).isFile(), "Game file is unavailable");
                const auto ext = QFileInfo(path).suffix().toLower();
                require(QStringList{"nsp","xci","nro","nso"}.contains(ext), "Unsupported library format");
                const auto metadata = adapter.metadata(path);
                for (auto it = metadata.begin(); it != metadata.end(); ++it) entry[it.key()] = it.value();
                entry["stamp"] = stamp(QFileInfo(path));
                if (entry.contains("icon")) {
                    const auto bytes = QByteArray::fromBase64(entry.take("icon").toString().toLatin1());
                    const auto cache = dataDir + "/cache/icons";
                    require(QDir().mkpath(cache), "Cannot create icon cache");
                    const auto iconPath = cache + "/" + QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) + ".img";
                    if (!QFileInfo(iconPath).exists()) save(iconPath, bytes);
                    entry["cover"] = iconPath;
                }
            } catch (const std::exception& error) { entry["error"] = QString::fromUtf8(error.what()).left(512); }
            games.append(entry);
        }
        result["games"] = games;
    } catch (const std::exception& error) { result["error"] = QString::fromUtf8(error.what()).left(512); }
    try { save(requestFile + ".result", QJsonDocument(result).toJson(QJsonDocument::Compact)); }
    catch (...) { return 1; }
    return result.contains("error") ? 1 : 0;
}
