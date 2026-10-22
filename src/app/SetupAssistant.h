#pragma once

#include <QObject>
#include <QSettings>
#include <QUrl>

class AppModel;
class LibraryResources;
class SettingsStore;

class SetupAssistant final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool completed READ completed NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled CONSTANT)
    Q_PROPERTY(int step READ step NOTIFY changed)
    Q_PROPERTY(bool legalAccepted READ legalAccepted NOTIFY changed)
    Q_PROPERTY(bool keysSelected READ keysSelected NOTIFY changed)
    Q_PROPERTY(bool keysValidated READ keysValidated NOTIFY resourceStateChanged)
    Q_PROPERTY(bool firmwareValidated READ firmwareValidated NOTIFY resourceStateChanged)
    Q_PROPERTY(bool resourceCheckComplete READ resourceCheckComplete NOTIFY resourceStateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY resourceStateChanged)
    Q_PROPERTY(QString status READ status NOTIFY resourceStateChanged)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString acceptedAt READ acceptedAt NOTIFY changed)
    Q_PROPERTY(QString firmwareResult READ firmwareResult NOTIFY resourceStateChanged)
    Q_PROPERTY(QString legalVersion READ legalVersion CONSTANT)
public:
    SetupAssistant(SettingsStore& settings, LibraryResources& resources, AppModel& model,
                   bool enabled, QObject* parent = nullptr);

    bool completed() const { return completed_; }
    bool enabled() const { return enabled_; }
    int step() const { return step_; }
    bool legalAccepted() const;
    bool keysSelected() const;
    bool keysValidated() const;
    bool firmwareValidated() const;
    bool resourceCheckComplete() const;
    bool busy() const;
    QString status() const;
    QString error() const { return error_; }
    QString acceptedAt() const;
    QString firmwareResult() const;
    QString legalVersion() const { return QStringLiteral("shireia-legal-2"); }

    Q_INVOKABLE void begin();
    Q_INVOKABLE bool acceptLegal(bool accepted);
    Q_INVOKABLE void back();
    Q_INVOKABLE bool selectKeys(const QUrl& file);
    Q_INVOKABLE bool selectFirmware(const QUrl& source);
    Q_INVOKABLE void retryResourceCheck();
    Q_INVOKABLE QString licenseText(const QString& name) const;
    Q_INVOKABLE bool continueFromKeys();
    Q_INVOKABLE bool continueFromFirmware();
    Q_INVOKABLE bool addGamesFolder(const QUrl& folder);
    Q_INVOKABLE void skipGames();
    Q_INVOKABLE bool finish();
signals:
    void changed();
    void resourceStateChanged();
private:
    SettingsStore& settings_;
    LibraryResources& resources_;
    AppModel& model_;
    QSettings store_;
    bool enabled_ = false;
    bool completed_ = false;
    int step_ = 0;
    QString error_;

    void saveStep(int step);
    void onResourceStateChanged();
};
