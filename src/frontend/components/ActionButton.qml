import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: control
    property bool primary: false
    property string glyph: ""
    property string iconName: ""
    implicitHeight: 42
    padding: 12
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    contentItem: RowLayout {
        spacing: 9
        Glyph { visible: control.iconName !== ""; name: control.iconName; Layout.preferredWidth: 22; font.pixelSize: 20; color: control.enabled ? "#dbc4fc" : "#746b89" }
        Text {
            visible: control.text !== ""
            text: (control.glyph ? control.glyph + "   " : "") + control.text
            font: control.font
            color: control.enabled ? "#e7ddff" : "#746b89"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
    background: Rectangle {
        radius: 4
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#d0a5ff" : control.primary || control.checked ? "#9149eb" : control.flat ? "transparent" : "#342d4f"
        gradient: Gradient {
            GradientStop { position: 0; color: control.primary || control.checked ? (control.down ? "#54278b" : "#7934cd") : (control.hovered ? "#302342" : control.flat ? "transparent" : "#242035") }
            GradientStop { position: 1; color: control.primary || control.checked ? "#512b95" : control.flat && !control.hovered ? "transparent" : "#1a1728" }
        }
        Behavior on border.color { ColorAnimation { duration: 100 } }
    }
}
