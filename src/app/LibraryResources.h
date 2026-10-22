#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <QFileSystemWatcher>
#include <QVariantList>
#include <QJsonObject>
#include <QUrl>
class AppModel;
class SettingsStore;
class EmulationManager;
class LibraryResources final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(bool keysLoaded READ keysLoaded NOTIFY statusChanged)
    Q_PROPERTY(bool firmwareReady READ firmwareReady NOTIFY statusChanged)
    Q_PROPERTY(bool validationComplete READ validationComplete NOTIFY statusChanged)
    Q_PROPERTY(QString firmwareResult READ firmwareResult NOTIFY statusChanged)
public:
    LibraryResources(AppModel& model, SettingsStore& settings, EmulationManager& session, QString dataDir, bool enabled = true);
    ~LibraryResources() override;
    QString status() const { return status_; }
    // Treat the debounce window as busy too: otherwise startup can launch the
    // core before the first metadata scan gets its exclusive resource lock.
    bool busy() const { return process_.state() != QProcess::NotRunning || debounce_.isActive() || !contentPath_.isEmpty(); }
    QVariantList profiles() const { return profiles_; }
    bool keysLoaded() const { return keysLoaded_; }
    bool firmwareReady() const { return firmwareReady_; }
    bool validationComplete() const { return validationComplete_; }
    QString firmwareResult() const { return firmwareResult_; }
    void setEnabled(bool enabled);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool selectProfile(const QString& uuid);
    Q_INVOKABLE bool createProfile(const QString& name);
    Q_INVOKABLE bool renameProfile(const QString& uuid, const QString& name);
    Q_INVOKABLE bool setProfilePhoto(const QString& uuid, const QUrl& photo);
    Q_INVOKABLE bool installContent(const QUrl& file);
signals:
    void statusChanged();
    void profilesChanged();
private:
    AppModel& model_;
    SettingsStore& settings_;
    EmulationManager& session_;
    QString dataDir_, status_, lastSignature_, requestFile_, contentPath_, firmwareResult_;
    QVariantList profiles_;
    QJsonObject profileAction_;
    bool enabled_, force_ = false, keysLoaded_ = false, firmwareReady_ = false, validationComplete_ = false;
    bool applyingMetadata_ = false, rescanPending_ = false;
    QProcess process_;
    QTimer debounce_, timeout_;
    QTemporaryDir temporary_;
    QFileSystemWatcher watcher_;
    void schedule();
    void start();
    void finish(int code, QProcess::ExitStatus exit);
};
