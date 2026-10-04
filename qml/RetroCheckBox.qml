import QtQuick
import QtQuick.Controls
CheckBox {
    id: control
    implicitHeight: 24
    spacing: 7
    padding: 2
    font.pixelSize: 12
    indicator: Rectangle {
        x: control.leftPadding; y: (control.height-height)/2; width: 15; height: 15; radius: 1
        color: "#171d23"; border.color: control.activeFocus ? "#e8b65d" : "#78818b"
        Label { anchors.centerIn: parent; text: "✓"; font.pixelSize: 13; color: "#edbf6c"; visible: control.checked }
    }
    contentItem: Label { leftPadding: control.indicator.width+control.spacing; text: control.text; font: control.font; color: control.enabled ? "#dce0e4" : "#858b93"; verticalAlignment: Text.AlignVCenter }
}
