#include "AppModel.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QUrl>
#include <QCryptographicHash>
#include <QSet>
#include <QDateTime>

AppModel::AppModel(QObject* parent) : QObject(parent) { scan(); }
void AppModel::setPauseOnMenu(bool value) {
    settings_.setValue("session/pauseOnMenu", value);
    emit settingsChanged();
}
void AppModel::addFolder(const QUrl& folder) {
    const auto path = QFileInfo(folder.toLocalFile()).canonicalFilePath();
    if (path.isEmpty() || !QFileInfo(path).isDir()) return;
    auto list = folders();
    if (!list.contains(path)) list.append(path);
    settings_.setValue("library/folders", list);
    emit foldersChanged();
    scan();
}
void AppModel::removeFolder(int index) {
    auto list = folders();
    if (index < 0 || index >= list.size()) return;
    list.removeAt(index);
    settings_.setValue("library/folders", list);
    emit foldersChanged();
    scan();
}
void AppModel::scan() {
    games_.clear();
    QSet<QString> seen;
    int missing = 0;
    for (const auto& folder : folders()) {
        if (!QFileInfo(folder).isDir()) { ++missing; continue; }
        // Deliberately non-recursive in phase 1: bounded scope, no symlink traversal.
        QDirIterator it(folder, QDir::Files | QDir::Readable);
        while (it.hasNext()) {
            QFileInfo file(it.next());
            const auto ext = file.suffix().toLower();
            if (!QStringList{"nsp", "xci", "nro", "nso"}.contains(ext)) continue;
            const auto path = file.canonicalFilePath();
            if (seen.contains(path)) continue;
            seen.insert(path);
            const auto id = QString(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex());
            auto metadata = settings_.value("library/metadata/" + id).toMap();
            const auto stamp = path + ":" + QString::number(file.size()) + ":" + QString::number(file.lastModified().toMSecsSinceEpoch());
            if (metadata.value("stamp").toString() != stamp) metadata.clear();
            const auto title = metadata.value("title").toString().trimmed();
            const auto customCover = settings_.value("library/covers/" + id).toString();
            games_.append(QVariantMap{{"title", title.isEmpty() ? file.completeBaseName() : title}, {"path", path},
                                     {"titleId", metadata.value("titleId").toString()},
                                     {"format", ext.toUpper()}, {"bytes", file.size()},
                                     {"favorite", settings_.value("library/favorites").toStringList().contains(path)},
                                     {"viewed", settings_.value("library/viewed/" + QString(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex()), 0)},
                                     {"cover", customCover.isEmpty() ? metadata.value("cover").toString() : customCover}});
        }
    }
    status_ = tr("%1 arquivo(s) candidato(s). Conteúdo e compatibilidade não validados. %2 pasta(s) indisponível(is).")
                  .arg(games_.size()).arg(missing);
    emit gamesChanged();
    emit statusChanged();
    emit scanCompleted();
}
void AppModel::updateMetadata(const QVariantList& entries) {
    for (const auto& entry : entries) {
        const auto data = entry.toMap(); const auto path = data.value("path").toString();
        const auto file = QFileInfo(path);
        const auto stamp = file.canonicalFilePath() + ":" + QString::number(file.size()) + ":" + QString::number(file.lastModified().toMSecsSinceEpoch());
        if (data.value("stamp").toString() != stamp) continue;
        const auto id = QString(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex());
        settings_.setValue("library/metadata/" + id, data);
    }
    scan();
}
void AppModel::toggleFavorite(const QString& path) {
    auto favorites = settings_.value("library/favorites").toStringList();
    if (favorites.contains(path)) favorites.removeAll(path); else favorites.append(path);
    settings_.setValue("library/favorites", favorites);
    scan();
}
void AppModel::noteViewed(const QString& path) {
    const auto id = QString(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex());
    settings_.setValue("library/viewed/" + id, QDateTime::currentMSecsSinceEpoch());
    // Update locally without resetting view models during every selection.
    for (auto& value : games_) {
        auto game = value.toMap();
        if (game.value("path").toString() == path) {
            game["viewed"] = settings_.value("library/viewed/" + id);
            value = game;
        }
    }
}
void AppModel::setCover(const QString& path, const QUrl& image) {
    if (!image.isLocalFile() || !QFileInfo(image.toLocalFile()).isFile()) return;
    const auto suffix = QFileInfo(image.toLocalFile()).suffix().toLower();
    if (!QStringList{"png", "jpg", "jpeg", "webp"}.contains(suffix)) return;
    const auto id = QString(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex());
    settings_.setValue("library/covers/" + id, image.toString());
    scan();
}
