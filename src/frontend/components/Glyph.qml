import QtQuick
Text {
    property string name: "games"
    readonly property var glyphs: ({games:"\uE7FC", library:"\uE8B7", recent:"\uE81C", favorite:"\uE734", system:"\uE770", graphics:"\uE7F4", audio:"\uE767", controls:"\uE7FC", online:"\uE701", interface:"\uE790", settings:"\uE713", profiles:"\uE77B", logs:"\uE8A5", debug:"\uE90F", grid:"\uE80A", list:"\uE8FD", search:"\uE721", play:"\uE768", back:"\uE72B"})
    text: glyphs[name] || glyphs.settings
    font.family: "Segoe MDL2 Assets"
    font.pixelSize: 23; color: "#c6afe9"
    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
}
