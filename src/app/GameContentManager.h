#pragma once
#include <QObject>
#include <QVariantList>
#include <QString>
#include <QJsonArray>

class GameContentManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit GameContentManager(QString dataDir, QObject* parent = nullptr);
    QString status() const { return status_; }
    Q_INVOKABLE QVariantList mods(const QString& titleId) const;
    Q_INVOKABLE bool importMod(const QString& titleId, const QString& folder);
    Q_INVOKABLE bool importCheat(const QString& titleId, const QString& filePath);
    Q_INVOKABLE bool setModEnabled(const QString& titleId, const QString& name, bool enabled);
    Q_INVOKABLE bool setCheatEnabled(const QString& titleId, const QString& modName, const QString& fileName, bool enabled);
    Q_INVOKABLE bool removeMod(const QString& titleId, const QString& name);
    Q_INVOKABLE bool openModsFolder(const QString& titleId);
signals:
    void statusChanged();
private:
    QString dataDir_, status_;
    bool validTitleId(const QString&) const;
    QJsonArray inspectCheat(const QString& filePath) const;
    void setStatus(QString status);
};
