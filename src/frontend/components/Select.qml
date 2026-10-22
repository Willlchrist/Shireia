import QtQuick
import QtQuick.Controls
ComboBox {
    id: control
    implicitHeight: 44
    implicitWidth: 220
    focusPolicy: Qt.StrongFocus
    Connections { target: control.popup; function onVisibleChanged() { controllers.popupActive = control.popup.visible } }
    Keys.onReturnPressed: popup.visible ? popup.close() : popup.open()
    Keys.onSpacePressed: popup.visible ? popup.close() : popup.open()
    contentItem: Text {
        text: control.displayText; color: control.enabled ? "#e7ddff" : "#82778f"
        font: control.font; verticalAlignment: Text.AlignVCenter
        leftPadding: 14; rightPadding: 30; elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 8; color: "#211d32"
        border.color: control.activeFocus ? "#bb79ff" : "#3a3252"
        border.width: control.activeFocus ? 2 : 1
    }
    delegate: ItemDelegate {
        required property var modelData
        required property int index
        width: control.width
        text: control.textRole ? modelData[control.textRole] : modelData
        highlighted: control.highlightedIndex === index
        contentItem: Text { text: parent.text; color: "#eee5ff"; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        background: Rectangle { color: parent.highlighted ? "#633294" : "#211c30" }
    }
}
