import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: root
    objectName: "quickMenu"
    property bool open: false
    property int selected: 0
    property int rememberedVolume: 100
    property int sessionFilter: 1
    property int sessionAntiAliasing: 0
    readonly property int sessionFsrSharpness: {
        const override = preferences.gameOverride(emulation.gamePath, "graphics/fsrSharpness")
        return override >= 0 ? override : Number(preferences.values["graphics/fsrSharpness"])
    }
    readonly property var filters: ["Nearest", "Bilinear", "Bicubic", "Gaussian", "Lanczos", "ScaleForce", "FSR", "Area", "ZeroTangent", "B-Spline", "Mitchell", "Spline1", "MMPX", "SGSR", "SGSR Edge"]
    readonly property var aaModes: ["Desligado", "FXAA", "SMAA"]
    readonly property var entries: ["Retomar jogo", "Filtro de imagem", "Anti-aliasing", "Volume", "Áudio", "Velocidade da emulação", "Pausar emulação", "Opções gráficas", "Voltar à biblioteca", "Encerrar jogo", window.performanceOverlayEnabled() ? "Ocultar métricas de FPS" : "Mostrar métricas de FPS", "Capturar screenshot", "Ocultar cursor com controle"]
    signal resumeRequested()
    signal graphicsRequested()
    signal libraryRequested()
    signal stopRequested()
    signal screenshotRequested()
    width: Math.min(Math.max(parent.width * 0.32, 330), Math.min(460, parent.width))
    visible: opacity > 0 && !window.screenshotPending
    opacity: open ? 1 : 0
    x: open ? 0 : width
    Behavior on opacity { NumberAnimation { duration: 160 } }
    Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

    function focusMenu() { selected = 0; forceActiveFocus() }
    function syncGameSettings() {
        if (!emulation.gamePath.length) return
        const filter = preferences.gameOverride(emulation.gamePath, "graphics/scalingFilter")
        const aa = preferences.gameOverride(emulation.gamePath, "graphics/antiAliasing")
        sessionFilter = filter >= 0 ? filter : Number(preferences.values["graphics/scalingFilter"])
        sessionAntiAliasing = aa >= 0 ? aa : Number(preferences.values["graphics/antiAliasing"])
    }
    function enabledAt(index) { return index !== 7 || emulation.state === "Running" || emulation.state === "Paused" }
    function move(delta) { let i = selected; do { i = (i + delta + entries.length) % entries.length } while (!enabledAt(i)); selected = i }
    function setHideCursor(value) {
        preferences.setValue("ui/hideCursorWithController", value)
        preferences.apply()
    }
    function activate() {
        if (selected === 0) resumeRequested()
        else if (selected === 1 || selected === 2) adjust(selected, 1)
        else if (selected === 4) { if (emulation.volume === 0) emulation.setVolume(rememberedVolume); else { rememberedVolume = emulation.volume; emulation.setVolume(0) } }
        else if (selected === 6) { if (emulation.state === "Paused") emulation.resumeGame(); else if (emulation.state === "Running") emulation.pauseGame() }
        else if (selected === 7) graphicsRequested()
        else if (selected === 8) libraryRequested()
        else if (selected === 9) stopRequested()
        else if (selected === 10) window.togglePerformanceOverlay()
        else if (selected === 11) screenshotRequested()
        else if (selected === 12) setHideCursor(!preferences.values["ui/hideCursorWithController"])
    }
    function adjust(index, direction) {
        if (index === 1 || index === 2) {
            const isFilter = index === 1
            const key = isFilter ? "graphics/scalingFilter" : "graphics/antiAliasing"
            const count = isFilter ? filters.length : aaModes.length
            const current = isFilter ? sessionFilter : sessionAntiAliasing
            const next = (current + direction + count) % count
            if (!preferences.setGameOverride(emulation.gamePath, key, next)) return
            if (isFilter) sessionFilter = next; else sessionAntiAliasing = next
            if (!emulation.setPresentationSettings(sessionFilter, sessionAntiAliasing, sessionFsrSharpness))
                console.warn("Não foi possível aplicar as opções gráficas à sessão atual")
        } else if (index === 3) emulation.setVolume(Math.max(0, Math.min(100, emulation.volume + direction * 5)))
        else if (index === 5) emulation.setSpeedLimit(Math.max(50, Math.min(200, emulation.speedLimit + direction * 5)))
    }
    Component.onCompleted: syncGameSettings()
    onOpenChanged: if (open) syncGameSettings()
    Connections { target: emulation; function onStateChanged() { root.syncGameSettings() } }
    Connections { target: preferences; function onValuesChanged() { root.syncGameSettings() } }
    function action(name) { if (name === "up" || name === "previous") move(-1); else if (name === "down" || name === "next") move(1); else if (name === "left" || name === "right") adjust(selected, name === "left" ? -1 : 1); else if (name === "confirm") activate(); else if (name === "back" || name === "gameMenu") resumeRequested() }
    Keys.onPressed: function(event) { if (event.key === Qt.Key_Up) move(-1); else if (event.key === Qt.Key_Down) move(1); else if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) adjust(selected, event.key === Qt.Key_Left ? -1 : 1); else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) activate(); else return; event.accepted = true }

    Rectangle { anchors.fill: parent; color: "#ee100e16"; border.color: "#563778"; radius: 18 }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 22; spacing: 8
        RowLayout {
            Layout.fillWidth: true; Layout.bottomMargin: 8
            ColumnLayout { Layout.fillWidth: true; spacing: 2
                Label { text: "Menu do jogo"; color: "#f6f0ff"; font.pixelSize: 20; font.bold: true }
                Label { text: window.currentGameTitle; color: "#b7a7c9"; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
            }
            Rectangle { width: 42; height: 42; radius: 14; color: "#2b2038"
                Label { anchors.centerIn: parent; text: "✦"; color: "#d9adff"; font.pixelSize: 20 }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#45354f"; Layout.bottomMargin: 6 }
        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            ColumnLayout {
                width: parent.width; spacing: 7
                Repeater {
                    model: root.entries
                    Rectangle {
                id: item
                required property string modelData
                required property int index
                property bool sliderRow: index === 3 || index === 5
                Layout.fillWidth: true
                implicitHeight: sliderRow ? 76 : 48
                radius: 13
                color: root.selected === index ? "#472b62" : "#17151d"
                border.width: root.selected === index ? 1.5 : 1
                border.color: root.selected === index ? "#b875f2" : "#2b2532"
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 13; anchors.rightMargin: 12; spacing: 12
                    Rectangle { width: 30; height: 30; radius: 10; color: root.selected === index ? "#8544c9" : "#292332"
                        Label { anchors.centerIn: parent; text: ["▶", "◈", "✧", "♪", "♫", "◷", "Ⅱ", "⚙", "⌂", "■", "⌁", "▣", "◉"][index]; color: "#f4eaff"; font.pixelSize: 14 }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: sliderRow ? 0 : 2
                        Label { Layout.fillWidth: true; text: modelData; color: root.enabledAt(index) ? "#f5effb" : "#807987"; font.pixelSize: 13; font.bold: root.selected === index; elide: Text.ElideRight }
                        Label {
                            visible: index === 1 || index === 2 || index === 12
                            Layout.fillWidth: true; color: "#b5a4c7"; font.pixelSize: 11; elide: Text.ElideRight
                            text: index === 1 ? root.filters[root.sessionFilter] : index === 2 ? root.aaModes[root.sessionAntiAliasing] : (preferences.values["ui/hideCursorWithController"] ? "Ativado · enquanto joga com controle" : "Desativado")
                        }
                        Slider {
                            id: valueSlider
                            objectName: index === 5 ? "quickMenuSpeedSlider" : index === 3 ? "quickMenuVolumeSlider" : ""
                            visible: index === 3 || index === 5
                            Layout.fillWidth: true
                            from: index === 3 ? 0 : 50; to: index === 3 ? 100 : 200; stepSize: 5
                            value: index === 3 ? emulation.volume : emulation.speedLimit
                            Accessible.name: index === 3 ? "Volume do jogo" : "Velocidade da emulação"
                            onMoved: if (index === 3) emulation.setVolume(Math.round(value)); else emulation.setSpeedLimit(Math.round(value))
                        }
                    }
                    Label {
                        visible: index === 3 || index === 5
                        text: (index === 3 ? emulation.volume : emulation.speedLimit) + "%"
                        color: "#d9adff"; font.pixelSize: 12; font.bold: true
                    }
                    Label { visible: !sliderRow; text: "›"; color: root.selected === index ? "#e4c4ff" : "#75677f"; font.pixelSize: 20 }
                }
                MouseArea {
                    anchors.fill: parent; enabled: !item.sliderRow && root.enabledAt(index)
                    onClicked: { root.selected = index; root.activate() }
                }
                    }
                }
            }
        }
        RowLayout { Layout.fillWidth: true
            Label { text: "B  Fechar"; color: "#c3b4d1"; font.pixelSize: 11 }
            Item { Layout.fillWidth: true }
            Label { text: "A  Selecionar"; color: "#c3b4d1"; font.pixelSize: 11 }
        }
    }
}
