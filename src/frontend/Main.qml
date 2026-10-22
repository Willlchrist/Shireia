import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Shireia 1.0
import "components"
import "pages"

ApplicationWindow {
    id: window
    objectName: "mainWindow"
    width: 1500; height: 940; minimumWidth: 800; minimumHeight: 560
    visible: true; color: "#0b0b10"
    title: "Shireia Next — " + (gameView ? "Jogando" : ["Biblioteca", "Configurações", "Perfis", "Diagnósticos"][page])
    property int page: 0
    property int category: 0
    property bool gameView: false
    property bool quickMenuOpen: false
    property bool startupBypass: false
    property bool startupComplete: false
    property string currentGameTitle: "Jogo"
    property bool screenshotPending: false
    property bool screenshotNoticeVisible: false
    property bool overlayShortcutOverrideActive: false
    property bool overlayShortcutOverride: false
    property var intervalHistory: []
    readonly property bool startupReady: !startupEnabled ||
        (firstRunSetup.completed && (startupComplete || startupBypass))
    readonly property var navigation: [
        {text:"Biblioteca", icon:"library"}, {text:"Configurações", icon:"settings"},
        {text:"Perfis", icon:"profiles"}, {text:"Diagnósticos", icon:"logs"}]
    palette.window: "#101016"; palette.windowText: "#ece9f3"; palette.text: "#ece9f3"
    palette.base: "#20202a"; palette.button: "#292533"; palette.buttonText: "#ece9f3"
    palette.highlight: "#8b49dc"; palette.highlightedText: "#ffffff"; palette.placeholderText: "#a69eaf"
    font.family: "Segoe UI"; font.pixelSize: 14
    onPageChanged: {
        if (page === 1) { preferences.refresh(); settingsPage.feedback = "" } else settingsPage.cancel()
        if (startupEnabled && page === 2 && emulation.state === "Ready") emulation.stopGame()
        else if (startupEnabled && page !== 2) warmCore()
    }
    onActiveFocusItemChanged: if (page === 1 && !gameView) Qt.callLater(function() { settingsPage.reveal(window.activeFocusItem) })
    Component.onCompleted: if (preferences.values["ui/fullscreen"]) showFullScreen()
    function sidebarFocus() { sideBar.focusCurrent() }
    function warmCore() {
        if (startupEnabled && firstRunSetup.completed && startupReady && page !== 2 && !libraryResources.busy && !emulation.active && emulation.coreAvailable && emulation.state !== "Stopping") emulation.startCore()
    }
    function focusPageContent() {
        if (page === 0) library.focusLibrary()
        else if (page === 1) settingsPage.focusContent()
        else if (page === 2) profilesPage.focusContent()
        else diagnosticsPage.focusContent()
    }
    function goSettings(index) { if (!gameView) { category = index; page = 1 } }
    function enterGame() { gameView = true; quickMenuOpen = false; gameFocus.forceActiveFocus() }
    function captureScreenshot() {
        if (!emulation.gameLoaded || emulation.state !== "Running" || gpuSurface.framesPresented < 1 || gpuSurface.error.length || screenshotPending) {
            return
        }
        screenshotNoticeVisible = false
        screenshotNoticeTimer.stop()
        screenshotPending = true
        screenshotTimer.start()
    }
    function showScreenshotNotice() {
        screenshotNoticeVisible = true
        screenshotNoticeTimer.restart()
    }
    Timer {
        id: screenshotNoticeTimer
        objectName: "screenshotNoticeTimer"
        interval: 4000
        repeat: false
        onTriggered: window.screenshotNoticeVisible = false
    }
    Timer {
        id: screenshotTimer
        interval: 80
        repeat: false
        onTriggered: {
            const image = gpuSurface.grabPresentedImage()
            screenshotPending = false
            if (!image || image.width <= 0 || image.height <= 0) qualityTools.reportScreenshotError("a superfície do jogo não retornou um frame válido")
            else qualityTools.saveScreenshot(image, currentGameTitle)
            window.showScreenshotNotice()
        }
    }
    function toggleGamePause() {
        if (emulation.state === "Running") emulation.pauseGame()
        else if (emulation.state === "Paused") emulation.resumeGame()
    }
    function performanceOverlayEnabled() {
        return overlayShortcutOverrideActive ? overlayShortcutOverride : preferences.values["ui/showFrameRate"]
    }
    function togglePerformanceOverlay() {
        overlayShortcutOverride = !performanceOverlayEnabled()
        overlayShortcutOverrideActive = true
    }
    function leaveGame() { quickMenuOpen = false; gameView = false; page = 0; library.focusLibrary() }
    function toggleQuickMenu() { if (!gameView) return; quickMenuOpen = !quickMenuOpen; if (quickMenuOpen) quickMenu.focusMenu(); else gameFocus.forceActiveFocus() }
    function back() {
        if (controllers.capturing) { controllers.cancelCapture(); return }
        if (gameView) { toggleQuickMenu(); return }
        if (page === 1) { settingsPage.cancel(); page = 0; library.focusLibrary() }
        else if (page !== 0) { page = 0; library.focusLibrary() }
        else sidebarFocus()
    }
    function action(name) {
        if (gameView && quickMenuOpen) { quickMenu.action(name); return }
        if (controllers.popupActive) { if (name === "next" || name === "previous") controllers.moveFocus(name === "previous"); else controllers.sendKey(name); return }
        if (name === "back") { back(); return }
        if (name === "confirm" && page === 0) { if (library.gameGridFocused) library.startSelected(); else controllers.sendKey("confirm"); return }
        if (name === "favorite" && page === 0) { library.favorite(); return }
        if (name === "menu" && page === 0) { library.openGameSettings(true); return }
        if (name === "context" && page === 0) { library.openContextMenuFromController(); return }
        if (name === "previous" || name === "next") { controllers.moveFocus(name === "previous"); return }
        controllers.sendKey(name)
    }
    Binding { target: controllers; property: "gameInput"; value: window.gameView && !window.quickMenuOpen && emulation.state === "Running" && !controllers.popupActive }
    Binding { target: controllers; property: "gameShortcutMode"; value: window.gameView && emulation.active }
    Connections { target: controllers
        function onAction(name) { window.action(name) }
        function onOpenGameMenu() { window.toggleQuickMenu() }
        function onKeyboardShortcut(action) {
            if (action === "fullscreen") { if (window.visibility === Window.FullScreen) window.showNormal(); else window.showFullScreen(); return }
            if (!window.gameView) return
            if (action === "screenshot") Qt.callLater(window.captureScreenshot)
            else if (action === "pause-resume") window.toggleGamePause()
            else if (action === "end-game") emulation.stopGame()
            else if (action === "performance-overlay") window.togglePerformanceOverlay()
        }
    }
    Connections { target: emulation
        function onFailed() {
            compatibility.recordSessionError("core", emulation.message)
            compatibility.finishSession("core-failure", emulation.message)
            window.leaveGame(); coreNotice.message = emulation.message; coreNotice.open()
        }
        function onStateChanged() {
            if (emulation.state === "Ready") window.startupComplete = true
            if (emulation.state === "Starting") coreNotice.close()
            if (["Loading", "AwaitingFrame", "Running", "Paused"].indexOf(emulation.state) >= 0)
                compatibility.updateSessionState(emulation.state)
            if (emulation.state === "Stopped") compatibility.finishSession("stopped", emulation.message)
            if (!emulation.active && window.gameView) window.leaveGame()
            if (emulation.state === "Stopped") window.warmCore()
        }
        function onGraphicsErrorChanged() {
            if (emulation.graphicsError.length) compatibility.recordSessionError("graphics", emulation.graphicsError)
        }
    }
    Connections { target: gpuSurface; function onErrorChanged() { if (gpuSurface.error.length) emulation.recordGraphicsError(gpuSurface.error) } }
    Connections { target: libraryResources; function onStatusChanged() { window.warmCore() } }
    Connections { target: firstRunSetup; function onChanged() { window.warmCore() } }
    Connections { target: preferences; function onApplied() { emulation.setVolume(preferences.values["audio/volume"]); window.overlayShortcutOverrideActive = false } }
    Popup { id: coreNotice; property string message: ""; x: (window.width-width)/2; y: 20; width: Math.min(640,window.width-60); padding: 18
        background: Rectangle { radius: 5; color: "#282030"; border.color: "#8b49dc" }
        contentItem: Label { text: coreNotice.message; color: "#f1ddff"; wrapMode: Text.WordWrap }
        Timer { interval: 8000; running: coreNotice.opened; onTriggered: coreNotice.close() }
    }
    Shortcut { sequence: "Ctrl+,"; enabled: !window.gameView; onActivated: window.goSettings(0) }
    Shortcut { sequence: "Escape"; enabled: window.gameView && !controllers.gameInput; onActivated: window.back() }
    Shortcut { sequence: "Escape"; enabled: !window.gameView && !controllers.popupActive; onActivated: window.back() }
    RowLayout { anchors.fill: parent; spacing: 0; visible: !window.gameView && (window.startupReady || window.startupBypass)
        MainSidebar { id: sideBar; Layout.preferredWidth: 208; Layout.fillHeight: true; currentPage: window.page; navigation: window.navigation
            onNavigate: function(index) { window.page = index; window.focusPageContent() }
            onContentRequested: window.focusPageContent() }
        StackLayout { Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: Math.min(24,window.width*0.018); currentIndex: window.page
            LibraryPage { id: library; shell: window; onRequestSidebar: window.sidebarFocus(); onRequestSettings: window.goSettings(0); onRequestSession: window.enterGame() }
            SettingsPage {
                id: settingsPage; category: window.category
                onCategoryRequested: function(index) { window.category = index }
                onCloseRequested: { window.page = 0; library.focusLibrary() }
            }
            FoundationPage { id: profilesPage; page: 2 }
            FoundationPage { id: diagnosticsPage; page: 3; shell: window; libraryPage: library }
        }
    }
    Item { id: gameFocus; objectName: "gamePresentation"; anchors.fill: parent; visible: window.gameView; focus: visible
        onVisibleChanged: if (visible) { lastPresented = gpuSurface.framesPresented; lastPresentedAt = Date.now(); presentedFps = 0 }
        Rectangle { anchors.fill: parent; color: "black" }
        GpuSurface { id: gpuSurface; objectName: "gpuSurface"; anchors.fill: parent; manager: emulation; visible: emulation.gpuDiagnostic }
        property int lastPresented: 0
        property double lastPresentedAt: 0
        property real presentedFps: 0
        Timer { interval: 2000; running: window.gameView; repeat: true
            onTriggered: {
                const now = Date.now()
                const elapsed = now - gameFocus.lastPresentedAt
                const frames = gpuSurface.framesPresented - gameFocus.lastPresented
                gameFocus.presentedFps = elapsed > 0 ? frames * 1000 / elapsed : 0
                gameFocus.lastPresented = gpuSurface.framesPresented
                gameFocus.lastPresentedAt = now
            }
        }
        Connections { target: emulation
            function onFrameIntervalReceived(milliseconds) {
                if (!preferences.values["ui/showFrameRate"]) return
                let values = window.intervalHistory.slice(-89); values.push(milliseconds); window.intervalHistory = values
                intervalChart.requestPaint()
            }
        }
        Rectangle { id: performanceOverlay; objectName: "performanceOverlay"; anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 12
            visible: !window.screenshotPending && window.performanceOverlayEnabled() && emulation.gameLoaded
            width: 310; height: overlayColumn.implicitHeight + 16; radius: 5; color: "#d9101018"; border.color: "#674b83"
            ColumnLayout { id: overlayColumn; anchors.fill: parent; anchors.margins: 8; spacing: 2
                Label { text: "Desempenho · " + window.currentGameTitle; font.bold: true; color: "#f0e5ff"; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { text: "Compositor: " + Number(emulation.performanceMetrics.compositorFps || 0).toFixed(1) + " FPS"; color: "#e8d8f7" }
                Label { text: "Publicados: " + Number(emulation.performanceMetrics.publishedFps || 0).toFixed(1) + " FPS"; color: "#e8d8f7" }
                Label { text: "Superfície Qt aceitos: " + gpuSurface.framesPresented + " · pulados no repasse: " + gpuSurface.framesSkipped; color: gpuSurface.framesSkipped ? "#ffc879" : "#c7b6d8"; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { text: "Escala configurada: " + (emulation.performanceMetrics.internalScale || "—") + " · saída: " + (emulation.performanceMetrics.outputResolution || "—"); color: "#c7b6d8" }
                Label { text: "Backend: " + (emulation.performanceMetrics.backend || "—"); color: "#a99bb9"; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                Canvas { id: intervalChart; Layout.fillWidth: true; Layout.preferredHeight: 38
                    onPaint: { const c = getContext("2d"); c.clearRect(0,0,width,height); c.strokeStyle="#9c67df"; c.lineWidth=1.5; c.beginPath(); const a=window.intervalHistory; for(let i=0;i<a.length;i++){ const x=a.length<2?width/2:i*width/(a.length-1); const y=height-Math.min(a[i],100)/100*height; if(i===0)c.moveTo(x,y);else c.lineTo(x,y) } c.stroke(); c.strokeStyle="#5b515f"; c.setLineDash([3,3]); c.beginPath(); c.moveTo(0,height-16.67/100*height); c.lineTo(width,height-16.67/100*height); c.stroke(); c.setLineDash([]) }
                }
                Label { text: "Intervalo de chegada pelo IPC (ms); não é frametime interno nem FPS do monitor."; color: "#a99bb9"; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: "Picos de chegada > 66,7 ms: " + window.intervalHistory.filter(v => v > 66.7).length; color: window.intervalHistory.some(v => v > 66.7) ? "#ffc879" : "#a99bb9"; font.pixelSize: 11 }
            }
        }
        Label { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 16; visible: window.screenshotNoticeVisible && qualityTools.status.length > 0 && !window.screenshotPending
            text: qualityTools.status; color: "#f0e5ff"; padding: 8; background: Rectangle { color: "#dd17121f"; radius: 4 } }
        Row { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 20; spacing: 8
            visible: emulation.state === "Loading" || emulation.state === "AwaitingFrame" || emulation.state === "Starting"
            BusyIndicator { width: 24; height: 24; running: parent.visible }
            Label { text: "Carregando jogo…"; color: "#d8d1e4"; anchors.verticalCenter: parent.verticalCenter }
        }
        MouseArea { objectName: "gameCursorArea"; anchors.fill: parent; acceptedButtons: Qt.NoButton; hoverEnabled: true
            cursorShape: !window.quickMenuOpen && preferences.values["ui/hideCursorWithController"] && controllers.devices.length > 0 ? Qt.BlankCursor : Qt.ArrowCursor
        }
        QuickMenu { id: quickMenu; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.right: parent.right; open: window.quickMenuOpen
            onResumeRequested: window.toggleQuickMenu()
            onGraphicsRequested: { window.leaveGame(); window.goSettings(2) }
            onLibraryRequested: window.leaveGame()
            onStopRequested: { emulation.stopGame(); window.leaveGame() }
            onScreenshotRequested: window.captureScreenshot()
        }
    }
    Rectangle {
        id: startupScreen
        z: 100
        anchors.fill: parent
        color: "#0b0b10"
        visible: startupEnabled && firstRunSetup.completed && !window.startupReady && !window.startupBypass
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(440, parent.width - 48)
            spacing: 18
            Brand { Layout.alignment: Qt.AlignHCenter }
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: startupScreen.visible && emulation.state !== "Failed" && emulation.state !== "Unavailable"
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: "#d8d1e4"
                text: emulation.state === "Failed" || emulation.state === "Unavailable" ? emulation.message :
                      libraryResources.busy ? libraryResources.status :
                      emulation.state === "Starting" ? "Iniciando emula\u00e7\u00e3o..." :
                      emulation.message.length > 0 ? emulation.message : "Preparando o emulador..."
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                ActionButton {
                    visible: emulation.state === "Failed" || emulation.state === "Unavailable"
                    text: "Tentar novamente"
                    enabled: !emulation.active && emulation.coreAvailable
                    onClicked: emulation.startCore()
                }
                ActionButton {
                    text: "Abrir biblioteca"
                    onClicked: window.startupBypass = true
                }
            }
        }
    }
    SetupWizard { anchors.fill: parent; z: 200; assistant: firstRunSetup; visible: firstRunEnabled && !firstRunSetup.completed }
}
