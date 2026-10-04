import QtQuick

Item {
    id: frame
    objectName: "windowResizeHandles"
    required property Window targetWindow
    width: targetWindow.width; height: targetWindow.height
    // ApplicationWindow's content area sits below its header. Cover the root
    // instead, so the resize borders also include the custom title bar.
    parent: targetWindow.contentItem.parent
    z: 100
    visible: targetWindow.visibility !== Window.Maximized && targetWindow.visibility !== Window.FullScreen
    readonly property int grip: 5

    Repeater {
        model: [Qt.TopEdge|Qt.LeftEdge, Qt.TopEdge|Qt.RightEdge,
                Qt.BottomEdge|Qt.LeftEdge, Qt.BottomEdge|Qt.RightEdge,
                Qt.LeftEdge, Qt.TopEdge, Qt.RightEdge, Qt.BottomEdge]
        MouseArea {
            required property int modelData
            required property int index
            objectName: "windowResize"+index
            readonly property int resizeEdges: modelData
            readonly property bool leftEdge: (resizeEdges & Qt.LeftEdge) !== 0
            readonly property bool rightEdge: (resizeEdges & Qt.RightEdge) !== 0
            readonly property bool topEdge: (resizeEdges & Qt.TopEdge) !== 0
            readonly property bool bottomEdge: (resizeEdges & Qt.BottomEdge) !== 0
            x: rightEdge ? frame.width-frame.grip : (leftEdge ? 0 : frame.grip)
            y: bottomEdge ? frame.height-frame.grip : (topEdge ? 0 : frame.grip)
            width: leftEdge || rightEdge ? frame.grip : frame.width-frame.grip*2
            height: topEdge || bottomEdge ? frame.grip : frame.height-frame.grip*2
            cursorShape: (leftEdge || rightEdge) && (topEdge || bottomEdge)
                ? ((leftEdge && topEdge) || (rightEdge && bottomEdge) ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor)
                : (leftEdge || rightEdge ? Qt.SizeHorCursor : Qt.SizeVerCursor)
            acceptedButtons: Qt.LeftButton
            onPressed: mouse => { mouse.accepted = frame.targetWindow.startSystemResize(resizeEdges) }
        }
    }
}
