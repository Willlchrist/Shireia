import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"

Item {
    id: root
    objectName: "setupWizard"
    property var assistant
    readonly property var steps: ["Boas-vindas", "Aviso legal", "Chaves", "Firmware", "Biblioteca", "Concluído"]

    function focusCurrent() {
        if (assistant.step === 0) startButton.forceActiveFocus()
        else if (assistant.step === 1) legalCheck.forceActiveFocus()
        else if (assistant.step === 2) keysButton.forceActiveFocus()
        else if (assistant.step === 3) firmwareFolderButton.forceActiveFocus()
        else if (assistant.step === 4) gamesFolderButton.forceActiveFocus()
        else finishButton.forceActiveFocus()
    }
    Component.onCompleted: Qt.callLater(focusCurrent)
    Connections { target: assistant; function onChanged() { Qt.callLater(root.focusCurrent) } }

    FileDialog {
        id: keysPicker
        title: "Selecionar seu arquivo local prod.keys"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Chaves de produção (prod.keys)"]
        onVisibleChanged: controllers.popupActive = visible
        onAccepted: assistant.selectKeys(selectedFile)
    }
    FileDialog {
        id: firmwareZipPicker
        title: "Selecionar pacote local de firmware"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Pacote ZIP de firmware (*.zip)"]
        onVisibleChanged: controllers.popupActive = visible
        onAccepted: assistant.selectFirmware(selectedFile)
    }
    FolderDialog {
        id: firmwareFolderPicker
        title: "Selecionar pasta de firmware extraído (.nca)"
        onVisibleChanged: controllers.popupActive = visible
        onAccepted: assistant.selectFirmware(selectedFolder)
    }
    FolderDialog {
        id: gamesFolderPicker
        title: "Selecionar pasta de jogos"
        onVisibleChanged: controllers.popupActive = visible
        onAccepted: assistant.addGamesFolder(selectedFolder)
    }

    Rectangle { anchors.fill: parent; color: "#0b0b10" }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18
        Brand { Layout.alignment: Qt.AlignHCenter }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 9
            Repeater {
                model: root.steps
                RowLayout {
                    required property string modelData
                    required property int index
                    spacing: 6
                    Rectangle { width: 24; height: 24; radius: 12; color: index <= assistant.step ? "#7934cd" : "#242035"; border.color: index === assistant.step ? "#d0a5ff" : "#514265"
                        Label { anchors.centerIn: parent; text: String(index + 1); font.pixelSize: 11; color: "#fff" }
                    }
                    Label { text: modelData; visible: root.width > 900; color: index === assistant.step ? "#e7ddff" : "#998da8"; font.pixelSize: 12 }
                }
            }
        }
        Panel {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: 940
            contentSpacing: 16
                Label { text: root.steps[assistant.step]; font.pixelSize: 26; font.bold: true; color: "#f0e5ff" }
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: assistant.step

                    ColumnLayout {
                        spacing: 14
                        Item { Layout.fillHeight: true }
                        Image { source: "qrc:/assets/brand/shireia-logo.png"; Layout.preferredWidth: 112; Layout.preferredHeight: 112; Layout.alignment: Qt.AlignHCenter; fillMode: Image.PreserveAspectFit; smooth: true }
                        Label { text: "Shireia Next"; font.pixelSize: 30; font.bold: true; color: "#f0e5ff"; Layout.alignment: Qt.AlignHCenter }
                        Label { text: "Emulação independente para compatibilidade, preservação e pesquisa tecnológica."; color: "#c9b6dd"; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
                        Label { text: "Antes de abrir a biblioteca, vamos confirmar um aviso legal e configurar os recursos locais necessários ao núcleo."; color: "#b9a7d2"; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
                        RowLayout { Layout.alignment: Qt.AlignHCenter
                            ActionButton { id: startButton; objectName: "setupStartButton"; text: "Começar configuração"; primary: true; onClicked: assistant.begin() }
                            ActionButton { text: "Fechar aplicativo"; onClicked: Qt.quit() }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: 10
                        Label { text: "Aviso legal — Shireia Next"; font.pixelSize: 18; font.bold: true; color: "#e7ddff" }
                        Flickable {
                            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                            contentWidth: width; contentHeight: legalText.implicitHeight + 16
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            Label {
                                id: legalText
                                width: parent.width - 16
                                wrapMode: Text.WordWrap
                                color: "#d1c5df"
                                text: "O Shireia Next é um projeto independente de emulação e não fornece, distribui, hospeda ou incentiva a obtenção não autorizada de jogos, firmware ou chaves criptográficas (keys).\n\nA obtenção, a procedência e a utilização dos arquivos adicionados ao emulador são de responsabilidade do usuário, conforme a legislação aplicável.\n\nO Shireia Next não verifica a origem dos arquivos nem exige comprovantes de propriedade."
                            }
                        }
                        CheckBox {
                            id: legalCheck
                            objectName: "setupLegalCheck"
                            text: "Li e compreendi o aviso. Reconheço minha responsabilidade pela origem e utilização dos arquivos que fornecer ao Shireia Next."
                            checked: false
                            focusPolicy: Qt.StrongFocus
                            Layout.fillWidth: true
                            contentItem: Text {
                                text: legalCheck.text
                                font: legalCheck.font
                                color: legalCheck.enabled ? "#d1c5df" : "#746b89"
                                wrapMode: Text.WordWrap
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: legalCheck.indicator.width + legalCheck.spacing
                            }
                        }
                        Label { text: "A confirmação é registrada localmente com a versão do aviso e a data; ela não verifica a procedência dos arquivos."; color: "#9f91af"; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11 }
                    }

                    ColumnLayout {
                        spacing: 14
                        Label { text: "O Shireia não fornece keys. Selecione voluntariamente um arquivo local prod.keys. As chaves serão validadas junto com o firmware na próxima etapa."; color: "#c9b6dd"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        RowLayout { Layout.fillWidth: true
                            ActionButton { id: keysButton; text: "Selecionar prod.keys…"; primary: true; enabled: !assistant.busy; onClicked: keysPicker.open() }
                            Label { text: assistant.keysValidated ? "Chaves reconhecidas pelo núcleo" : assistant.keysSelected ? "Arquivo selecionado; validação na etapa do firmware" : "Selecione um arquivo prod.keys"; color: assistant.keysValidated ? "#9be0b1" : "#c9b6dd"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                        Label { text: "Formato aceito: arquivo local chamado prod.keys. Chaves e firmware serão verificados juntos na próxima etapa."; color: "#9f91af"; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: 14
                        Label { text: "O firmware não acompanha o Shireia. Escolha um ZIP de firmware ou uma pasta local com arquivos NCA extraídos. A importação usa o núcleo e preserva a instalação anterior se ocorrer um erro."; color: "#c9b6dd"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        RowLayout { Layout.fillWidth: true
                            ActionButton { id: firmwareFolderButton; text: "Selecionar pasta de NCA…"; enabled: assistant.keysSelected && !assistant.busy; onClicked: firmwareFolderPicker.open() }
                            ActionButton { text: "Selecionar ZIP…"; enabled: assistant.keysSelected && !assistant.busy; onClicked: firmwareZipPicker.open() }
                        }
                        RowLayout { Layout.fillWidth: true
                            Label { text: assistant.firmwareValidated ? "Firmware importado e reconhecido pelo núcleo" : assistant.resourceCheckComplete ? "Firmware ausente ou não reconhecido" : "Aguardando validação do núcleo"; color: assistant.firmwareValidated ? "#9be0b1" : "#c9b6dd"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                        Label { visible: !assistant.firmwareValidated && assistant.firmwareResult.length > 0; text: assistant.firmwareResult; color: "#ffb2bd"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Label { text: "A instalação ocorre na área privada do Shireia; os arquivos de origem permanecem intactos. O verificador atual não fornece uma versão de firmware."; color: "#9f91af"; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: 14
                        Label { text: "A biblioteca é opcional. Você pode escolher uma pasta local agora ou pular e adicioná-la depois na biblioteca."; color: "#c9b6dd"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Label { text: appModel.folders.length ? "Pastas configuradas: " + appModel.folders.length : "Nenhuma pasta de jogos configurada."; color: "#e0d3ee" }
                        RowLayout { Layout.alignment: Qt.AlignHCenter
                            ActionButton { id: gamesFolderButton; text: "Selecionar pasta de jogos"; primary: true; onClicked: gamesFolderPicker.open() }
                            ActionButton { text: "Pular por enquanto"; onClicked: assistant.skipGames() }
                        }
                        Label { text: "São adicionadas somente pastas selecionadas por você. Os arquivos não são movidos, copiados ou excluídos."; color: "#9f91af"; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: 14
                        Label { text: "A configuração obrigatória foi validada pelo núcleo."; color: "#9be0b1"; font.pixelSize: 17; font.bold: true }
                        Label { text: "Aviso legal: confirmado em " + assistant.acceptedAt; color: "#d1c5df"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Label { text: "Keys: " + (assistant.keysValidated ? "reconhecidas" : "pendente") + " · conteúdo não exibido"; color: "#d1c5df"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Label { text: "Firmware: " + (assistant.firmwareValidated ? "instalado e reconhecido" : "pendente"); color: "#d1c5df"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Label { text: appModel.folders.length ? "Biblioteca: " + appModel.folders.length + " pasta(s) selecionada(s)" : "Biblioteca: ignorada por enquanto"; color: "#d1c5df"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Label { visible: assistant.error.length > 0; text: assistant.error; color: "#ff9aa8"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                    }
                }
                Label { visible: assistant.error.length > 0 && assistant.step < 5; text: assistant.error; color: "#ff9aa8"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    Layout.fillWidth: true
                    ActionButton { text: assistant.step === 1 ? "Recusar e sair" : "Fechar aplicativo"; visible: assistant.step === 0 || assistant.step === 1; onClicked: Qt.quit() }
                    ActionButton { text: "Voltar"; visible: assistant.step > 0 && assistant.step < 5; onClicked: assistant.back() }
                    Item { Layout.fillWidth: true }
                    ActionButton { objectName: "setupAgreeButton"; text: "Concordo e continuar"; visible: assistant.step === 1; primary: true; enabled: legalCheck.checked; onClicked: assistant.acceptLegal(legalCheck.checked) }
                    ActionButton { text: "Continuar"; visible: assistant.step === 2; primary: true; enabled: assistant.keysSelected && !assistant.busy; onClicked: assistant.continueFromKeys() }
                    ActionButton { text: "Continuar"; visible: assistant.step === 3; primary: true; enabled: assistant.firmwareValidated && !assistant.busy; onClicked: assistant.continueFromFirmware() }
                    ActionButton { text: "Concluir etapa opcional"; visible: assistant.step === 4; primary: true; onClicked: assistant.skipGames() }
                    ActionButton { id: finishButton; text: "Abrir Shireia Next"; visible: assistant.step === 5; primary: true; enabled: assistant.keysValidated && assistant.firmwareValidated; onClicked: assistant.finish() }
                }
        }
    }
    Rectangle {
        anchors.fill: parent
        z: 1000
        visible: assistant.busy
        color: "#d90b0b10"
        Column {
            anchors.centerIn: parent
            spacing: 16
            BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: assistant.busy; width: 48; height: 48 }
            Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Analisando keys e firmware…"; color: "#f0e5ff"; font.pixelSize: 18; font.bold: true }
            Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Validando recursos locais com o núcleo original. Isso pode levar alguns instantes."; color: "#c9b6dd"; font.pixelSize: 13 }
        }
    }
}
