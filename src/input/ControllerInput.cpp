#include "ControllerInput.h"
#include <QGuiApplication>
#include <QWindow>
#include <QKeyEvent>
#include <QKeySequence>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace {
const QStringList gameActions{"A","B","X","Y","LStick","RStick","L","R","ZL","ZR","Plus","Minus",
    "DLeft","DUp","DRight","DDown","SL","SR","Home","Capture"};
const QStringList axisActions{"LX","LY","RX","RY"};
const QStringList keyboardAxisActions{"LX+","LX-","LY+","LY-","RX+","RX-","RY+","RY-"};
QString physicalName(int code, bool pad) {
    if (code < 0) return QObject::tr("Não atribuído");
    if (code >= 1000) return QKeySequence(code - 1000).toString();
    if (code >= 200) return QObject::tr("Direcional %1").arg(QStringList{"esquerda","cima","direita","baixo"}.value(code - 200));
    if (code >= 100) return QObject::tr("Eixo %1").arg(code - 100);
    const char* label = pad ? SDL_GameControllerGetStringForButton(SDL_GameControllerButton(code)) : nullptr;
    return label ? QString::fromUtf8(label).toUpper() : QObject::tr("Botão %1").arg(code);
}
}

ControllerInput::ControllerInput(QObject* parent, bool testMode) : QObject(parent), testMode_(testMode) {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");
    initialized_ = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) == 0;
    if (!initialized_) error_ = QString::fromUtf8(SDL_GetError());
    clock_.start();
    if (initialized_) for (int i = 0; i < SDL_NumJoysticks(); ++i) addDevice(i);
    connect(&timer_, &QTimer::timeout, this, &ControllerInput::poll);
    timer_.start(16);
    qApp->installEventFilter(this);
}
ControllerInput::~ControllerInput() {
    qApp->removeEventFilter(this);
    timer_.stop();
    for (auto& device : devices_) {
        if (device.pad) SDL_GameControllerClose(device.pad); else SDL_JoystickClose(device.joystick);
    }
    if (initialized_) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS);
}
bool ControllerInput::foreground() const { return testMode_ || QGuiApplication::applicationState() == Qt::ApplicationActive; }
bool ControllerInput::enabled() const { return settings_.value("ui/controller", true).toBool(); }
ControllerInput::Device* ControllerInput::current() {
    return selected_ >= 0 && selected_ < int(devices_.size()) ? &devices_[selected_] : nullptr;
}
QStringList ControllerInput::devices() const { QStringList list; for (const auto& d : devices_) list.append(d.name); return list; }
QString ControllerInput::status() const {
    if (!initialized_) return tr("SDL2 indisponível: %1").arg(error_);
    if (devices_.empty()) return tr("Nenhum controle conectado · SDL2");
    return tr("%1 · %2").arg(devices_[selected_].name, devices_[selected_].pad ? tr("mapeado SDL2") : tr("genérico / remapeável"));
}
void ControllerInput::clearHeld() { held_.clear(); axisDirection_ = 0; neutralRequired_ = true; }
void ControllerInput::setSelected(int index) {
    if (index < 0 || index >= int(devices_.size()) || selected_ == index) return;
    selected_ = index; cancelCapture(); clearHeld(); sentButtons_=0; sentAxes_={}; emit gameState(0,0,0,0,0); emit devicesChanged(); emit bindingsChanged();
}
int ControllerInput::faceLayout() const {
    const auto* d = selected_ >= 0 && selected_ < int(devices_.size()) ? &devices_[selected_] : nullptr;
    return d ? layoutDrafts_.value(d->guid, settings_.value("input/game/" + d->guid + "/layout", 0).toInt()) : 0;
}
void ControllerInput::setFaceLayout(int layout) {
    auto* d = current(); if (!d || layout < 0 || layout > 1 || layout == faceLayout()) return;
    auto map = gameMapping(*d);
    layoutDrafts_[d->guid] = layout;
    const auto defaults = gameDefaults(*d);
    for (const auto& action : {"A","B","X","Y"}) map[action] = defaults[action];
    gameDrafts_[d->guid] = map; emit bindingsChanged();
}
QMap<QString,int> ControllerInput::gameDefaults(const Device& d) const {
    const bool letters = layoutDrafts_.value(d.guid, settings_.value("input/game/" + d.guid + "/layout", 0).toInt()) == 1;
    QMap<QString,int> map{{"A",letters ? 0 : 1},{"B",letters ? 1 : 0},{"X",letters ? 2 : 3},{"Y",letters ? 3 : 2},
        {"LStick",10},{"RStick",11},{"L",9},{"R",10},{"ZL",100 + SDL_CONTROLLER_AXIS_TRIGGERLEFT},
        {"ZR",100 + SDL_CONTROLLER_AXIS_TRIGGERRIGHT},{"Plus",SDL_CONTROLLER_BUTTON_START},{"Minus",SDL_CONTROLLER_BUTTON_BACK},
        {"DLeft",SDL_CONTROLLER_BUTTON_DPAD_LEFT},{"DUp",SDL_CONTROLLER_BUTTON_DPAD_UP},
        {"DRight",SDL_CONTROLLER_BUTTON_DPAD_RIGHT},{"DDown",SDL_CONTROLLER_BUTTON_DPAD_DOWN},
        {"SL",-1},{"SR",-1},{"Home",-1},{"Capture",-1},
        {"LX",SDL_CONTROLLER_AXIS_LEFTX},{"LY",SDL_CONTROLLER_AXIS_LEFTY},
        {"RX",SDL_CONTROLLER_AXIS_RIGHTX},{"RY",SDL_CONTROLLER_AXIS_RIGHTY}};
    map["LStick"] = SDL_CONTROLLER_BUTTON_LEFTSTICK;
    map["RStick"] = SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    map["L"] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    map["R"] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    if (!d.pad) {
        for (int i = 0; i < 4; ++i) map[gameActions[i]] = letters ? i : (i ^ 1);
        map["ZL"] = -1; map["ZR"] = -1;
        for (int i = 0; i < 4; ++i) map[gameActions[12 + i]] = 200 + i;
        map["LX"] = 0; map["LY"] = 1; map["RX"] = 2; map["RY"] = 3;
    }
    return map;
}
QMap<QString,int> ControllerInput::gameMapping(const Device& d) const {
    if (gameDrafts_.contains(d.guid)) return gameDrafts_[d.guid];
    auto map = gameDefaults(d);
    for (auto it = map.begin(); it != map.end(); ++it)
        it.value() = settings_.value("input/game/" + d.guid + "/" + it.key(), it.value()).toInt();
    return map;
}
QMap<QString,int> ControllerInput::keyboardMapping() const {
    QMap<QString,int> map{{"A",Qt::Key_Z},{"B",Qt::Key_X},{"X",Qt::Key_C},{"Y",Qt::Key_V},
        {"L",Qt::Key_Q},{"R",Qt::Key_E},{"ZL",Qt::Key_1},{"ZR",Qt::Key_3},
        {"Plus",Qt::Key_Return},{"Minus",Qt::Key_Backspace},
        {"DLeft",Qt::Key_Left},{"DUp",Qt::Key_Up},{"DRight",Qt::Key_Right},{"DDown",Qt::Key_Down},
        {"LX+",Qt::Key_D},{"LX-",Qt::Key_A},{"LY+",Qt::Key_W},{"LY-",Qt::Key_S},
        {"RX+",Qt::Key_L},{"RX-",Qt::Key_J},{"RY+",Qt::Key_I},{"RY-",Qt::Key_K}};
    for (auto it = map.begin(); it != map.end(); ++it)
        it.value() = keyboardDraft_.value(it.key(), settings_.value("input/keyboard/" + it.key(), it.value()).toInt());
    return map;
}
bool ControllerInput::isGameKeyboardKey(int key) const {
    return key > 0 && keyboardMapping().values().contains(key);
}
QVariantList ControllerInput::gameBindings() const {
    QVariantList result;
    if (auto* d = const_cast<ControllerInput*>(this)->current()) {
        const auto map = gameMapping(*d);
        for (const auto& action : gameActions)
            result.append(QVariantMap{{"action","game/" + action},{"label",action},{"source",physicalName(map.value(action,-1),d->pad)}});
        for (const auto& action : axisActions)
            result.append(QVariantMap{{"action","axis/" + action},{"label",action},
                {"source",physicalName(map.value(action,-1) < 0 ? -1 : 100 + map.value(action),d->pad)}});
    }
    const auto keys = keyboardMapping();
    for (const auto& action : gameActions) if (keys.contains(action))
        result.append(QVariantMap{{"action","key/" + action},{"label","Teclado " + action},{"source",physicalName(1000 + keys[action],false)}});
    for (const auto& action : keyboardAxisActions)
        result.append(QVariantMap{{"action","key/" + action},{"label","Teclado " + action},{"source",physicalName(1000 + keys[action],false)}});
    return result;
}
QStringList ControllerInput::gameProfiles() const {
    settings_.beginGroup("input/game-profiles"); const auto names = settings_.childGroups(); settings_.endGroup(); return names;
}
void ControllerInput::captureGame(const QString& action) {
    const auto parts = action.split('/');
    if (parts.size() != 2 || !QStringList{"game","axis","key"}.contains(parts[0])) return;
    if ((parts[0] == "axis" && !axisActions.contains(parts[1])) ||
        (parts[0] == "game" && !gameActions.contains(parts[1])) ||
        (parts[0] == "key" && !gameActions.contains(parts[1]) && !keyboardAxisActions.contains(parts[1]))) return;
    if (parts[0] != "key" && !current()) return;
    capture_ = action; captureUntil_ = clock_.elapsed() + 10000; emit bindingsChanged();
}
void ControllerInput::resetGameBindings() {
    if (auto* d = current()) gameDrafts_[d->guid] = gameDefaults(*d);
    const QMap<QString,int> defaults{{"A",Qt::Key_Z},{"B",Qt::Key_X},{"X",Qt::Key_C},{"Y",Qt::Key_V},
        {"L",Qt::Key_Q},{"R",Qt::Key_E},{"ZL",Qt::Key_1},{"ZR",Qt::Key_3},
        {"Plus",Qt::Key_Return},{"Minus",Qt::Key_Backspace},{"DLeft",Qt::Key_Left},
        {"DUp",Qt::Key_Up},{"DRight",Qt::Key_Right},{"DDown",Qt::Key_Down},
        {"LX+",Qt::Key_D},{"LX-",Qt::Key_A},{"LY+",Qt::Key_W},{"LY-",Qt::Key_S},
        {"RX+",Qt::Key_L},{"RX-",Qt::Key_J},{"RY+",Qt::Key_I},{"RY-",Qt::Key_K}};
    keyboardDraft_ = defaults;
    emit bindingsChanged();
}
void ControllerInput::addDevice(int index) {
    const auto id = SDL_JoystickGetDeviceInstanceID(index);
    if (id < 0 || std::any_of(devices_.begin(), devices_.end(), [id](const auto& d) { return d.id == id; })) return;
    SDL_GameController* pad = SDL_IsGameController(index) ? SDL_GameControllerOpen(index) : nullptr;
    auto* joystick = pad ? SDL_GameControllerGetJoystick(pad) : SDL_JoystickOpen(index);
    if (!joystick) return;
    char guid[33]; SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), guid, sizeof(guid));
    const auto* name = SDL_JoystickName(joystick);
    devices_.push_back({id, pad, joystick, name ? QString::fromUtf8(name) : tr("Controle genérico"), QString::fromLatin1(guid)});
    if (selected_ < 0) selected_ = 0;
    clearHeld(); emit devicesChanged(); emit bindingsChanged();
}
void ControllerInput::removeDevice(SDL_JoystickID id) {
    auto it = std::find_if(devices_.begin(), devices_.end(), [id](const auto& d) { return d.id == id; });
    if (it == devices_.end()) return;
    const auto activeId = current() ? current()->id : -1;
    if (it->pad) SDL_GameControllerClose(it->pad); else SDL_JoystickClose(it->joystick);
    devices_.erase(it);
    selected_ = devices_.empty() ? -1 : 0;
    for (int i = 0; i < int(devices_.size()); ++i) if (devices_[i].id == activeId) selected_ = i;
    clearHeld(); cancelCapture(); sentButtons_=0; sentAxes_={}; emit gameState(0,0,0,0,0); emit devicesChanged(); emit bindingsChanged();
}
QMap<QString, int> ControllerInput::defaults(const Device& d) const {
    const auto type = d.pad ? SDL_GameControllerGetType(d.pad) : SDL_CONTROLLER_TYPE_UNKNOWN;
    const bool nintendo = type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO ||
        type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT ||
        type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT ||
        type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR;
    return {{"confirm", nintendo ? 1 : 0}, {"back", nintendo ? 0 : 1}, {"favorite", SDL_CONTROLLER_BUTTON_X},
            {"context", SDL_CONTROLLER_BUTTON_Y}, {"menu", d.pad ? int(SDL_CONTROLLER_BUTTON_START) : 7},
            {"gameMenu", d.pad ? int(SDL_CONTROLLER_BUTTON_GUIDE) : 6},
            {"previous", d.pad ? int(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) : 4},
            {"next", d.pad ? int(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) : 5}};
}
QMap<QString, int> ControllerInput::mapping(const Device& d) const {
    if (drafts_.contains(d.guid)) return drafts_[d.guid];
    auto map = defaults(d);
    for (auto it = map.begin(); it != map.end(); ++it) it.value() = settings_.value("input/" + d.guid + "/" + it.key(), it.value()).toInt();
    // Old builds assigned X to general settings and Y to game settings. Honor
    // the new standard actions for those existing profiles without writing to
    // the user's settings store just because the library was opened.
    if (settings_.contains("input/" + d.guid + "/manage")) {
        map["favorite"] = defaults(d).value("favorite");
        map["context"] = defaults(d).value("context");
        map.remove("manage");
    }
    return map;
}
QVariantList ControllerInput::bindings() const {
    QVariantList result;
    if (selected_ < 0) return result;
    const auto& d = devices_[selected_];
    const auto map = mapping(d);
    const QMap<QString, QString> labels{{"confirm", tr("Confirmar")}, {"back", tr("Voltar")},
        {"favorite", tr("Favoritar (X)")}, {"context", tr("Menu do jogo / botão direito (Y)")},
        {"menu", tr("Configurações do jogo (Start)")}, {"previous", tr("Foco anterior")},
        {"next", tr("Próximo foco")}, {"gameMenu",tr("Abrir interface durante o jogo")}};
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        const char* label = d.pad ? SDL_GameControllerGetStringForButton(SDL_GameControllerButton(it.value())) : nullptr;
        result.append(QVariantMap{{"action", it.key()}, {"label", labels.value(it.key())},
            {"button", label ? QString::fromUtf8(label).toUpper() : tr("Botão %1").arg(it.value())}});
    }
    return result;
}
void ControllerInput::capture(const QString& actionName) {
    auto* d = current();
    if (!d || !defaults(*d).contains(actionName)) return;
    capture_ = actionName; captureUntil_ = clock_.elapsed() + 10000; emit bindingsChanged();
}
bool ControllerInput::saveGameProfile(const QString& name) {
    auto* d = current();
    const auto trimmed = name.trimmed();
    static const QRegularExpression safeName("^[\\p{L}\\p{N} _.-]{1,40}$");
    if (!d || !safeName.match(trimmed).hasMatch()) { profileStatus_ = tr("Use um nome de 1 a 40 caracteres: letras, números, espaço, ponto, hífen ou sublinhado."); emit bindingsChanged(); return false; }
    const auto path = "input/game-profiles/" + trimmed + "/";
    const auto map = gameMapping(*d);
    for (auto it = map.begin(); it != map.end(); ++it) settings_.setValue(path + it.key(), it.value());
    settings_.setValue(path + "layout", faceLayout()); settings_.sync();
    const bool ok = settings_.status() == QSettings::NoError;
    profileStatus_ = ok ? tr("Perfil ‘%1’ salvo neste computador.").arg(trimmed) : tr("Não foi possível salvar o perfil.");
    emit bindingsChanged(); return ok;
}
bool ControllerInput::loadGameProfile(const QString& name) {
    auto* d = current(); if (!d || !gameProfiles().contains(name)) { profileStatus_ = tr("Conecte um controle e escolha um perfil disponível."); emit bindingsChanged(); return false; }
    const auto path = "input/game-profiles/" + name + "/";
    layoutDrafts_[d->guid] = settings_.value(path + "layout", 0).toInt();
    auto map = gameDefaults(*d);
    for (auto it = map.begin(); it != map.end(); ++it) it.value() = settings_.value(path + it.key(), it.value()).toInt();
    gameDrafts_[d->guid] = map; currentGameProfile_ = name; profileStatus_ = tr("Perfil ‘%1’ carregado como rascunho. Clique Aplicar para salvar o mapeamento.").arg(name); clearHeld(); emit bindingsChanged(); return true;
}
bool ControllerInput::deleteGameProfile(const QString& name) {
    if (!gameProfiles().contains(name)) { profileStatus_ = tr("Escolha um perfil disponível."); emit bindingsChanged(); return false; }
    settings_.beginGroup("input/game-profiles/" + name);
    settings_.remove("");
    settings_.endGroup();
    settings_.sync();
    const bool ok = settings_.status() == QSettings::NoError;
    if (currentGameProfile_ == name) currentGameProfile_.clear();
    profileStatus_ = ok ? tr("Perfil ‘%1’ removido.").arg(name) : tr("Não foi possível remover o perfil.");
    emit bindingsChanged();
    return ok;
}
void ControllerInput::cancelCapture() { if (!capture_.isEmpty()) { capture_.clear(); emit bindingsChanged(); } }
void ControllerInput::resetBindings() { if (auto* d = current()) { drafts_[d->guid] = defaults(*d); emit bindingsChanged(); } }
void ControllerInput::applyBindings() {
    cancelCapture();
    for (auto it = drafts_.cbegin(); it != drafts_.cend(); ++it)
        for (auto m = it.value().cbegin(); m != it.value().cend(); ++m) settings_.setValue("input/" + it.key() + "/" + m.key(), m.value());
    for (auto it = gameDrafts_.cbegin(); it != gameDrafts_.cend(); ++it)
        for (auto m = it.value().cbegin(); m != it.value().cend(); ++m) settings_.setValue("input/game/" + it.key() + "/" + m.key(), m.value());
    for (auto it = keyboardDraft_.cbegin(); it != keyboardDraft_.cend(); ++it) settings_.setValue("input/keyboard/" + it.key(), it.value());
    for (auto it = layoutDrafts_.cbegin(); it != layoutDrafts_.cend(); ++it) settings_.setValue("input/game/" + it.key() + "/layout", it.value());
    settings_.sync(); drafts_.clear(); gameDrafts_.clear(); keyboardDraft_.clear(); layoutDrafts_.clear(); clearHeld(); emit bindingsChanged();
}
void ControllerInput::cancelBindings() { cancelCapture(); drafts_.clear(); gameDrafts_.clear(); keyboardDraft_.clear(); layoutDrafts_.clear(); clearHeld(); emit bindingsChanged(); }
void ControllerInput::button(SDL_JoystickID id, int value) {
    auto* d = current();
    if (!d || d->id != id || !foreground()) return;
    auto map = mapping(*d);
    if (capture_.startsWith("game/")) {
        if (value == map.value("gameMenu")) return;
        auto game = gameMapping(*d); game[capture_.mid(5)] = value; gameDrafts_[d->guid] = game;
        cancelCapture(); clearHeld(); emit bindingsChanged(); return;
    }
    if (value == map.value("gameMenu")) { emit openGameMenu(); return; }
    if (gameInput_) return;
    if (capture_.startsWith("axis/") || capture_.startsWith("key/")) return;
    if (!capture_.isEmpty()) {
        const int old = map[capture_];
        for (auto it = map.begin(); it != map.end(); ++it) if (it.key() != capture_ && it.value() == value) it.value() = old;
        map[capture_] = value; drafts_[d->guid] = map;
        cancelCapture(); clearHeld(); emit bindingsChanged(); return;
    }
    if (!enabled() || neutralRequired_) return;
    for (auto it = map.cbegin(); it != map.cend(); ++it) if (it.value() == value) { emit action(it.key()); break; }
}
QString ControllerInput::direction(const Device& d) {
    if (d.pad) {
        if (SDL_GameControllerGetButton(d.pad, SDL_CONTROLLER_BUTTON_DPAD_UP)) return "up";
        if (SDL_GameControllerGetButton(d.pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) return "down";
        if (SDL_GameControllerGetButton(d.pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) return "left";
        if (SDL_GameControllerGetButton(d.pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) return "right";
    } else if (SDL_JoystickNumHats(d.joystick) > 0) {
        const auto hat = SDL_JoystickGetHat(d.joystick, 0);
        if (hat & SDL_HAT_UP) return "up"; if (hat & SDL_HAT_DOWN) return "down";
        if (hat & SDL_HAT_LEFT) return "left"; if (hat & SDL_HAT_RIGHT) return "right";
    }
    const int x = d.pad ? SDL_GameControllerGetAxis(d.pad, SDL_CONTROLLER_AXIS_LEFTX) : SDL_JoystickNumAxes(d.joystick) > 0 ? SDL_JoystickGetAxis(d.joystick, 0) : 0;
    const int y = d.pad ? SDL_GameControllerGetAxis(d.pad, SDL_CONTROLLER_AXIS_LEFTY) : SDL_JoystickNumAxes(d.joystick) > 1 ? SDL_JoystickGetAxis(d.joystick, 1) : 0;
    const int threshold = axisDirection_ ? 11500 : 18000;
    if (std::max(std::abs(x), std::abs(y)) < threshold) { axisDirection_ = 0; return {}; }
    axisDirection_ = 1;
    return std::abs(x) > std::abs(y) ? (x < 0 ? "left" : "right") : (y < 0 ? "up" : "down");
}
void ControllerInput::poll() {
    if (!initialized_) return;
    if (!foreground()) { clearHeld(); cancelCapture(); }
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_JOYDEVICEADDED) addDevice(event.jdevice.which);
        else if (event.type == SDL_JOYDEVICEREMOVED) removeDevice(event.jdevice.which);
        else if (event.type == SDL_CONTROLLERBUTTONDOWN) button(event.cbutton.which, event.cbutton.button);
        else if (event.type == SDL_JOYBUTTONDOWN) {
            auto* d = current(); if (d && !d->pad) button(event.jbutton.which, event.jbutton.button);
        }
        else if (event.type == SDL_CONTROLLERAXISMOTION && current() && current()->id == event.caxis.which
                 && std::abs(int(event.caxis.value)) > 20000 && !capture_.isEmpty()) {
            auto* d = current(); auto map = gameMapping(*d);
            if (capture_.startsWith("axis/")) map[capture_.mid(5)] = event.caxis.axis;
            else if (capture_.startsWith("game/")) map[capture_.mid(5)] = 100 + event.caxis.axis;
            else continue;
            gameDrafts_[d->guid] = map; cancelCapture(); clearHeld(); emit bindingsChanged();
        }
        else if (event.type == SDL_JOYAXISMOTION && current() && !current()->pad && current()->id == event.jaxis.which
                 && std::abs(int(event.jaxis.value)) > 20000 && !capture_.isEmpty()) {
            auto* d = current(); auto map = gameMapping(*d);
            if (capture_.startsWith("axis/")) map[capture_.mid(5)] = event.jaxis.axis;
            else if (capture_.startsWith("game/")) map[capture_.mid(5)] = 100 + event.jaxis.axis;
            else continue;
            gameDrafts_[d->guid] = map; cancelCapture(); clearHeld(); emit bindingsChanged();
        }
        else if (event.type == SDL_JOYHATMOTION && current() && current()->id == event.jhat.which
                 && capture_.startsWith("game/") && event.jhat.value != SDL_HAT_CENTERED) {
            int direction = event.jhat.value & SDL_HAT_LEFT ? 0 : event.jhat.value & SDL_HAT_UP ? 1 :
                event.jhat.value & SDL_HAT_RIGHT ? 2 : 3;
            auto* d = current(); auto map = gameMapping(*d); map[capture_.mid(5)] = 200 + direction;
            gameDrafts_[d->guid] = map; cancelCapture(); clearHeld(); emit bindingsChanged();
        }
    }
    if (!capture_.isEmpty() && clock_.elapsed() > captureUntil_) cancelCapture();
    if (gameInput_) { publishGameState(); return; }
    auto* d = current();
    if (!d || !enabled() || !foreground() || !capture_.isEmpty()) { held_.clear(); return; }
    const auto dir = direction(*d);
    if (neutralRequired_) {
        bool pressed = false;
        for (int i = 0; i < SDL_JoystickNumButtons(d->joystick); ++i) pressed |= SDL_JoystickGetButton(d->joystick, i) != 0;
        if (dir.isEmpty() && !pressed) neutralRequired_ = false;
        return;
    }
    const auto now = clock_.elapsed();
    if (dir != held_) { held_ = dir; repeatAt_ = now + 350; if (!dir.isEmpty()) emit action(dir); }
    else if (!dir.isEmpty() && now >= repeatAt_) { repeatAt_ = now + 110; emit action(dir); }
}
void ControllerInput::sendKey(const QString& name) {
    const QMap<QString, int> keys{{"up", Qt::Key_Up}, {"down", Qt::Key_Down}, {"left", Qt::Key_Left},
        {"right", Qt::Key_Right}, {"confirm", Qt::Key_Space}, {"back", Qt::Key_Escape}};
    if (!keys.contains(name) || !QGuiApplication::focusWindow()) return;
    QKeyEvent press(QEvent::KeyPress, keys[name], Qt::NoModifier), release(QEvent::KeyRelease, keys[name], Qt::NoModifier);
    QCoreApplication::sendEvent(QGuiApplication::focusWindow(), &press);
    QCoreApplication::sendEvent(QGuiApplication::focusWindow(), &release);
}
void ControllerInput::moveFocus(bool backwards) {
    if (!QGuiApplication::focusWindow()) return;
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Tab, backwards ? Qt::ShiftModifier : Qt::NoModifier);
    QCoreApplication::sendEvent(QGuiApplication::focusWindow(), &press);
}
