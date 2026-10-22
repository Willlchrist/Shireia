#include "QualityTools.h"
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QDesktopServices>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

QualityTools::QualityTools(QString dataDir, QObject* parent)
    : QObject(parent), dataDir_(QDir::cleanPath(QFileInfo(dataDir).absoluteFilePath())) {}

void QualityTools::setStatus(QString status) {
    if (status_ == status) return;
    status_ = std::move(status); emit statusChanged();
}

void QualityTools::reportScreenshotError(const QString& reason) {
    setStatus(tr("Falha ao capturar screenshot: %1").arg(reason.trimmed().left(256)));
}

QString QualityTools::safeGameFolder(const QString& title) const {
    QString value = title.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f]")), "_");
    value.replace(QRegularExpression(QStringLiteral("[. ]+$")), "");
    value = value.left(80);
    if (value.isEmpty() || value == "." || value == "..") value = QStringLiteral("Jogo");
    return value;
}

QString QualityTools::saveScreenshot(const QImage& image, const QString& gameTitle) {
    if (image.isNull() || image.width() <= 0 || image.height() <= 0 ||
        qint64(image.width()) * image.height() > 20'000'000) {
        setStatus(tr("Captura recusada: a imagem não está disponível ou excede o limite.")); return {};
    }
    const auto folder = QDir(dataDir_).filePath("screenshots/" + safeGameFolder(gameTitle));
    if (!QDir().mkpath(folder) || QFileInfo(folder).isSymLink()) {
        setStatus(tr("Não foi possível criar a pasta privada de capturas.")); return {};
    }
    const auto stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz");
    QString path = QDir(folder).filePath("Shireia-" + stamp + ".png");
    for (int index = 1; QFileInfo::exists(path); ++index)
        path = QDir(folder).filePath(QString("Shireia-%1-%2.png").arg(stamp).arg(index));
    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG") || encoded.isEmpty() || encoded.size() > 80 * 1024 * 1024) {
        setStatus(tr("Falha ao codificar a captura em PNG.")); return {};
    }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(encoded) != encoded.size() || !output.commit()) {
        output.cancelWriting(); setStatus(tr("Falha ao salvar a captura.")); return {};
    }
    setStatus(tr("Screenshot salvo: %1").arg(QFileInfo(path).fileName()));
    return path;
}

bool QualityTools::openScreenshotFolder(const QString& gameTitle) {
    const auto path = QDir(dataDir_).filePath("screenshots/" + safeGameFolder(gameTitle));
    if (!QDir().mkpath(path) || QFileInfo(path).isSymLink()) {
        setStatus(tr("Não foi possível abrir a pasta de capturas.")); return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool QualityTools::openScreenshotRoot() {
    const auto path = QDir(dataDir_).filePath("screenshots");
    if (!QDir().mkpath(path)) { setStatus(tr("Não foi possível abrir a pasta de capturas.")); return false; }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString QualityTools::cachePath(const QString& titleId) const {
    static const QRegularExpression validId(QStringLiteral("^[0-9A-Fa-f]{16}$"));
    if (!validId.match(titleId).hasMatch() || titleId.toULongLong(nullptr, 16) == 0) return {};
    return QDir(QDir(dataDir_).filePath("shader")).filePath(titleId.toLower());
}

QVariantMap QualityTools::gameCacheInfo(const QString& titleId) const {
    const auto path = cachePath(titleId);
    if (path.isEmpty() || QFileInfo(path).isSymLink() || !QFileInfo(path).isDir())
        return {{"files",0},{"bytes",0LL},{"path",path}};
    qint64 bytes = 0; int files = 0;
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext()) { it.next(); bytes += qMax<qint64>(0, it.fileInfo().size()); ++files; }
    return {{"files",files},{"bytes",bytes},{"path",path}};
}

bool QualityTools::openGameCache(const QString& titleId) {
    const auto path = cachePath(titleId);
    if (path.isEmpty()) { setStatus(tr("Title ID inválido para consultar cache.")); return false; }
    const auto root = QDir(dataDir_).filePath("shader");
    if (!QDir().mkpath(root) || QFileInfo(root).isSymLink() || !QDir().mkpath(path) || QFileInfo(path).isSymLink()) {
        setStatus(tr("Não foi possível abrir o cache deste jogo.")); return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool QualityTools::clearGameCache(const QString& titleId, bool cacheInUse) {
    const auto path = cachePath(titleId);
    if (path.isEmpty()) { setStatus(tr("Title ID inválido para limpar cache.")); return false; }
    if (cacheInUse) { setStatus(tr("Encerre a sessão do jogo antes de limpar o cache.")); return false; }
    const auto root = QDir(QDir(dataDir_).filePath("shader")).absolutePath();
    const auto target = QDir(path).absolutePath();
    if (QFileInfo(root).isSymLink() || QFileInfo(target).isSymLink() ||
        QFileInfo(target).absolutePath() != root || !QFileInfo(target).isDir()) {
        setStatus(tr("Cache do jogo inexistente ou caminho recusado.")); return false;
    }
    if (!QDir(target).removeRecursively()) { setStatus(tr("Falha ao limpar somente o cache deste jogo.")); return false; }
    setStatus(tr("Cache de shaders e pipelines deste jogo foi limpo.")); return true;
}
