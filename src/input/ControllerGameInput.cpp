#include "ControllerInput.h"
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <algorithm>
#include <cmath>

void ControllerInput::setGameInput(bool value) {
    if (gameInput_ == value) return;
    gameInput_ = value; gameKeys_.clear(); clearHeld();
    gameNeutralDevices_.clear();
    if (value) for (const auto& device : devices_) gameNeutralDevices_.insert(device.id);
    sentButtons_=0; sentAxes_={}; sentPlayer2Buttons_=0; sentPlayer2Axes_={};
    emit gameState(0,0,0,0,0); emit gamePlayerState(0,0,0,0,0,0);
    emit gamePlayerState(1,0,0,0,0,0); emit gameInputChanged();
}
void ControllerInput::setGameShortcutMode(bool value) {
    if (gameShortcutMode_ == value) return;
    gameShortcutMode_ = value;
    emit gameShortcutModeChanged();
}

bool ControllerInput::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::ApplicationDeactivate) {
        gameKeys_.clear();
        if (gameInput_) {
            sentButtons_=0; sentAxes_={}; sentPlayer2Buttons_=0; sentPlayer2Axes_={};
            gameNeutralDevices_.clear();
            for (const auto& device : devices_) gameNeutralDevices_.insert(device.id);
            emit gameState(0,0,0,0,0); emit gamePlayerState(0,0,0,0,0,0);
            emit gamePlayerState(1,0,0,0,0,0);
        }
    }
    if (!foreground() || (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)) return QObject::eventFilter(object,event);
    auto* key = static_cast<QKeyEvent*>(event);
    if (capture_.startsWith("key/")) {
        if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) {
            if (key->key() == Qt::Key_Escape) cancelCapture();
            else { keyboardDraft_[capture_.mid(4)] = key->key(); cancelCapture(); emit bindingsChanged(); }
        }
        return true;
    }
    if (gameShortcutMode_ && key->key() == Qt::Key_F1 + settings_.value("input/gameMenuKey",0).toInt()) {
        if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) emit openGameMenu();
        return true;
    }
    if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) {
        const auto mapping = keyboardMapping();
        const auto mapped = [&](int code) { return mapping.values().contains(code); };
        const auto matches = [&](const QString& sequence) {
            return QKeySequence::fromString(sequence) == QKeySequence(key->key() | int(key->modifiers()));
        };
        const auto conflict = [&](const QString& sequence, int terminalKey) {
            return mapped(terminalKey) || (sequence.contains("Ctrl+") && mapped(Qt::Key_Control)) ||
                (sequence.contains("Shift+") && mapped(Qt::Key_Shift));
        };
        if (key->key() == Qt::Key_F11 && key->modifiers() == Qt::NoModifier && (!gameInput_ || !mapped(Qt::Key_F11))) {
            emit keyboardShortcut(QStringLiteral("fullscreen")); return true;
        }
        if (gameShortcutMode_) {
            const QString screenshot = settings_.value("input/screenshotShortcut", QStringLiteral("F12")).toString();
            const struct { const char* sequence; const char* action; int key; } shortcuts[]{
                {"Ctrl+Shift+F9", "pause-resume", Qt::Key_F9},
                {"Ctrl+Shift+F10", "end-game", Qt::Key_F10},
                {"Ctrl+Shift+F8", "performance-overlay", Qt::Key_F8}};
            if (matches(screenshot) && !conflict(screenshot, key->key())) { emit keyboardShortcut(QStringLiteral("screenshot")); return true; }
            for (const auto& shortcut : shortcuts) {
                const auto sequence = QString::fromLatin1(shortcut.sequence);
                if (matches(sequence) && !conflict(sequence, shortcut.key)) {
                    emit keyboardShortcut(QString::fromLatin1(shortcut.action)); return true;
                }
            }
        }
    }
    if (!gameInput_) return QObject::eventFilter(object,event);
    QSet<int> accepted;
    for (const auto value : keyboardMapping()) if (value > 0) accepted.insert(value);
    if (!accepted.contains(key->key())) return QObject::eventFilter(object,event);
    if (!key->isAutoRepeat()) {
        if (event->type()==QEvent::KeyPress) gameKeys_.insert(key->key()); else gameKeys_.remove(key->key());
        publishGameState();
    }
    return true;
}

void ControllerInput::publishGameState() {
    const QString names[]{"A","B","X","Y","LStick","RStick","L","R","ZL","ZR","Plus","Minus",
        "DLeft","DUp","DRight","DDown","SL","SR","Home","Capture"};
    const QString axesNames[]{"LX","LY","RX","RY"};
    auto* first = current();
    Device* second = nullptr;
    for (auto& device : devices_) if (&device != first) { second = &device; break; }

    for (int player = 0; player < 2; ++player) {
        quint32 buttons = 0; std::array<float,4> axes{};
        auto* d = player == 0 ? first : second;
        if (gameInput_ && foreground()) {
            if (player == 0) {
                const auto keys = keyboardMapping();
                for (int i=0;i<20;++i) if (keys.value(names[i],0)>0 && gameKeys_.contains(keys[names[i]])) buttons|=1u<<i;
                for (int i=0;i<4;++i) axes[i]=float(gameKeys_.contains(keys.value(axesNames[i]+"+",0)))
                    -float(gameKeys_.contains(keys.value(axesNames[i]+"-",0)));
            }
            if (d) {
                auto pressed=[&](int b) { return b>=0 && b<SDL_JoystickNumButtons(d->joystick) && SDL_JoystickGetButton(d->joystick,b)!=0; };
                bool any=false; for(int i=0;i<SDL_JoystickNumButtons(d->joystick);++i) any |= pressed(i);
                for(int i=0;i<std::min(4,SDL_JoystickNumAxes(d->joystick));++i) any |= std::abs(SDL_JoystickGetAxis(d->joystick,i))>8000;
                if(d->pad) any |= SDL_GameControllerGetAxis(d->pad,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>8000 || SDL_GameControllerGetAxis(d->pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>8000;
                if (gameNeutralDevices_.contains(d->id)) {
                    if (!any && direction(*d).isEmpty()) gameNeutralDevices_.remove(d->id);
                } else {
                    const auto map=gameMapping(*d); const int guide=mapping(*d).value("gameMenu",-1);
                    auto active=[&](int code) {
                        if(code<0 || code==guide) return false;
                        if(code>=200) { if(SDL_JoystickNumHats(d->joystick)<1) return false;
                            const int bits[]{SDL_HAT_LEFT,SDL_HAT_UP,SDL_HAT_RIGHT,SDL_HAT_DOWN};
                            return (SDL_JoystickGetHat(d->joystick,0)&bits[code-200])!=0; }
                        if(code>=100) { const int axis=code-100;
                            return axis<SDL_JoystickNumAxes(d->joystick) &&
                                (d->pad ? SDL_GameControllerGetAxis(d->pad,SDL_GameControllerAxis(axis)) : SDL_JoystickGetAxis(d->joystick,axis))>16000; }
                        return d->pad ? SDL_GameControllerGetButton(d->pad,SDL_GameControllerButton(code))!=0 : pressed(code);
                    };
                    for(int i=0;i<20;++i) if(active(map.value(names[i],-1))) buttons|=1u<<i;
                    for(int i=0;i<4;++i) {
                        const int source=map.value(axesNames[i],-1);
                        if(source<0 || source>=SDL_JoystickNumAxes(d->joystick)) continue;
                        const int axis=d->pad ? SDL_GameControllerGetAxis(d->pad,SDL_GameControllerAxis(source)) : SDL_JoystickGetAxis(d->joystick,source);
                        if(std::abs(axis)>8000) axes[i]=std::clamp(float(axis)/32767.0f,-1.0f,1.0f)*(i%2 ? -1.0f:1.0f);
                    }
                }
            }
        }
        if (player == 0) {
            if (buttons != sentButtons_ || axes != sentAxes_) {
                sentButtons_=buttons; sentAxes_=axes;
                emit gameState(buttons,axes[0],axes[1],axes[2],axes[3]);
                emit gamePlayerState(0,buttons,axes[0],axes[1],axes[2],axes[3]);
            }
        } else if (buttons != sentPlayer2Buttons_ || axes != sentPlayer2Axes_) {
            sentPlayer2Buttons_=buttons; sentPlayer2Axes_=axes;
            emit gamePlayerState(1,buttons,axes[0],axes[1],axes[2],axes[3]);
        }
    }
}
