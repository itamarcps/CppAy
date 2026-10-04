import QtQuick
import QtQuick.Controls
Slider {
    id: control
    implicitHeight: 24
    implicitWidth: 120
    padding: 5
    background: Rectangle {
        x: control.leftPadding; y: control.topPadding + control.availableHeight/2 - height/2
        width: control.availableWidth; height: 6; radius: 1
        color: "#12171c"; border.color: "#5b626a"
        Rectangle { x: 1; y: 1; height: parent.height-2; width: Math.max(0,control.visualPosition*(parent.width-2)); color: "#9e783b" }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition*(control.availableWidth-width)
        y: control.topPadding + control.availableHeight/2-height/2
        width: 13; height: 18; radius: 2
        border.color: control.activeFocus ? "#e6b95e" : "#12151a"
        gradient: Gradient { GradientStop { position: 0; color: control.pressed ? "#9c906e" : "#aeb5bc" } GradientStop { position: 1; color: "#565f6b" } }
        Rectangle { anchors.fill: parent; anchors.margins: 1; color: "transparent"; border.color: Qt.rgba(1,1,1,.2); radius: 1 }
        Rectangle { anchors.centerIn: parent; width: 1; height: 8; color: "#303741" }
    }
}
