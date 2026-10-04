import QtQuick
import QtQuick.Controls
ComboBox {
    id: control
    implicitHeight: 28
    implicitWidth: 160
    font.pixelSize: 12
    padding: 7; rightPadding: 26
    background: Rectangle { radius: 2; color: "#222830"; border.color: control.activeFocus ? "#e8b65d" : "#79828b" }
    contentItem: Label { text: control.displayText; color: "#e3e7eb"; font: control.font; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
    indicator: Label { x: control.width-width-9; y: (control.height-height)/2; text: "▾"; color: "#edbf6c"; font.pixelSize: 14 }
}
