#include "SettingsStore.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <functional>

namespace {
QString gameOverrideGroup(const QString& path) {
    const auto canonical = QFileInfo(path).canonicalFilePath();
    if (canonical.isEmpty()) return {};
    const auto id = QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha256).toHex();
    const auto metadata = QSettings().value(QStringLiteral("library/metadata/") + QString::fromLatin1(id)).toMap();
    const auto titleId = metadata.value("titleId").toString().toUpper();
    static const QRegularExpression titlePattern(QStringLiteral("^[0-9A-F]{16}$"));
    if (titlePattern.match(titleId).hasMatch()) return QStringLiteral("library/gameOverrides/title/") + titleId;
    return QStringLiteral("library/gameOverrides/path/") + QString::fromLatin1(id);
}
QString legacyGameOverrideGroup(const QString& path) {
    const auto canonical = QFileInfo(path).canonicalFilePath();
    if (canonical.isEmpty()) return {};
    const auto id = QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("library/gameOverrides/") + QString::fromLatin1(id);
}
bool validGameOverride(const QString& key, int value) {
    if (key == "graphics/resolution") return value >= 0 && value <= 12;
    if (key == "graphics/scalingFilter") return value >= 0 && value <= 14;
    if (key == "graphics/vsync") return value >= 0 && value <= 3;
    if (key == "graphics/antiAliasing") return value >= 0 && value <= 2;
    if (key == "graphics/docked") return value >= 0 && value <= 1;
    if (key == "graphics/vulkanDevice") return value >= 0 && value <= 31;
    if (key == "graphics/gpuAccuracy" || key == "graphics/vramUsage") return value >= 0 && value <= 1;
    if (key == "graphics/astcDecode" || key == "graphics/astcRecompression" || key == "graphics/nvdec" || key == "graphics/antiAliasing") return value >= 0 && value <= 2;
    if (key == "graphics/anisotropy") return value >= 0 && value <= 8;
    if (key == "graphics/aspectRatio") return value >= 0 && value <= 5;
    if (key == "graphics/fsrSharpness") return value >= 0 && value <= 200;
    if (key == "graphics/dmaAccuracy") return value >= 0 && value <= 2;
    if (key == "graphics/gpuFence") return value >= 0 && value <= 4;
    if (key == "graphics/fastGpuDivisor" || key == "core/cpuOverclock") return value >= 0 && value <= 2;
    if (key == "core/language") return value >= 0 && value <= 19;
    if (key == "core/region") return value >= 0 && value <= 6;
    if (key == "core/cpuAccuracy") return value >= 0 && value <= 3;
    if (key == "core/memoryLayout") return value >= 0 && value <= 4;
    if (key == "core/cpuTicks") return value >= 77 && value <= 65535;
    if (key == "core/speedLimit") return value >= 50 && value <= 200;
    if (key == "audio/volume") return value >= 0 && value <= 100;
    return false;
}
bool validBoolOverride(const QString& key) {
    return key == "graphics/shaderCache" || key == "graphics/asynchronousShaders" || key == "graphics/asynchronousGpu" ||
           key == "graphics/pipelineCache" || key == "graphics/reactiveFlushing" || key == "graphics/fastGpuTime" ||
           key == "graphics/skipCpuInvalidation" || key == "core/multicore" || key == "core/customCpuTicks";
}
bool knownIntOverride(const QString& key) {
    return key == "graphics/resolution" || key == "graphics/scalingFilter" || key == "graphics/vsync" ||
           key == "graphics/antiAliasing" || key == "graphics/docked" || key == "graphics/gpuAccuracy" ||
           key == "graphics/vramUsage" || key == "graphics/astcDecode" || key == "graphics/astcRecompression" ||
           key == "graphics/nvdec" || key == "graphics/anisotropy" || key == "graphics/aspectRatio" ||
           key == "graphics/fsrSharpness" || key == "graphics/dmaAccuracy" || key == "graphics/gpuFence" ||
           key == "graphics/fastGpuDivisor" || key == "core/cpuOverclock" || key == "core/language" ||
           key == "core/region" || key == "core/cpuAccuracy" || key == "core/memoryLayout" ||
           key == "core/cpuTicks" || key == "core/speedLimit" || key == "audio/volume";
}
}

QVariantMap SettingsStore::initial() {
    return {{"library/folders", QStringList{}}, {"ui/view", 0}, {"ui/sort", 0},
            {"ui/thumb", 160}, {"ui/fullscreen", false}, {"ui/controller", true}, {"ui/hideCursorWithController", true},
            {"ui/animations", true}, {"ui/showPaths", true}, {"session/pauseOnMenu", true},
            {"system/prodKeys", QString{}}, {"system/firmwareFolder", QString{}},
            {"system/language",1}, {"system/region",1}, {"core/multicore",true},
            {"core/cpuAccuracy",0}, {"core/memoryLayout",0},
            {"input/gameMenuKey",0}, {"input/screenshotShortcut",QStringLiteral("F12")}, {"ui/showFrameRate",false},
            {"graphics/resolution",3}, {"graphics/shaderCache",true}, {"graphics/docked",false},
            {"graphics/vsync",2}, {"graphics/asynchronousShaders",true},
            {"graphics/asynchronousGpu",true}, {"graphics/pipelineCache",true},
            {"graphics/vulkanDevice",0}, {"graphics/gpuAccuracy",1}, {"graphics/astcDecode",1},
            {"graphics/anisotropy",0}, {"graphics/scalingFilter",1}, {"graphics/antiAliasing",0},
            {"graphics/vramUsage",0}, {"graphics/astcRecompression",0},
            {"graphics/aspectRatio",0}, {"graphics/fsrSharpness",25},
            {"graphics/nvdec",2}, {"graphics/reactiveFlushing",true},
            {"graphics/dmaAccuracy",0}, {"graphics/gpuFence",0},
            {"graphics/fastGpuTime",false}, {"graphics/fastGpuDivisor",1}, {"graphics/skipCpuInvalidation",false},
            {"core/cpuOverclock",0}, {"core/customCpuTicks",false}, {"core/cpuTicks",16000},
            {"audio/volume",100}, {"audio/outputDevice",QString{"auto"}}, {"core/speedLimit",100}, {"system/userUuid",QString{}}};
}
SettingsStore::SettingsStore(QObject* parent) : QObject(parent) {
    if (!settings_.contains("graphics/schemaVersion")) {
        if (settings_.contains("graphics/resolution")) {
            static constexpr int oldScaleMap[]{1, 3, 6};
            const int oldScale = settings_.value("graphics/resolution").toInt();
            if (oldScale >= 0 && oldScale < 3) settings_.setValue("graphics/resolution", oldScaleMap[oldScale]);
        }
        if (settings_.contains("graphics/scalingFilter")) {
            static constexpr int oldFilterMap[]{0, 1, 2, 4, 6};
            const int oldFilter = settings_.value("graphics/scalingFilter").toInt();
            if (oldFilter >= 0 && oldFilter < 5) settings_.setValue("graphics/scalingFilter", oldFilterMap[oldFilter]);
        }
        settings_.setValue("graphics/schemaVersion", 1);
        settings_.sync();
    }
    refresh();
}
QString SettingsStore::coreDataPath() const { return QCoreApplication::applicationDirPath()+"/core-data"; }
bool SettingsStore::openCoreDirectory(const QString& name) {
    if (!QStringList{"nand","sdmc","save","screenshots","log","shader","keys"}.contains(name)) return false;
    const auto path=coreDataPath()+"/"+name;
    return QDir().mkpath(path) && QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
void SettingsStore::refresh() {
    values_ = initial();
    for (auto it = values_.begin(); it != values_.end(); ++it) it.value() = settings_.value(it.key(), it.value());
    draft_ = values_;
    emit draftChanged();
    emit valuesChanged();
}
void SettingsStore::setValue(const QString& key, const QVariant& value) {
    if (!initial().contains(key)) return;
    if (key == "ui/thumb") draft_[key] = qBound(120, value.toInt(), 240);
    else if (key == "ui/view") draft_[key] = qBound(0, value.toInt(), 1);
    else if (key == "ui/sort") draft_[key] = qBound(0, value.toInt(), 3);
    else if (key == "input/gameMenuKey") draft_[key] = qBound(0, value.toInt(), 3);
    else if (key == "input/screenshotShortcut") {
        const auto shortcut = value.toString();
        if (!QStringList{"F12", "Ctrl+Shift+S", "Ctrl+P"}.contains(shortcut)) return;
        draft_[key] = shortcut;
    }
    else if (key == "graphics/resolution") draft_[key] = qBound(0, value.toInt(), 12);
    else if (key == "graphics/vsync") draft_[key] = qBound(0, value.toInt(), 3);
    else if (key == "graphics/vulkanDevice") draft_[key] = qBound(0, value.toInt(), 31);
    else if (key == "graphics/gpuAccuracy" || key == "graphics/vramUsage") draft_[key] = qBound(0, value.toInt(), 1);
    else if (key == "graphics/dmaAccuracy") draft_[key] = qBound(0, value.toInt(), 2);
    else if (key == "graphics/gpuFence") draft_[key] = qBound(0, value.toInt(), 4);
    else if (key == "graphics/astcDecode" || key == "graphics/antiAliasing" || key == "graphics/astcRecompression" || key == "graphics/nvdec") draft_[key] = qBound(0, value.toInt(), 2);
    else if (key == "graphics/anisotropy") draft_[key] = qBound(0, value.toInt(), 8);
    else if (key == "graphics/scalingFilter") draft_[key] = qBound(0, value.toInt(), 14);
    else if (key == "graphics/aspectRatio") draft_[key] = qBound(0, value.toInt(), 5);
    else if (key == "graphics/fsrSharpness") draft_[key] = qBound(0, value.toInt(), 200);
    else if (key == "system/language") draft_[key] = qBound(0, value.toInt(), 19);
    else if (key == "system/region") draft_[key] = qBound(0, value.toInt(), 6);
    else if (key == "core/cpuAccuracy") draft_[key] = qBound(0, value.toInt(), 3);
    else if (key == "core/memoryLayout") draft_[key] = qBound(0, value.toInt(), 4);
    else if (key == "graphics/fastGpuDivisor" || key == "core/cpuOverclock") draft_[key] = qBound(0, value.toInt(), 2);
    else if (key == "core/cpuTicks") draft_[key] = qBound(77, value.toInt(), 65535);
    else if (key == "audio/volume") draft_[key] = qBound(0, value.toInt(), 100);
    else if (key == "core/speedLimit") draft_[key] = qBound(50, value.toInt(), 200);
    else if (key == "audio/outputDevice") draft_[key] = value.toString().left(256);
    else if (key == "library/folders") draft_[key] = value.toStringList();
    // Nonempty paths enter through the selectors, which check local resources.
    else if (key == "system/prodKeys" || key == "system/firmwareFolder" || key == "system/userUuid") {
        if (!value.toString().isEmpty()) return;
        draft_[key] = QString{};
    }
    else draft_[key] = value.toBool();
    emit draftChanged();
}
bool SettingsStore::selectActiveUser(const QString& uuid) {
    static const QRegularExpression pattern("^[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}$",
                                            QRegularExpression::CaseInsensitiveOption);
    if (!pattern.match(uuid).hasMatch()) return false;
    settings_.setValue("system/userUuid", uuid.toLower());
    settings_.sync();
    if (settings_.status() != QSettings::NoError) return false;
    values_["system/userUuid"] = uuid.toLower();
    draft_["system/userUuid"] = uuid.toLower();
    emit valuesChanged(); emit draftChanged();
    return true;
}
bool SettingsStore::apply() {
    for (auto it = draft_.cbegin(); it != draft_.cend(); ++it) settings_.setValue(it.key(), it.value());
    settings_.sync();
    if (settings_.status() != QSettings::NoError) return false;
    values_ = draft_;
    emit valuesChanged();
    emit draftChanged();
    emit applied();
    return true;
}
void SettingsStore::cancel() { draft_ = values_; emit draftChanged(); }
void SettingsStore::defaults() { draft_ = initial(); emit draftChanged(); }
void SettingsStore::addFolder(const QUrl& folder) {
    const auto path = QFileInfo(folder.toLocalFile()).canonicalFilePath();
    if (path.isEmpty() || !QFileInfo(path).isDir()) return;
    auto folders = draft_.value("library/folders").toStringList();
    if (!folders.contains(path)) folders.append(path);
    draft_["library/folders"] = folders;
    emit draftChanged();
}
void SettingsStore::removeFolder(int index) {
    auto folders = draft_.value("library/folders").toStringList();
    if (index < 0 || index >= folders.size()) return;
    folders.removeAt(index);
    draft_["library/folders"] = folders;
    emit draftChanged();
}
void SettingsStore::openFolder(const QString& path) {
    if (QFileInfo(path).isDir()) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
int SettingsStore::gameOverride(const QString& path, const QString& key) const {
    if (!knownIntOverride(key) && !validBoolOverride(key)) return -1;
    const auto group = gameOverrideGroup(path);
    if (group.isEmpty()) return -1;
    const auto setting = group + "/" + key;
    if (settings_.contains(setting)) return settings_.value(setting).toInt();
    const auto legacy = legacyGameOverrideGroup(path) + "/" + key;
    return settings_.contains(legacy) ? settings_.value(legacy).toInt() : -1;
}
QVariant SettingsStore::gameOverrideValue(const QString& path, const QString& key) const {
    const auto group = gameOverrideGroup(path);
    if (group.isEmpty()) return {};
    QSettings settings;
    const auto item = group + "/" + key;
    if (settings.contains(item)) return settings.value(item);
    const auto legacy = legacyGameOverrideGroup(path) + "/" + key;
    return settings.contains(legacy) ? settings.value(legacy) : QVariant{};
}
bool SettingsStore::hasGameOverrides(const QString& path) const {
    const auto group = gameOverrideGroup(path);
    if (group.isEmpty()) return false;
    QSettings settings;
    settings.beginGroup(group);
    const bool present = !settings.allKeys().isEmpty();
    settings.endGroup();
    if (present) return true;
    settings.beginGroup(legacyGameOverrideGroup(path));
    const bool legacyPresent = !settings.allKeys().isEmpty();
    settings.endGroup();
    return legacyPresent;
}
bool SettingsStore::setGameOverride(const QString& path, const QString& key, int value) {
    const auto group = gameOverrideGroup(path);
    if (group.isEmpty() || !knownIntOverride(key) || (value >= 0 && !validGameOverride(key, value))) return false;
    const auto setting = group + "/" + key;
    if (value < 0) settings_.remove(setting);
    else settings_.setValue(setting, value);
    settings_.sync();
    return settings_.status() == QSettings::NoError;
}
bool SettingsStore::setGameOverrideValue(const QString& path, const QString& key, const QVariant& value) {
    if (validBoolOverride(key)) {
        if (value.metaType().id() != QMetaType::Bool) return false;
    } else if (key == "audio/outputDevice") {
        if (value.metaType().id() != QMetaType::QString || value.toString().size() > 256) return false;
    } else if (!value.canConvert<int>() || !validGameOverride(key, value.toInt())) return false;
    const auto group = gameOverrideGroup(path);
    if (group.isEmpty()) return false;
    QSettings settings;
    settings.setValue(group + "/" + key, value);
    settings.sync();
    return settings.status() == QSettings::NoError;
}
bool SettingsStore::clearGameOverride(const QString& path, const QString& key) {
    if (!knownIntOverride(key) && !validBoolOverride(key) && key != "audio/outputDevice") return false;
    const auto group = gameOverrideGroup(path);
    if (group.isEmpty()) return false;
    settings_.remove(group + "/" + key);
    const auto legacy = legacyGameOverrideGroup(path);
    if (!legacy.isEmpty()) settings_.remove(legacy + "/" + key);
    settings_.sync();
    return settings_.status() == QSettings::NoError;
}
bool SettingsStore::clearGameOverrides(const QString& path) {
    const auto group = gameOverrideGroup(path);
    if (group.isEmpty()) return false;
    settings_.beginGroup(group);
    settings_.remove("");
    settings_.endGroup();
    const auto legacy = legacyGameOverrideGroup(path);
    if (!legacy.isEmpty()) settings_.remove(legacy);
    settings_.sync();
    return settings_.status() == QSettings::NoError;
}
QString SettingsStore::selectProdKeys(const QUrl& url) {
    const QFileInfo info(url.toLocalFile());
    if (!url.isLocalFile() || !info.isFile() || info.fileName().compare("prod.keys", Qt::CaseInsensitive) != 0)
        return tr("Selecione um arquivo local chamado prod.keys.");
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly) || file.size() == 0)
        return tr("O arquivo prod.keys está vazio ou não pode ser lido.");
    // Do not read, log, copy or store key contents in QSettings.
    draft_["system/prodKeys"] = info.canonicalFilePath();
    emit draftChanged();
    return {};
}
QString SettingsStore::selectFirmware(const QUrl& url) {
    const QFileInfo info(url.toLocalFile());
    if (!url.isLocalFile() || !info.isReadable() || (!info.isDir() && !(info.isFile() && info.suffix().compare("zip", Qt::CaseInsensitive) == 0)))
        return tr("Selecione uma pasta de firmware extraído ou um arquivo ZIP local.");
    if (info.isFile()) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly) || file.size() < 22 || file.read(4) != "PK\003\004")
            return tr("O arquivo selecionado não é um ZIP de firmware legível.");
        draft_["system/firmwareFolder"] = info.canonicalFilePath();
        emit draftChanged();
        return {};
    }
    bool found = false;
    int visited = 0;
    std::function<void(const QString&, int)> scan = [&](const QString& directory, int depth) {
        if (found || depth > 8 || visited >= 4096) return;
        const auto entries = QDir(directory).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
        for (const auto& entry : entries) {
            if (++visited > 4096) return;
            if (entry.isSymLink()) continue;
            if (entry.isDir()) scan(entry.absoluteFilePath(), depth + 1);
            else if (entry.isFile() && entry.isReadable() && entry.suffix().compare("nca", Qt::CaseInsensitive) == 0 && entry.size() > 0) { found = true; return; }
            if (found) return;
        }
    };
    scan(info.canonicalFilePath(), 0);
    if (!found) return tr("A pasta deve conter arquivos .nca de firmware extraído, diretamente dentro dela.");
    draft_["system/firmwareFolder"] = info.canonicalFilePath();
    emit draftChanged();
    return {};
}
