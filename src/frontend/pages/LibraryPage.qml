import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"

FocusScope {
    id: root
    objectName: "libraryPage"
    property int filter: 0
    property string selectedPath: ""
    property string query: ""
    property string identificationStatus: ""
    property string identificationTitle: ""
    property bool identificationPending: false
    property string launchStatus: ""
    property int viewMode: preferences.values["ui/view"]
    property int sortMode: preferences.values["ui/sort"]
    property var shell
    signal requestSidebar()
    signal requestSettings()
    signal requestSession()
    readonly property var displayGames: {
        let games = appModel.games.filter(function(g) {
            return g.title.toLowerCase().indexOf(query.toLowerCase()) >= 0
                    && (root.filter !== 1 || g.favorite) && (root.filter !== 2 || Number(g.viewed) > 0)
        })
        games.sort(function(a, b) {
            if (sortMode === 2) return Number(b.viewed) - Number(a.viewed) || a.title.localeCompare(b.title)
            if (sortMode === 3) return Number(b.bytes) - Number(a.bytes) || a.title.localeCompare(b.title)
            return a.title.localeCompare(b.title) * (sortMode === 1 ? -1 : 1)
        })
        return games
    }
    readonly property var selectedGame: grid.currentIndex >= 0 && grid.currentIndex < displayGames.length ? displayGames[grid.currentIndex] : null
    readonly property bool loadingGame: emulation.state === "Loading" || emulation.state === "AwaitingFrame"
    readonly property bool gameGridFocused: grid.activeFocus
    property bool settingsFromController: false
    property bool contextMenuFromController: false
    property point contextMenuAnchor: Qt.point(0, 0)
    onDisplayGamesChanged: Qt.callLater(function() {
        let index = displayGames.findIndex(function(g) { return g.path === selectedPath })
        grid.currentIndex = index >= 0 ? index : (displayGames.length ? 0 : -1)
    })
    function focusLibrary() { if (displayGames.length) grid.forceActiveFocus(); else addFolder.forceActiveFocus() }
    function favorite() { if (selectedGame) appModel.toggleFavorite(selectedGame.path) }
    function openContextMenuFromController() {
        if (!selectedGame) return
        contextMenuFromController = true
        const point = detailImage.mapToItem(root, detailImage.width + 12, 8)
        contextMenuAnchor = point
        gameMenu.popup(root, point)
    }
    function openContextMenuAt(item, x, y) {
        if (!selectedGame) return
        contextMenuFromController = false
        const point = item.mapToItem(root, x, y)
        contextMenuAnchor = point
        gameMenu.popup(root, point)
    }
    function menuNearItem(item) {
        if (!selectedGame) return
        contextMenuFromController = false
        const point = item.mapToItem(root, item.width, 0)
        contextMenuAnchor = point
        gameMenu.popup(root, point)
    }
    function startSelected() {
        if (emulation.gameLoaded) { root.requestSession(); return }
        if (!root.selectedGame || !emulation.coreAvailable || root.loadingGame || (emulation.active && emulation.state !== "Ready")) return
        identificationPending = false
        const game = root.selectedGame
        if (!emulation.startGame(game.path)) {
            launchStatus = emulation.message
            return
        }
        identificationPending = false
        launchStatus = ""
        root.shell.currentGameTitle = game.title
        compatibility.beginSession(game, root.compatibilityConfiguration())
        compatibility.updateSessionState(emulation.state)
        root.requestSession()
    }
    function identifySelected() {
        if (!root.selectedGame || root.identificationPending) return
        identificationTitle = root.selectedGame.title
        identificationStatus = "Consultando o loader original…"
        identificationPending = true
        if (!emulation.identifyFile(root.selectedGame.path)) {
            identificationPending = false
            identificationStatus = emulation.message
        }
    }
    function compatibilityConfiguration() {
        const keys = ["graphics/resolution", "graphics/docked", "graphics/vsync", "graphics/gpuAccuracy",
            "graphics/asynchronousShaders", "graphics/asynchronousGpu", "graphics/pipelineCache",
            "graphics/antiAliasing", "core/multicore", "core/cpuAccuracy", "core/speedLimit"]
        const values = {}
        for (let i = 0; i < keys.length; ++i) {
            const override = preferences.gameOverrideValue(selectedGame.path, keys[i])
            values[keys[i]] = override === undefined || override === null ? preferences.values[keys[i]] : override
        }
        return values
    }
    function openCompatibility() {
        if (!selectedGame || !selectedGame.titleId) return
        compatibilityDialog.targetTitleId = selectedGame.titleId
        compatibilityDialog.targetTitle = selectedGame.title
        const saved = compatibility.record(selectedGame.titleId)
        compatibilityDialog.draftAssessment = saved.assessment || "untested"
        compatibilityDialog.historyItems = compatibility.history(selectedGame.titleId)
        compatibilityDialog.open()
    }
    function openGameSettings(fromController, fromMouse) {
        if (!selectedGame) return
        settingsFromController = !!fromController
        gameSettingsDialog.settingsFromMouse = !!fromMouse
        gameSettingsDialog.targetPath = selectedGame.path
        gameSettingsDialog.title = "Configurações do jogo · " + selectedGame.title
        gameSettingsDialog.open()
        if (settingsFromController) Qt.callLater(gameSettingsDialog.positionBesideCover)
        else if (fromMouse) Qt.callLater(gameSettingsDialog.positionNearCursor)
    }
    function openGameSettingsForTitleId(titleId) {
        const index = displayGames.findIndex(function(game) { return game.titleId === titleId })
        if (index < 0) return
        grid.currentIndex = index
        selectedPath = displayGames[index].path
        Qt.callLater(function() { root.openGameSettings(false, false) })
    }
    function openMods() {
        modDialog.targetPath = selectedGame ? selectedGame.path : ""
        modDialog.titleId = selectedGame ? selectedGame.titleId : ""
        modDialog.reloadMods()
        modDialog.open()
    }
    Connections {
        target: emulation
        function onIdentified(format) {
            if (!root.identificationPending) return
            root.identificationPending = false
            root.identificationStatus = emulation.message
        }
        function onMessageChanged() {
            if (!root.identificationPending) return
            const message = emulation.message
            if (message.startsWith("Arquivo de programa inexistente") ||
                message.startsWith("Aguarde") ||
                message.startsWith("Operação rejeitada pelo núcleo:")) {
                root.identificationPending = false
                root.identificationStatus = message
            }
        }
        function onFailed() {
            if (!root.identificationPending) return
            root.identificationPending = false
            root.identificationStatus = emulation.message
        }
    }
    function closeMenu() { gameMenu.close() }
    function chooseFilter(value) { root.filter = value; appModel.scan() }
    function persistView(value) { preferences.setValue("ui/view", value); preferences.apply() }
    FileDialog {
        id: coverPicker
        title: "Escolher capa local"
        nameFilters: ["Imagens (*.png *.jpg *.jpeg *.webp)"]
        onAccepted: if (root.selectedGame) appModel.setCover(root.selectedGame.path, selectedFile)
        options: FileDialog.DontUseNativeDialog
        onVisibleChanged: controllers.popupActive = visible
    }
    FolderDialog { id: folderPicker; title: "Adicionar pasta de jogos"; onVisibleChanged: controllers.popupActive = visible; onAccepted: { appModel.addFolder(selectedFolder); preferences.refresh() } }
    Menu {
        id: gameMenu
        objectName: "gameContextMenu"
        modal: true
        padding: 8
        implicitWidth: 290
        background: Rectangle { radius: 8; color: "#17131f"; border.color: "#76539a"; border.width: 1 }
        delegate: MenuItem {
            id: menuDelegate
            implicitHeight: 42
            contentItem: Label { text: menuDelegate.text; color: menuDelegate.enabled ? (menuDelegate.highlighted ? "#ffffff" : "#e9ddf5") : "#857b91"; verticalAlignment: Text.AlignVCenter; leftPadding: 10; rightPadding: 10; elide: Text.ElideRight }
            background: Rectangle { radius: 5; color: menuDelegate.highlighted ? "#713fc0" : "transparent" }
        }
        MenuItem { text: root.selectedGame ? root.selectedGame.title : "Jogo selecionado"; enabled: false }
        MenuSeparator {}
        onVisibleChanged: controllers.popupActive = visible
        MenuItem { text: "Favoritar / remover favorito"; onTriggered: root.favorite() }
        MenuItem { text: "Escolher capa local…"; onTriggered: coverPicker.open() }
        MenuItem { text: "Abrir pasta do arquivo"; onTriggered: if (root.selectedGame) preferences.openFolder(root.selectedGame.path.substring(0, root.selectedGame.path.lastIndexOf("/"))) }
        MenuItem { objectName: "gameSettingsMenuAction"; text: "Configurações deste jogo…"; onTriggered: root.openGameSettings(root.contextMenuFromController, !root.contextMenuFromController) }
        MenuItem { text: "Compatibilidade e histórico…"; enabled: !!root.selectedGame && !!root.selectedGame.titleId; onTriggered: { gameMenu.close(); Qt.callLater(root.openCompatibility) } }
        MenuItem { objectName: "gameModsMenuAction"; text: "Mods e cheats"; enabled: !!root.selectedGame; onTriggered: { gameMenu.close(); Qt.callLater(root.openMods) } }
        MenuItem { text: "Iniciar jogo"; enabled: root.selectedGame !== null && emulation.coreAvailable && (emulation.state === "Ready" || !emulation.active); onTriggered: root.startSelected() }
        MenuItem { objectName: "gameIdentifyMenuAction"; text: root.identificationPending ? "Identificando arquivo…" : "Identificar com o núcleo original"; enabled: emulation.coreAvailable && root.selectedGame !== null && !emulation.gameLoaded && !root.identificationPending; onTriggered: root.identifySelected() }
        onClosed: if (!root.shell.gameView) root.focusLibrary()
    }
    Dialog {
        id: gameSettingsDialog
        objectName: "gameSettingsDialog"
        property string targetPath: ""
        property bool settingsFromMouse: false
        property var cacheInfo: ({files: 0, bytes: 0})
        title: "Configurações do jogo"
        modal: true
        parent: root.shell.contentItem
        width: Math.min(560, root.width - 32)
        height: Math.min(690, root.height - 32)
        x: {
            if (!root.settingsFromController) return (root.shell.width - width) / 2
            const p = detailImage.mapToItem(null, detailImage.width + 12, 0)
            if (p.x + width <= root.shell.width - 8) return Math.max(8, p.x)
            const left = detailImage.mapToItem(null, -12, 0).x - width
            return Math.max(8, Math.min(root.shell.width - width - 8, left))
        }
        y: {
            if (!root.settingsFromController) return (root.shell.height - height) / 2
            const p = detailImage.mapToItem(null, detailImage.width + 12, 0)
            return Math.max(8, Math.min(root.shell.height - height - 8, p.y))
        }
        function boolIndex(key) {
            const value = preferences.gameOverrideValue(targetPath, key)
            return value === undefined || value === null ? 0 : (value ? 2 : 1)
        }
        function saveBool(path, key, index) {
            return index === 0 ? preferences.clearGameOverride(path, key)
                               : preferences.setGameOverrideValue(path, key, index === 2)
        }
        function refreshCacheInfo() { cacheInfo = qualityTools.gameCacheInfo(root.selectedGame ? root.selectedGame.titleId : "") }
        function positionBesideCover() {
            const p = detailImage.mapToItem(null, detailImage.width + 12, 0)
            const left = detailImage.mapToItem(null, -12, 0).x - width
            x = p.x + width <= root.shell.width - 8 ? Math.max(8, p.x) : Math.max(8, Math.min(root.shell.width - width - 8, left))
            y = Math.max(8, Math.min(root.shell.height - height - 8, p.y))
        }
        function positionNearCursor() {
            const p = root.mapToItem(null, root.contextMenuAnchor.x, root.contextMenuAnchor.y)
            x = Math.max(8, Math.min(root.shell.width - width - 8, p.x))
            y = Math.max(8, Math.min(root.shell.height - height - 8, p.y))
        }
        onOpened: {
            refreshCacheInfo()
            resolution.currentIndex = preferences.gameOverride(targetPath, "graphics/resolution") + 1
            filter.currentIndex = preferences.gameOverride(targetPath, "graphics/scalingFilter") + 1
            vsync.currentIndex = preferences.gameOverride(targetPath, "graphics/vsync") + 1
            aa.currentIndex = preferences.gameOverride(targetPath, "graphics/antiAliasing") + 1
            mode.currentIndex = preferences.gameOverride(targetPath, "graphics/docked") + 1
            shaderCache.currentIndex = boolIndex("graphics/shaderCache")
            asyncShaders.currentIndex = boolIndex("graphics/asynchronousShaders")
            pipelineCache.currentIndex = boolIndex("graphics/pipelineCache")
            multicore.currentIndex = boolIndex("core/multicore")
            gpuAccuracy.currentIndex = preferences.gameOverride(targetPath, "graphics/gpuAccuracy") + 1
            cpuAccuracy.currentIndex = preferences.gameOverride(targetPath, "core/cpuAccuracy") + 1
            speedLimit.currentIndex = [50,75,100,125,150,200].indexOf(preferences.gameOverride(targetPath, "core/speedLimit")) + 1
            vulkanDevice.currentIndex = preferences.gameOverride(targetPath, "graphics/vulkanDevice") + 1
            const audioOverride = preferences.gameOverrideValue(targetPath, "audio/outputDevice")
            const audioIndex = audioOverride === undefined || audioOverride === null ? -1 : emulation.audioDevices.indexOf(audioOverride)
            audioDevice.currentIndex = audioIndex < 0 ? 0 : audioIndex + 1
        }
        onVisibleChanged: controllers.popupActive = visible
        contentItem: ScrollView {
          clip: true
          contentWidth: availableWidth
          ColumnLayout {
            width: parent.width
            spacing: 10
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 70; radius: 6; color: "#21182f"; border.color: "#513b6d"
                RowLayout { anchors.fill: parent; anchors.margins: 12; spacing: 12
                    Glyph { name: "settings"; font.pixelSize: 26; color: "#ce9dff" }
                    ColumnLayout { Layout.fillWidth: true; spacing: 2
                        Label { text: root.selectedGame ? root.selectedGame.title : "Jogo selecionado"; font.pixelSize: 17; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                        Label { text: "Configurações aplicadas somente a este jogo"; color: "#b9a7d2"; font.pixelSize: 12 }
                    }
                }
            }
            Label { text: "Os valores herdados seguem as configurações globais. As exceções são salvas por arquivo e aplicadas ao iniciar o jogo novamente."; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#b9a7d2" }
            Label { text: "Escala interna de resolução" }
            Select { id: resolution; Layout.fillWidth: true; model: ["Global", "0,25×", "0,5×", "0,75×", "1×", "1,25×", "1,5×", "2×", "3×", "4×", "5×", "6×", "7×", "8×"] }
            Label { text: "Filtro de escala (upscaling)" }
            Select { id: filter; Layout.fillWidth: true; model: ["Global", "Nearest", "Bilinear", "Bicubic", "Gaussian", "Lanczos", "ScaleForce", "FSR 1", "Area", "ZeroTangent", "B-Spline", "Mitchell", "Spline1", "MMPX", "SGSR 1", "SGSR 1 Edge"] }
            Label { text: "Sincronização vertical (VSync)" }
            Select { id: vsync; Layout.fillWidth: true; model: ["Global", "Immediate", "Mailbox", "FIFO", "FIFO Relaxed"] }
            Label { text: "Anti-aliasing" }
            Select { id: aa; Layout.fillWidth: true; model: ["Global", "Desligado", "FXAA", "SMAA"] }
            Label { text: "Modo do console" }
            Select { id: mode; Layout.fillWidth: true; model: ["Global", "Portátil", "Docked"] }
            Label { text: "Cache de shaders" }
            Select { id: shaderCache; Layout.fillWidth: true; model: ["Global", "Desligado", "Ligado"] }
            Label { text: "Compilação assíncrona de shaders" }
            Select { id: asyncShaders; Layout.fillWidth: true; model: ["Global", "Desligada", "Ligada"] }
            Label { text: "Cache persistente de pipelines Vulkan" }
            Select { id: pipelineCache; Layout.fillWidth: true; model: ["Global", "Desligado", "Ligado"] }
            Label { text: "Precisão da GPU" }
            Select { id: gpuAccuracy; Layout.fillWidth: true; model: ["Global", "Normal", "Alta"] }
            Label { text: "Dispositivo Vulkan" }
            Select { id: vulkanDevice; Layout.fillWidth: true; model: ["Global"].concat(emulation.vulkanDevices) }
            Label { text: "CPU multicore" }
            Select { id: multicore; Layout.fillWidth: true; model: ["Global", "Desligado", "Ligado"] }
            Label { text: "Precisão da CPU" }
            Select { id: cpuAccuracy; Layout.fillWidth: true; model: ["Global", "Auto", "Precisa", "Mais precisa", "Unsafe"] }
            Label { text: "Limite de velocidade" }
            Select { id: speedLimit; Layout.fillWidth: true; model: ["Global", "50%", "75%", "100%", "125%", "150%", "200%"] }
            Label { text: "Saída de áudio" }
            Select { id: audioDevice; Layout.fillWidth: true; model: ["Global"].concat(emulation.audioDevices) }
            Label { text: "Cache deste jogo (shaders e pipelines)"; font.bold: true }
            Label { text: gameSettingsDialog.cacheInfo.files + " arquivos · " + (gameSettingsDialog.cacheInfo.bytes / 1048576).toFixed(2) + " MiB"; color: "#b9a7d2" }
            RowLayout {
                ActionButton { text: "Abrir pasta do cache"; enabled: !!root.selectedGame && !!root.selectedGame.titleId; onClicked: qualityTools.openGameCache(root.selectedGame.titleId) }
                ActionButton { text: "Limpar cache…"; enabled: !!root.selectedGame && !!root.selectedGame.titleId && !emulation.gameLoaded; onClicked: clearCacheConfirmation.open() }
            }
            Label { text: qualityTools.status; visible: text.length > 0; color: "#d4afff"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                ActionButton { text: "Limpar excecoes"; enabled: preferences.hasGameOverrides(gameSettingsDialog.targetPath); onClicked: { preferences.clearGameOverrides(gameSettingsDialog.targetPath); gameSettingsDialog.close() } }
                Item { Layout.fillWidth: true }
                ActionButton { text: "Cancelar"; onClicked: gameSettingsDialog.close() }
                ActionButton { text: "Salvar"; primary: true; onClicked: {
                    const path = gameSettingsDialog.targetPath
                    const ok = preferences.setGameOverride(path, "graphics/resolution", resolution.currentIndex - 1)
                        && preferences.setGameOverride(path, "graphics/scalingFilter", filter.currentIndex - 1)
                        && preferences.setGameOverride(path, "graphics/vsync", vsync.currentIndex - 1)
                        && preferences.setGameOverride(path, "graphics/antiAliasing", aa.currentIndex - 1)
                        && preferences.setGameOverride(path, "graphics/docked", mode.currentIndex - 1)
                        && saveBool(path, "graphics/shaderCache", shaderCache.currentIndex)
                        && saveBool(path, "graphics/asynchronousShaders", asyncShaders.currentIndex)
                        && saveBool(path, "graphics/pipelineCache", pipelineCache.currentIndex)
                        && preferences.setGameOverride(path, "graphics/gpuAccuracy", gpuAccuracy.currentIndex - 1)
                        && preferences.setGameOverride(path, "graphics/vulkanDevice", vulkanDevice.currentIndex - 1)
                        && (audioDevice.currentIndex === 0 ? preferences.clearGameOverride(path, "audio/outputDevice") : preferences.setGameOverrideValue(path, "audio/outputDevice", emulation.audioDevices[audioDevice.currentIndex - 1]))
                        && saveBool(path, "core/multicore", multicore.currentIndex)
                        && preferences.setGameOverride(path, "core/cpuAccuracy", cpuAccuracy.currentIndex - 1)
                        && preferences.setGameOverride(path, "core/speedLimit", speedLimit.currentIndex === 0 ? -1 : [50,75,100,125,150,200][speedLimit.currentIndex - 1])
                    if (ok) gameSettingsDialog.close()
                } }
            }
          }
        }
    }
    Dialog {
        id: compatibilityDialog
        objectName: "gameCompatibilityDialog"
        property string targetTitleId: ""
        property string targetTitle: ""
        property string draftAssessment: "untested"
        property var historyItems: []
        readonly property var assessmentKeys: ["untested", "does-not-start", "major-issues", "playable-issues", "playable-no-known-issues", "extensively-tested"]
        title: "Compatibilidade · " + targetTitle
        modal: true
        parent: root.shell.contentItem
        width: Math.min(720, root.width - 32)
        height: Math.min(700, root.height - 32)
        anchors.centerIn: Overlay.overlay
        onVisibleChanged: controllers.popupActive = visible
        contentItem: ColumnLayout {
            spacing: 10
            Label { text: "Title ID: " + compatibilityDialog.targetTitleId; color: "#b9a7d2" }
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2"
                text: "A avaliação é manual. Abrir um jogo ou receber um frame não o marca automaticamente como jogável. A versão do jogo não é fornecida pelo leitor e aparece como não informada."
            }
            Label { text: "Estado de compatibilidade"; font.bold: true }
            Select {
                id: compatibilityAssessment
                Layout.fillWidth: true
                model: ["Não testado", "Não inicia", "Problemas graves", "Jogável com problemas", "Jogável sem problemas conhecidos", "Testado extensivamente"]
                currentIndex: Math.max(0, compatibilityDialog.assessmentKeys.indexOf(compatibilityDialog.draftAssessment))
                onActivated: compatibilityDialog.draftAssessment = compatibilityDialog.assessmentKeys[currentIndex]
            }
            Label { text: "Observações / problemas conhecidos"; font.bold: true }
            TextArea {
                id: compatibilityNotes
                Layout.fillWidth: true; Layout.preferredHeight: 86
                wrapMode: TextEdit.Wrap
                placeholderText: "Anote condições e problemas observados em testes reais."
                text: compatibility.record(compatibilityDialog.targetTitleId).notes || ""
            }
            Label {
                text: "Histórico de sessões · Shireia " + (compatibility.record(compatibilityDialog.targetTitleId).emulatorVersion || "versão desconhecida")
                    + " · jogo " + (compatibility.record(compatibilityDialog.targetTitleId).gameVersion || "não informado")
                color: "#c9b6dd"; font.bold: true
            }
            ListView {
                id: compatibilityHistory
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
                model: compatibilityDialog.historyItems
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width; height: Math.max(58, sessionText.implicitHeight + 16)
                    radius: 5; color: "#17131f"; border.color: "#3a2d4b"
                    Label {
                        id: sessionText
                        anchors.fill: parent; anchors.margins: 8
                        color: "#d8c9e7"; wrapMode: Text.WordWrap
                        text: modelData.startedUtc + " · " + modelData.outcome + " · " + modelData.durationSeconds + " s"
                            + (modelData.detail ? "\n" + modelData.detail : "")
                            + (modelData.events && modelData.events.length ? "\nEventos associados: " + modelData.events.length : "")
                    }
                }
                Label { anchors.centerIn: parent; visible: compatibilityDialog.historyItems.length === 0; text: "Nenhuma sessão registrada. Iniciar um jogo cria uma tentativa; a avaliação permanece manual."; color: "#a995bf"; wrapMode: Text.WordWrap; width: parent.width - 24; horizontalAlignment: Text.AlignHCenter }
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                ActionButton { text: "Cancelar"; onClicked: compatibilityDialog.close() }
                ActionButton {
                    text: "Salvar avaliação"; primary: true
                    onClicked: {
                        if (compatibility.setAssessment(compatibilityDialog.targetTitleId, compatibilityDialog.draftAssessment, compatibilityNotes.text)) {
                            compatibilityDialog.close()
                        }
                    }
                }
            }
        }
    }
    Dialog {
        id: clearCacheConfirmation
        title: "Limpar cache deste jogo?"
        modal: true
        width: Math.min(520, root.width - 32)
        height: 170
        standardButtons: Dialog.Yes | Dialog.No
        anchors.centerIn: Overlay.overlay
        contentItem: Label { text: "Serão removidos apenas os caches de shaders e pipelines do Title ID selecionado. Saves, DLCs, mods e firmware não serão alterados."; wrapMode: Text.WordWrap; padding: 16; width: clearCacheConfirmation.width - 32 }
        onAccepted: { if (root.selectedGame) qualityTools.clearGameCache(root.selectedGame.titleId, emulation.gameLoaded); gameSettingsDialog.refreshCacheInfo() }
    }
    FolderDialog {
        id: modFolderPicker
        title: "Importar pasta de mod Suyu"
        onAccepted: {
            if (gameContent.importMod(modDialog.titleId, selectedFolder.toLocalFile())) modDialog.reloadMods()
        }
    }
    FileDialog {
        id: cheatFilePicker
        title: "Importar arquivo de cheat Suyu"
        nameFilters: ["Cheats por Build ID (*.txt)"]
        onAccepted: {
            if (gameContent.importCheat(modDialog.titleId, selectedFile.toLocalFile())) modDialog.reloadMods()
        }
    }
    Dialog {
        id: modDialog
        objectName: "gameModsDialog"
        property string targetPath: ""
        property string titleId: ""
        property var modItems: []
        title: "Mods e cheats"
        modal: true
        parent: root.shell.contentItem
        width: Math.min(680, root.width - 32)
        height: Math.min(650, root.height - 32)
        anchors.centerIn: Overlay.overlay
        function reloadMods() { modItems = gameContent.mods(titleId) }
        onVisibleChanged: controllers.popupActive = visible
        contentItem: ColumnLayout {
            spacing: 10
            Label { text: "Title ID: " + (modDialog.titleId || "indisponível"); color: "#b9a7d2" }
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#edbd68"
                visible: !modDialog.titleId
                text: "Não foi possível identificar o Title ID deste arquivo. A lista pode ser consultada, mas importar ou abrir mods exige metadados válidos do jogo."
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#d4afff"
                text: emulation.gameLoaded ? "O jogo continua rodando. Alterações em mods e cheats só valem após encerrar e iniciar o jogo novamente." : "O Suyu aplica mods LayeredFS/ExeFS no próximo início. Cheats em cheats/<Build ID>.txt são associados ao Build ID; o Suyu decide a compatibilidade ao carregar."
            }
            RowLayout {
                ActionButton { text: "Importar pasta"; primary: true; enabled: !!modDialog.titleId && !emulation.gameLoaded; onClicked: modFolderPicker.open() }
                ActionButton { text: "Importar cheat .txt"; enabled: !!modDialog.titleId && !emulation.gameLoaded; onClicked: cheatFilePicker.open() }
                ActionButton { text: "Abrir pasta de mods"; enabled: !!modDialog.titleId; onClicked: gameContent.openModsFolder(modDialog.titleId) }
                Item { Layout.fillWidth: true }
                ActionButton { text: "Atualizar lista"; onClicked: modDialog.reloadMods() }
            }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: modDialog.modItems; spacing: 8
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width; height: 86; radius: 5; color: "#17131f"; border.color: "#3a2d4b"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: (modelData.kind === "cheat" ? "Cheat · " : "Mod · ") + modelData.name + (modelData.version ? " · " + modelData.version : ""); font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: modelData.description || modelData.folder; color: "#a995bf"; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: modelData.kind === "cheat" ? (modelData.valid ? "Parser Suyu aceitou · Build ID conferido ao iniciar" : "Formato incompatível com o parser Suyu") : (modelData.conflicts && modelData.conflicts.length ? "Possível conflito de arquivos: " + modelData.conflicts.join(", ") : ""); visible: text.length > 0; color: modelData.valid === false ? "#ff8b9a" : "#edbd68"; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: modelData.kind === "cheat" ? (modelData.enabled ? "Arquivo ativo · Suyu confere Build ID ao carregar" : "Desativado") : (modelData.enabled ? "Ativo · LayeredFS/ExeFS Suyu" : "Desativado"); color: modelData.enabled ? "#b992e8" : "#9b91a5" }
                        }
                        ActionButton { enabled: modelData.kind !== "cheat" || modelData.valid; text: modelData.enabled ? "Desativar" : "Ativar"; onClicked: {
                            const changed = modelData.kind === "cheat"
                                ? gameContent.setCheatEnabled(modDialog.titleId, modelData.folder, modelData.file, !modelData.enabled)
                                : gameContent.setModEnabled(modDialog.titleId, modelData.folder, !modelData.enabled)
                            if (changed) modDialog.reloadMods()
                        } }
                        ActionButton { visible: modelData.kind !== "cheat"; text: "Remover"; onClicked: { removeModDialog.modName = modelData.folder; removeModDialog.open() } }
                    }
                }
                Label { anchors.centerIn: parent; visible: modDialog.modItems.length === 0; text: "Nenhum mod compatível encontrado para este Title ID."; color: "#a995bf" }
            }
            Label { text: gameContent.status; visible: text.length > 0; color: "#d4afff"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout { Item { Layout.fillWidth: true } ActionButton { text: "Fechar"; onClicked: modDialog.close() } }
        }
    }
    Dialog {
        id: removeModDialog
        property string modName: ""
        title: "Remover mod?"
        modal: true
        width: Math.min(480, root.width - 32)
        anchors.centerIn: Overlay.overlay
        contentItem: ColumnLayout {
            Label { text: "O mod ‘" + removeModDialog.modName + "’ será removido da pasta privada do Shireia. Saves e arquivos do jogo não serão alterados."; wrapMode: Text.WordWrap; Layout.maximumWidth: 420 }
            RowLayout {
                Item { Layout.fillWidth: true }
                ActionButton { text: "Cancelar"; onClicked: removeModDialog.close() }
                ActionButton { text: "Remover"; primary: true; onClicked: { gameContent.removeMod(modDialog.titleId, removeModDialog.modName); removeModDialog.close(); modDialog.reloadMods() } }
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 18
        RowLayout {
            Layout.fillWidth: true; spacing: 20
            ColumnLayout {
                Label { text: "Biblioteca"; font.pixelSize: 28; font.bold: true; color: "#eee2ff" }
                Label { text: "Gerencie sua biblioteca de jogos."; color: "#b7a4d0"; font.pixelSize: 16 }
            }
            Item { Layout.fillWidth: true }
            Field { id: search; objectName: "librarySearch"; Layout.preferredWidth: Math.min(root.width * 0.47, 540); placeholderText: "⌕   Buscar jogos…"; onTextChanged: root.query = text }
        }
        RowLayout {
            spacing: 8
            Repeater {
                model: ["Todos", "Favoritos", "Recentes"]
                ActionButton { required property string modelData; required property int index; text: modelData; iconName: ["grid", "favorite", "recent"][index]; checked: root.filter === index; onClicked: root.chooseFilter(index) }
            }
            ActionButton { text: "Instalados · futuro"; enabled: false }
            Item { Layout.fillWidth: true }
            ActionButton { iconName: "grid"; checked: root.viewMode === 0; onClicked: root.persistView(0); Accessible.name: "Exibir grade" }
            ActionButton { iconName: "list"; checked: root.viewMode === 1; onClicked: root.persistView(1); Accessible.name: "Exibir lista" }
            Select {
                objectName: "librarySortSelector"
                Layout.preferredWidth: root.width > 1050 ? 210 : 160
                model: ["Nome (A–Z)", "Nome (Z–A)", "Consulta recente", "Tamanho"]
                currentIndex: root.sortMode
                onActivated: { preferences.setValue("ui/sort", currentIndex); preferences.apply() }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 20
            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                GridView {
                    id: grid
                    objectName: "gameGrid"
                    anchors.fill: parent; clip: true
                    focus: true; activeFocusOnTab: true
                    model: root.displayGames
                    cellWidth: root.viewMode === 1 ? width : width / Math.max(1, Math.floor(width / Math.max(130, Number(preferences.values["ui/thumb"]))))
                    cellHeight: root.viewMode === 1 ? 96 : cellWidth * 1.42 + 50
                    keyNavigationEnabled: true
                    highlightFollowsCurrentItem: true
                    highlightMoveDuration: 100
                    ScrollBar.vertical: ScrollBar {}
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Left && (currentIndex < 0 || currentIndex % Math.max(1, Math.round(width / cellWidth)) === 0)) { root.requestSidebar(); event.accepted = true }
                        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { root.startSelected(); event.accepted = true }
                        else if (event.key === Qt.Key_F) { root.favorite(); event.accepted = true }
                    }
                    onCurrentIndexChanged: {
                        if (root.selectedGame) { root.selectedPath = root.selectedGame.path; appModel.noteViewed(root.selectedPath) }
                        positionViewAtIndex(currentIndex, GridView.Contain)
                    }
                    delegate: Item {
                        id: tile
                        objectName: "gameTile_" + index
                        required property var modelData
                        required property int index
                        width: grid.cellWidth; height: grid.cellHeight
                        Rectangle {
                            anchors.fill: parent; anchors.margins: 5; radius: 4
                            color: grid.currentIndex === tile.index ? "#21182f" : "transparent"
                            border.width: grid.currentIndex === tile.index ? 2 : 0
                            border.color: grid.activeFocus ? "#c27cff" : "#8057ae"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 7; spacing: 12
                                Rectangle {
                                    id: cover
                                    Layout.alignment: Qt.AlignTop
                                    Layout.preferredWidth: root.viewMode === 1 ? 65 : parent.width
                                    Layout.preferredHeight: root.viewMode === 1 ? 72 : parent.width * 1.28
                                    radius: 4; color: "#211b33"; clip: true
                                    Image { id: coverImage; anchors.fill: parent; source: tile.modelData.cover || ""; fillMode: Image.PreserveAspectFit; asynchronous: true }
                                    Column {
                                        anchors.centerIn: parent; spacing: 8; visible: coverImage.status !== Image.Ready
                                        Glyph { anchors.horizontalCenter: parent.horizontalCenter; name: "games"; font.pixelSize: 35; color: "#795c99" }
                                        Label { text: "Sem capa"; color: "#a895bd"; font.pixelSize: 11 }
                                    }
                                    Label { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 7; text: "★"; color: "#dfb9ff"; font.pixelSize: 24; visible: tile.modelData.favorite }
                                }
                                ColumnLayout {
                                    visible: root.viewMode === 1; Layout.fillWidth: true
                                    Label { text: tile.modelData.title; font.pixelSize: 17; elide: Text.ElideRight; Layout.fillWidth: true }
                                    Label { text: tile.modelData.format + " · " + (tile.modelData.bytes / 1073741824).toFixed(2) + " GiB"; color: "#a08cb7" }
                                }
                            }
                            Column {
                                visible: root.viewMode === 0
                                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 10; spacing: 4
                                Label { width: parent.width; text: tile.modelData.title; elide: Text.ElideRight; color: "#ede3fc" }
                                Label { text: tile.modelData.format + " · arquivo local"; color: "#8e7ca6"; font.pixelSize: 11 }
                            }
            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function(mouse) { grid.currentIndex = tile.index; grid.forceActiveFocus(); if (mouse.button === Qt.RightButton) root.openContextMenuAt(tile, mouse.x, mouse.y) }
                                onDoubleClicked: root.startSelected()
                            }
                        }
                    }
                }
                ColumnLayout {
                    anchors.centerIn: parent; width: Math.min(parent.width - 32, 420)
                    visible: root.displayGames.length === 0; spacing: 18
                    Glyph { name: "library"; font.pixelSize: 60; Layout.alignment: Qt.AlignHCenter }
                    Label { text: appModel.games.length ? "Nenhum arquivo neste filtro" : "Sua biblioteca começa aqui"; font.pixelSize: 23; wrapMode: Text.WordWrap; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                    Label { text: "Adicione uma pasta local. Capas e metadados serão exibidos quando disponíveis."; color: "#a995bf"; wrapMode: Text.WordWrap; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                    ActionButton { id: addFolder; text: "Adicionar pasta"; primary: true; Layout.alignment: Qt.AlignHCenter; onClicked: folderPicker.open() }
                }
            }
            Panel {
                Layout.preferredWidth: Math.min(270, root.width * 0.28); Layout.fillHeight: true
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: Math.min(220, root.height * 0.27); color: "#241b35"; radius: 9; clip: true
                    Image { id: detailImage; objectName: "gameCoverImage"; anchors.fill: parent; source: root.selectedGame ? root.selectedGame.cover : ""; fillMode: Image.PreserveAspectFit; asynchronous: true }
                    Column {
                        anchors.centerIn: parent; spacing: 12; visible: detailImage.status !== Image.Ready
                        Glyph { name: "games"; font.pixelSize: 52; anchors.horizontalCenter: parent.horizontalCenter }
                        Label { text: "Capa não disponível"; color: "#ac95c7" }
                    }
                }
                Label { text: root.selectedGame ? root.selectedGame.title : "Selecione um jogo"; font.pixelSize: 22; font.bold: true; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: root.selectedGame ? root.selectedGame.format + "   ·   Arquivo local" : "Biblioteca vazia"; color: "#b29bcf" }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#3a2d4b" }
                Label { text: root.selectedGame ? "Tamanho: " + (root.selectedGame.bytes / 1073741824).toFixed(2) + " GiB" : "Nenhum arquivo selecionado"; color: "#c4b3d7" }
                Label { text: root.selectedGame && root.selectedGame.titleId ? "Title ID: " + root.selectedGame.titleId : "Title ID indisponível · atualize os metadados para gerenciar mods"; color: "#9c88b4"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: "Compatibilidade: " + (root.selectedGame && root.selectedGame.titleId ? compatibility.statusLabel(root.selectedGame.titleId) : "sem identificação por Title ID"); color: "#c9b6dd"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                ActionButton { text: "Compatibilidade e histórico"; Layout.fillWidth: true; enabled: !!root.selectedGame && !!root.selectedGame.titleId; onClicked: root.openCompatibility() }
                Label { visible: preferences.values["ui/showPaths"]; text: root.selectedGame ? root.selectedGame.path : ""; color: "#9c88b4"; font.pixelSize: 11; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                Label { text: root.filter === 2 ? "Recentes registra arquivos consultados nesta interface; não indica execução." : "Selecione uma capa local pelo menu do jogo. Nenhum metadado será inventado."; color: "#b09acb"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Item { Layout.fillHeight: true }
                RowLayout {
                    ActionButton { text: emulation.gameLoaded ? "Retornar ao jogo" : root.loadingGame ? "Carregando jogo…" : "Iniciar jogo"; Layout.fillWidth: true; enabled: emulation.coreAvailable && (emulation.gameLoaded || (root.selectedGame !== null && (emulation.state === "Ready" || !emulation.active))); onClicked: root.startSelected() }
                    ActionButton { id: more; text: "•••"; enabled: root.selectedGame !== null; onClicked: root.menuNearItem(more); Accessible.name: "Menu do jogo" }
                }
            }
        }
        RowLayout {
            Label { text: root.displayGames.length + " arquivo(s)  ·  " + (root.filter === 2 ? "Recentes consultados" : "Conteúdo não validado"); color: "#8e7ba7"; font.pixelSize: 11 }
            Item { Layout.fillWidth: true }
            ActionButton { text: "＋ Pasta"; onClicked: folderPicker.open() }
            ActionButton { text: "Atualizar"; onClicked: { appModel.scan(); libraryResources.refresh() } }
        }
        Label { text: libraryResources.status; visible: text !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2"; font.pixelSize: 11; maximumLineCount: 3; elide: Text.ElideRight }
        Label { text: root.identificationTitle + ": " + root.identificationStatus; visible: root.identificationStatus !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: root.identificationPending ? "#d4afff" : "#b9a7d2"; font.pixelSize: 11; maximumLineCount: 3; elide: Text.ElideRight }
        Label { text: root.launchStatus; visible: text !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#ffb2bd"; font.pixelSize: 11; maximumLineCount: 3; elide: Text.ElideRight }
    }
}
