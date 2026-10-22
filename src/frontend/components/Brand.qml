import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    spacing: 2
    implicitWidth: 150; implicitHeight: 88
    Image { source: "qrc:/assets/brand/shireia-logo.png"; Layout.preferredWidth: 52; Layout.preferredHeight: 52; Layout.alignment: Qt.AlignHCenter; fillMode: Image.PreserveAspectFit; smooth: true }
    Label { text: "Shireia Next"; font.pixelSize: 16; font.bold: true; color: "#eee9f4"; Layout.alignment: Qt.AlignHCenter }
}
