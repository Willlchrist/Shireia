#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <QTemporaryDir>
#include <QQuickWindow>
#include <QFile>
#include <QDir>
#include <QIcon>
#include "AppModel.h"
#include "CompatibilityStore.h"
#include "SetupAssistant.h"
#include "LibraryResources.h"
#include "GameContentManager.h"
#include "QualityTools.h"
#include "../settings/SettingsStore.h"
#include "../input/ControllerInput.h"
#include "../../tests/FrontendSmoke.h"
#include "../emulation/EmulationManager.h"
#include "../graphics/GpuSurface.h"

int main(int argc, char* argv[]) {
    // The game image is presented by Qt Quick's D3D11 swap chain. Ask Qt's
    // threaded scene graph not to use display V-Sync as its frame pacing source;
    // the emulator core has its own per-game V-Sync setting. This is process
    // local and leaves the user's Windows VRR/G-SYNC configuration untouched.
    qputenv("QSG_NO_VSYNC", "1");
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Shireia");
    app.setApplicationName("ShireiaNext");
#ifdef SHIREIA_VERSION
    app.setApplicationVersion(QStringLiteral(SHIREIA_VERSION));
#endif
    app.setWindowIcon(QIcon("qrc:/assets/brand/shireia-logo.png"));
    const int programTest = app.arguments().indexOf("--program-smoke-test");
    const bool gpuSmoke = app.arguments().contains("--gpu-smoke-test") || programTest >= 0;
    const bool setupSmoke = app.arguments().contains("--setup-smoke-test");
    const bool smoke = app.arguments().contains("--smoke-test") || gpuSmoke || setupSmoke;
    if (gpuSmoke) QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QTemporaryDir temporary;
    if (smoke) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    }
    QQuickStyle::setStyle("Basic");
    qmlRegisterType<GpuSurface>("Shireia", 1, 0, "GpuSurface");
    AppModel model;
    if (smoke) {
        QDir().mkpath(temporary.path() + "/games");
        QFile candidate(temporary.path() + "/games/Test.nro");
        if (!candidate.open(QIODevice::WriteOnly)) return 4;
        candidate.write("test fixture, not an executable");
        candidate.close();
        model.addFolder(QUrl::fromLocalFile(temporary.path() + "/games"));
        model.addFolder(QUrl::fromLocalFile(temporary.path() + "/games"));
        if (model.games().size() != 1 || model.folders().size() != 1) return 5;
        model.setPauseOnMenu(false);
        AppModel restored;
        if (restored.pauseOnMenu() || restored.games().size() != 1) return 6;
    }
    EmulationManager emulation(nullptr, {}, smoke ? temporary.path() + "/core-data" : QString{});
    SettingsStore preferences;
    CompatibilityStore compatibility;
    GameContentManager gameContent(smoke ? temporary.path() + "/core-data" : QCoreApplication::applicationDirPath() + "/core-data");
    QualityTools qualityTools(smoke ? temporary.path() + "/core-data" : QCoreApplication::applicationDirPath() + "/core-data");
    if (programTest >= 0) {
        preferences.setValue("graphics/resolution", 0);
        preferences.setValue("graphics/shaderCache", false);
        preferences.setValue("graphics/docked", true);
        if (!preferences.apply()) return 7;
        if (!preferences.setGameOverride(app.arguments().value(programTest + 1), "graphics/resolution", 1)) return 9;
    }
    LibraryResources resources(model, preferences, emulation, QCoreApplication::applicationDirPath() + "/core-data", false);
    SetupAssistant setup(preferences, resources, model, !smoke || setupSmoke);
    ControllerInput controllers(nullptr, smoke);
    QObject::connect(&controllers, &ControllerInput::gamePlayerState, &emulation, &EmulationManager::sendPlayerInput);
    QObject::connect(&preferences, &SettingsStore::applied, &model, &AppModel::scan);
    QQmlApplicationEngine engine;
    int qmlWarnings = 0;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [&](const QList<QQmlError>& errors) { qmlWarnings += errors.size(); });
    engine.rootContext()->setContextProperty("appModel", &model);
    engine.rootContext()->setContextProperty("emulation", &emulation);
    engine.rootContext()->setContextProperty("preferences", &preferences);
    engine.rootContext()->setContextProperty("compatibility", &compatibility);
    engine.rootContext()->setContextProperty("firstRunSetup", &setup);
    engine.rootContext()->setContextProperty("libraryResources", &resources);
    engine.rootContext()->setContextProperty("gameContent", &gameContent);
    engine.rootContext()->setContextProperty("qualityTools", &qualityTools);
    engine.rootContext()->setContextProperty("controllers", &controllers);
    engine.rootContext()->setContextProperty("startupEnabled", !smoke);
    engine.rootContext()->setContextProperty("firstRunEnabled", !smoke || setupSmoke);
    engine.load(QUrl("qrc:/src/frontend/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    if (!smoke) {
        // Start the core after first-run setup is complete and the initial
        // resource job releases its shared lock. Starting it during onboarding
        // prevents the key/firmware validator from acquiring that same lock.
        bool startupRequested = false;
        auto tryStartCore = [&] {
            if (startupRequested || !setup.completed() || resources.busy()) return;
            startupRequested = true;
            QTimer::singleShot(0, &emulation, [&emulation] { emulation.startCore(); });
        };
        QObject::connect(&resources, &LibraryResources::statusChanged, &app, tryStartCore);
        QObject::connect(&emulation, &EmulationManager::stateChanged, &app, tryStartCore);
        QObject::connect(&setup, &SetupAssistant::changed, &app, tryStartCore);
        QTimer::singleShot(0, &app, tryStartCore);
    }
    if (smoke) {
        auto* root = engine.rootObjects().first();
        QTimer::singleShot(200, &app, [&, root] {
            if (setupSmoke) { const int result = runSetupSmoke(engine, setup, resources, model); app.exit(qmlWarnings ? 8 : result); return; }
            const auto before = model.games();
            for (int page = 0; page < 4; ++page) {
                root->setProperty("page", page);
                QCoreApplication::processEvents();
                if (root->property("page").toInt() != page) { app.exit(2); return; }
            }
            emulation.startGame("unused");
            if (before.size() != model.games().size() || emulation.active()) { app.exit(3); return; }
            const auto result = programTest >= 0 ? runProgramSmoke(engine, controllers, emulation, app.arguments().value(programTest+1)) : runFrontendSmoke(engine, model, preferences, controllers, emulation, app.arguments().contains("--capture"));
            app.exit(qmlWarnings ? 8 : result);
        });
    }
    return app.exec();
}
