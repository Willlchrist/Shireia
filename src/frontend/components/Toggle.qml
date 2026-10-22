import QtQuick
import QtQuick.Controls
Switch {
    id: control
    implicitHeight: 36
    spacing: 14
    focusPolicy: Qt.StrongFocus
    Keys.onReturnPressed: { toggle(); toggled() }
    indicator: Rectangle {
        x: 0; y: (parent.height - height) / 2
        width: 44; height: 24; radius: 12
        color: control.checked ? "#8938dc" : "#262139"
        border.color: control.activeFocus ? "#e2bdff" : "#594369"
        border.width: control.activeFocus ? 2 : 1
        Rectangle {
            x: control.checked ? 22 : 3; y: 3; width: 18; height: 18; radius: 9
            color: control.enabled ? "#e9daff" : "#80718e"
            Behavior on x { NumberAnimation { duration: 100 } }
        }
    }
    contentItem: Text {
        text: control.text; font: control.font
        color: control.enabled ? "#e4dcf3" : "#82778f"
        leftPadding: control.indicator.width + control.spacing
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
    }
}
