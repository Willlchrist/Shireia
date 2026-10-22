import QtQuick
import QtQuick.Controls
TextField {
    id: control
    implicitHeight: 44
    color: "#eee6ff"
    placeholderTextColor: "#aaa0c0"
    selectByMouse: true
    leftPadding: 14
    background: Rectangle {
        radius: 8; color: "#1c192d"
        border.color: control.activeFocus ? "#bb79ff" : "#383050"
        border.width: control.activeFocus ? 2 : 1
    }
}
