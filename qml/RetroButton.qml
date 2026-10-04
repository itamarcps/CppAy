import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool accent: false
    property string symbol: ""
    implicitWidth: Math.max(54, contentItem.implicitWidth + 22)
    implicitHeight: 28
    padding: 7
    hoverEnabled: true
    font.pixelSize: 12
    background: Rectangle {
        radius: 3
        border.color: control.activeFocus ? "#e7b761" : control.checked ? "#bc9457" : control.down ? "#14171b" : "#111317"
        gradient: Gradient {
            GradientStop { position: 0; color: control.down || control.checked ? "#34383e" : control.accent ? "#b68a43" : control.hovered ? "#69717c" : "#555c66" }
            GradientStop { position: .48; color: control.down || control.checked ? "#282d33" : control.accent ? "#89612e" : "#424951" }
            GradientStop { position: .5; color: control.down || control.checked ? "#282d33" : control.accent ? "#795329" : "#353b43" }
            GradientStop { position: 1; color: control.down || control.checked ? "#3b414a" : control.accent ? "#9b7138" : "#454c55" }
        }
        Rectangle { anchors.fill: parent; anchors.margins: 1; color: "transparent"; radius: 2; border.color: control.down ? "#272c32" : Qt.rgba(1,1,1,.15) }
        opacity: control.enabled ? 1 : .45
    }
    contentItem: Item {
        implicitWidth: control.symbol ? 20 : caption.implicitWidth
        implicitHeight: 16
        Label {
            id: caption
            visible: !control.symbol
            anchors.centerIn: parent
            text: control.text
            font: control.font
            color: control.enabled ? (control.checked ? "#f4ca7c" : "#f3f3ed") : "#858990"
            style: Text.Raised; styleColor: "#1b1d22"
        }
        Canvas {
            id: glyph
            visible: !!control.symbol
            anchors.centerIn: parent; width: 20; height: 16
            onPaint: {
                let c = getContext("2d"); c.reset(); c.fillStyle = control.enabled ? "#f4eedf" : "#858990"
                function triangle(x, reverse) { c.beginPath(); c.moveTo(x,2); c.lineTo(x+(reverse ? -9 : 9),8); c.lineTo(x,14); c.closePath(); c.fill() }
                if(control.symbol === "play") triangle(6,false)
                else if(control.symbol === "pause") { c.fillRect(5,2,4,12); c.fillRect(12,2,4,12) }
                else if(control.symbol === "stop") c.fillRect(5,3,11,11)
                else if(control.symbol === "previous") { c.fillRect(3,2,2,12); triangle(16,true) }
                else if(control.symbol === "next") { triangle(4,false); c.fillRect(15,2,2,12) }
            }
            Connections { target: control; function onSymbolChanged(){glyph.requestPaint()} function onEnabledChanged(){glyph.requestPaint()} }
        }
    }
    ToolTip.visible: hovered && symbol.length > 0
    ToolTip.delay: 600
    ToolTip.text: text
}
