import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"

FocusScope {
    id: root
    objectName: "settingsPage"
    property int category: 0
    readonly property var categories: ["Geral", "Sistema", "Gráficos", "Áudio", "Controles", "Online", "Interface", "Emulador", "Logs", "Gráficos avançados"]
    readonly property var symbols: ["settings", "system", "graphics", "audio", "controls", "online", "interface", "system", "logs", "graphics"]
    property string feedback: ""
    property int controlSection: 0
    readonly property string legalNoticeText: "O Shireia Next é um projeto independente de emulação e não fornece, distribui, hospeda ou incentiva a obtenção não autorizada de jogos, firmware ou chaves criptográficas (keys).\n\nA obtenção, a procedência e a utilização dos arquivos adicionados ao emulador são de responsabilidade do usuário, conforme a legislação aplicável.\n\nO Shireia Next não verifica a origem dos arquivos nem exige comprovantes de propriedade."
    Dialog {
        id: deleteProfileDialog
        width: 360
        title: "Excluir perfil de controle?"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay
        contentItem: Label { text: "O perfil ‘" + savedGameProfile.currentText + "’ será removido permanentemente. Isso não altera o mapeamento atualmente aplicado."; wrapMode: Text.WordWrap; padding: 18 }
        onAccepted: controllers.deleteGameProfile(savedGameProfile.currentText)
    }
    Dialog {
        id: legalNoticeDialog
        objectName: "legalNoticeDialog"
        width: Math.max(280, Math.min(700, root.width - 40))
        height: Math.min(Math.max(260, root.height - 40), Math.max(280, legalNoticeScroll.contentHeight + 120))
        title: "Aviso legal — Shireia Next"
        modal: true
        standardButtons: Dialog.NoButton
        anchors.centerIn: Overlay.overlay
        contentItem: ScrollView {
            id: legalNoticeScroll
            clip: true
            contentWidth: availableWidth
            contentHeight: legalNoticeTextItem.implicitHeight + 32
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            Text {
                id: legalNoticeTextItem
                objectName: "legalNoticeTextContent"
                x: 16
                y: 16
                width: Math.max(1, legalNoticeScroll.availableWidth - 32)
                height: implicitHeight
                text: root.legalNoticeText
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                font.pixelSize: 15
                color: "#e0d6ed"
            }
        }
        background: Rectangle { radius: 6; color: "#191620"; border.color: "#5d3b7c" }
        footer: RowLayout {
            Item { Layout.fillWidth: true }
            ActionButton { objectName: "legalNoticeCloseButton"; text: "Fechar"; primary: true; onClicked: legalNoticeDialog.close() }
        }
    }
    Dialog {
        id: licenseDialog
        objectName: "licensesDialog"
        width: Math.max(300, Math.min(900, root.width - 40))
        height: Math.max(300, Math.min(root.height - 40, licenseName.currentIndex === 0 ? 440 : 720))
        title: "Licenças e atribuições incluídas"
        modal: true
        standardButtons: Dialog.NoButton
        anchors.centerIn: Overlay.overlay
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                text: "Consulte os componentes incluídos e leia os textos integrais das licenças."
                color: "#cdbce0"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            ComboBox {
                id: licenseName
                objectName: "licenseNamePicker"
                Layout.fillWidth: true
                model: ["Componentes incluídos", "Suyu — GNU GPL v3 ou posterior", "FFmpeg — GNU GPL v3 ou posterior", "GNU GPL v2 ou posterior",
                    "GNU GPL v3 ou posterior", "Qt — GNU LGPL v3", "Qt — exceção GPL",
                    "GNU LGPL v2.1 ou posterior", "SDL 2 — licença zlib", "SDL 3 — licença zlib", "libarchive — licença BSD",
                    "OpenSSL — Apache 2.0", "zlib — licença zlib", "Boost — BSL 1.0",
                    "Dynarmic — 0BSD", "fmt — MIT", "ENet — MIT", "Sirit — GNU AGPL v3", "cubeb — ISC",
                    "MPL 2.0 — decodificador BC", "BSD 2 cláusulas — tz", "Apache 2.0 com exceção LLVM",
                    "Creative Commons Atribuição 4.0 — dados SPIR-V", "MIT — stb"]
            }
            ScrollView {
                id: licenseScroll
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                TextArea {
                    objectName: "licenseTextView"
                    width: licenseScroll.availableWidth
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    text: firstRunSetup.licenseText(licenseName.currentText)
                    font.family: licenseName.currentIndex === 0 ? "Segoe UI" : "Consolas"
                    font.pixelSize: licenseName.currentIndex === 0 ? 14 : 12
                    color: "#e0d6ed"
                    padding: 14
                    background: Rectangle { radius: 4; color: "#111018"; border.color: "#342d4f" }
                }
            }
        }
        background: Rectangle { radius: 6; color: "#191620"; border.color: "#5d3b7c" }
        footer: RowLayout {
            Item { Layout.fillWidth: true }
            ActionButton { objectName: "licensesCloseButton"; text: "Fechar"; primary: true; onClicked: licenseDialog.close() }
        }
    }
    signal closeRequested()
    signal categoryRequested(int index)
    function apply() {
        if (!preferences.apply()) { feedback = "Não foi possível salvar as configurações."; return false }
        controllers.applyBindings()
        feedback = "Configurações salvas."
        return true
    }
    function cancel() { preferences.cancel(); controllers.cancelBindings(); feedback = "" }
    function focusContent() { firstControl.forceActiveFocus() }
    function reveal(item) {
        if (!item || !item.visible) return
        let ancestor = item
        while (ancestor && ancestor !== scroller.contentItem) ancestor = ancestor.parent
        if (!ancestor) return
        const pos = item.mapToItem(scroller.contentItem, 0, 0)
        if (pos.y < scroller.contentY) scroller.contentY = Math.max(0, pos.y - 12)
        else if (pos.y + item.height > scroller.contentY + scroller.height) scroller.contentY = Math.min(scroller.contentHeight - scroller.height, pos.y + item.height - scroller.height + 18)
    }
    FolderDialog { id: folderPicker; title: "Adicionar pasta à configuração"; onVisibleChanged: controllers.popupActive = visible; onAccepted: preferences.addFolder(selectedFolder) }
    FileDialog {
        id: keysPicker
        title: "Selecionar prod.keys"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Chaves de produção (prod.keys)"]
        onVisibleChanged: controllers.popupActive = visible
        onAccepted: {
            const error = preferences.selectProdKeys(selectedFile)
            root.feedback = error || "prod.keys selecionado. Aplicar importa as chaves e atualiza a biblioteca."
        }
    }
    FolderDialog {
        id: firmwarePicker
        title: "Selecionar pasta de firmware extraído (.nca)"
        onVisibleChanged: controllers.popupActive = visible
        onAccepted: {
            const error = preferences.selectFirmware(selectedFolder)
            root.feedback = error || "Pasta selecionada. Aplicar analisa e importa o firmware."
        }
    }
    FileDialog {
        id: firmwareZipPicker
        title: "Selecionar firmware ZIP"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Firmware ZIP (*.zip)"]
        onVisibleChanged: controllers.popupActive = visible
        onAccepted: {
            const error = preferences.selectFirmware(selectedFile)
            root.feedback = error || "ZIP selecionado. Aplicar analisa e importa o firmware."
        }
    }
    FileDialog {
        id: contentPicker
        title: "Instalar update ou DLC NSP na NAND privada"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Pacotes NSP (*.nsp)"]
        onVisibleChanged: controllers.popupActive = visible
        onAccepted: root.feedback = libraryResources.installContent(selectedFile)
            ? "Instalação iniciada. O estado aparece abaixo." : "Selecione um NSP legível e encerre o jogo antes de instalar."
    }
    onCategoryChanged: { scroller.contentY = 0; feedback = "" }
    ColumnLayout {
        anchors.fill: parent; spacing: 16
        RowLayout {
            spacing: 16
            Rectangle { width: 58; height: 58; radius: 12; color: "#332047"; border.color: "#65348c"; Glyph { anchors.centerIn: parent; name: root.symbols[root.category]; font.pixelSize: 34 } }
            ColumnLayout {
                Label { text: root.categories[root.category]; font.pixelSize: 30; font.bold: true }
                Label { text: "Configurações do Shireia Next"; color: "#b9a7d2" }
            }
            Item { Layout.fillWidth: true }
            Field {
                id: firstControl
                Layout.preferredWidth: Math.min(300, root.width * 0.35)
                placeholderText: "\u2315  Buscar nas configura\u00e7\u00f5es\u2026"
                onTextEdited: {
                    const keywords = ["geral idioma aparencia diretorios pasta jogos comportamento", "sistema regiao relogio prod.keys keys chaves firmware", "graficos vulkan resolucao upscaling fsr", "audio volume dispositivo", "controles remapear joystick gamepad", "online rede", "interface tamanho miniaturas grade lista tema ordenar", "emulador nucleo pausa", "logs registros depuracao debug", "avancado hacks fast gpu cpu overclock ticks"]
                    if (!text.trim()) return
                    const index = keywords.findIndex(function(value) { return value.indexOf(firstControl.text.toLowerCase()) >= 0 })
                    if (index >= 0) root.categoryRequested(index)
                    root.feedback = index >= 0 ? "Categoria encontrada: " + root.categories[index] : "Nenhuma categoria encontrada."
                }
            }
        }
        Flickable {
            Layout.fillWidth: true; Layout.preferredHeight: 42
            contentWidth: categoryRow.implicitWidth; contentHeight: height; clip: true
            boundsBehavior: Flickable.StopAtBounds
            RowLayout { id: categoryRow; height: parent.height; spacing: 4
                Repeater { model: root.categories
                    ActionButton { required property string modelData; required property int index
                        text: modelData; checked: root.category === index; implicitHeight: 36
                        onClicked: root.categoryRequested(index)
                    }
                }
            }
        }
        Flickable {
            id: scroller
            objectName: "settingsScroll"
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; contentWidth: width; contentHeight: content.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                id: content
                width: scroller.width; spacing: 14
                Panel {
                    Layout.fillWidth: true; visible: root.category === 1
                    Label { text: "Chaves e Firmware"; font.pixelSize: 20; font.bold: true }
                    Label { text: "Estado validado pelo núcleo: keys " + (libraryResources.keysLoaded ? "reconhecidas" : libraryResources.validationComplete ? "ausentes ou inválidas" : "aguardando validação") + " · firmware " + (libraryResources.firmwareReady ? "reconhecido" : libraryResources.validationComplete ? "ausente ou inválido" : "aguardando validação"); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: libraryResources.keysLoaded && libraryResources.firmwareReady ? "#9be0b1" : "#c9b6dd" }
                    RowLayout {
                        Layout.fillWidth: true
                        ActionButton { objectName: "legalNoticeButton"; text: "Consultar aviso legal"; onClicked: legalNoticeDialog.open() }
                        ActionButton { objectName: "licensesButton"; text: "Licenças e atribuições"; onClicked: licenseDialog.open() }
                    }
                    Label { text: "Arquivo prod.keys"; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true
                        Field { Layout.fillWidth: true; readOnly: true; text: preferences.draft["system/prodKeys"]; placeholderText: "Nenhum arquivo selecionado"; Accessible.name: "Caminho de prod.keys" }
                        ActionButton { text: "Selecionar arquivo…"; onClicked: keysPicker.open() }
                        ActionButton { text: "Limpar"; enabled: preferences.draft["system/prodKeys"] !== ""; onClicked: preferences.setValue("system/prodKeys", "") }
                    }
                    Label { text: "Firmware extraído (.nca) ou ZIP"; font.bold: true }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Field { Layout.fillWidth: true; readOnly: true; text: preferences.draft["system/firmwareFolder"]; placeholderText: "Nenhum firmware selecionado"; Accessible.name: "Fonte de firmware" }
                        RowLayout {
                            ActionButton { text: "Selecionar pasta…"; onClicked: firmwarePicker.open() }
                            ActionButton { text: "Selecionar ZIP…"; onClicked: firmwareZipPicker.open() }
                            ActionButton { text: "Limpar"; enabled: preferences.draft["system/firmwareFolder"] !== ""; onClicked: preferences.setValue("system/firmwareFolder", "") }
                        }
                    }
                    Label {
                        text: "Aplicar/OK importa prod.keys e analisa/importa os arquivos .nca da pasta ou do ZIP na área própria do Shireia. Nomes e capas são atualizados quando legíveis pelo núcleo. Cancelar descarta a seleção. A importação aguarda o núcleo de sessão ser encerrado; os arquivos de origem são preservados."
                        Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2"
                    }
                    Label { text: libraryResources.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#d4afff" }
                    ActionButton { text: libraryResources.busy ? "Analisando…" : "Analisar e atualizar agora"; enabled: !libraryResources.busy; onClicked: libraryResources.refresh() }
                    Label { text: "Idioma do sistema emulado"; font.bold: true }
                    Select { Layout.fillWidth: true; model: ["Japonês", "Inglês (EUA)", "Francês", "Alemão", "Italiano", "Espanhol", "Chinês", "Coreano", "Holandês", "Português", "Russo", "Chinês (Taiwan)", "Inglês (Reino Unido)", "Francês (Canadá)", "Espanhol (América Latina)", "Chinês simplificado", "Chinês tradicional", "Português (Brasil)", "Polonês", "Tailandês"]; currentIndex: preferences.draft["system/language"]; onActivated: preferences.setValue("system/language", currentIndex) }
                    Label { text: "Região do sistema"; font.bold: true }
                    Select { Layout.fillWidth: true; model: ["Japão", "EUA", "Europa", "Austrália", "China", "Coreia", "Taiwan"]; currentIndex: preferences.draft["system/region"]; onActivated: preferences.setValue("system/region", currentIndex) }
                    Label { text: "Idioma e região são aplicados ao iniciar o próximo jogo."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                }
                Panel {
                    Layout.fillWidth: true; visible: root.category === 0
                    Label { text: "Idioma e Aparência"; font.pixelSize: 18; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: "Idioma da Interface" }
                            Select { Layout.fillWidth: true; model: ["Português (Brasil)"]; enabled: false }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: "Tema da Interface" }
                            RowLayout {
                                ActionButton { text: "☀  Claro · futuro"; enabled: false; Layout.fillWidth: true }
                                ActionButton { text: "☾  Escuro"; checked: true; Layout.fillWidth: true }
                                ActionButton { text: "Automático \u00b7 futuro"; enabled: false; Layout.fillWidth: true }
                            }
                        }
                    }
                    Label { text: "Interface em português e tema escuro nesta fase."; color: "#9884ac"; font.pixelSize: 11 }
                }
                Panel {
                    Layout.fillWidth: true; visible: root.category === 0
                    Label { text: "Diretórios"; font.pixelSize: 18; font.bold: true }
                    Repeater {
                        model: preferences.draft["library/folders"]
                        RowLayout {
                            required property string modelData
                            required property int index
                            Layout.fillWidth: true
                            Glyph { name: "games" }
                            Field { text: modelData; readOnly: true; Layout.fillWidth: true; Accessible.name: "Pasta de jogos" }
                            ActionButton { text: "Abrir"; onClicked: preferences.openFolder(modelData) }
                            ActionButton { text: "Remover"; onClicked: preferences.removeFolder(index) }
                        }
                    }
                    RowLayout {
                        Label { text: "Pastas de jogos"; Layout.fillWidth: true; color: "#c2afd7" }
                        ActionButton { text: "＋  Adicionar pasta…"; primary: true; onClicked: folderPicker.open() }
                    }
                    RowLayout { Layout.fillWidth: true
                        Label { text: "Updates e DLC"; Layout.fillWidth: true }
                        ActionButton { text: "Instalar NSP…"; enabled: !libraryResources.busy && !emulation.gameLoaded; onClicked: contentPicker.open() }
                    }
                    Label { text: "O Suyu valida o NSP e instala somente update/DLC na NAND privada; jogos base são recusados. Um update existente pode ser substituído."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                    Repeater {
                        model: ["Save Data", "Screenshots"]
                        RowLayout {
                            required property string modelData
                            Layout.fillWidth: true
                            Label { text: modelData; Layout.preferredWidth: 175; color: "#93809f" }
                            Field { Layout.fillWidth: true; placeholderText: "Disponível após integração do núcleo"; enabled: false }
                            ActionButton { text: "…"; enabled: false }
                        }
                    }
                    Label { text: "Alterações de pastas entram em vigor ao aplicar. Varredura sem subpastas."; color: "#9f8bb3"; font.pixelSize: 11 }
                }
                Panel {
                    Layout.fillWidth: true; visible: root.category === 0 || root.category === 6
                    Label { text: "Comportamento e Lista de Jogos"; font.pixelSize: 18; font.bold: true }
                    RowLayout {
                        spacing: 22; Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            Toggle { text: "Verificar atualizações \u00b7 futuro"; enabled: false; Layout.fillWidth: true }
                            Toggle { text: "Iniciar em tela cheia"; checked: preferences.draft["ui/fullscreen"]; onToggled: preferences.setValue("ui/fullscreen", checked); Layout.fillWidth: true }
                            Toggle { text: "Confirmar ao fechar jogo · futuro"; enabled: false; Layout.fillWidth: true }
                            Toggle { text: "Pausar em segundo plano · futuro"; enabled: false; Layout.fillWidth: true }
                            Toggle { text: "Overlay de desempenho durante o jogo"; checked: preferences.draft["ui/showFrameRate"]; onToggled: preferences.setValue("ui/showFrameRate",checked); Layout.fillWidth: true }
                            Label { text: "Atalho para screenshot" }
                            Select { Layout.fillWidth: true; model: ["F12", "Ctrl+Shift+S", "Ctrl+P"]; currentIndex: ["F12", "Ctrl+Shift+S", "Ctrl+P"].indexOf(preferences.draft["input/screenshotShortcut"]); onActivated: preferences.setValue("input/screenshotShortcut", model[currentIndex]) }
                            Label { text: "Durante o jogo: Ctrl+Shift+F9 pausa/retoma, Ctrl+Shift+F10 encerra a sessão e Ctrl+Shift+F8 alterna o overlay. Esses atalhos ficam inativos quando a tecla final está mapeada para o jogo."; color: "#a08ab4"; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
                            Toggle { text: "Navegação por controle na interface"; checked: preferences.draft["ui/controller"]; onToggled: preferences.setValue("ui/controller", checked); Layout.fillWidth: true }
                            Toggle { text: "Transições suaves"; checked: preferences.draft["ui/animations"]; onToggled: preferences.setValue("ui/animations", checked); Layout.fillWidth: true }
                            Toggle { text: "Mostrar caminho nos detalhes"; checked: preferences.draft["ui/showPaths"]; onToggled: preferences.setValue("ui/showPaths", checked); Layout.fillWidth: true }
                        }
                        Panel {
                            Layout.preferredWidth: Math.max(280, content.width * 0.43)
                            Label { text: "Lista de Jogos"; font.bold: true }
                            Label { text: "Modo de visualização" }
                            Select { Layout.fillWidth: true; model: ["Grade (Miniaturas)", "Lista"]; currentIndex: preferences.draft["ui/view"]; onActivated: preferences.setValue("ui/view", currentIndex) }
                            Label { text: "Tamanho das miniaturas · " + preferences.draft["ui/thumb"] + " px" }
                            Slider {
                                id: thumbnailSlider
                                objectName: "thumbnailSlider"
                                Layout.fillWidth: true; from: 120; to: 240; stepSize: 10
                                value: preferences.draft["ui/thumb"]
                                onMoved: preferences.setValue("ui/thumb", Math.round(value))
                                background: Rectangle { x: thumbnailSlider.leftPadding; y: thumbnailSlider.topPadding + thumbnailSlider.availableHeight / 2 - 3; width: thumbnailSlider.availableWidth; height: 6; radius: 3; color: "#4c346d"; border.color: thumbnailSlider.activeFocus ? "#dbb4ff" : "transparent" }
                                handle: Rectangle { x: thumbnailSlider.leftPadding + thumbnailSlider.visualPosition * (thumbnailSlider.availableWidth - width); y: thumbnailSlider.topPadding + thumbnailSlider.availableHeight / 2 - height / 2; width: 20; height: 20; radius: 10; color: "#a651f4"; border.color: thumbnailSlider.activeFocus ? "#f0d7ff" : "#bb7fed"; border.width: 2 }
                            }
                            Label { text: "Ordenar por" }
                            Select { Layout.fillWidth: true; model: ["Nome (A–Z)", "Nome (Z–A)", "Consulta recente", "Tamanho"]; currentIndex: preferences.draft["ui/sort"]; onActivated: preferences.setValue("ui/sort", currentIndex) }
                            Label { text: "Tela cheia é aplicada na próxima inicialização."; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#9f8bb3"; font.pixelSize: 11 }
                        }
                    }
                }
                Panel {
                    Layout.fillWidth: true; visible: root.category === 4
                    Label { text: "Controles"; font.pixelSize: 20; font.bold: true }
                    Label { text: "Configure separadamente a navegação do Shireia e os comandos enviados ao jogo. Alterações ficam pendentes até Aplicar."; color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    RowLayout { Layout.fillWidth: true
                        ActionButton { text: "Interface"; primary: root.controlSection === 0; onClicked: root.controlSection = 0 }
                        ActionButton { text: "Jogo"; primary: root.controlSection === 1; onClicked: root.controlSection = 1 }
                        ActionButton { text: "Perfis"; primary: root.controlSection === 2; onClicked: root.controlSection = 2 }
                    }
                    Label { visible: root.controlSection === 0; text: "Interface \u00b7 controle físico e atalhos"; font.pixelSize: 16; font.bold: true }
                    Label { visible: root.controlSection === 1; text: "Jogo · controle e teclado"; font.pixelSize: 16; font.bold: true }
                    Label { visible: root.controlSection === 2; text: "Perfis · reutilize seus mapeamentos"; font.pixelSize: 16; font.bold: true }
                    Label { text: "Controles físicos \u00b7 SDL2"; font.pixelSize: 20; font.bold: true }
                    ColumnLayout { visible: root.controlSection === 0; Layout.fillWidth: true; spacing: 12
                    Label { text: "Abrir menu durante a execução (teclado)" }
                    Select { model: ["F1", "F2", "F3", "F4"]; currentIndex: preferences.draft["input/gameMenuKey"]; onActivated: preferences.setValue("input/gameMenuKey",currentIndex) }
                    Label { text: "No controle: botão Guide/Home (remapeável abaixo). Entradas são neutralizadas ao abrir o menu ou perder foco."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                    Label { text: controllers.status; color: "#c4a5e4"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Select { Layout.fillWidth: true; model: controllers.devices; currentIndex: controllers.selected; enabled: controllers.devices.length > 0; onActivated: controllers.selected = currentIndex }
                    Label { text: "Xbox e PlayStation usam os mapeamentos SDL disponíveis. Genéricos usam hat, eixos 0/1 e botões remapeáveis. D-pad/analógico: navegar. LB/RB: percorrer todos os campos."; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#ac94c2" }
                    Toggle { text: "Navegação por controle na interface"; checked: preferences.draft["ui/controller"]; onToggled: preferences.setValue("ui/controller", checked) }
                    Label { text: controllers.capturing ? "Pressione um botão para: " + controllers.capturing + " (10 segundos). Esc cancela." : "Remapeamento por modelo de controle (GUID). Aplicar salva; Cancelar descarta."; color: "#d5abff"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Repeater {
                        model: controllers.bindings
                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Label { text: modelData.label; Layout.fillWidth: true }
                            Label { text: modelData.button; color: "#ba9ddb"; Layout.preferredWidth: 130 }
                            ActionButton { text: "Remapear"; onClicked: controllers.capture(modelData.action) }
                        }
                    }
                    RowLayout {
                        ActionButton { text: "Restaurar botões"; enabled: controllers.devices.length > 0; onClicked: controllers.resetBindings() }
                        ActionButton { text: "Cancelar captura"; visible: controllers.capturing !== ""; onClicked: controllers.cancelCapture() }
                    }
                    Label { text: "Zona morta: 55% para ativar / 35% para liberar. Repetição: 350 ms, depois 110 ms. Captura suspensa sem foco na janela."; color: "#a08ab4"; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
                    }
                    ColumnLayout { visible: root.controlSection === 1; Layout.fillWidth: true; spacing: 12
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#3a2d4b" }
                    Label { text: "Controles do jogo · Suyu VirtualGamepad"; font.pixelSize: 18; font.bold: true }
                    Label { text: "Botões lidos por posição. Xbox A (baixo) corresponde ao B do Switch; Xbox B (direita) corresponde ao A. Controles Nintendo e 8BitDo seguem a mesma posição física."; color: "#b9a7d2"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Label { text: "Correspondência dos botões frontais" }
                    Select { Layout.fillWidth: true; enabled: controllers.devices.length > 0; model: ["Posição Switch", "Mesmo rótulo Xbox"]; currentIndex: controllers.faceLayout; onActivated: controllers.faceLayout = currentIndex }
                    Label { text: "Pressione Remapear e depois o botão, gatilho, direcional ou eixo desejado. Guide/Home permanece reservado ao menu do Shireia."; color: "#a994bc"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "Multijogador local: o teclado e o controle selecionado comandam o jogador 1; o próximo controle conectado comanda o jogador 2 (na ordem detectada pelo SDL)."; color: "#b9a7d2"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Repeater {
                        model: controllers.gameBindings
                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Label { text: modelData.label; Layout.fillWidth: true }
                            Label { text: modelData.source; color: "#ba9ddb"; Layout.preferredWidth: 130; elide: Text.ElideRight }
                            ActionButton { text: "Remapear"; onClicked: controllers.captureGame(modelData.action) }
                        }
                    }
                    RowLayout {
                        ActionButton { text: "Restaurar jogo e teclado"; onClicked: controllers.resetGameBindings() }
                    }
                    }
                    ColumnLayout { visible: root.controlSection === 2; Layout.fillWidth: true; spacing: 12
                    Label { text: controllers.status + (controllers.currentGameProfile ? " · Perfil carregado: " + controllers.currentGameProfile : ""); color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "Salve o mapeamento do controle atual para reutilizar depois. Inclui o layout dos botões A/B/X/Y."; color: "#a994bc"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "Perfis de mapeamento do controle"; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true
                        Field { id: gameProfileName; Layout.fillWidth: true; placeholderText: "Ex.: Controle Xbox"; Accessible.name: "Nome do novo perfil de controle" }
                        ActionButton { text: "Salvar"; primary: true; enabled: controllers.devices.length > 0 && gameProfileName.text.trim().length > 0; onClicked: { if (controllers.saveGameProfile(gameProfileName.text)) gameProfileName.text = "" } }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Select { id: savedGameProfile; Layout.fillWidth: true; model: controllers.gameProfiles; enabled: controllers.gameProfiles.length > 0; Accessible.name: "Perfis de controle salvos" }
                        ActionButton { text: "Carregar"; primary: true; enabled: controllers.devices.length > 0 && savedGameProfile.currentIndex >= 0; onClicked: controllers.loadGameProfile(savedGameProfile.currentText) }
                        ActionButton { text: "Excluir"; enabled: savedGameProfile.currentIndex >= 0; onClicked: deleteProfileDialog.open() }
                    }
                    Label { text: controllers.profileStatus; visible: text.length > 0; color: "#d4afff"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: controllers.gameProfiles.length === 0 ? "Nenhum perfil salvo ainda." : "Carregue um perfil, revise os botões e clique Aplicar para usá-lo."; color: "#a994bc"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                }
                Panel {
                    Layout.fillWidth: true; visible: [7,8].indexOf(root.category) >= 0
                    Label { text: root.category === 7 ? "Núcleo e dados locais" : "Diagnóstico da sessão"; font.pixelSize: 20; font.bold: true }
                    Label { text: "Estado: " + emulation.state + " · PID: " + emulation.processId; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Label { text: preferences.coreDataPath; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; color: "#b9a7d2" }
                    Label { text: emulation.message; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Repeater {
                        model: root.category === 7 ? ["nand","sdmc","save","keys","shader","screenshots"] : ["log"]
                        ActionButton { required property string modelData; text: "Abrir pasta: " + modelData; onClicked: if (!preferences.openCoreDirectory(modelData)) root.feedback = "Não foi possível abrir a pasta." }
                    }
                    ActionButton { text: "Consultar estado do núcleo"; enabled: emulation.active && emulation.state !== "Stopping"; onClicked: emulation.queryState() }
                    Label { text: "Mensagens recentes desta abertura; os arquivos de log ficam na pasta log. Taxa de apresentação não representa FPS interno do jogo."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                    Label { text: emulation.log.slice(-12).join("\n"); visible: root.category !== 7; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; font.pixelSize: 11 }
                }
                Panel {
                    Layout.fillWidth: true; visible: root.category === 7
                    Label { text: "Núcleo Suyu"; font.pixelSize: 20; font.bold: true }
                    Toggle { text: "Emulação multi-core"; checked: preferences.draft["core/multicore"]; onToggled: preferences.setValue("core/multicore", checked) }
                    Label { text: "Usa as threads do núcleo. Exige iniciar o próximo jogo para aplicar."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                    Label { text: "Precisão da CPU" }
                    Select { Layout.fillWidth: true; model: ["Automática (padrão)", "Precisa", "Insegura (mais rápida, pode causar erros)", "Paranoica (mais lenta)"]; currentIndex: preferences.draft["core/cpuAccuracy"]; onActivated: preferences.setValue("core/cpuAccuracy", currentIndex) }
                    Label { text: "O modo Inseguro pode aumentar desempenho em alguns jogos, mas compromete compatibilidade; Automática mantém o padrão do Suyu."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                    Label { text: "Layout de memória do console" }
                    Select { Layout.fillWidth: true; model: ["4 GB (padrão)", "6 GB", "8 GB", "10 GB", "12 GB"]; currentIndex: preferences.draft["core/memoryLayout"]; onActivated: preferences.setValue("core/memoryLayout", currentIndex) }
                    Label { text: "Precisão e layout são aplicados no próximo jogo. Alguns títulos exigem o layout padrão de 4 GB."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                }
                Panel {
                    Layout.fillWidth: true; visible: root.category === 3
                    Label { text: "Áudio Cubeb"; font.pixelSize: 20; font.bold: true }
                    Label { text: "Dispositivo de saída" }
                    Select { Layout.fillWidth: true; model: emulation.audioDevices
                        currentIndex: Math.max(0, emulation.audioDevices.indexOf(preferences.draft["audio/outputDevice"]))
                        onActivated: preferences.setValue("audio/outputDevice", emulation.audioDevices[currentIndex]) }
                    Label { text: "Automático acompanha o dispositivo padrão do sistema. A troca de dispositivo entra em vigor no próximo jogo."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                    Label { text: "Volume · " + preferences.draft["audio/volume"] + "%" }
                    Slider { Layout.fillWidth: true; from: 0; to: 100; stepSize: 5
                        value: preferences.draft["audio/volume"]
                        onMoved: preferences.setValue("audio/volume", Math.round(value)) }
                    Label { text: "Aplicar atualiza o volume da sessão ativa e salva para os próximos jogos."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                }
                Panel {
                    Layout.fillWidth: true; visible: root.category === 2
                    Label { text: "Renderização Vulkan"; font.pixelSize: 20; font.bold: true }
                    Label { text: "Escala interna de resolução" }
                    Select { id: resolutionScale; Layout.fillWidth: true; model: ["0,25\u00d7", "0,5\u00d7", "0,75\u00d7", "1\u00d7 (padr\u00e3o)", "1,25\u00d7", "1,5\u00d7", "2\u00d7", "3\u00d7", "4\u00d7", "5\u00d7", "6\u00d7", "7\u00d7", "8\u00d7"]; onActivated: preferences.setValue("graphics/resolution", currentIndex) }
                    Binding { target: resolutionScale; property: "currentIndex"; value: preferences.draft["graphics/resolution"] }
                    Label { text: "Escala interna de renderização; não altera a resolução da tela. Escalas altas aumentam muito o uso de VRAM."; color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Toggle { text: "Modo docked \u00b7 apresentação 1080p (portátil: 720p)"; checked: preferences.draft["graphics/docked"]; onToggled: preferences.setValue("graphics/docked", checked) }
                    Label { text: "Sincronização vertical (VSync) · Vulkan" }
                    Select { Layout.fillWidth: true; model: ["Immediate", "Mailbox", "FIFO", "FIFO Relaxed"]; currentIndex: preferences.draft["graphics/vsync"]; onActivated: preferences.setValue("graphics/vsync", currentIndex) }
                    Label { text: "O VSync do núcleo é aplicado à emulação. Como esta versão usa composição headless, a apresentação na janela é controlada pelo Qt."; color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "Preparação de shaders"; font.pixelSize: 18; font.bold: true }
                    Toggle { text: "Cache de shaders em disco"; checked: preferences.draft["graphics/shaderCache"]; onToggled: preferences.setValue("graphics/shaderCache", checked) }
                    Label { text: "Ao iniciar, o núcleo carrega e pré-compila os pipelines já conhecidos no cache. Pipelines novos são salvos para as próximas sessões."; color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Toggle { text: "Emulação assíncrona da GPU"; checked: preferences.draft["graphics/asynchronousGpu"]; onToggled: preferences.setValue("graphics/asynchronousGpu", checked) }
                    Toggle { text: "Cache persistente de pipelines Vulkan"; checked: preferences.draft["graphics/pipelineCache"]; onToggled: preferences.setValue("graphics/pipelineCache", checked) }
                    Toggle { text: "Compilação assíncrona de shaders (reduz travadas)"; checked: preferences.draft["graphics/asynchronousShaders"]; onToggled: preferences.setValue("graphics/asynchronousShaders", checked) }
                    Toggle {
                        text: "Modo recomendado para shaders"
                        checked: preferences.draft["graphics/shaderCache"] && preferences.draft["graphics/pipelineCache"] && preferences.draft["graphics/asynchronousShaders"]
                        onToggled: {
                            const enable = checked
                            preferences.setValue("graphics/shaderCache", enable)
                            preferences.setValue("graphics/pipelineCache", enable)
                            preferences.setValue("graphics/asynchronousShaders", enable)
                        }
                    }
                    Label { text: "Ligada: novos pipelines compilam em workers para reduzir esperas; desenhos que dependem deles podem aparecer um pouco depois. Desligada: a thread de renderização espera o pipeline terminar. Recomendado: cache em disco, cache Vulkan e compilação assíncrona."; color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "Precisão da GPU" }
                    Select { Layout.fillWidth: true; model: ["Normal (Low)", "High (padrão)"]; currentIndex: preferences.draft["graphics/gpuAccuracy"]; onActivated: preferences.setValue("graphics/gpuAccuracy", currentIndex) }
                    Label { text: "Esta versão do Suyu oferece os níveis Low e High; Extreme não está disponível."; color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "Decodificação ASTC" }
                    Select { Layout.fillWidth: true; model: ["CPU", "GPU (padrão)", "CPU assíncrona"]; currentIndex: preferences.draft["graphics/astcDecode"]; onActivated: preferences.setValue("graphics/astcDecode", currentIndex) }
                    Label { text: "Filtragem anisotrópica" }
                    Select { Layout.fillWidth: true; model: ["Autom\u00e1tica (padr\u00e3o)", "Do jogo", "2\u00d7", "4\u00d7", "8\u00d7", "16\u00d7", "32\u00d7", "64\u00d7", "Desativada"]; currentIndex: preferences.draft["graphics/anisotropy"]; onActivated: preferences.setValue("graphics/anisotropy", currentIndex) }
                    Label { text: "Filtro de escalonamento" }
                    Select { Layout.fillWidth: true; model: ["Nearest", "Bilinear (padrão)", "Bicubic", "Gaussian", "Lanczos", "ScaleForce", "FSR 1 (espacial)", "Area", "ZeroTangent", "B-Spline", "Mitchell", "Spline1", "MMPX", "SGSR 1 (espacial)", "SGSR 1 Edge"]; currentIndex: preferences.draft["graphics/scalingFilter"]; onActivated: preferences.setValue("graphics/scalingFilter", currentIndex) }
                    Label { text: "FSR 1 e SGSR 1 são filtros espaciais: melhoram a escala, mas não criam quadros. O FSR 1 também permite ajustar a nitidez. Este renderizador não oferece upscaling temporal."; color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "Nitidez FSR" }
                    Slider { Layout.fillWidth: true; from: 0; to: 200; stepSize: 5; enabled: preferences.draft["graphics/scalingFilter"] === 6
                        value: preferences.draft["graphics/fsrSharpness"]; onMoved: preferences.setValue("graphics/fsrSharpness", Math.round(value)) }
                    Label { text: "A nitidez só é usada pelo filtro FSR; o núcleo aplica o valor no próximo jogo."; color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "Proporção da imagem" }
                    Select { Layout.fillWidth: true; model: ["16:9 (padrão)", "4:3", "21:9", "16:10", "32:9", "Esticar"]; currentIndex: preferences.draft["graphics/aspectRatio"]; onActivated: preferences.setValue("graphics/aspectRatio", currentIndex) }
                    Label { text: "Anti-aliasing" }
                    Select { Layout.fillWidth: true; model: ["Desligado (padrão)", "FXAA", "SMAA"]; currentIndex: preferences.draft["graphics/antiAliasing"]; onActivated: preferences.setValue("graphics/antiAliasing", currentIndex) }
                    Label { text: "Uso de memória gráfica" }
                    Select { Layout.fillWidth: true; model: ["Conservador (padrão)", "Agressivo"]; currentIndex: preferences.draft["graphics/vramUsage"]; onActivated: preferences.setValue("graphics/vramUsage", currentIndex) }
                    Label { text: "Decodificação de vídeo NVDEC" }
                    Select { Layout.fillWidth: true; model: ["Desativada", "CPU", "GPU (padrão)"]; currentIndex: preferences.draft["graphics/nvdec"]; onActivated: preferences.setValue("graphics/nvdec", currentIndex) }
                    Toggle { text: "Descarga reativa de memória gráfica"; checked: preferences.draft["graphics/reactiveFlushing"]; onToggled: preferences.setValue("graphics/reactiveFlushing", checked) }
                    Label { text: "A liberação reativa de recursos pode reduzir o trabalho com a memória gráfica. Desative somente se notar falhas visuais."; color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "Precisão de DMA" }
                    Select { Layout.fillWidth: true; model: ["Padrão", "Insegura (mais rápida, pode causar erros)", "Segura (maior compatibilidade)"]; currentIndex: preferences.draft["graphics/dmaAccuracy"]; onActivated: preferences.setValue("graphics/dmaAccuracy", currentIndex) }
                    Label { text: "Comportamento de fences da GPU" }
                    Select { Layout.fillWidth: true; model: ["Padrão", "Imediato", "Balanceado", "Preciso", "Estrito"]; currentIndex: preferences.draft["graphics/gpuFence"]; onActivated: preferences.setValue("graphics/gpuFence", currentIndex) }
                    Label { text: "Recompressão ASTC em VRAM" }
                    Select { Layout.fillWidth: true; model: ["Sem recompressão (padrão)", "BC1", "BC3"]; currentIndex: preferences.draft["graphics/astcRecompression"]; onActivated: preferences.setValue("graphics/astcRecompression", currentIndex) }
                    Label { text: "A apresentação assíncrona do Suyu exige uma swapchain e não se aplica ao modo headless usado aqui."; color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "GPU Vulkan (índice na enumeração do driver)" }
                    Select { Layout.fillWidth: true; model: emulation.vulkanDevices; currentIndex: Math.max(0, Math.min(preferences.draft["graphics/vulkanDevice"], emulation.vulkanDevices.length - 1)); onActivated: preferences.setValue("graphics/vulkanDevice", currentIndex) }
                    Label { text: "A lista vem da enumeração Vulkan do núcleo Suyu; a troca entra em vigor no próximo jogo."; color: "#b9a7d2"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "Aplicar salva os valores. Eles entram em vigor ao iniciar o próximo jogo; a sessão ativa não é reiniciada."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                }
                Panel {
                    Layout.fillWidth: true; visible: root.category === 9
                    Label { text: "Gráficos avançados · ajustes do núcleo Suyu"; font.pixelSize: 20; font.bold: true }
                    Label { text: "Opções experimentais aplicadas no próximo jogo. Começam desligadas ou no padrão do núcleo."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#3a2d4b" }
                    Label { text: "Fast GPU Time"; font.pixelSize: 17; font.bold: true }
                    Toggle { text: "Ativar Fast GPU Time (hack)"; checked: preferences.draft["graphics/fastGpuTime"]; onToggled: preferences.setValue("graphics/fastGpuTime", checked) }
                    Label { text: "Altera o tempo de GPU observado pelo jogo e pode mudar limites de FPS ou resolução dinâmica. Não aumenta a frequência física da GPU nem garante mais desempenho; pode acelerar a simulação ou causar incompatibilidade."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#cbb8dc" }
                    Label { text: "Divisor de tempo informado ao jogo" }
                    Select { Layout.fillWidth: true; enabled: preferences.draft["graphics/fastGpuTime"]; model: ["Normal (sem divisor)", "Médio (÷256)", "Alto (÷512)"]; currentIndex: preferences.draft["graphics/fastGpuDivisor"]; onActivated: preferences.setValue("graphics/fastGpuDivisor", currentIndex) }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#3a2d4b" }
                    Label { text: "Tempo e precisão da CPU"; font.pixelSize: 17; font.bold: true }
                    Label { text: "CPU emulada" }
                    Select { Layout.fillWidth: true; model: ["Padrão", "Boost (1700 MHz)", "Fast (2000 MHz)"]; currentIndex: preferences.draft["core/cpuOverclock"]; onActivated: preferences.setValue("core/cpuOverclock", currentIndex) }
                    Label { text: "Muda o clock observado pelo jogo para tentar remover limites internos de FPS. Pode piorar o desempenho em CPUs mais fracas ou alterar o comportamento de alguns jogos."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#cbb8dc" }
                    Toggle { text: "Usar ticks customizados da CPU (experimental)"; checked: preferences.draft["core/customCpuTicks"]; onToggled: preferences.setValue("core/customCpuTicks", checked) }
                    Label { text: "Ticks por fatia: " + preferences.draft["core/cpuTicks"] }
                    Slider { Layout.fillWidth: true; from: 77; to: 21000; stepSize: 77; enabled: preferences.draft["core/customCpuTicks"]; value: preferences.draft["core/cpuTicks"]; onMoved: preferences.setValue("core/cpuTicks", Math.round(value)) }
                    Label { text: "Ticks personalizados podem causar travamentos ou deadlocks. A faixa recomendada pelo Suyu é de 77 a 21.000."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#cbb8dc" }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#3a2d4b" }
                    Label { text: "Compatibilidade de cache da GPU"; font.pixelSize: 17; font.bold: true }
                    Toggle { text: "Ignorar invalidação interna de dados da CPU (ajuste experimental)"; checked: preferences.draft["graphics/skipCpuInvalidation"]; onToggled: preferences.setValue("graphics/skipCpuInvalidation", checked) }
                    Label { text: "Pode reduzir invalidações do cache da GPU, mas também deixar texturas ou objetos incorretos. Use apenas para testar e desative se notar falhas visuais."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#cbb8dc" }
                    Label { text: "Aplicar salva as alterações. Reinicie o jogo para usá-las; se notar falhas gráficas ou de tempo, restaure os padrões."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b9a7d2" }
                }
                Panel {
                    Layout.fillWidth: true; visible: root.category === 5
                    Label { text: "Integração futura"; font.pixelSize: 20; font.bold: true }
                    Label { text: "Estas opções dependem do núcleo ou de serviços ainda não integrados. Não são aplicadas nesta fase."; color: "#aa93bd"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: ["", "Região e idioma do sistema", "API gráfica", "Dispositivo de saída", "", "Conexão e serviços online", "", "", "Versão do núcleo", "Registro de sessão", "Diagnóstico nativo"][root.category] }
                    Select { Layout.fillWidth: true; enabled: false; model: [root.category === 2 ? "Vulkan / OpenGL \u00b7 futuro" : "Não disponível nesta fase"] }
                    Label { visible: root.category === 3; text: "Volume da emulação \u00b7 futuro"; color: "#8f7d9e" }
                    Slider { visible: root.category === 3; enabled: false; Layout.fillWidth: true; value: 0.5 }
                    Toggle { text: "Ativar recurso · futuro"; enabled: false }
                    Toggle { visible: root.category === 7; text: "Preferir pausa ao abrir menu (futuro)"; checked: preferences.draft["session/pauseOnMenu"]; enabled: false }
                }
            }
        }
        Label { visible: root.feedback !== ""; text: root.feedback; color: "#d4afff"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        RowLayout {
            Layout.fillWidth: true
            ActionButton { text: "\u21bb  Restaurar padrões"; onClicked: { preferences.defaults(); controllers.resetBindings(); root.feedback = "Padrões no rascunho. Aplicar confirma as alterações." } }
            Item { Layout.fillWidth: true }
            Label { text: preferences.dirty || controllers.dirty ? "Alterações pendentes" : ""; color: "#a58abb"; font.pixelSize: 11 }
            ActionButton { text: "Aplicar"; enabled: preferences.dirty || controllers.dirty; onClicked: root.apply() }
            ActionButton { text: "Cancelar"; onClicked: { root.cancel(); root.closeRequested() } }
            ActionButton { text: "OK"; primary: true; onClicked: if (root.apply()) root.closeRequested() }
        }
    }
}
