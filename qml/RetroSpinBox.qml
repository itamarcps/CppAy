import QtQuick
import QtQuick.Controls
SpinBox {
    id: control
    implicitHeight: 28
    implicitWidth: 160
    font.pixelSize: 12
    leftPadding: 7; rightPadding: 27
    background: Rectangle { radius: 2; color: "#222830"; border.color: control.activeFocus ? "#e8b65d" : "#79828b" }
    contentItem: TextInput {
        text: control.textFromValue(control.value, control.locale)
        font: control.font; color: "#e3e7eb"; selectionColor: "#886836"
        readOnly: !control.editable; validator: control.validator
        inputMethodHints: control.inputMethodHints
        verticalAlignment: Text.AlignVCenter
    }
    up.indicator: Rectangle {
        x: control.width-width; width: 22; height: control.height/2
        color: control.up.pressed ? "#5b636e" : "#454d57"; border.color: "#171c22"
        Label { anchors.centerIn: parent; text: "+"; color: "#dfe4e8"; font.pixelSize: 12 }
    }
    down.indicator: Rectangle {
        x: control.width-width; y: control.height/2; width: 22; height: control.height/2
        color: control.down.pressed ? "#5b636e" : "#454d57"; border.color: "#171c22"
        Label { anchors.centerIn: parent; text: "−"; color: "#dfe4e8"; font.pixelSize: 12 }
    }
}
