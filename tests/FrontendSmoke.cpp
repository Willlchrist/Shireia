#include "FrontendSmoke.h"
#include "../src/app/AppModel.h"
#include "../src/settings/SettingsStore.h"
#include "../src/input/ControllerInput.h"
#include "../src/emulation/EmulationManager.h"
#include "../src/graphics/GpuSurface.h"
#include "../src/app/GameContentManager.h"
#include "../src/app/QualityTools.h"
#include "../src/app/CompatibilityStore.h"
#include "../src/app/SetupAssistant.h"
#include "../src/app/LibraryResources.h"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QDir>
#include <QTemporaryDir>
#include <QDebug>
#include <QMouseEvent>
#include <algorithm>
#include <QSet>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QCryptographicHash>
#include <QSettings>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
void wait(int milliseconds = 70) { QEventLoop loop; QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit); loop.exec(); }
void invoke(QObject* target, const char* method) { QMetaObject::invokeMethod(target, method); }
QQuickItem* findVisual(QQuickItem* parent, const QString& name) {
    if (parent->objectName() == name) return parent;
    for (auto* child : parent->childItems()) if (auto* found = findVisual(child, name)) return found;
    return nullptr;
}
}
#define CHECK_UI(condition) do { if (!(condition)) { QFile f("build/ui-smoke-failure.txt"); if (f.open(QIODevice::WriteOnly | QIODevice::Append)) f.write(QByteArray::number(__LINE__) + " " #condition "\n"); qCritical() << "UI FAIL line" << __LINE__ << #condition; return 20; } } while (false)

int runProgramSmoke(QQmlApplicationEngine& engine, ControllerInput& input, EmulationManager& emulation, const QString& path) {
    auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first()); CHECK_UI(window);
    auto* surface=window->findChild<GpuSurface*>("gpuSurface"); CHECK_UI(surface);
    auto until=[&](auto predicate,int limit=15000) { QElapsedTimer timer; timer.start(); while(!predicate() && timer.elapsed()<limit) wait(20); return predicate(); };
    window->requestActivate();
    emulation.startGame(path);
    CHECK_UI(until([&] { return emulation.state()=="Running" || emulation.state()=="Failed"; }));
    if(emulation.state()!="Running") qCritical() << emulation.message() << emulation.log();
    CHECK_UI(emulation.state()=="Running" && emulation.gameLoaded());
    CHECK_UI(emulation.gpuDescriptor()["width"].toInt() == 1920 && emulation.gpuDescriptor()["height"].toInt() == 1080);
    CHECK_UI(emulation.log().join("\n").contains("resolution 1 docked"));
    window->setProperty("gameView",true); wait(100);
    CHECK_UI(until([&] { return surface->framesPresented()>=10 || !surface->error().isEmpty(); }));
    CHECK_UI(surface->error().isEmpty() && surface->framesPresented()>=10);
    auto* preferences = static_cast<SettingsStore*>(engine.rootContext()->contextProperty("preferences").value<QObject*>());
    auto* qualityTools = static_cast<QualityTools*>(engine.rootContext()->contextProperty("qualityTools").value<QObject*>());
    CHECK_UI(preferences && qualityTools);
    auto* overlay = findVisual(window->contentItem(), "performanceOverlay"); CHECK_UI(overlay);
    preferences->setValue("ui/showFrameRate", true); CHECK_UI(preferences->apply()); wait(100);
    CHECK_UI(overlay->isVisible());
    wait(1100);
    CHECK_UI(emulation.performanceMetrics().value("publishedFps").toDouble() > 0.0);
    CHECK_UI(emulation.performanceMetrics().value("compositorFps").toDouble() > 0.0);
    const auto itemGrab = surface->grabToImage(); CHECK_UI(itemGrab);
    bool imageReady = false;
    QObject::connect(itemGrab.data(), &QQuickItemGrabResult::ready, window, [&] { imageReady = true; });
    CHECK_UI(until([&] { return imageReady; }));
    const auto shotPath = qualityTools->saveScreenshot(itemGrab->image(), "Test 2048");
    CHECK_UI(!shotPath.isEmpty() && QFileInfo(shotPath).suffix() == "png" && QImage(shotPath).size() == itemGrab->image().size());
    qualityTools->reportScreenshotError("atalho de teste");
    QKeyEvent screenshotShortcut(QEvent::KeyPress,Qt::Key_F12,Qt::NoModifier); QCoreApplication::sendEvent(window,&screenshotShortcut);
    CHECK_UI(until([&] { return qualityTools->status().startsWith("Screenshot salvo:"); }));
    preferences->setValue("ui/showFrameRate", false); CHECK_UI(preferences->apply()); wait(100);
    CHECK_UI(!overlay->isVisible());
    QKeyEvent overlayOn(QEvent::KeyPress,Qt::Key_F8,Qt::ControlModifier|Qt::ShiftModifier); QCoreApplication::sendEvent(window,&overlayOn); wait(100);
    CHECK_UI(window->property("overlayShortcutOverrideActive").toBool() && window->property("overlayShortcutOverride").toBool() && overlay->isVisible());
    QKeyEvent overlayOff(QEvent::KeyPress,Qt::Key_F8,Qt::ControlModifier|Qt::ShiftModifier); QCoreApplication::sendEvent(window,&overlayOff); wait(100);
    CHECK_UI(!window->property("overlayShortcutOverride").toBool() && !overlay->isVisible());
    emulation.recordGraphicsError("fixture path C:/Users/private/game.nsp token 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    CHECK_UI(emulation.exportDiagnostics() && QFileInfo(emulation.diagnosticsExportPath()).isFile());
    QFile reportFile(emulation.diagnosticsExportPath()); CHECK_UI(reportFile.open(QIODevice::ReadOnly));
    const auto reportBytes = reportFile.readAll(); reportFile.close();
    CHECK_UI(!reportBytes.contains("C:/Users/private") && !reportBytes.contains("0123456789abcdef"));
    QElapsedTimer rate; rate.start();
    const auto firstPresented = surface->framesPresented();
    wait(2000);
    qInfo() << "Presented FPS:" << (surface->framesPresented() - firstPresented) * 1000.0 / rate.elapsed();
    const auto pid=emulation.processId();
    const auto firstImage = window->grabWindow();
    CHECK_UI(firstImage.save("build/program-integrated-before.png"));
    if (QFileInfo(path).fileName().compare("2048.nro", Qt::CaseInsensitive) == 0) {
        const auto center = firstImage.pixelColor(firstImage.width() / 2, firstImage.height() / 2);
        CHECK_UI(center.red() + center.green() + center.blue() > 100);
    }
    CHECK_UI(input.gameInput());
    QKeyEvent down(QEvent::KeyPress,Qt::Key_Z,Qt::NoModifier);
    QCoreApplication::sendEvent(window,&down);
    CHECK_UI(until([&] { return (emulation.lastHidButtons()&1)!=0; }));
    QKeyEvent up(QEvent::KeyRelease,Qt::Key_Z,Qt::NoModifier); QCoreApplication::sendEvent(window,&up);
    CHECK_UI(until([&] { return emulation.lastHidButtons()==0; }));
    for(int key : {Qt::Key_Left,Qt::Key_Down,Qt::Key_Right,Qt::Key_Up}) {
        QKeyEvent press(QEvent::KeyPress,key,Qt::NoModifier); QCoreApplication::sendEvent(window,&press); wait(150);
        QKeyEvent release(QEvent::KeyRelease,key,Qt::NoModifier); QCoreApplication::sendEvent(window,&release); wait(180);
    }
    CHECK_UI(window->grabWindow().save("build/program-integrated-after.png"));
    const auto count=surface->framesPresented();
    QKeyEvent menu(QEvent::KeyPress,Qt::Key_F1,Qt::NoModifier); QCoreApplication::sendEvent(window,&menu); wait(100);
    QKeyEvent menuRelease(QEvent::KeyRelease,Qt::Key_F1,Qt::NoModifier); QCoreApplication::sendEvent(window,&menuRelease);
    CHECK_UI(window->property("quickMenuOpen").toBool() && !input.gameInput() && emulation.processId()==pid);
    auto* speedSlider = findVisual(window->contentItem(), "quickMenuSpeedSlider"); CHECK_UI(speedSlider && speedSlider->isVisible());
    auto* cursorArea = findVisual(window->contentItem(), "gameCursorArea"); CHECK_UI(cursorArea);
    CHECK_UI(window->findChild<QObject*>("quickMenu") != nullptr);
    CHECK_UI(emulation.lastHidButtons()==0);
    CHECK_UI(until([&] { return surface->framesPresented()>count+5; }));
    CHECK_UI(window->grabWindow().save("build/program-menu.png"));
    QCoreApplication::sendEvent(window,&up); wait(50);
    QKeyEvent closeMenu(QEvent::KeyPress,Qt::Key_F1,Qt::NoModifier); QCoreApplication::sendEvent(window,&closeMenu); wait(100);
    CHECK_UI(!window->property("quickMenuOpen").toBool());
    QMetaObject::invokeMethod(window,"leaveGame"); wait(100);
    CHECK_UI(!window->property("gameView").toBool() && emulation.processId()==pid);
    QMetaObject::invokeMethod(window,"enterGame"); wait(100);
    CHECK_UI(until([&] { return surface->framesPresented()>count+5; }));
    QKeyEvent pauseShortcut(QEvent::KeyPress,Qt::Key_F9,Qt::ControlModifier|Qt::ShiftModifier); QCoreApplication::sendEvent(window,&pauseShortcut);
    CHECK_UI(until([&] { return emulation.state()=="Paused" || emulation.state()=="Failed"; })); CHECK_UI(emulation.state()=="Paused");
    QKeyEvent resumeShortcut(QEvent::KeyPress,Qt::Key_F9,Qt::ControlModifier|Qt::ShiftModifier); QCoreApplication::sendEvent(window,&resumeShortcut);
    CHECK_UI(until([&] { return emulation.state()=="Running" || emulation.state()=="Failed"; })); CHECK_UI(emulation.state()=="Running");
    QKeyEvent stopShortcut(QEvent::KeyPress,Qt::Key_F10,Qt::ControlModifier|Qt::ShiftModifier); QCoreApplication::sendEvent(window,&stopShortcut);
    CHECK_UI(until([&] { return !emulation.active(); })); CHECK_UI(emulation.state()=="Stopped");
    emulation.startGame(path); CHECK_UI(until([&] { return emulation.state()=="Running" || emulation.state()=="Failed"; })); CHECK_UI(emulation.state()=="Running");
#ifdef _WIN32
    const auto crashPid=emulation.processId();
    auto child=OpenProcess(PROCESS_TERMINATE,FALSE,DWORD(crashPid)); CHECK_UI(child);
    CHECK_UI(TerminateProcess(child,73)); CloseHandle(child);
    CHECK_UI(until([&] { return emulation.state()=="Failed" && !emulation.active(); }));
    CHECK_UI(window->isVisible());
    emulation.startGame(path);
    CHECK_UI(until([&] { return emulation.state()=="Running" || emulation.state()=="Failed"; })); CHECK_UI(emulation.state()=="Running");
#endif
    emulation.stopGame(); CHECK_UI(until([&] { return !emulation.active(); })); CHECK_UI(emulation.state()=="Stopped");
    QDir().mkpath(QCoreApplication::applicationDirPath()+"/test-content");
    const auto invalid=QCoreApplication::applicationDirPath()+"/test-content/invalid-session.nro";
    QFile bad(invalid); CHECK_UI(bad.open(QIODevice::WriteOnly)); bad.write("not an executable"); bad.close();
    emulation.startGame(invalid); CHECK_UI(until([&] { return emulation.state()=="Failed" && !emulation.active(); }));
    CHECK_UI(window->isVisible());
    qInfo() << "PASS: authorized program loaded, real compositor frames imported into QML, keyboard -> IPC -> original HID, menu/context neutralization, library/settings/return, pause/resume, normal stop, session restart after forced guest crash, invalid file isolation. Audio audibility and native Vulkan GPU interop remain unverified.";
    return 0;
}

int runFrontendSmoke(QQmlApplicationEngine& engine, AppModel& model, SettingsStore& settings, ControllerInput& input, EmulationManager& emulation, bool capture) {
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    CHECK_UI(window);
    QMetaObject::invokeMethod(window, "togglePerformanceOverlay");
    CHECK_UI(window->property("overlayShortcutOverrideActive").toBool() && window->property("overlayShortcutOverride").toBool());
    CHECK_UI(!settings.values().value("ui/showFrameRate").toBool());
    QMetaObject::invokeMethod(window, "togglePerformanceOverlay");
    CHECK_UI(!window->property("overlayShortcutOverride").toBool());
    auto* compatibility = qobject_cast<CompatibilityStore*>(engine.rootContext()->contextProperty("compatibility").value<QObject*>());
    CHECK_UI(compatibility);
    {
        const QString titleId = "010000000000ABCD";
        const QVariantMap game{{"title", "Compatibility fixture"}, {"titleId", titleId}};
        const QVariantMap config{{"graphics/resolution", 3}, {"graphics/docked", true}, {"unrelated", "discard"}};
        CHECK_UI(compatibility->beginSession(game, config));
        CHECK_UI(compatibility->statusLabel(titleId) == "Não testado");
        compatibility->updateSessionState("Loading");
        compatibility->updateSessionState("Running");
        compatibility->recordSessionError("graphics", "fixture renderer failure");
        compatibility->finishSession("stopped", "fixture session completed");
        auto history = compatibility->history(titleId);
        CHECK_UI(history.size() == 1);
        auto session = history.last().toMap();
        CHECK_UI(session.value("outcome").toString() == "stopped");
        CHECK_UI(session.value("sawFirstFrame").toBool());
        CHECK_UI(session.value("emulatorVersion").toString() == QCoreApplication::applicationVersion());
        CHECK_UI(session.value("gameVersion").toString() == "Não informado pelo leitor");
        CHECK_UI(session.value("configuration").toMap().size() == 2);
        CHECK_UI(session.value("events").toList().size() == 1);
        CHECK_UI(compatibility->setAssessment(titleId, "playable-issues", "Falha gráfica reproduzível no fixture."));
        CHECK_UI(compatibility->statusLabel(titleId) == "Jogável com problemas");
        CHECK_UI(compatibility->record(titleId).value("notes").toString().contains("Falha gráfica"));

        CHECK_UI(compatibility->beginSession(game, config));
        CompatibilityStore recovery;
        history = recovery.history(titleId);
        CHECK_UI(history.size() == 2 && history.last().toMap().value("outcome").toString() == "interrupted-or-app-closed");
        CHECK_UI(recovery.statusLabel(titleId) == "Jogável com problemas");
        CHECK_UI(!compatibility->beginSession(QVariantMap{{"title", "No ID"}}, config));
        CHECK_UI(compatibility->beginSession(game, config));
        compatibility->recordSessionError("core", "fixture core failure");
        compatibility->finishSession("core-failure", "fixture core failure");
        const auto crashes = compatibility->crashHistory();
        CHECK_UI(crashes.size() == 1);
        const auto gameCrashes = crashes.first().toMap().value("crashes").toList();
        CHECK_UI(gameCrashes.size() >= 1);
        CHECK_UI(gameCrashes.first().toMap().value("outcome").toString() == "core-failure");
        const auto failedSession = gameCrashes.first().toMap().value("sessionId").toString();
        compatibility->markResolutionRequested(failedSession, true);
        CHECK_UI(compatibility->crashHistory().first().toMap().value("crashes").toList().first().toMap().value("resolutionRequested").toBool());
        compatibility->markResolutionRequested(failedSession, false);
        CHECK_UI(compatibility->crashHistory().first().toMap().value("crashes").toList().first().toMap().value("resolutionDeclined").toBool());
    }
    {
        QTemporaryDir modData;
        CHECK_UI(modData.isValid());
        QualityTools tools(modData.path() + "/private-core-data");
        const auto shaderRoot = modData.path() + "/private-core-data/shader";
        CHECK_UI(QDir().mkpath(shaderRoot + "/0100000000001000"));
        CHECK_UI(QDir().mkpath(shaderRoot + "/0100000000002000"));
        QFile cacheA(shaderRoot + "/0100000000001000/vulkan.bin"); CHECK_UI(cacheA.open(QIODevice::WriteOnly)); CHECK_UI(cacheA.write("pipeline") == 8); cacheA.close();
        QFile cacheB(shaderRoot + "/0100000000002000/vulkan.bin"); CHECK_UI(cacheB.open(QIODevice::WriteOnly)); CHECK_UI(cacheB.write("keep") == 4); cacheB.close();
        const auto cacheInfo = tools.gameCacheInfo("0100000000001000"); CHECK_UI(cacheInfo.value("files").toInt() == 1 && cacheInfo.value("bytes").toLongLong() == 8);
        CHECK_UI(!tools.clearGameCache("0100000000001000", true) && QFileInfo::exists(cacheA.fileName()));
        CHECK_UI(tools.clearGameCache("0100000000001000", false));
        CHECK_UI(!QFileInfo::exists(shaderRoot + "/0100000000001000") && QFileInfo::exists(cacheB.fileName()));
        CHECK_UI(!tools.clearGameCache("../../../../", false));
        const auto source = modData.path() + "/sample-mod";
        CHECK_UI(QDir().mkpath(source + "/romfs/Actor"));
        CHECK_UI(QDir().mkpath(source + "/cheats"));
        QFile asset(source + "/romfs/Actor/test.txt"); CHECK_UI(asset.open(QIODevice::WriteOnly));
        CHECK_UI(asset.write("fixture") == 7); asset.close();
        QFile cheat(source + "/cheats/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA.txt");
        const QByteArray cheatText = "[Fixture cheat]\n04000000 00000001\n";
        CHECK_UI(cheat.open(QIODevice::WriteOnly)); CHECK_UI(cheat.write(cheatText) == cheatText.size()); cheat.close();
        GameContentManager content(modData.path() + "/private-core-data");
        const QString titleId = "0100000000001000";
        const auto badSource = modData.path() + "/invalid-mod";
        CHECK_UI(QDir().mkpath(badSource + "/romfs")); CHECK_UI(QDir().mkpath(badSource + "/cheats"));
        QFile invalidCheat(badSource + "/cheats/BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB.txt");
        CHECK_UI(invalidCheat.open(QIODevice::WriteOnly)); invalidCheat.write("not a cheat"); invalidCheat.close();
        CHECK_UI(!content.importMod(titleId, badSource));
        CHECK_UI(content.mods(titleId).isEmpty());
        CHECK_UI(!content.importCheat(titleId, invalidCheat.fileName()));
        CHECK_UI(content.importMod(titleId, source));
        auto mods = content.mods(titleId);
        CHECK_UI(mods.size() == 2 && mods.first().toMap().value("enabled").toBool());
        bool foundValidCheat = false;
        for (const auto& item : mods) if (item.toMap().value("kind").toString() == "cheat")
            foundValidCheat = item.toMap().value("valid").toBool() && item.toMap().value("names").toStringList().contains("Fixture cheat");
        CHECK_UI(foundValidCheat);
        const auto standaloneCheatPath = modData.path() + "/CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.txt";
        QFile standaloneCheat(standaloneCheatPath);
        CHECK_UI(standaloneCheat.open(QIODevice::WriteOnly)); CHECK_UI(standaloneCheat.write(cheatText) == cheatText.size()); standaloneCheat.close();
        CHECK_UI(content.importCheat(titleId, standaloneCheatPath));
        CHECK_UI(content.mods(titleId).size() == 4);
        CHECK_UI(!content.importCheat(titleId, standaloneCheatPath));
        QString folder;
        for (const auto& item : mods) if (item.toMap().value("folder").toString() == "sample-mod") folder = "sample-mod";
        CHECK_UI(!folder.isEmpty());
        CHECK_UI(!content.importMod(titleId, source));
        CHECK_UI(content.setModEnabled(titleId, folder, false));
        mods = content.mods(titleId);
        bool foundDisabledMod = false;
        for (const auto& item : mods) if (item.toMap().value("folder").toString() == folder)
            foundDisabledMod = !item.toMap().value("enabled").toBool();
        CHECK_UI(mods.size() == 3 && foundDisabledMod);
        CHECK_UI(content.setModEnabled(titleId, folder, true));
        mods = content.mods(titleId); CHECK_UI(mods.size() == 4);
        CHECK_UI(content.setCheatEnabled(titleId, folder, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA.txt", false));
        mods = content.mods(titleId);
        bool foundDisabledCheat = false;
        for (const auto& item : mods) if (item.toMap().value("kind").toString() == "cheat") foundDisabledCheat = !item.toMap().value("enabled").toBool();
        CHECK_UI(foundDisabledCheat);
        CHECK_UI(content.setCheatEnabled(titleId, folder, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA.txt", true));
        CHECK_UI(content.removeMod(titleId, folder) && content.mods(titleId).size() == 2);
        CHECK_UI(content.removeMod(titleId, "Cheats importados") && content.mods(titleId).isEmpty());
        CHECK_UI(!content.importMod("../bad", source));
    }
    auto* library = window->findChild<QQuickItem*>("libraryPage");
    auto* grid = window->findChild<QQuickItem*>("gameGrid");
    auto* settingsPage = window->findChild<QQuickItem*>("settingsPage");
    auto* search = window->findChild<QQuickItem*>("librarySearch");
    CHECK_UI(library && grid && settingsPage && search);
    CHECK_UI(findVisual(window->contentItem(), "nav0") && findVisual(window->contentItem(), "nav3"));
    CHECK_UI(!findVisual(window->contentItem(), "nav4"));
    auto* presentation = window->findChild<QQuickItem*>("gamePresentation"); CHECK_UI(presentation);
    CHECK_UI(qAbs(presentation->width()-window->width())<1 && qAbs(presentation->height()-window->height())<1);
    for (int i = 0; i < 24; ++i) {
        QFile fixture(model.folders().first() + QString("/Teste UI %1.nro").arg(i, 2, 10, QChar('0')));
        CHECK_UI(fixture.open(QIODevice::WriteOnly)); fixture.write("Synthetic UI fixture; not a game.");
    }
    model.scan(); wait();
    {
        const auto path = model.games().first().toMap().value("path").toString();
        const QFileInfo info(path);
        const auto stamp = info.canonicalFilePath() + ":" + QString::number(info.size()) + ":" + QString::number(info.lastModified().toMSecsSinceEpoch());
        model.updateMetadata(QVariantList{QVariantMap{{"path",path},{"stamp",stamp},{"titleId","0100000000001000"}}});
        const auto globalResolution = settings.values().value("graphics/resolution");
        CHECK_UI(settings.setGameOverride(path, "graphics/resolution", 1));
        CHECK_UI(settings.setGameOverrideValue(path, "graphics/shaderCache", false));
        CHECK_UI(settings.setGameOverrideValue(path, "audio/outputDevice", QStringLiteral("fixture-device")));
        const auto key = QString::fromLatin1(QCryptographicHash::hash(info.canonicalFilePath().toUtf8(), QCryptographicHash::Sha256).toHex());
        CHECK_UI(QSettings().contains("library/gameOverrides/title/0100000000001000/graphics/resolution"));
        CHECK_UI(settings.values().value("graphics/resolution") == globalResolution);
        SettingsStore restored;
        CHECK_UI(restored.gameOverride(path, "graphics/resolution") == 1);
        CHECK_UI(restored.gameOverrideValue(path, "graphics/shaderCache").isValid() && !restored.gameOverrideValue(path, "graphics/shaderCache").toBool());
        CHECK_UI(restored.gameOverrideValue(path, "audio/outputDevice").toString() == "fixture-device");
        CHECK_UI(restored.clearGameOverrides(path) && !restored.hasGameOverrides(path));
        Q_UNUSED(key);
    }
    wait();
    window->requestActivate(); window->setProperty("page", 0); wait(170);
    invoke(library, "focusLibrary"); wait();
    CHECK_UI(grid->hasActiveFocus());
    QMetaObject::invokeMethod(library, "openCompatibility"); wait();
    auto* compatibilityDialog = window->findChild<QObject*>("gameCompatibilityDialog"); CHECK_UI(compatibilityDialog);
    CHECK_UI(compatibilityDialog->property("visible").toBool());
    CHECK_UI(compatibilityDialog->property("targetTitleId").toString() == "0100000000001000");
    CHECK_UI(compatibility->statusLabel("0100000000001000") == "Não testado");
    QMetaObject::invokeMethod(compatibilityDialog, "close"); wait();
    auto* gameSettingsDialog = window->findChild<QObject*>("gameSettingsDialog"); CHECK_UI(gameSettingsDialog);
    QMetaObject::invokeMethod(library, "openGameSettings", Q_ARG(QVariant, QVariant(false)), Q_ARG(QVariant, QVariant(false))); wait();
    CHECK_UI(gameSettingsDialog->property("visible").toBool());
    QMetaObject::invokeMethod(gameSettingsDialog, "close"); wait();
    window->setProperty("page", 3); wait();
    auto* diagnosticsPage = window->findChild<QObject*>("diagnosticsPage"); CHECK_UI(diagnosticsPage);
    diagnosticsPage->setProperty("resolverTitle", "Compatibility fixture");
    diagnosticsPage->setProperty("resolverSessionId", compatibility->crashHistory().first().toMap().value("crashes").toList().first().toMap().value("sessionId"));
    auto* resolveCrashDialog = window->findChild<QObject*>("resolveCrashDialog"); CHECK_UI(resolveCrashDialog);
    CHECK_UI(QMetaObject::invokeMethod(resolveCrashDialog, "open")); wait();
    CHECK_UI(resolveCrashDialog->property("visible").toBool());
    QMetaObject::invokeMethod(resolveCrashDialog, "close"); wait();
    window->setProperty("page", 0); wait();
    const auto deviceIndex = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
        SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 1);
    CHECK_UI(deviceIndex >= 0);
    auto* joystick = SDL_JoystickOpen(deviceIndex); CHECK_UI(joystick);
    wait(); input.setSelected(input.devices().size() - 1); wait();
    auto press = [&](int button) {
        SDL_JoystickSetVirtualButton(joystick, button, 1); SDL_JoystickUpdate(); input.poll(); wait();
        SDL_JoystickSetVirtualButton(joystick, button, 0); SDL_JoystickUpdate(); input.poll(); wait();
    };
    input.poll(); wait();
    auto* sortSelector = window->findChild<QQuickItem*>("librarySortSelector"); CHECK_UI(sortSelector);
    sortSelector->forceActiveFocus(); wait();
    press(SDL_CONTROLLER_BUTTON_A);
    CHECK_UI(input.popupActive && !emulation.active());
    press(SDL_CONTROLLER_BUTTON_B); CHECK_UI(!input.popupActive && !emulation.active());
    invoke(library, "focusLibrary"); wait();
    const auto beforeIndex = grid->property("currentIndex").toInt();
    press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    if (grid->property("currentIndex").toInt() != beforeIndex + 1)
        qWarning() << "Grid navigation:" << beforeIndex << "->" << grid->property("currentIndex") << "focus" << window->activeFocusItem() << input.status();
    CHECK_UI(grid->property("currentIndex").toInt() == beforeIndex + 1);
    SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, 25000); wait();
    SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, 0); wait();
    CHECK_UI(grid->property("currentIndex").toInt() > beforeIndex + 1);
    for (int i = 0; i < 5; ++i) press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    CHECK_UI(grid->property("contentY").toDouble() > 0);
    auto* contextMenu = window->findChild<QObject*>("gameContextMenu"); CHECK_UI(contextMenu);
    press(SDL_CONTROLLER_BUTTON_Y);
    CHECK_UI(input.popupActive);
    CHECK_UI(contextMenu->property("visible").toBool());
    auto* gameModsAction = window->findChild<QObject*>("gameModsMenuAction"); CHECK_UI(gameModsAction);
    bool modsHighlighted = false;
    for (int i = 0; i < 10 && !modsHighlighted; ++i) {
        press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        modsHighlighted = gameModsAction->property("highlighted").toBool();
    }
    CHECK_UI(modsHighlighted);
    press(SDL_CONTROLLER_BUTTON_A); wait();
    auto* gameModsDialog = window->findChild<QObject*>("gameModsDialog"); CHECK_UI(gameModsDialog);
    CHECK_UI(gameModsDialog->property("visible").toBool());
    QMetaObject::invokeMethod(gameModsDialog, "close"); wait();
    CHECK_UI(!input.popupActive);
    const int tileIndex = grid->property("currentIndex").toInt();
    auto* tile = findVisual(window->contentItem(), "gameTile_" + QString::number(tileIndex)); CHECK_UI(tile);
    const QPointF clickPoint = tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2));
    QMouseEvent rightDown(QEvent::MouseButtonPress, clickPoint, window->mapToGlobal(clickPoint.toPoint()), Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &rightDown);
    QMouseEvent rightUp(QEvent::MouseButtonRelease, clickPoint, window->mapToGlobal(clickPoint.toPoint()), Qt::RightButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &rightUp); wait();
    CHECK_UI(contextMenu->property("visible").toBool());
    const QPointF expectedMenu = library->mapFromScene(clickPoint);
    CHECK_UI(qAbs(contextMenu->property("x").toDouble() - expectedMenu.x()) < 40);
    CHECK_UI(gameModsAction->property("enabled").toBool());
    gameModsDialog = window->findChild<QObject*>("gameModsDialog"); CHECK_UI(gameModsDialog);
    auto* gameModsItem = qobject_cast<QQuickItem*>(gameModsAction); CHECK_UI(gameModsItem);
    const QPointF modsClick = gameModsItem->mapToScene(QPointF(gameModsItem->width() / 2, gameModsItem->height() / 2));
    QMouseEvent modsDown(QEvent::MouseButtonPress, modsClick, window->mapToGlobal(modsClick.toPoint()), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &modsDown);
    QMouseEvent modsUp(QEvent::MouseButtonRelease, modsClick, window->mapToGlobal(modsClick.toPoint()), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &modsUp); wait();
    CHECK_UI(gameModsDialog->property("visible").toBool());
    QMetaObject::invokeMethod(gameModsDialog, "close"); wait();
    auto* gameSettingsAction = window->findChild<QObject*>("gameSettingsMenuAction"); CHECK_UI(gameSettingsAction);
    QMetaObject::invokeMethod(gameSettingsAction, "triggered"); wait();
    CHECK_UI(gameSettingsDialog->property("visible").toBool() && gameSettingsDialog->property("settingsFromMouse").toBool());
    const auto expectedCursorScene = library->mapToScene(expectedMenu);
    CHECK_UI(qAbs(gameSettingsDialog->property("x").toDouble() - expectedCursorScene.x()) < 40);
    QMetaObject::invokeMethod(gameSettingsDialog, "close"); wait();
    press(SDL_CONTROLLER_BUTTON_START);
    CHECK_UI(gameSettingsDialog->property("visible").toBool());
    CHECK_UI(library->property("settingsFromController").toBool());
    CHECK_UI(gameSettingsDialog->property("targetPath").toString() == library->property("selectedGame").toMap().value("path").toString());
    auto* gameCover = findVisual(window->contentItem(), "gameCoverImage"); CHECK_UI(gameCover);
    const auto coverRight = gameCover->mapToScene(QPointF(gameCover->width() + 12, 0)).x();
    const auto coverLeft = gameCover->mapToScene(QPointF(-12, 0)).x() - gameSettingsDialog->property("width").toDouble();
    const auto expectedSettingsX = coverRight + gameSettingsDialog->property("width").toDouble() <= window->width() - 8 ? coverRight : coverLeft;
    CHECK_UI(qAbs(gameSettingsDialog->property("x").toDouble() - expectedSettingsX) < 40);
    QMetaObject::invokeMethod(gameSettingsDialog, "close"); wait();
    press(SDL_CONTROLLER_BUTTON_X); wait();
    CHECK_UI(window->property("page").toInt() == 0 && !input.popupActive);
    QMetaObject::invokeMethod(library, "chooseFilter", Q_ARG(QVariant, QVariant(1))); wait();
    CHECK_UI(grid->property("count").toInt() == 1);
    QMetaObject::invokeMethod(library, "chooseFilter", Q_ARG(QVariant, QVariant(2))); wait();
    CHECK_UI(grid->property("count").toInt() > 0);
    QMetaObject::invokeMethod(library, "chooseFilter", Q_ARG(QVariant, QVariant(0))); wait();
    settings.setValue("ui/view", 1); CHECK_UI(settings.apply()); wait();
    CHECK_UI(qAbs(grid->property("cellWidth").toDouble() - grid->width()) < 1);
    settings.setValue("ui/view", 0); settings.setValue("ui/sort", 1); CHECK_UI(settings.apply()); wait();
    CHECK_UI(grid->property("cellWidth").toDouble() < grid->width());
    settings.setValue("ui/sort", 0); CHECK_UI(settings.apply()); wait();
    search->setProperty("text", "NO_MATCH_EXPECTED"); wait(); CHECK_UI(grid->property("count").toInt() == 0);
    search->setProperty("text", ""); wait(); CHECK_UI(grid->property("count").toInt() == 25);
    press(SDL_CONTROLLER_BUTTON_X); CHECK_UI(window->property("page").toInt() == 0);
    window->setProperty("page", 1); wait();
    if (!QCoreApplication::arguments().contains("--gpu-smoke-test")) {
        QSet<QQuickItem*> visited;
        for (int i = 0; i < 18; ++i) {
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            visited.insert(window->activeFocusItem());
        }
        CHECK_UI(visited.size() >= 10);
    }
    auto* slider = window->findChild<QQuickItem*>("thumbnailSlider");
    CHECK_UI(slider);
    slider->forceActiveFocus(); wait();
    auto* scroll = window->findChild<QQuickItem*>("settingsScroll");
    CHECK_UI(scroll && scroll->property("contentY").toDouble() > 0);
    window->resize(800, 560); wait(100);
    window->setProperty("category", 1); wait();
    auto* legalButton = window->findChild<QObject*>("legalNoticeButton");
    auto* legalDialog = window->findChild<QObject*>("legalNoticeDialog"); CHECK_UI(legalButton && legalDialog);
    CHECK_UI(QMetaObject::invokeMethod(legalButton, "clicked")); wait();
    CHECK_UI(legalDialog->property("visible").toBool());
    auto* legalClose = window->findChild<QObject*>("legalNoticeCloseButton"); CHECK_UI(legalClose);
    CHECK_UI(legalClose->property("text").toString() == "Fechar");
    CHECK_UI(legalDialog->property("width").toDouble() <= 800 && legalDialog->property("height").toDouble() <= 560);
    auto* legalTextContent = window->findChild<QQuickItem*>("legalNoticeTextContent"); CHECK_UI(legalTextContent);
    CHECK_UI(legalTextContent->property("lineCount").toInt() > 1);
    CHECK_UI(legalTextContent->height() > 100 && legalTextContent->height() < 500);
    CHECK_UI(settingsPage->property("legalNoticeText").toString().contains("não fornece, distribui, hospeda ou incentiva"));
    CHECK_UI(settingsPage->property("legalNoticeText").toString().contains("não verifica a origem dos arquivos nem exige comprovantes de propriedade."));
    if (capture) CHECK_UI(window->grabWindow().save("build/legal-notice-small.png"));
    window->resize(1600, 1000); wait(100);
    CHECK_UI(legalDialog->property("width").toDouble() <= 700 && legalDialog->property("height").toDouble() <= 420);
    if (capture) CHECK_UI(window->grabWindow().save("build/legal-notice-large.png"));
    QMetaObject::invokeMethod(legalDialog, "close"); wait();
    CHECK_UI(QMetaObject::invokeMethod(window, "showScreenshotNotice")); wait();
    CHECK_UI(window->property("screenshotNoticeVisible").toBool());
    wait(4100);
    CHECK_UI(!window->property("screenshotNoticeVisible").toBool());
    auto* licensesButton = window->findChild<QObject*>("licensesButton");
    auto* licensesDialog = window->findChild<QObject*>("licensesDialog"); CHECK_UI(licensesButton && licensesDialog);
    CHECK_UI(QMetaObject::invokeMethod(licensesButton, "clicked")); wait();
    auto* licenseText = window->findChild<QQuickItem*>("licenseTextView"); CHECK_UI(licensesDialog->property("visible").toBool() && licenseText);
    auto* licensePicker = window->findChild<QObject*>("licenseNamePicker"); CHECK_UI(licensePicker);
    auto* licenseClose = window->findChild<QObject*>("licensesCloseButton"); CHECK_UI(licenseClose);
    CHECK_UI(licenseClose->property("text").toString() == "Fechar");
    licensePicker->setProperty("currentIndex", 0); wait();
    const auto componentNotices = licenseText->property("text").toString();
    CHECK_UI(componentNotices.contains("Núcleo de emulação") && componentNotices.contains("Qt 6") && componentNotices.contains("SDL 2"));
    CHECK_UI(!componentNotices.contains("C:/Users/") && !componentNotices.contains("C:\\Users\\") && !componentNotices.contains("Fase 3") && !componentNotices.contains("build-ucrt64"));
    if (capture) CHECK_UI(window->grabWindow().save("build/licenses-components.png"));
    licensePicker->setProperty("currentIndex", 3); wait();
    CHECK_UI(licenseText->property("text").toString().contains("GNU GENERAL PUBLIC LICENSE"));
    if (capture) CHECK_UI(window->grabWindow().save("build/licenses-gpl.png"));
    licensePicker->setProperty("currentIndex", 5); wait();
    CHECK_UI(licenseText->property("text").toString().contains("GNU LESSER GENERAL PUBLIC LICENSE"));
    licensePicker->setProperty("currentIndex", 19); wait();
    CHECK_UI(licenseText->property("text").toString().contains("Mozilla Public License Version 2.0"));
    licensePicker->setProperty("currentIndex", 21); wait();
    CHECK_UI(licenseText->property("text").toString().contains("LLVM Exceptions to the Apache 2.0 License"));
    licensePicker->setProperty("currentIndex", 9); wait();
    CHECK_UI(licenseText->property("text").toString().contains("This software is provided 'as-is'"));
    licensePicker->setProperty("currentIndex", 14); wait();
    CHECK_UI(licenseText->property("text").toString().contains("merryhime"));
    licensePicker->setProperty("currentIndex", 15); wait();
    CHECK_UI(licenseText->property("text").toString().contains("Victor Zverovich"));
    QMetaObject::invokeMethod(licensesDialog, "close"); wait();
    // The previous UI checks leave Settings on the Licenses category (1), where
    // the thumbnail slider is hidden. Return to its category before testing input.
    window->setProperty("category", 0); wait();
    CHECK_UI(slider->isVisible());
    const auto oldThumb = settings.values()["ui/thumb"].toInt();
    int focusSteps = 0;
    while (window->activeFocusItem() != slider && focusSteps < 32) {
        press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        ++focusSteps;
    }
    CHECK_UI(window->activeFocusItem() == slider);
    press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    CHECK_UI(settings.draft()["ui/thumb"].toInt() > oldThumb);
    CHECK_UI(settings.values()["ui/thumb"].toInt() == oldThumb);
    invoke(settingsPage, "cancel"); CHECK_UI(settings.draft()["ui/thumb"].toInt() == oldThumb);
    slider->forceActiveFocus(); wait(); press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    invoke(settingsPage, "apply"); CHECK_UI(settings.values()["ui/thumb"].toInt() > oldThumb);
    for (int category = 0; category < 10; ++category) { window->setProperty("category", category); wait(25); }
    window->setProperty("category", 4); wait();
    press(SDL_CONTROLLER_BUTTON_B); CHECK_UI(window->property("page").toInt() == 0);
    SDL_JoystickClose(joystick); CHECK_UI(SDL_JoystickDetachVirtual(deviceIndex) == 0); wait();
    // Mouse activation of the settings shortcut in the real window scene.
    auto* nav = findVisual(window->contentItem(), "nav1"); CHECK_UI(nav);
    const auto pos = nav->mapToScene(QPointF(nav->width() / 2, nav->height() / 2));
    QMouseEvent down(QEvent::MouseButtonPress, pos, window->mapToGlobal(pos.toPoint()), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent up(QEvent::MouseButtonRelease, pos, window->mapToGlobal(pos.toPoint()), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &down); QCoreApplication::sendEvent(window, &up); wait();
    CHECK_UI(window->property("page").toInt() == 1);
    input.sendKey("back"); wait(); CHECK_UI(window->property("page").toInt() == 0);
    window->resize(1100, 720); wait();
    window->setProperty("page", 1); window->setProperty("category", 0); wait();
    slider->forceActiveFocus(); wait(); CHECK_UI(scroll->property("contentY").toDouble() > 0);
    window->setProperty("page", 0); window->resize(1500, 940); wait();
    if (emulation.coreAvailable()) {
        auto until = [&](auto predicate) {
            QElapsedTimer timer; timer.start();
            while (!predicate() && timer.elapsed() < 15000) wait(20);
            return predicate();
        };
        emulation.startCore(); CHECK_UI(until([&] { return emulation.state() == "Ready" || emulation.state() == "Failed"; }));
        CHECK_UI(emulation.state() == "Ready");
        const auto corePid = emulation.processId();
        // A library entry whose file disappeared after scanning must not
        // enter a black game view or create a compatibility session.
        const auto selectedPath = library->property("selectedGame").toMap().value("path").toString();
        QFile selectedFixture(selectedPath); CHECK_UI(selectedFixture.exists());
        const auto movedFixturePath = selectedPath + ".temporarily-moved";
        CHECK_UI(selectedFixture.rename(movedFixturePath));
        CHECK_UI(QMetaObject::invokeMethod(library, "startSelected")); wait();
        CHECK_UI(!window->property("gameView").toBool());
        CHECK_UI(emulation.state() == "Ready" && !emulation.gameLoaded());
        CHECK_UI(!library->property("launchStatus").toString().isEmpty());
        QFile movedFixture(movedFixturePath); CHECK_UI(movedFixture.rename(selectedPath));
        // Loader identification is metadata probing only: it must report its
        // result in the library without entering the game presentation view.
        window->setProperty("page", 0); window->setProperty("gameView", false); wait();
        CHECK_UI(QMetaObject::invokeMethod(library, "identifySelected"));
        CHECK_UI(!window->property("gameView").toBool());
        CHECK_UI(until([&] { return !library->property("identificationPending").toBool(); }));
        CHECK_UI(emulation.state() == "Ready" && !emulation.gameLoaded());
        CHECK_UI(library->property("identificationStatus").toString().contains("Isso não valida o programa"));
        if (QCoreApplication::arguments().contains("--gpu-smoke-test")) {
            auto* surface = window->findChild<GpuSurface*>("gpuSurface"); CHECK_UI(surface);
            window->setProperty("gameView", true); wait(180);
            emulation.startGpuDiagnostic();
            CHECK_UI(until([&] { return emulation.gpuDiagnostic() || emulation.state() == "Failed"; }));
            CHECK_UI(emulation.gpuDiagnostic());
            CHECK_UI(until([&] { return surface->framesPresented() >= 10 || !surface->error().isEmpty(); }));
            if (!surface->error().isEmpty()) qCritical() << surface->error();
            CHECK_UI(surface->error().isEmpty() && surface->framesPresented() >= 10);
            const auto image = window->grabWindow();
            const auto center = surface->mapToScene(QPointF(surface->width()/2, surface->height()/2));
            const auto pixel = image.pixelColor(center.toPoint());
            CHECK_UI(pixel.red() > 30 && pixel.blue() > 60 && pixel.green() < 70);
            auto samePixel = [&](int x, int y) { return image.rect().contains(x,y) && image.pixelColor(x,y) == pixel; };
            int left = int(center.x()), right = left, top = int(center.y()), bottom = top;
            while (samePixel(left-1, int(center.y()))) --left;
            while (samePixel(right+1, int(center.y()))) ++right;
            while (samePixel(int(center.x()), top-1)) --top;
            while (samePixel(int(center.x()), bottom+1)) ++bottom;
            CHECK_UI(qAbs(double(right-left+1)/double(bottom-top+1) - 16.0/9.0) < 0.04);
            QKeyEvent fullScreenKey(QEvent::KeyPress,Qt::Key_F11,Qt::NoModifier); QCoreApplication::sendEvent(window,&fullScreenKey);
            wait(200); CHECK_UI(window->visibility() == QWindow::FullScreen);
            QKeyEvent windowedKey(QEvent::KeyPress,Qt::Key_F11,Qt::NoModifier); QCoreApplication::sendEvent(window,&windowedKey);
            wait(180); CHECK_UI(window->visibility() != QWindow::FullScreen);
            window->setProperty("gameView", false); wait(200);
            CHECK_UI(emulation.processId() == corePid);
            const auto count = surface->framesPresented();
            window->resize(1100,720); window->setProperty("gameView",true); wait(250);
            CHECK_UI(until([&] { return surface->framesPresented() > count + 5; }));
            CHECK_UI(surface->error().isEmpty());
            window->resize(1500,940); wait(180);
            const auto capturePath = QDir(QCoreApplication::applicationDirPath()).filePath("gpu-transport-smoke.png");
            CHECK_UI(window->grabWindow().save(capturePath));
            qInfo() << "PASS: imported shared GPU textures, actual pixel readback verified, resize, hidden library/backpressure and return; diagnostic content only";
        }
        window->setProperty("gameView", false); wait();
        window->setProperty("page", 0); wait();
        window->setProperty("page", 1); wait();
        CHECK_UI(emulation.processId() == corePid && emulation.state() == "Ready");
        if (capture) { window->setProperty("gameView", true); wait(180); CHECK_UI(window->grabWindow().save("build/core-smoke.png")); }
#ifdef _WIN32
        const auto handle = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(corePid));
        CHECK_UI(handle); CHECK_UI(TerminateProcess(handle, 38)); CloseHandle(handle);
        CHECK_UI(until([&] { return emulation.state() == "Failed" && !emulation.active(); }));
        CHECK_UI(window->isVisible() && window->property("page").toInt() == 0 && model.games().size() == 25);
        emulation.startCore(); CHECK_UI(until([&] { return emulation.state() == "Ready" || emulation.state() == "Failed"; })); CHECK_UI(emulation.state() == "Ready");
        if (QCoreApplication::arguments().contains("--gpu-smoke-test")) {
            auto* surface = window->findChild<GpuSurface*>("gpuSurface"); CHECK_UI(surface);
            const auto before = surface->framesPresented();
            window->setProperty("gameView",true); wait(100); emulation.startGpuDiagnostic(); emulation.startGpuDiagnostic();
            CHECK_UI(until([&] { return surface->framesPresented() > before + 5 || !surface->error().isEmpty(); }));
            CHECK_UI(surface->error().isEmpty() && surface->framesPresented() > before + 5);
        }
#endif
        emulation.stopGame(); CHECK_UI(until([&] { return !emulation.active(); })); CHECK_UI(emulation.state() == "Stopped");
        qInfo() << "PASS: real child process retained across pages; frontend survived child termination and restarted it.";
    }
    if (capture) {
        invoke(library, "focusLibrary"); grid->setProperty("currentIndex", 0); wait(250);
        CHECK_UI(window->grabWindow().save("build/frontend-smoke.png"));
        window->setProperty("page", 1); window->setProperty("category", 0); wait(250);
        CHECK_UI(window->grabWindow().save("build/settings-smoke.png"));
        window->setProperty("category", 4); wait(180);
        CHECK_UI(window->grabWindow().save("build/controllers-smoke.png"));
    }
    qInfo() << "PASS: SDL virtual input -> Qt focus, grid scroll, Y -> selected game settings, menu/back, search, settings slider/apply/cancel, mouse and keyboard.";
    return 0;
}

int runSetupSmoke(QQmlApplicationEngine& engine, SetupAssistant& setup, LibraryResources& resources, AppModel& model) {
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first()); CHECK_UI(window);
    auto* wizard = findVisual(window->contentItem(), "setupWizard"); CHECK_UI(wizard && wizard->isVisible());
    CHECK_UI(setup.step() == 0 && !setup.completed());
    auto* settings = static_cast<SettingsStore*>(engine.rootContext()->contextProperty("preferences").value<QObject*>());
    CHECK_UI(settings);
    QSettings legacyTerms;
    legacyTerms.setValue("setup/completed", true);
    legacyTerms.setValue("setup/step", 5);
    legacyTerms.setValue("setup/legalVersion", "shireia-legal-1");
    legacyTerms.setValue("setup/legalAcceptedAtUtc", "2026-01-01T00:00:00Z");
    legacyTerms.sync();
    SetupAssistant termsUpdate(*settings, resources, model, false);
    CHECK_UI(!termsUpdate.completed() && termsUpdate.step() == 0 && !termsUpdate.legalAccepted());
    auto* start = window->findChild<QObject*>("setupStartButton"); CHECK_UI(start);
    CHECK_UI(QMetaObject::invokeMethod(start, "clicked")); wait(20);
    CHECK_UI(setup.step() == 1);
    auto* legalCheck = window->findChild<QObject*>("setupLegalCheck");
    auto* agree = window->findChild<QObject*>("setupAgreeButton"); CHECK_UI(legalCheck && agree);
    CHECK_UI(window->activeFocusItem() == qobject_cast<QQuickItem*>(legalCheck));
    CHECK_UI(!resources.busy());
    CHECK_UI(!legalCheck->property("checked").toBool() && !agree->property("enabled").toBool());
    CHECK_UI(!setup.acceptLegal(false) && !setup.legalAccepted());
    legalCheck->setProperty("checked", true); wait(20); CHECK_UI(agree->property("enabled").toBool());
    legalCheck->setProperty("checked", false); wait(20); CHECK_UI(!agree->property("enabled").toBool());
    legalCheck->setProperty("checked", true); CHECK_UI(QMetaObject::invokeMethod(agree, "clicked")); wait(20);
    CHECK_UI(setup.legalAccepted() && setup.step() == 2);
    resources.setEnabled(false); // Keep this UI test isolated from any real resource import.
    CHECK_UI(!setup.selectKeys(QUrl::fromLocalFile(QDir::tempPath() + "/missing-prod.keys")));
    CHECK_UI(!setup.selectFirmware(QUrl::fromLocalFile(QDir::tempPath() + "/missing-firmware.zip")));
    CHECK_UI(!setup.continueFromKeys() && !setup.continueFromFirmware() && !setup.finish());
    QTemporaryDir setupFiles;
    CHECK_UI(setupFiles.isValid());
    QFile selectedKeys(setupFiles.filePath("prod.keys"));
    CHECK_UI(selectedKeys.open(QIODevice::WriteOnly)); selectedKeys.write("test-only-placeholder"); selectedKeys.close();
    CHECK_UI(setup.selectKeys(QUrl::fromLocalFile(selectedKeys.fileName())));
    CHECK_UI(setup.keysSelected() && !setup.keysValidated() && !setup.busy());
    CHECK_UI(setup.continueFromKeys() && setup.step() == 3);
    CHECK_UI(!setup.continueFromFirmware() && !setup.finish());
    CHECK_UI(setup.licenseText("GNU GPL v3 ou posterior").contains("GNU GENERAL PUBLIC LICENSE"));
    const auto componentNotices = setup.licenseText("Componentes incluídos");
    CHECK_UI(componentNotices.contains("Núcleo de emulação") && componentNotices.contains("Qt 6"));
    CHECK_UI(!componentNotices.contains("C:/Users/") && !componentNotices.contains("C:\\Users\\") && !componentNotices.contains("Fase 3"));
    CHECK_UI(setup.licenseText("SDL 2 — licença zlib").contains("Sam Lantinga"));
    CHECK_UI(setup.licenseText("libarchive — licença BSD").contains("Redistribution and use"));
    CHECK_UI(setup.licenseText("MPL 2.0 — decodificador BC").contains("Mozilla Public License Version 2.0"));
    CHECK_UI(setup.licenseText("BSD 2 cláusulas — tz").contains("Redistribution and use"));
    CHECK_UI(setup.licenseText("Dynarmic — 0BSD").contains("merryhime"));
    CHECK_UI(setup.licenseText("fmt — MIT").contains("Victor Zverovich"));
    SetupAssistant resumed(*settings, resources, model, false);
    CHECK_UI(!resumed.completed() && resumed.step() == 3 && resumed.legalAccepted());
    window->resize(1040, 700); wait(100);
    CHECK_UI(wizard->width() == window->width() && wizard->height() == window->height());
    qInfo() << "PASS: isolated first-run welcome/legal UI, unchecked consent gate, reject/accept state, re-confirmation after terms update, key selection before combined key/firmware validation, mandatory-step blocking, bundled license access, resume persistence and resize. Key contents and firmware were not validated.";
    return 0;
}
