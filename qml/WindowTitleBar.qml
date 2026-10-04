import QtQuick
import QtQuick.Controls

Item {
    id: bar
    objectName: "windowTitleBar"
    required property Window targetWindow
    property alias menuHost: menuSlot
    readonly property bool maximized: targetWindow.visibility === Window.Maximized
    implicitHeight: 34

    function toggleMaximized() {
        if (maximized) targetWindow.showNormal()
        else targetWindow.showMaximized()
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: bar.targetWindow.active ? "#505964" : "#424952" }
            GradientStop { position: 1; color: "#30363e" }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#151a20" }
    }
    Image {
        id: appIcon
        x: 10; anchors.verticalCenter: parent.verticalCenter
        width: 18; height: 18; source: "qrc:/assets/cppay-icon.png"
        fillMode: Image.PreserveAspectFit; smooth: true
        Accessible.role: Accessible.Graphic; Accessible.name: "C++Ay"
    }
    Item {
        id: menuSlot
        anchors.left: appIcon.right; anchors.leftMargin: 8
        height: parent.height; width: childrenRect.width
    }
    Item {
        id: dragArea
        objectName: "windowDragArea"
        anchors.left: menuSlot.right; anchors.leftMargin: 10
        anchors.right: controls.left; anchors.rightMargin: 12
        height: parent.height
        Label {
            anchors.fill: parent
            text: bar.targetWindow.title; elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 11; color: bar.targetWindow.active ? "#dfe3e8" : "#9ca5af"
        }
        // Let a tap finish before requesting a native drag, so double-clicks
        // can maximize and the platform owns movement (including Wayland).
        DragHandler {
            target: null; acceptedButtons: Qt.LeftButton
            onActiveChanged: if (active) bar.targetWindow.startSystemMove()
        }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: bar.toggleMaximized()
        }
    }

    component CaptionButton: Button {
        id: button
        required property string glyph
        property bool closeButton: false
        width: 44; height: bar.height
        padding: 0; hoverEnabled: true
        Accessible.name: text
        background: Rectangle {
            color: button.down ? (button.closeButton ? "#943f39" : "#626c78")
                 : button.hovered ? (button.closeButton ? "#c24e45" : "#505b68") : "transparent"
            Rectangle { anchors.left: parent.left; height: parent.height; width: 1; color: "#252b33" }
            Rectangle {
                anchors.fill: parent; anchors.margins: 3; color: "transparent"
                border.color: button.activeFocus ? "#e7b761" : "transparent"
            }
        }
        contentItem: Canvas {
            onPaint: {
                let c = getContext("2d"); c.reset()
                c.strokeStyle = button.hovered || bar.targetWindow.active ? "#edf0f3" : "#a5adb6"
                c.lineWidth = 1
                let x = Math.floor(width/2)-5+.5, y = Math.floor(height/2)-5+.5
                if (button.glyph === "minimize") {
                    c.beginPath(); c.moveTo(x,y+7); c.lineTo(x+10,y+7); c.stroke()
                } else if (button.glyph === "close") {
                    c.beginPath(); c.moveTo(x,y); c.lineTo(x+10,y+10)
                    c.moveTo(x+10,y); c.lineTo(x,y+10); c.stroke()
                } else if (button.glyph === "restore") {
                    c.beginPath(); c.moveTo(x+3,y+2); c.lineTo(x+3,y)
                    c.lineTo(x+10,y); c.lineTo(x+10,y+7); c.lineTo(x+8,y+7); c.stroke()
                    c.strokeRect(x,y+3,7,7)
                } else c.strokeRect(x,y,10,10)
            }
            Connections { target: button; function onGlyphChanged() { button.contentItem.requestPaint() } function onHoveredChanged() { button.contentItem.requestPaint() } }
            Connections { target: bar.targetWindow; function onActiveChanged() { button.contentItem.requestPaint() } }
        }
        ToolTip.visible: hovered; ToolTip.delay: 600; ToolTip.text: text
    }
    Row {
        id: controls
        anchors.right: parent.right
        height: parent.height
        CaptionButton { objectName: "windowMinimizeButton"; text: "Minimize"; glyph: "minimize"; onClicked: bar.targetWindow.showMinimized() }
        CaptionButton { objectName: "windowMaximizeButton"; text: bar.maximized ? "Restore" : "Maximize"; glyph: bar.maximized ? "restore" : "maximize"; onClicked: bar.toggleMaximized() }
        CaptionButton { objectName: "windowCloseButton"; text: "Close"; glyph: "close"; closeButton: true; onClicked: bar.targetWindow.close() }
    }
}
