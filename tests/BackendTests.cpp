#include <QGuiApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QImage>
#include <QKeyEvent>
#include <algorithm>
#include "../src/input/ControllerInput.h"
#include "../src/settings/SettingsStore.h"
#include "../src/graphics/PixelConvert.h"
#include "../src/app/AppModel.h"

#define CHECK(condition) do { if (!(condition)) { qCritical() << "FAIL line" << __LINE__ << #condition; return 1; } } while (false)

int main(int argc, char** argv) {
    // Regression guard for the RGBA bridge: channel order, alpha and vector-tail handling.
    constexpr std::uint8_t bgra[]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                  13, 14, 15, 16, 17, 18, 19, 20};
    std::uint8_t rgba[sizeof(bgra)]{};
    bgraToRgba(bgra, rgba, sizeof(bgra));
    constexpr std::uint8_t expected[]{3, 2, 1, 4, 7, 6, 5, 8, 11, 10, 9, 12,
                                      15, 14, 13, 16, 19, 18, 17, 20};
    CHECK(std::equal(std::begin(rgba), std::end(rgba), std::begin(expected)));
    QGuiApplication app(argc, argv);
    app.setOrganizationName("ShireiaTests"); app.setApplicationName("BackendTests");
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    SettingsStore settings;
    settings.setValue("ui/thumb", 230); CHECK(settings.dirty());
    settings.cancel(); CHECK(settings.draft()["ui/thumb"].toInt() == 160);
    settings.setValue("ui/thumb", 999); CHECK(settings.draft()["ui/thumb"].toInt() == 240);
    settings.setValue("graphics/resolution", 99); CHECK(settings.draft()["graphics/resolution"].toInt() == 12);
    settings.setValue("graphics/resolution", 0);
    settings.setValue("graphics/shaderCache", false);
    settings.setValue("graphics/docked", true);
    settings.setValue("graphics/vsync", 99);
    CHECK(settings.draft()["graphics/vsync"].toInt() == 3);
    settings.setValue("graphics/vsync", 1);
    settings.setValue("graphics/asynchronousShaders", true);
    settings.setValue("graphics/gpuAccuracy", 99);
    CHECK(settings.draft()["graphics/gpuAccuracy"].toInt() == 1);
    settings.setValue("graphics/astcDecode", 2);
    settings.setValue("graphics/anisotropy", 4);
    settings.setValue("graphics/scalingFilter", 4);
    settings.setValue("graphics/antiAliasing", 2);
    settings.setValue("graphics/vramUsage", 1);
    settings.setValue("graphics/astcRecompression", 2);
    settings.setValue("audio/volume", 140);
    CHECK(settings.draft()["audio/volume"].toInt() == 100);
    settings.setValue("audio/volume", 35);
    settings.setValue("audio/outputDevice", "auto");
    settings.setValue("core/speedLimit", 300);
    CHECK(settings.draft()["core/speedLimit"].toInt() == 200);
    settings.setValue("core/speedLimit", 120);
    CHECK(settings.apply());
    SettingsStore restored;
    CHECK(restored.values()["ui/thumb"].toInt() == 240);
    CHECK(restored.values()["graphics/resolution"].toInt() == 0);
    CHECK(!restored.values()["graphics/shaderCache"].toBool());
    CHECK(restored.values()["graphics/docked"].toBool());
    CHECK(restored.values()["graphics/vsync"].toInt() == 1);
    CHECK(restored.values()["graphics/asynchronousShaders"].toBool());
    CHECK(restored.values()["graphics/astcDecode"].toInt() == 2);
    CHECK(restored.values()["graphics/anisotropy"].toInt() == 4);
    CHECK(restored.values()["graphics/scalingFilter"].toInt() == 4);
    CHECK(restored.values()["graphics/antiAliasing"].toInt() == 2);
    CHECK(restored.values()["graphics/vramUsage"].toInt() == 1);
    CHECK(restored.values()["graphics/astcRecompression"].toInt() == 2);
    CHECK(restored.values()["audio/volume"].toInt() == 35);
    CHECK(restored.values()["audio/outputDevice"].toString() == "auto");
    CHECK(restored.values()["core/speedLimit"].toInt() == 120);
    const QString selectedUser = "12345678-1234-1234-1234-123456789abc";
    CHECK(!settings.selectActiveUser("invalid"));
    CHECK(settings.selectActiveUser(selectedUser));
    SettingsStore userRestored;
    CHECK(userRestored.values()["system/userUuid"].toString() == selectedUser);
    settings.defaults(); CHECK(settings.dirty()); CHECK(settings.values()["ui/thumb"].toInt() == 240);
    settings.cancel(); CHECK(!settings.dirty());
    CHECK(!settings.selectProdKeys(QUrl::fromLocalFile(temporary.path() + "/missing.keys")).isEmpty());
    QDir().mkpath(temporary.path() + "/firmware");
    CHECK(!settings.selectFirmware(QUrl::fromLocalFile(temporary.path() + "/firmware")).isEmpty());
    QFile keysFixture(temporary.path() + "/prod.keys");
    CHECK(keysFixture.open(QIODevice::WriteOnly));
    keysFixture.write("Test placeholder only; contains no cryptographic keys."); keysFixture.close();
    QDir().mkpath(temporary.path() + "/firmware/contents/registered");
    QFile firmwareFixture(temporary.path() + "/firmware/contents/registered/fixture.nca");
    CHECK(firmwareFixture.open(QIODevice::WriteOnly));
    firmwareFixture.write("Test placeholder only; not firmware."); firmwareFixture.close();
    CHECK(settings.selectProdKeys(QUrl::fromLocalFile(keysFixture.fileName())).isEmpty());
    CHECK(settings.selectFirmware(QUrl::fromLocalFile(temporary.path() + "/firmware")).isEmpty());
    CHECK(settings.values()["system/prodKeys"].toString().isEmpty());
    settings.cancel(); CHECK(settings.draft()["system/prodKeys"].toString().isEmpty());
    CHECK(settings.selectProdKeys(QUrl::fromLocalFile(keysFixture.fileName())).isEmpty());
    CHECK(settings.selectFirmware(QUrl::fromLocalFile(temporary.path() + "/firmware")).isEmpty());
    CHECK(settings.apply());
    SettingsStore systemRestored;
    CHECK(systemRestored.values()["system/prodKeys"].toString() == QFileInfo(keysFixture).canonicalFilePath());
    CHECK(systemRestored.values()["system/firmwareFolder"].toString() == QFileInfo(temporary.path() + "/firmware").canonicalFilePath());
    settings.setValue("system/prodKeys", ""); settings.cancel();
    CHECK(!settings.draft()["system/prodKeys"].toString().isEmpty());
    settings.defaults(); CHECK(settings.draft()["system/firmwareFolder"].toString().isEmpty());
    settings.cancel();
    QDir().mkpath(temporary.path() + "/games");
    QFile file(temporary.path() + "/games/Synthetic.nro");
    CHECK(file.open(QIODevice::WriteOnly)); file.write("Not a ROM"); file.close();
    CHECK(settings.setGameOverride(file.fileName(), "graphics/resolution", 1));
    CHECK(settings.gameOverride(file.fileName(), "graphics/resolution") == 1);
    CHECK(settings.hasGameOverrides(file.fileName()));
    CHECK(!settings.setGameOverride(file.fileName(), "graphics/resolution", 99));
    CHECK(!settings.setGameOverride(file.fileName(), "graphics/shaderCache", 1));
    SettingsStore gameSettingsRestored;
    CHECK(gameSettingsRestored.gameOverride(file.fileName(), "graphics/resolution") == 1);
    CHECK(settings.setGameOverride(file.fileName(), "graphics/resolution", -1));
    CHECK(settings.gameOverride(file.fileName(), "graphics/resolution") == -1);
    CHECK(settings.setGameOverride(file.fileName(), "graphics/scalingFilter", 6));
    CHECK(settings.setGameOverride(file.fileName(), "graphics/docked", 1));
    CHECK(settings.clearGameOverrides(file.fileName()));
    CHECK(!settings.hasGameOverrides(file.fileName()));
    settings.addFolder(QUrl::fromLocalFile(temporary.path() + "/games"));
    CHECK(restored.values()["library/folders"].toStringList().isEmpty());
    CHECK(settings.apply());
    AppModel library;
    CHECK(library.games().size() == 1);
    library.toggleFavorite(file.fileName());
    CHECK(library.games()[0].toMap()["favorite"].toBool());
    library.noteViewed(file.fileName()); library.scan();
    CHECK(library.games()[0].toMap()["viewed"].toLongLong() > 0);
    AppModel libraryRestored;
    CHECK(libraryRestored.games()[0].toMap()["favorite"].toBool());
    QImage cover(16, 16, QImage::Format_RGB32); cover.fill(Qt::darkMagenta);
    CHECK(cover.save(temporary.path() + "/fixture-cover.png"));
    library.setCover(file.fileName(), QUrl::fromLocalFile(temporary.path() + "/fixture-cover.png"));
    CHECK(library.games()[0].toMap()["cover"].toString().endsWith("fixture-cover.png"));

    ControllerInput input(nullptr, true);
    QSettings().setValue("input/keyboard/L", Qt::Key_F9);
    CHECK(input.isGameKeyboardKey(Qt::Key_F9));
    CHECK(!input.isGameKeyboardKey(Qt::Key_F10));
    QStringList keyboardShortcuts;
    QObject::connect(&input, &ControllerInput::keyboardShortcut, [&](const QString& action) { keyboardShortcuts.append(action); });
    input.setGameShortcutMode(true);
    QKeyEvent conflictingShortcut(QEvent::KeyPress, Qt::Key_F9, Qt::ControlModifier | Qt::ShiftModifier);
    QCoreApplication::sendEvent(&input, &conflictingShortcut);
    CHECK(keyboardShortcuts.isEmpty());
    QSettings().remove("input/keyboard/L");
    QKeyEvent pauseShortcut(QEvent::KeyPress, Qt::Key_F9, Qt::ControlModifier | Qt::ShiftModifier);
    QCoreApplication::sendEvent(&input, &pauseShortcut);
    CHECK(keyboardShortcuts == QStringList{"pause-resume"});
    QKeyEvent screenshotShortcut(QEvent::KeyPress, Qt::Key_F12, Qt::NoModifier);
    QCoreApplication::sendEvent(&input, &screenshotShortcut);
    CHECK(keyboardShortcuts.last() == "screenshot");
    input.setGameShortcutMode(false);
    QKeyEvent inactiveShortcut(QEvent::KeyPress, Qt::Key_F9, Qt::ControlModifier | Qt::ShiftModifier);
    QCoreApplication::sendEvent(&input, &inactiveShortcut);
    CHECK(keyboardShortcuts.size() == 2);
    const auto hardware = input.devices().size();
    qInfo() << "Physical devices reported before virtual tests:" << hardware;
    const int deviceIndex = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
        SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 1);
    CHECK(deviceIndex >= 0);
    auto* virtualJoystick = SDL_JoystickOpen(deviceIndex); CHECK(virtualJoystick);
    auto pump = [&] { SDL_JoystickUpdate(); input.poll(); };
    pump(); input.setSelected(input.devices().size() - 1); pump();
    CHECK(input.devices().size() == hardware + 1);
    QStringList actions;
    QObject::connect(&input, &ControllerInput::action, [&](const QString& action) { actions.append(action); });
    SDL_JoystickSetVirtualAxis(virtualJoystick, SDL_CONTROLLER_AXIS_LEFTX, 25000); pump();
    CHECK(actions == QStringList{"right"});
    pump(); CHECK(actions.size() == 1);
    SDL_JoystickSetVirtualAxis(virtualJoystick, SDL_CONTROLLER_AXIS_LEFTX, 0); pump();
    SDL_JoystickSetVirtualAxis(virtualJoystick, SDL_CONTROLLER_AXIS_LEFTX, 9000); pump(); CHECK(actions.size() == 1);
    SDL_JoystickSetVirtualAxis(virtualJoystick, SDL_CONTROLLER_AXIS_LEFTX, 0); pump();
    SDL_JoystickSetVirtualButton(virtualJoystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, 1); pump(); CHECK(actions.last() == "down");
    SDL_JoystickSetVirtualButton(virtualJoystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, 0); pump();
    SDL_JoystickSetVirtualButton(virtualJoystick, 0, 1); pump(); CHECK(actions.last() == "confirm");
    SDL_JoystickSetVirtualButton(virtualJoystick, 0, 0); pump();
    input.capture("confirm");
    SDL_JoystickSetVirtualButton(virtualJoystick, 1, 1); pump(); CHECK(input.capturing().isEmpty()); CHECK(input.dirty());
    SDL_JoystickSetVirtualButton(virtualJoystick, 1, 0); pump();
    input.cancelBindings(); CHECK(!input.dirty());
    input.capture("confirm");
    SDL_JoystickSetVirtualButton(virtualJoystick, 1, 1); pump();
    SDL_JoystickSetVirtualButton(virtualJoystick, 1, 0); pump();
    input.applyBindings(); pump(); CHECK(!input.dirty());
    SDL_JoystickSetVirtualButton(virtualJoystick, 1, 1); pump(); CHECK(actions.last() == "confirm");
    SDL_JoystickSetVirtualButton(virtualJoystick, 1, 0); pump();
    CHECK(QSettings().value("input/" + [&] { char g[33]; SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(virtualJoystick), g, sizeof(g)); return QString(g); }() + "/confirm").toInt() == 1);
    const auto count = actions.size();
    QSettings().setValue("ui/controller", false);
    SDL_JoystickSetVirtualButton(virtualJoystick, 1, 1); pump(); CHECK(actions.size() == count);
    SDL_JoystickSetVirtualButton(virtualJoystick, 1, 0); pump();
    QSettings().setValue("ui/controller", true);
    SDL_JoystickClose(virtualJoystick);
    CHECK(SDL_JoystickDetachVirtual(deviceIndex) == 0); pump();
    CHECK(input.devices().size() == hardware);

    const int reconnected = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
        SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 1);
    CHECK(reconnected >= 0);
    virtualJoystick = SDL_JoystickOpen(reconnected); CHECK(virtualJoystick);
    pump(); input.setSelected(input.devices().size() - 1); pump();
    const auto beforeReconnect = actions.size();
    SDL_JoystickSetVirtualButton(virtualJoystick, 1, 1); pump();
    CHECK(actions.size() == beforeReconnect + 1 && actions.last() == "confirm");
    SDL_JoystickSetVirtualButton(virtualJoystick, 1, 0); pump();
    SDL_JoystickClose(virtualJoystick); CHECK(SDL_JoystickDetachVirtual(reconnected) == 0); pump();
    CHECK(input.devices().size() == hardware);

    const auto generic = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 8, 1);
    CHECK(generic >= 0);
    virtualJoystick = SDL_JoystickOpen(generic); CHECK(virtualJoystick);
    pump(); input.setSelected(input.devices().size() - 1); pump();
    CHECK(input.devices().size() == hardware + 1);
    SDL_JoystickSetVirtualHat(virtualJoystick, 0, SDL_HAT_UP); pump(); CHECK(actions.last() == "up");
    SDL_JoystickSetVirtualHat(virtualJoystick, 0, SDL_HAT_CENTERED); pump();
    SDL_JoystickSetVirtualButton(virtualJoystick, 0, 1); pump(); CHECK(actions.last() == "confirm");
    SDL_JoystickSetVirtualButton(virtualJoystick, 0, 0); pump();
    SDL_JoystickClose(virtualJoystick); CHECK(SDL_JoystickDetachVirtual(generic) == 0); pump();
    CHECK(input.devices().size() == hardware);
    const int gameDevice=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,1);
    CHECK(gameDevice>=0);
    auto* gameJoystick=SDL_JoystickOpen(gameDevice); CHECK(gameJoystick);
    pump(); input.setSelected(input.devices().size()-1); pump();
    input.resetBindings(); input.applyBindings(); pump();
    quint32 gameButtons=0; float gameX=0;
    QObject::connect(&input,&ControllerInput::gameState,[&](quint32 buttons,float x,float,float,float) { gameButtons=buttons; gameX=x; });
    input.setGameInput(true); pump(); const auto actionsBeforeGame=actions.size();
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_A,1); pump();
    CHECK((gameButtons&(1u<<1))!=0 && (gameButtons&1)==0 && actions.size()==actionsBeforeGame);
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_A,0); pump();
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_B,1); pump();
    CHECK((gameButtons&1)!=0 && (gameButtons&(1u<<1))==0);
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_B,0); pump();
    input.setFaceLayout(1); pump();
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_A,1); pump();
    CHECK((gameButtons&1)!=0);
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_A,0); pump();
    input.setFaceLayout(0); pump();
    input.setGameInput(false);
    input.captureGame("game/A");
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_Y,1); pump();
    CHECK(input.capturing().isEmpty() && input.dirty());
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_Y,0); pump();
    input.setGameInput(false);
    input.captureGame("game/ZL");
    SDL_JoystickSetVirtualAxis(gameJoystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,25000); pump();
    CHECK(input.capturing().isEmpty());
    SDL_JoystickSetVirtualAxis(gameJoystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768); pump();
    input.captureGame("axis/LX");
    SDL_JoystickSetVirtualAxis(gameJoystick,SDL_CONTROLLER_AXIS_RIGHTX,25000); pump();
    CHECK(input.capturing().isEmpty());
    SDL_JoystickSetVirtualAxis(gameJoystick,SDL_CONTROLLER_AXIS_RIGHTX,0); pump();
    input.captureGame("key/A");
    QKeyEvent remapKey(QEvent::KeyPress,Qt::Key_H,Qt::NoModifier);
    QCoreApplication::sendEvent(qApp,&remapKey);
    CHECK(input.capturing().isEmpty());
    input.applyBindings();
    CHECK(QSettings().value("input/keyboard/A").toInt() == Qt::Key_H);
    input.setGameInput(true); pump();
    SDL_JoystickSetVirtualAxis(gameJoystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,25000); pump();
    CHECK((gameButtons&(1u<<8))!=0);
    SDL_JoystickSetVirtualAxis(gameJoystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768); pump();
    SDL_JoystickSetVirtualAxis(gameJoystick,SDL_CONTROLLER_AXIS_RIGHTX,25000); pump();
    CHECK(gameX>0.5f);
    SDL_JoystickSetVirtualAxis(gameJoystick,SDL_CONTROLLER_AXIS_RIGHTX,0); pump();
    QKeyEvent gameKey(QEvent::KeyPress,Qt::Key_H,Qt::NoModifier);
    QCoreApplication::sendEvent(qApp,&gameKey);
    CHECK((gameButtons&1)!=0);
    QKeyEvent gameKeyUp(QEvent::KeyRelease,Qt::Key_H,Qt::NoModifier);
    QCoreApplication::sendEvent(qApp,&gameKeyUp);
    CHECK(input.saveGameProfile("Teste SDL"));
    input.applyBindings();
    CHECK(input.gameProfiles().contains("Teste SDL"));
    CHECK(input.loadGameProfile("Teste SDL"));
    input.applyBindings();
    input.setGameInput(true); pump();
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_Y,1); pump();
    CHECK((gameButtons&1)!=0);
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_Y,0); pump();
    SDL_JoystickSetVirtualAxis(gameJoystick,SDL_CONTROLLER_AXIS_LEFTX,25000); pump(); CHECK(std::abs(gameX)<0.1f);
    input.setGameInput(false); CHECK(gameButtons==0 && gameX==0);
    const int secondPlayerDevice=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,1);
    CHECK(secondPlayerDevice>=0);
    auto* secondPlayerJoystick=SDL_JoystickOpen(secondPlayerDevice); CHECK(secondPlayerJoystick);
    pump();
    // When one physical controller is already connected, the routing policy assigns it
    // to player 2 unless we explicitly select it as player 1. Keep this test deterministic
    // while still checking that a second device reaches player 2.
    auto* expectedPlayerTwo = secondPlayerJoystick;
    if (hardware == 1) {
        input.setSelected(0); // the physical controller is enumerated before test devices
        expectedPlayerTwo = gameJoystick;
    }
    int lastPlayer=-1; quint32 secondPlayerButtons=0; QStringList playerTwoEvents;
    QObject::connect(&input,&ControllerInput::gamePlayerState,[&](int player,quint32 buttons,float,float,float,float) {
        if(player==1) { lastPlayer=player; secondPlayerButtons=buttons; playerTwoEvents.append(QString::number(buttons)); }
    });
    input.setGameInput(true); pump();
    SDL_JoystickSetVirtualButton(expectedPlayerTwo,SDL_CONTROLLER_BUTTON_A,1); pump();
    if (!(lastPlayer==1 && (secondPlayerButtons&(1u<<1))!=0))
        qWarning() << "Player-two fixture diagnostic:" << "physical count" << hardware << "devices" << input.devices()
                   << "game joystick" << SDL_JoystickInstanceID(gameJoystick)
                   << "second joystick" << SDL_JoystickInstanceID(secondPlayerJoystick)
                   << "expected joystick" << SDL_JoystickInstanceID(expectedPlayerTwo)
                   << "observed player-two button states" << playerTwoEvents;
    CHECK(lastPlayer==1 && (secondPlayerButtons&(1u<<1))!=0);
    SDL_JoystickSetVirtualButton(expectedPlayerTwo,SDL_CONTROLLER_BUTTON_A,0); pump();
    input.setGameInput(false);
    SDL_JoystickClose(secondPlayerJoystick); CHECK(SDL_JoystickDetachVirtual(secondPlayerDevice)==0); pump();
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_A,0);
    SDL_JoystickSetVirtualAxis(gameJoystick,SDL_CONTROLLER_AXIS_LEFTX,0); pump();
    int guideCount=0;
    QObject::connect(&input,&ControllerInput::openGameMenu,[&] { ++guideCount; input.setGameInput(false); });
    input.setGameInput(true); pump();
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_GUIDE,1); pump();
    CHECK(guideCount==1 && !input.gameInput() && gameButtons==0 && gameX==0);
    const auto actionsInMenu=actions.size();
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_A,1); pump();
    CHECK(actions.size()==actionsInMenu);
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_GUIDE,0);
    SDL_JoystickSetVirtualButton(gameJoystick,SDL_CONTROLLER_BUTTON_A,0); pump();
    input.setGameInput(true); pump();
    CHECK(gameButtons==0 && gameX==0);
    SDL_JoystickClose(gameJoystick); CHECK(SDL_JoystickDetachVirtual(gameDevice)==0); pump();
    qInfo() << "PASS: settings and game-profile persistence, per-game graphics overrides, SDL mapped/generic devices, two-player input, remapping/hotplug, exclusive game input and neutralization.";
    return 0;
}
