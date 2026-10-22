#pragma once
#include <QObject>
#include <QVariantMap>
#include <QSettings>
#include <QUrl>

class SettingsStore final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap values READ values NOTIFY valuesChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY draftChanged)
    Q_PROPERTY(QString coreDataPath READ coreDataPath CONSTANT)
public:
    explicit SettingsStore(QObject* parent = nullptr);
    QVariantMap draft() const { return draft_; }
    QVariantMap values() const { return values_; }
    bool dirty() const { return draft_ != values_; }
    QString coreDataPath() const;
    Q_INVOKABLE bool openCoreDirectory(const QString& name);
    Q_INVOKABLE void setValue(const QString& key, const QVariant& value);
    Q_INVOKABLE bool apply();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void defaults();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void addFolder(const QUrl& folder);
    Q_INVOKABLE void removeFolder(int index);
    Q_INVOKABLE void openFolder(const QString& path);
    Q_INVOKABLE int gameOverride(const QString& path, const QString& key) const;
    Q_INVOKABLE QVariant gameOverrideValue(const QString& path, const QString& key) const;
    Q_INVOKABLE bool hasGameOverrides(const QString& path) const;
    Q_INVOKABLE bool setGameOverride(const QString& path, const QString& key, int value);
    Q_INVOKABLE bool setGameOverrideValue(const QString& path, const QString& key, const QVariant& value);
    Q_INVOKABLE bool clearGameOverride(const QString& path, const QString& key);
    Q_INVOKABLE bool clearGameOverrides(const QString& path);
    Q_INVOKABLE QString selectProdKeys(const QUrl& file);
    Q_INVOKABLE QString selectFirmware(const QUrl& folder);
    bool selectActiveUser(const QString& uuid);
signals:
    void draftChanged();
    void valuesChanged();
    void applied();
private:
    static QVariantMap initial();
    QSettings settings_;
    QVariantMap draft_, values_;
};
