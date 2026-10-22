#pragma once
#include <QObject>
#include <QSettings>
#include <QVariantList>

class AppModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    Q_PROPERTY(QStringList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool pauseOnMenu READ pauseOnMenu WRITE setPauseOnMenu NOTIFY settingsChanged)
public:
    explicit AppModel(QObject* parent = nullptr);
    QVariantList games() const { return games_; }
    QStringList folders() const { return settings_.value("library/folders").toStringList(); }
    QString status() const { return status_; }
    bool pauseOnMenu() const { return settings_.value("session/pauseOnMenu", true).toBool(); }
    void setPauseOnMenu(bool value);
    Q_INVOKABLE void addFolder(const QUrl& folder);
    Q_INVOKABLE void removeFolder(int index);
    Q_INVOKABLE void scan();
    Q_INVOKABLE void toggleFavorite(const QString& path);
    Q_INVOKABLE void noteViewed(const QString& path);
    Q_INVOKABLE void setCover(const QString& path, const QUrl& image);
    void updateMetadata(const QVariantList& entries);
signals:
    void gamesChanged();
    void foldersChanged();
    void statusChanged();
    void settingsChanged();
    void scanCompleted();
private:
    QSettings settings_;
    QVariantList games_;
    QString status_;
};
