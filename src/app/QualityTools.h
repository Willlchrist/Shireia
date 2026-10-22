#pragma once
#include <QObject>
#include <QImage>
#include <QVariantMap>

class QualityTools final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit QualityTools(QString dataDir, QObject* parent = nullptr);
    QString status() const { return status_; }
    Q_INVOKABLE QString saveScreenshot(const QImage& image, const QString& gameTitle);
    Q_INVOKABLE void reportScreenshotError(const QString& reason);
    Q_INVOKABLE bool openScreenshotFolder(const QString& gameTitle);
    Q_INVOKABLE bool openScreenshotRoot();
    Q_INVOKABLE QVariantMap gameCacheInfo(const QString& titleId) const;
    Q_INVOKABLE bool openGameCache(const QString& titleId);
    Q_INVOKABLE bool clearGameCache(const QString& titleId, bool cacheInUse);
signals:
    void statusChanged();
private:
    QString dataDir_, status_;
    void setStatus(QString status);
    QString safeGameFolder(const QString& title) const;
    QString cachePath(const QString& titleId) const;
};
