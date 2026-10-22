import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Control {
    id: control
    default property alias contents: layout.data
    property alias contentSpacing: layout.spacing
    padding: 16
    implicitHeight: layout.implicitHeight + topPadding + bottomPadding
    contentItem: ColumnLayout { id: layout; spacing: 12 }
    background: Rectangle {
        radius: 5; border.color: "#302b39"
        gradient: Gradient {
            GradientStop { position: 0; color: "#1b1a22" }
            GradientStop { position: 1; color: "#16151c" }
        }
    }
}
