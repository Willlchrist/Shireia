import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"

ColumnLayout {
    id: root
    objectName: page === 3 ? "diagnosticsPage" : "profilesPage"
    property int page: 2
    property var shell
    property var libraryPage
    property string profileFeedback: ""
    property string editingUuid: ""
    property string resolverTitleId: ""
    property string resolverSessionId: ""
    property string resolverTitle: ""
    property var crashItems: []
    function refreshCrashHistory() { root.crashItems = compatibility.crashHistory() }
    function openCrashResolver(session) {
        root.resolverTitleId = session.titleId || ""
        root.resolverSessionId = session.sessionId || ""
        root.resolverTitle = session.title || "Jogo"
        resolveCrashDialog.open()
    }
    function focusContent() { if (page === 2) createAccount.forceActiveFocus(); else sessionLog.forceActiveFocus() }
    spacing: 16
    Connections { target: compatibility; function onCrashHistoryChanged() { root.refreshCrashHistory() } }
    Component.onCompleted: root.refreshCrashHistory()
    FileDialog {
        id: photoPicker
        title: "Escolher foto da conta"
        nameFilters: ["Imagens (*.png *.jpg *.jpeg *.webp)"]
        onVisibleChanged: controllers.popupActive = visible
        onAccepted: root.profileFeedback = libraryResources.setProfilePhoto(root.editingUuid, selectedFile) ? "Atualizando foto…" : "Não foi possível usar esta imagem."
    }
    Dialog {
        id: profileDialog
        property bool creating: false
        title: creating ? "Criar conta" : "Renomear conta"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay
        onOpened: nameField.forceActiveFocus()
        onAccepted: root.profileFeedback = (creating ? libraryResources.createProfile(nameField.text) : libraryResources.renameProfile(root.editingUuid, nameField.text)) ? "Atualizando contas…" : "Encerre a sessão e aguarde a leitura das contas."
        contentItem: ColumnLayout {
            Label { text: "Nome (até 31 bytes UTF-8)" }
            Field { id: nameField; Layout.preferredWidth: 300 }
        }
    }
    Dialog {
        id: resolveCrashDialog
        objectName: "resolveCrashDialog"
        title: "Tentar resolver problema?"
        modal: true
        standardButtons: Dialog.NoButton
        width: 560
        anchors.centerIn: Overlay.overlay
        onVisibleChanged: controllers.popupActive = visible
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: "Falha registrada em “" + root.resolverTitle + "”. Deseja tentar resolver?"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label { text: "A tentativa não muda configurações automaticamente. Você pode revisar as configurações específicas do jogo ou consultar os logs desta sessão."; color: "#b9b0c4"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                ActionButton { text: "Agora não"; onClicked: { compatibility.markResolutionRequested(root.resolverSessionId, false); resolveCrashDialog.close() } }
                Item { Layout.fillWidth: true }
                ActionButton { text: "Abrir logs"; onClicked: { compatibility.markResolutionRequested(root.resolverSessionId, true); resolveCrashDialog.close(); sessionLog.forceActiveFocus() } }
                ActionButton { text: "Configurações do jogo"; primary: true; enabled: appModel.games.some(function(item) { return item.titleId === root.resolverTitleId })
                    onClicked: {
                        compatibility.markResolutionRequested(root.resolverSessionId, true)
                        root.shell.page = 0
                        Qt.callLater(function() { root.libraryPage.openGameSettingsForTitleId(root.resolverTitleId) })
                        resolveCrashDialog.close()
                    }
                }
            }
        }
    }
    Label { text: root.page === 2 ? "Perfis" : "Diagnósticos"; font.pixelSize: 28; font.bold: true }
    Panel { visible: root.page === 2; Layout.fillWidth: true
        Label { text: "Contas do console"; font.pixelSize: 20; font.bold: true }
        Label { text: "Escolha a conta usada no próximo jogo. Também é possível criar contas locais e editar o nome e a foto."; color: "#b9b0c4"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        RowLayout { Layout.fillWidth: true
            ActionButton { id: createAccount; text: "Criar conta"; primary: true; enabled: !libraryResources.busy && !emulation.active && libraryResources.profiles.length < 8; onClicked: { profileDialog.creating = true; nameField.text = ""; profileDialog.open() } }
            ActionButton { text: libraryResources.busy ? "Atualizando..." : "Atualizar contas"; enabled: !libraryResources.busy; onClicked: libraryResources.refresh() }
            Label { visible: libraryResources.profiles.length >= 8; text: "Limite de 8 contas atingido"; color: "#b9a7d2" }
        }
        Label { visible: root.profileFeedback.length > 0; text: root.profileFeedback; color: "#d4afff"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: libraryResources.status; visible: text.length > 0; color: "#a994bc"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    ScrollView { visible: root.page === 2; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
        ColumnLayout { width: parent.width; spacing: 10
            Repeater { model: libraryResources.profiles
                Panel {
                    required property var modelData
                    required property int index
                    readonly property bool activeProfile: preferences.values["system/userUuid"]
                        ? preferences.values["system/userUuid"] === modelData.uuid : index === 0
                    Layout.fillWidth: true
                    RowLayout { Layout.fillWidth: true; spacing: 14
                        Rectangle { Layout.preferredWidth: 56; Layout.preferredHeight: 56; radius: 28; color: "#252033"; border.color: activeProfile ? "#c68aff" : "#514265"
                            Image { anchors.fill: parent; anchors.margins: 2; source: modelData.photo ? "file:///" + modelData.photo.replace(/\\/g, "/") : ""; fillMode: Image.PreserveAspectCrop }
                            Label { anchors.centerIn: parent; visible: !modelData.photo; text: modelData.name.length ? modelData.name.charAt(0).toUpperCase() : "?"; color: "#d4afff"; font.pixelSize: 22; font.bold: true }
                        }
                        ColumnLayout { Layout.fillWidth: true; spacing: 3
                            Label { text: modelData.name; color: activeProfile ? "#e2c5ff" : "#eee5ff"; font.pixelSize: 17; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: activeProfile ? "Selecionada para o próximo jogo" : "Conta local do console"; color: "#b9a7d2"; font.pixelSize: 12 }
                            Label { text: modelData.uuid; color: "#82778f"; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideMiddle }
                        }
                    }
                    RowLayout { Layout.fillWidth: true
                        ActionButton { text: activeProfile ? "Selecionada" : "Usar no próximo jogo"; primary: !activeProfile; enabled: !activeProfile && !emulation.active; onClicked: root.profileFeedback = libraryResources.selectProfile(modelData.uuid) ? "Conta selecionada para o próximo jogo." : "Não foi possível selecionar esta conta." }
                        ActionButton { text: "Alterar nome"; enabled: !libraryResources.busy && !emulation.active; onClicked: { root.editingUuid = modelData.uuid; profileDialog.creating = false; nameField.text = modelData.name; profileDialog.open() } }
                        ActionButton { text: "Alterar foto"; enabled: !libraryResources.busy && !emulation.active; onClicked: { root.editingUuid = modelData.uuid; photoPicker.open() } }
                    }
                }
            }
            Label { visible: libraryResources.profiles.length === 0 && !libraryResources.busy; text: "Nenhuma conta encontrada. Atualize para ler as contas locais."; color: "#b9b0c4" }
        }
    }
    Panel { visible: root.page === 3; Layout.fillWidth: true
        Label { text: "Histórico de falhas por jogo"; font.pixelSize: 20; font.bold: true }
        Label { text: "Falhas do núcleo e sessões interrompidas inesperadamente. Um primeiro frame não é classificado como compatibilidade."; color: "#b9b0c4"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        ScrollView { visible: root.crashItems.length > 0; Layout.fillWidth: true; Layout.preferredHeight: 260; clip: true
            ColumnLayout { width: parent.width; spacing: 8
                Repeater { model: root.crashItems
                    Panel {
                        required property var modelData
                        Layout.fillWidth: true
                        ColumnLayout { Layout.fillWidth: true; spacing: 7
                            Label { text: modelData.title || "Jogo sem nome"; font.pixelSize: 16; font.bold: true; color: "#f0e5ff"; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: modelData.crashes.length + (modelData.crashes.length === 1 ? " falha registrada" : " falhas registradas"); color: "#c9b6dd" }
                            Repeater { model: modelData.crashes
                                ColumnLayout { required property var modelData; Layout.fillWidth: true; spacing: 5
                                    Rectangle { Layout.fillWidth: true; height: 1; color: "#3a2d4b" }
                                    Label { text: (modelData.outcome === "interrupted-or-app-closed" ? "Sessão interrompida" : "Falha do núcleo") + (modelData.startedUtc ? " · " + modelData.startedUtc : "") + " · Shireia " + (modelData.emulatorVersion || "versão desconhecida"); color: "#c9b6dd"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                    Label { visible: !!modelData.detail || (modelData.events && modelData.events.length > 0); text: modelData.detail || (modelData.events && modelData.events.length ? modelData.events[modelData.events.length - 1].detail : ""); color: "#ffb2bd"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                    RowLayout { Layout.fillWidth: true
                                        Label { Layout.fillWidth: true; text: modelData.resolutionRequested ? "Tentativa de solução solicitada" : modelData.resolutionDeclined ? "Solução adiada" : "Deseja tentar resolver?"; color: "#b9a7d2"; wrapMode: Text.WordWrap }
                                        ActionButton { text: modelData.resolutionRequested ? "Tentar novamente" : "Tentar resolver"; primary: true; onClicked: root.openCrashResolver(modelData) }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        Label { visible: root.crashItems.length === 0; text: "Nenhuma falha de jogo registrada."; color: "#a995bf"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "Supervisão do núcleo"; font.pixelSize: 20 }
        Label { text: "Estado: " + emulation.state; color: "#d0abf3" }
        Label { text: "FPS do compositor: " + Number(emulation.performanceMetrics.compositorFps || 0).toFixed(1) + " · frames publicados: " + Number(emulation.performanceMetrics.publishedFps || 0).toFixed(1); color: "#cbb5df"; visible: emulation.gameLoaded }
        Label { text: "Erro gráfico: " + emulation.graphicsError; color: "#ff9aa8"; visible: emulation.graphicsError.length > 0; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "Capture durante uma cena de jogo estável por 30–60 segundos; evite menus e telas de carregamento."; color: "#b9b0c4"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Toggle {
            text: "Sonda Vulkan: registrar causas de quadros acima de 16,7 ms (requer reiniciar o núcleo)"
            checked: emulation.stallProbeEnabled
            enabled: !emulation.gameLoaded && (!emulation.active || emulation.state === "Ready")
            Layout.fillWidth: true
            onToggled: emulation.stallProbeEnabled = checked
        }
        Label { text: emulation.state === "Ready" && emulation.active ? "O núcleo ocioso será reiniciado automaticamente para aplicar esta alteração." : "A sonda só é aplicada ao iniciar o núcleo."; color: "#9989aa"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "A sonda mede esperas de pipelines, shaders, agendador e GPU. Ela não altera as opções gráficas; deixe-a desligada durante o uso normal."; color: "#9989aa"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        RowLayout {
            ActionButton { text: emulation.performanceCaptureActive ? "Capturando…" : "Iniciar captura de desempenho"; primary: true; enabled: emulation.state === "Running" && !emulation.performanceCaptureActive; onClicked: emulation.startPerformanceCapture() }
            ActionButton { text: "Parar e salvar"; enabled: emulation.performanceCaptureActive; onClicked: emulation.stopPerformanceCapture() }
        }
        RowLayout {
            ActionButton { text: "Exportar diagnóstico sanitizado"; primary: true; onClicked: emulation.exportDiagnostics() }
            ActionButton { text: "Abrir pasta de screenshots"; onClicked: qualityTools.openScreenshotRoot() }
        }
        Label { visible: emulation.diagnosticsExportPath.length > 0; text: "Relatório: " + emulation.diagnosticsExportPath; color: "#d4afff"; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
        Label { visible: emulation.performanceCapturePath.length > 0; text: "Arquivo: " + emulation.performanceCapturePath; color: "#d4afff"; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
        Repeater { model: emulation.exits
            Label { required property var modelData; text: modelData.time + " · " + modelData.status + " · código " + modelData.code; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
    ScrollView { visible: root.page === 3; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
        TextArea { id: sessionLog; text: emulation.log.join("\n"); readOnly: true; wrapMode: TextEdit.WrapAnywhere; color: "#c2a7d9"; background: Rectangle { color: "#15101f"; radius: 4 } }
    }
}
