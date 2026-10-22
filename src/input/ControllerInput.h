#pragma once
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QSettings>
#include <QVariantList>
#include <QMap>
#include <vector>
#include <QSet>
#include <array>

// Sole physical input owner. UI actions and full game state use exclusive contexts.
class ControllerInput final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY devicesChanged)
    Q_PROPERTY(QStringList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(int selected READ selected WRITE setSelected NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList bindings READ bindings NOTIFY bindingsChanged)
    Q_PROPERTY(QVariantList gameBindings READ gameBindings NOTIFY bindingsChanged)
    Q_PROPERTY(QStringList gameProfiles READ gameProfiles NOTIFY bindingsChanged)
    Q_PROPERTY(QString currentGameProfile READ currentGameProfile NOTIFY bindingsChanged)
    Q_PROPERTY(QString profileStatus READ profileStatus NOTIFY bindingsChanged)
    Q_PROPERTY(int faceLayout READ faceLayout WRITE setFaceLayout NOTIFY bindingsChanged)
    Q_PROPERTY(QString capturing READ capturing NOTIFY bindingsChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY bindingsChanged)
    Q_PROPERTY(bool popupActive MEMBER popupActive NOTIFY popupActiveChanged)
    Q_PROPERTY(bool gameInput READ gameInput WRITE setGameInput NOTIFY gameInputChanged)
    Q_PROPERTY(bool gameShortcutMode READ gameShortcutMode WRITE setGameShortcutMode NOTIFY gameShortcutModeChanged)
public:
    explicit ControllerInput(QObject* parent = nullptr, bool testMode = false);
    ~ControllerInput() override;
    QString status() const;
    QStringList devices() const;
    int selected() const { return selected_; }
    void setSelected(int index);
    QVariantList bindings() const;
    QString capturing() const { return capture_; }
    bool dirty() const { return !drafts_.isEmpty() || !gameDrafts_.isEmpty() || !keyboardDraft_.isEmpty() || !layoutDrafts_.isEmpty(); }
    QVariantList gameBindings() const;
    QStringList gameProfiles() const;
    QString currentGameProfile() const { return currentGameProfile_; }
    QString profileStatus() const { return profileStatus_; }
    int faceLayout() const;
    void setFaceLayout(int layout);
    bool popupActive = false;
    Q_INVOKABLE void capture(const QString& action);
    Q_INVOKABLE void captureGame(const QString& action);
    Q_INVOKABLE void resetGameBindings();
    Q_INVOKABLE bool saveGameProfile(const QString& name);
    Q_INVOKABLE bool loadGameProfile(const QString& name);
    Q_INVOKABLE bool deleteGameProfile(const QString& name);
    Q_INVOKABLE void cancelCapture();
    Q_INVOKABLE void resetBindings();
    Q_INVOKABLE void applyBindings();
    Q_INVOKABLE void cancelBindings();
    Q_INVOKABLE void sendKey(const QString& action);
    Q_INVOKABLE void moveFocus(bool backwards = false);
    Q_INVOKABLE bool isGameKeyboardKey(int key) const;
    void poll();
    bool gameInput() const { return gameInput_; }
    void setGameInput(bool value);
    bool gameShortcutMode() const { return gameShortcutMode_; }
    void setGameShortcutMode(bool value);
signals:
    void gameInputChanged();
    void gameShortcutModeChanged();
    void keyboardShortcut(const QString& action);
    void popupActiveChanged();
    void openGameMenu();
    void gameState(quint32 buttons, float lx, float ly, float rx, float ry);
    void gamePlayerState(int player, quint32 buttons, float lx, float ly, float rx, float ry);
    void action(const QString& name);
    void devicesChanged();
    void bindingsChanged();
private:
    bool eventFilter(QObject* object, QEvent* event) override;
    void publishGameState();
    bool gameInput_ = false;
    bool gameShortcutMode_ = false;
    QSet<int> gameKeys_;
    quint32 sentButtons_ = 0;
    std::array<float,4> sentAxes_{};
    quint32 sentPlayer2Buttons_ = 0;
    std::array<float,4> sentPlayer2Axes_{};
    QSet<SDL_JoystickID> gameNeutralDevices_;
    struct Device { SDL_JoystickID id; SDL_GameController* pad; SDL_Joystick* joystick; QString name, guid; };
    void addDevice(int index);
    void removeDevice(SDL_JoystickID id);
    void button(SDL_JoystickID id, int button);
    QMap<QString, int> defaults(const Device& device) const;
    QMap<QString, int> mapping(const Device& device) const;
    Device* current();
    QString direction(const Device& device);
    bool foreground() const;
    bool enabled() const;
    void clearHeld();
    std::vector<Device> devices_;
    QMap<QString, QMap<QString, int>> drafts_;
    QMap<QString, QMap<QString, int>> gameDrafts_;
    QMap<QString, int> keyboardDraft_;
    QMap<QString,int> layoutDrafts_;
    QMap<QString,int> gameDefaults(const Device& device) const;
    QMap<QString,int> gameMapping(const Device& device) const;
    QMap<QString,int> keyboardMapping() const;
    int physicalCode(const Device& device, const QString& action) const;
    mutable QSettings settings_;
    QTimer timer_;
    QElapsedTimer clock_;
    QString error_, capture_, held_;
    QString currentGameProfile_, profileStatus_;
    qint64 repeatAt_ = 0, captureUntil_ = 0;
    int selected_ = -1, axisDirection_ = 0;
    bool initialized_ = false, testMode_ = false, neutralRequired_ = true;
};
