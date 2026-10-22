import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#101016"
    property int currentPage: 0
    property var navigation: []
    signal navigate(int index)
    signal contentRequested()
    function focusCurrent() { entries.itemAt(currentPage).forceActiveFocus() }
    Rectangle { width: 1; anchors.right: parent.right; height: parent.height; color: "#292431" }
    ColumnLayout { anchors.fill: parent; anchors.margins: 16; spacing: 18
        Brand { Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 14 }
        ColumnLayout { Layout.fillWidth: true; spacing: 4
            Repeater { id: entries; model: root.navigation
                ActionButton { id: nav; required property var modelData; required property int index
                    objectName: "nav" + index; flat: true; checked: root.currentPage === index
                    Layout.fillWidth: true; implicitHeight: 42
                    contentItem: RowLayout { spacing: 12
                        Glyph { name: nav.modelData.icon; Layout.preferredWidth: 22 }
                        Text { text: nav.modelData.text; color: "#e4dfe9"; font.pixelSize: 14; Layout.fillWidth: true }
                    }
                    onClicked: root.navigate(index)
                    Keys.onUpPressed: entries.itemAt(Math.max(0,index-1)).forceActiveFocus()
                    Keys.onDownPressed: entries.itemAt(Math.min(entries.count-1,index+1)).forceActiveFocus()
                    Keys.onRightPressed: root.contentRequested()
                }
            }
        }
        Item { Layout.fillHeight: true }
        Label { text: emulation.coreAvailable ? "Núcleo original · Vulkan" : "Núcleo indisponível"; color: "#9c93a8"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
}
