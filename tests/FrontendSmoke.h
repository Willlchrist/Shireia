#pragma once
class ControllerInput;
class EmulationManager;
class QQmlApplicationEngine;
class QString;
int runProgramSmoke(QQmlApplicationEngine&, ControllerInput&, EmulationManager&, const QString&);
class AppModel;
class SettingsStore;
class SetupAssistant;
class LibraryResources;
int runFrontendSmoke(QQmlApplicationEngine&, AppModel&, SettingsStore&, ControllerInput&, EmulationManager&, bool capture);
int runSetupSmoke(QQmlApplicationEngine&, SetupAssistant&, LibraryResources&, AppModel&);
