import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: 1060; height: 680
    minimumWidth: 760; minimumHeight: 520
    title: player.title === "C++Ay" ? "C++Ay" : player.title + " — C++Ay"
    color: "#292e35"
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : "Noto Sans"
    font.pixelSize: 12
    palette.window: "#343a42"
    palette.base: "#191e24"
    palette.text: "#dfe3e8"
    palette.windowText: "#dfe3e8"
    palette.button: "#474f59"
    palette.buttonText: "#e9edf0"
    palette.highlight: "#886636"
    palette.highlightedText: "#fff0cd"
    property string monoFont: Qt.platform.os === "windows" ? "Consolas" : "DejaVu Sans Mono"
    function time(seconds) {
        let s = Math.max(0, Math.floor(seconds))
        return Math.floor(s/60).toString().padStart(2,"0") + ":" + (s%60).toString().padStart(2,"0")
    }
    function showMixer() { mixer.open() }
    function hideMixer() { mixer.close() }
    component SkinMenu: Menu {
        implicitWidth: 240; popupType: Popup.Item
        padding: 3
        background: Rectangle { color: "#353c45"; border.color: "#89929e"; radius: 2 }
        delegate: MenuItem {
            id: menuEntry
            implicitWidth: 234; implicitHeight: 27
            contentItem: Label { text: menuEntry.text; font: menuEntry.font; color: menuEntry.enabled ? "#edf0f2" : "#858b95"; leftPadding: 23; rightPadding: 34; verticalAlignment: Text.AlignVCenter }
            background: Rectangle { color: menuEntry.highlighted ? "#775c35" : "transparent"; border.color: menuEntry.highlighted ? "#be9859" : "transparent"; radius: 1 }
            indicator: Label { x: 8; anchors.verticalCenter: parent.verticalCenter; text: "✓"; visible: menuEntry.checked; color: "#efc67c" }
        }
    }
    component SkinDialog: Dialog {
        id: dialogControl
        padding: 12
        background: Rectangle { color: "#343b45"; border.color: "#9ca7b2"; radius: 3 }
        header: Rectangle {
            implicitHeight: 33
            gradient: Gradient { GradientStop { position: 0; color: "#687580" } GradientStop { position: 1; color: "#424d59" } }
            Label { anchors.fill: parent; anchors.leftMargin: 12; text: dialogControl.title; verticalAlignment: Text.AlignVCenter; color: "#f0ddb6"; font.bold: true; font.pixelSize: 12 }
        }
        footer: DialogButtonBox {
            standardButtons: dialogControl.standardButtons
            padding: 10; spacing: 6; alignment: Qt.AlignRight
            buttonLayout: DialogButtonBox.WinLayout
            delegate: RetroButton { implicitWidth: 80 }
            background: Rectangle { color: "#2c333c"; border.color: "#515e6b" }
            onAccepted: dialogControl.accept()
            onRejected: dialogControl.reject()
        }
        Overlay.modal: Rectangle { color: Qt.rgba(0,0,0,.55) }
    }
    menuBar: MenuBar {
        implicitHeight: 26
        background: Rectangle {
            gradient: Gradient { GradientStop { position: 0; color: "#545c67" } GradientStop { position: 1; color: "#353c45" } }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#181d23" }
        }
        delegate: MenuBarItem {
            id: menuButton
            implicitHeight: 25; leftPadding: 12; rightPadding: 12
            contentItem: Label { text: menuButton.text; color: "#e7eaed"; font: menuButton.font; verticalAlignment: Text.AlignVCenter }
            background: Rectangle { color: menuButton.highlighted ? "#74603f" : "transparent"; border.color: menuButton.highlighted ? "#b2955c" : "transparent" }
        }
        SkinMenu {
            title: "File"
            Action { text: "Add files…"; shortcut: "L"; onTriggered: player.browse("files") }
            Action { text: "Add folder…"; shortcut: "Shift+L"; onTriggered: player.browse("folder") }
            Action { text: "Open playlist…"; onTriggered: player.browse("playlistOpen") }
            Action { text: "Save playlist…"; onTriggered: player.browse("playlistSave") }
            MenuSeparator {}
            Action { text: "Export song to WAV…"; enabled: player.duration>0 && !player.busy; onTriggered: player.browse("wav") }
            Action { text: "Export playlist to WAV…"; enabled: player.playlistModel.count>0 && !player.exporting; onTriggered: player.browse("batch") }
            MenuSeparator {}
            Action { text: "Exit"; shortcut: "Ctrl+Q"; onTriggered: Qt.quit() }
        }
        SkinMenu {
            title: "Playback"
            Action { text: "Play"; shortcut: "X"; onTriggered: player.play() }
            Action { text: "Pause / resume"; shortcut: "C"; onTriggered: player.pause() }
            Action { text: "Stop"; shortcut: "V"; onTriggered: player.stop() }
            Action { text: "Previous track"; shortcut: "Z"; onTriggered: player.next(-1) }
            Action { text: "Next track"; shortcut: "B"; onTriggered: player.next(1) }
            Action { text: "Repeat full song"; checkable: true; checked: player.loop; shortcut: "R"; onTriggered: player.loop=!player.loop }
        }
        SkinMenu {
            title: "Tools"
            Action { text: "Mixer & emulation…"; shortcut: "G"; onTriggered: mixer.open() }
            Action { text: "About C++Ay"; onTriggered: about.open() }
        }
    }
    Shortcut { sequence: "Left"; enabled: !mixer.visible; onActivated: player.seek(player.position-5) }
    Shortcut { sequence: "Right"; enabled: !mixer.visible; onActivated: player.seek(player.position+5) }
    Shortcut { sequence: "Up"; enabled: !list.activeFocus && !mixer.visible; onActivated: player.volume=Math.min(1,player.volume+.05) }
    Shortcut { sequence: "Down"; enabled: !list.activeFocus && !mixer.visible; onActivated: player.volume=Math.max(0,player.volume-.05) }

    ColumnLayout {
        id: layoutRoot; objectName: "layoutRoot"
        anchors.fill: parent; anchors.margins: 7; spacing: 6
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: window.height<600 ? 40 : window.height>=700 && window.width>=1000 ? 92 : 72; Layout.minimumHeight: Layout.preferredHeight
            radius: 3; border.color: "#161b21"
            gradient: Gradient { GradientStop { position: 0; color: "#65707c" } GradientStop { position: .5; color: "#444c57" } GradientStop { position: 1; color: "#353d47" } }
            RowLayout {
                anchors.fill: parent; anchors.margins: 6; spacing: 6
                Image {
                    objectName: "brandLogo"
                    source: "qrc:/assets/cppay-logo.png"
                    fillMode: Image.PreserveAspectFit; smooth: true; mipmap: true
                    Layout.preferredWidth: window.height<600 ? 140 : window.height>=700 && window.width>=1000 ? 400 : 300
                    Layout.preferredHeight: window.height<600 ? 28 : window.height>=700 && window.width>=1000 ? 80 : 60
                    Accessible.role: Accessible.Graphic; Accessible.name: "C++Ay · Chip Music System"
                }
                Item { Layout.fillWidth: true }
                RetroButton { text: "Export WAV…"; enabled: player.duration>0 && !player.exporting; onClicked: player.browse("wav") }
                RetroButton { text: "Mixer…"; onClicked: mixer.open() }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0; spacing: 6
            RetroPanel {
                id: deck; objectName: "playerDeck"
                Layout.preferredWidth: 302; Layout.minimumWidth: 302; Layout.maximumWidth: 302
                Layout.fillHeight: true; clip: true
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 6
                    Rectangle {
                        objectName: "positionDisplay"
                        Layout.fillWidth: true; Layout.preferredHeight: 108; Layout.minimumHeight: 108
                        color: "#111a1d"; border.color: "#798187"; radius: 2
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 9; spacing: 3
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: player.playing ? "PLAYING" : player.duration>0 ? "READY / PAUSED" : "READY"; color: "#b8a77b"; font.pixelSize: 9; font.letterSpacing: 1 }
                                Item { Layout.fillWidth: true }
                                Rectangle { width: 5; height: 5; radius: 1; color: player.playing ? "#edc471" : "#4b524b" }
                                Label { text: player.ym ? "YM2149F" : "AY-3-8910"; color: "#899990"; font.pixelSize: 9 }
                            }
                            Label { text: player.title; Layout.fillWidth: true; elide: Text.ElideRight; color: "#efcb82"; font.bold: true; font.pixelSize: 14; ToolTip.visible: titleHover.hovered; ToolTip.text: text; HoverHandler { id: titleHover } }
                            Label { text: player.author; Layout.fillWidth: true; elide: Text.ElideRight; color: "#9faeaa"; font.pixelSize: 11 }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: time(timeline.pressed ? timeline.value : player.position); font.family: window.monoFont; font.pixelSize: 28; color: "#f4ce81" }
                                Label { text: "/ "+time(player.duration); color: "#8f9a91"; font.family: window.monoFont; font.pixelSize: 12 }
                                Item { Layout.fillWidth: true }
                                Column {
                                    Label { text: (player.rate/1000).toFixed(1)+" kHz"; color: "#a8b4ac"; font.pixelSize: 10; anchors.right: parent.right }
                                    Label { text: "STEREO · 16 BIT"; color: "#788980"; font.pixelSize: 8 }
                                }
                            }
                        }
                    }
                    GridLayout {
                        id: scopeGrid; objectName: "scopeGrid"
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
                        columns: player.voiceCount>3 ? 2 : 1; rowSpacing: 4; columnSpacing: 4
                        Repeater {
                            model: player.voiceCount
                            delegate: Rectangle {
                                required property int index
                                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
                                color: "#10191d"; border.color: "#56606a"; radius: 1; clip: true
                                Layout.row: index%3; Layout.column: Math.floor(index/3)
                                property color traceColor: ["#b8cd86","#86bcc8","#e1b46d"][index%3]
                                Label { x: 6; y: 3; text: (player.voiceCount>3 ? "AY "+(Math.floor(index/3)+1)+" / " : "CHANNEL ")+["A","B","C"][index%3]; font.pixelSize: 8; color: traceColor; font.letterSpacing: 1 }
                                Canvas {
                                    id: voiceScope; objectName: "channelScope"+index; clip: true
                                    anchors.fill: parent; anchors.margins: 5; anchors.topMargin: 16
                                    onPaint: {
                                        let c=getContext("2d");c.reset();c.save();c.beginPath();c.rect(0,0,width,height);c.clip();c.lineWidth=1;c.strokeStyle="#233035"
                                        for(let x=0;x<width;x+=20){c.beginPath();c.moveTo(x,0);c.lineTo(x,height);c.stroke()}
                                        for(let y=height/2%12;y<height;y+=12){c.beginPath();c.moveTo(0,y);c.lineTo(width,y);c.stroke()}
                                        let channels=player.channelScopes, v=channels.length>index ? channels[index] : []
                                        c.strokeStyle=parent.traceColor;c.lineWidth=1.2;c.beginPath()
                                        if(v.length<2){c.moveTo(0,height/2);c.lineTo(width,height/2)}
                                        else {
                                            let low=v[0],high=v[0];for(let a of v){low=Math.min(low,a);high=Math.max(high,a)}
                                            let center=(low+high)/2,scale=.43/Math.max(.5,(high-low)/2)
                                            for(let i=0;i<v.length;i++){
                                                let x=i*width/(v.length-1),y=Math.max(2,Math.min(height-2,height/2-(v[i]-center)*height*scale))
                                                if(i===0)c.moveTo(x,y);else c.lineTo(x,y)
                                            }
                                        }
                                        c.stroke();c.restore()
                                    }
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()
                                    Connections { target: player; function onVisualChanged(){voiceScope.requestPaint()} }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; Layout.preferredHeight: 14; Layout.minimumHeight: 14; spacing: 5
                        Repeater {
                            model: 2
                            delegate: RowLayout {
                                required property int index
                                Layout.fillWidth: true; spacing: 4
                                Label { text: index===0 ? "L" : "R"; color: "#a8b1ba"; font.pixelSize: 9 }
                                Rectangle {
                                    Layout.fillWidth: true; height: 9; color: "#151c22"; border.color: "#677079"
                                    Rectangle { x: 1; y: 1; height: parent.height-2; width: Math.min(1,index===0 ? player.peakLeft : player.peakRight)*(parent.width-2); gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: "#718c60" } GradientStop { position: .75; color: "#bea66c" } GradientStop { position: 1; color: "#cb7058" } } }
                                    Repeater { model: 14; Rectangle { required property int index; x: (index+1)*parent.width/15; width: 1; height: parent.height; color: "#252d32" } }
                                }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 35; Layout.minimumHeight: 35
                        color: "#141c22"; border.color: "#68717c"; clip: true
                        Canvas {
                            id: waveform; anchors.fill: parent; anchors.margins: 3
                            onPaint: {
                                let c=getContext("2d");c.reset();let v=player.waveform
                                c.strokeStyle="#334047";c.beginPath();c.moveTo(0,height/2);c.lineTo(width,height/2);c.stroke()
                                for(let i=0;i<v.length;i++){let x=i*width/v.length,h=Math.max(1,v[i]*height*.85);c.fillStyle=i/v.length<player.position/Math.max(1,player.duration)?"#c7a267":"#586963";c.fillRect(x,(height-h)/2,Math.max(1,width/v.length-1),h)}
                            }
                            onWidthChanged: requestPaint()
                        }
                        Rectangle { width: 1; y: 1; height: parent.height-2; color: "#f1d49d"; x: 1+player.position/Math.max(1,player.duration)*(parent.width-3) }
                        MouseArea { objectName: "seekWaveform"; anchors.fill: parent; enabled: player.duration>0; cursorShape: Qt.PointingHandCursor; onPressed: mouse => player.seek(mouse.x/width*player.duration); onPositionChanged: mouse => {if(pressed)player.seek(mouse.x/width*player.duration)} }
                        Connections { target: player; function onVisualChanged(){waveform.requestPaint()} function onPositionChanged(){waveform.requestPaint()} }
                    }
                    RetroSlider { id: timeline; objectName: "timeline"; Layout.fillWidth: true; Layout.preferredHeight: 22; Layout.minimumHeight: 22; from: 0; to: Math.max(1,player.duration); value: player.position; enabled: player.duration>0; onMoved: player.seek(value); Accessible.name: "Playback position" }
                    RowLayout {
                        objectName: "volumeControls"
                        Layout.fillWidth: true; Layout.preferredHeight: 24; Layout.minimumHeight: 24; spacing: 5
                        Label { text: "VOL"; color: "#b2bac2"; font.pixelSize: 9 }
                        RetroSlider { Layout.fillWidth: true; from: 0; to: 1; value: player.volume; onMoved: player.volume=value; Accessible.name: "Volume" }
                        Label { text: Math.round(player.volume*100)+"%"; Layout.preferredWidth: 30; color: "#d6c299"; font.family: window.monoFont; font.pixelSize: 10 }
                        RetroButton { implicitWidth: 48; implicitHeight: 24; text: "REPEAT"; font.pixelSize: 9; checkable: true; checked: player.loop; onToggled: player.loop=checked; Accessible.name: "Repeat full song"; ToolTip.visible: hovered; ToolTip.text: "Repeat the whole song (R)" }
                    }
                    RowLayout {
                        objectName: "transportControls"
                        Layout.fillWidth: true; Layout.preferredHeight: 37; Layout.minimumHeight: 37; spacing: 4
                        RetroButton { Layout.fillWidth: true; implicitWidth: 42; implicitHeight: 36; symbol: "previous"; text: "Previous (Z)"; Accessible.name: "Previous track"; onClicked: player.next(-1) }
                        RetroButton { Layout.fillWidth: true; implicitWidth: 68; implicitHeight: 36; accent: true; symbol: player.playing && player.selected===player.currentTrack ? "pause" : "play"; text: player.playing && player.selected===player.currentTrack ? "Pause (C)" : "Play (X)"; Accessible.name: player.playing && player.selected===player.currentTrack ? "Pause" : "Play"; enabled: player.playlistModel.count>0 && !player.busy; onClicked: {if(player.selected!==player.currentTrack)player.play();else player.pause()} }
                        RetroButton { Layout.fillWidth: true; implicitWidth: 42; implicitHeight: 36; symbol: "stop"; text: "Stop (V)"; Accessible.name: "Stop"; onClicked: player.stop() }
                        RetroButton { Layout.fillWidth: true; implicitWidth: 42; implicitHeight: 36; symbol: "next"; text: "Next (B)"; Accessible.name: "Next track"; onClicked: player.next(1) }
                    }
                }
            }
            RetroPanel {
                id: playlistPanel; objectName: "playlistPanel"
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 420; clip: true
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 6; spacing: 4
                    RowLayout {
                        Layout.fillWidth: true; Layout.preferredHeight: 23; Layout.minimumHeight: 23
                        Label { text: "PLAYLIST"; color: "#dce2e9"; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                        Item { Layout.fillWidth: true }
                        Label { text: player.playlistModel.count+" TRACKS"; color: "#b6a481"; font.pixelSize: 9; font.family: window.monoFont }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 23; Layout.minimumHeight: 23
                        border.color: "#1a1e24"
                        gradient: Gradient { GradientStop { position: 0; color: "#76808b" } GradientStop { position: 1; color: "#48535e" } }
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 5; anchors.rightMargin: 14; spacing: 8
                            Label { text: "#"; Layout.preferredWidth: 28; color: "#ebedf0"; font.pixelSize: 10 }
                            Label { text: "Title"; Layout.fillWidth: true; color: "#ebedf0"; font.pixelSize: 11 }
                            Label { text: "Author"; visible: playlistPanel.width>650; Layout.preferredWidth: 155; color: "#ebedf0"; font.pixelSize: 11 }
                            Label { text: "Type"; Layout.preferredWidth: 40; color: "#ebedf0"; font.pixelSize: 11 }
                            Label { text: "Time"; Layout.preferredWidth: 43; horizontalAlignment: Text.AlignRight; color: "#ebedf0"; font.pixelSize: 11 }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
                        color: "#181d23"; border.color: "#68727d"
                        ListView {
                            id: list; objectName: "playlistView"
                            anchors.fill: parent; anchors.margins: 1; clip: true
                            model: player.playlistModel; currentIndex: player.selected; reuseItems: true; spacing: 0
                            boundsBehavior: Flickable.StopAtBounds; keyNavigationEnabled: true
                            onCurrentIndexChanged: {if(currentIndex>=0 && currentIndex!==player.selected)player.select(currentIndex)}
                            ScrollBar.vertical: ScrollBar { width: 12; policy: ScrollBar.AsNeeded }
                            Keys.onReturnPressed: player.activate(currentIndex)
                            Keys.onEnterPressed: player.activate(currentIndex)
                            Keys.onDeletePressed: player.remove(currentIndex)
                            delegate: ItemDelegate {
                                id: trackRow
                                required property int index
                                required property string filePath
                                required property string trackTitle
                                required property string trackAuthor
                                required property string trackFormat
                                required property real trackDuration
                                required property string metadataError
                                width: ListView.view.width; height: 22; padding: 0
                                background: Rectangle {
                                    color: trackRow.index===player.selected ? "#6d5736" : trackRow.hovered ? "#333f49" : trackRow.index%2===0 ? "#181d23" : "#20262d"
                                    border.color: trackRow.index===player.selected ? "#ad8c54" : "transparent"
                                }
                                contentItem: RowLayout {
                                    spacing: 8
                                    Label { text: trackRow.index===player.currentTrack ? "▶" : (trackRow.index+1).toString().padStart(2,"0"); Layout.leftMargin: 5; Layout.preferredWidth: 28; color: trackRow.index===player.currentTrack ? "#f0c275" : "#717f8d"; font.family: window.monoFont; font.pixelSize: 10 }
                                    Label { text: trackRow.trackTitle; Layout.fillWidth: true; elide: Text.ElideRight; color: trackRow.index===player.currentTrack ? "#f3d69b" : "#d4dde5"; font.pixelSize: 11 }
                                    Label { text: trackRow.trackAuthor; visible: playlistPanel.width>650; Layout.preferredWidth: 155; elide: Text.ElideRight; color: "#8f9ca9"; font.pixelSize: 10 }
                                    Label { text: trackRow.trackFormat; Layout.preferredWidth: 40; elide: Text.ElideRight; color: "#8eacbd"; font.pixelSize: 10 }
                                    Label { text: trackRow.trackDuration>=0 ? time(trackRow.trackDuration) : "—"; Layout.preferredWidth: 43; Layout.rightMargin: 14; horizontalAlignment: Text.AlignRight; color: "#aab8bf"; font.pixelSize: 10; font.family: window.monoFont }
                                }
                                ToolTip.visible: hovered; ToolTip.delay: 600
                                ToolTip.text: trackTitle+(trackAuthor ? "\n"+trackAuthor : "")+"\n"+filePath+(metadataError ? "\n"+metadataError : "")
                                onClicked: {list.forceActiveFocus();list.currentIndex=index;player.select(index)}
                                onDoubleClicked: player.activate(index)
                            }
                            Label { anchors.centerIn: parent; visible: player.playlistModel.count===0; text: "Your music library\n\nAdd files or a folder to begin."; horizontalAlignment: Text.AlignHCenter; color: "#788795"; font.pixelSize: 12 }
                            Connections { target: player; function onChanged(){list.currentIndex=player.selected} }
                        }
                    }
                    RowLayout {
                        objectName: "playlistToolsBar"
                        Layout.fillWidth: true; Layout.preferredHeight: 28; Layout.minimumHeight: 28; spacing: 4
                        RetroButton { objectName: "addFilesButton"; text: "Add…"; onClicked: player.browse("files") }
                        RetroButton { objectName: "addFolderButton"; text: "Folder…"; onClicked: player.browse("folder") }
                        RetroButton { text: "Remove"; enabled: player.selected>=0 && !player.busy; onClicked: player.remove(player.selected) }
                        RetroButton { text: "Save list…"; onClicked: player.browse("playlistSave") }
                        Item { Layout.fillWidth: true }
                        RetroButton { id: listToolsButton; objectName: "listToolsButton"; text: "List tools ▾"; onClicked: playlistTools.popup(listToolsButton, listToolsButton.width-playlistTools.width, -playlistTools.height-4) }
                    }
                    Label { text: player.importing ? "Reading track information…" : player.importStatus || "Enter / double-click to play · Delete to remove"; Layout.fillWidth: true; Layout.preferredHeight: 14; Layout.minimumHeight: 14; elide: Text.ElideRight; color: "#a4aeb7"; font.pixelSize: 9 }
                }
                SkinMenu {
                    id: playlistTools; objectName: "playlistToolsMenu"
                    Action { text: "Move up"; enabled: player.selected>0 && !player.busy; onTriggered: player.move(player.selected,player.selected-1) }
                    Action { text: "Move down"; enabled: player.selected>=0 && player.selected<player.playlistModel.count-1 && !player.busy; onTriggered: player.move(player.selected,player.selected+1) }
                    MenuSeparator {}
                    Action { text: "Open playlist…"; onTriggered: player.browse("playlistOpen") }
                    Action { text: "Save playlist…"; onTriggered: player.browse("playlistSave") }
                    Action { text: "Clear playlist"; enabled: !player.busy; onTriggered: player.clearPlaylist() }
                    MenuSeparator {}
                    Action { text: "Export current song…"; enabled: player.duration>0; onTriggered: player.browse("wav") }
                    Action { text: "Export playlist…"; enabled: player.playlistModel.count>0 && !player.exporting; onTriggered: player.browse("batch") }
                }
            }
        }
        Rectangle {
            objectName: "statusStrip"
            Layout.fillWidth: true; Layout.preferredHeight: 25; Layout.minimumHeight: 25
            color: player.error.length>0 ? "#503730" : "#242b33"; border.color: player.error.length>0 ? "#a37760" : "#68727c"; radius: 1
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 7; anchors.rightMargin: 5; spacing: 7
                Label { text: player.error || player.status; Layout.fillWidth: true; elide: Text.ElideRight; color: player.error ? "#f0c6ae" : "#bec7d0"; font.pixelSize: 10; ToolTip.visible: statusHover.hovered; ToolTip.text: text; HoverHandler { id: statusHover } }
                Label { text: player.device || "OUTPUT: STANDBY"; visible: !player.busy && !player.exporting && window.width>950; Layout.maximumWidth: 300; elide: Text.ElideRight; color: "#8999a7"; font.pixelSize: 9 }
                ProgressBar { visible: player.busy || player.exporting; value: player.progress; Layout.preferredWidth: 100; Layout.preferredHeight: 9 }
                RetroButton { text: "Cancel"; visible: player.busy || player.exporting; implicitHeight: 20; onClicked: player.cancel() }
            }
        }
    }
    SkinDialog {
        id: mixer; objectName: "mixerDialog"; title: "Mixer & emulation"; modal: true; anchors.centerIn: parent
        function pan(mode) {
            let gains = [255,255,255,255,255,255]
            if (mode !== "Mono") {
                let echo = chip.currentIndex === 0 ? 85 : 13
                let slots = [[255,echo],[170,170],[echo,255]]
                for (let i=0;i<3;i++) { let channel=mode.charCodeAt(i)-65; gains[channel*2]=slots[i][0];gains[channel*2+1]=slots[i][1] }
            }
            al.value=gains[0];ar.value=gains[1];bl.value=gains[2];br.value=gains[3];cl.value=gains[4];cr.value=gains[5]
        }
        width: Math.min(window.width-40, 590); height: Math.min(window.height-70, 450); standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: { chip.currentIndex=player.ym?1:0; sampleRate.currentIndex=sampleRate.find(player.rate.toString()); filtering.checked=player.filtered; clock.value=player.clock; interrupts.value=Math.round(player.interruptHz*1000); preamp.value=player.preamp; let g=player.gains; al.value=g[0];ar.value=g[1];bl.value=g[2];br.value=g[3];cl.value=g[4];cr.value=g[5]; fileTiming.checked=player.fileTiming; stereo.currentIndex=0 }
        onAccepted: player.applySettings(chip.currentIndex===1, clock.value, interrupts.value/1000, parseInt(sampleRate.currentText), preamp.value, filtering.checked, [al.value,ar.value,bl.value,br.value,cl.value,cr.value],fileTiming.checked)
        component MixerGroup: GroupBox {
            padding: 9; topPadding: 16
            label: Label { x: 9; y: -1; text: parent.title; color: "#e4c589"; font.pixelSize: 11; font.bold: true }
            background: Rectangle { y: 6; height: parent.height-6; color: "#30363e"; border.color: "#626b75"; radius: 2 }
        }
        component GainSlider: RetroSlider {
            required property var coefficient
            objectName: coefficient.objectName + "Slider"
            Layout.fillWidth: true; implicitHeight: 22; from: 0; to: 255; stepSize: 1
            value: coefficient.value
            onMoved: coefficient.value = Math.round(value)
        }
        component GainValue: RetroSpinBox { from: 0; to: 255; editable: true; implicitWidth: 68; implicitHeight: 23 }
        contentItem: GridLayout {
            columns: 2; columnSpacing: 10; rowSpacing: 8
            MixerGroup {
                objectName: "mixerChannels"; title: "Channel amplification"; Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: implicitHeight; Layout.preferredWidth: 330
                ColumnLayout {
                    anchors.fill: parent; spacing: 3
                    GridLayout {
                        columns: 3; columnSpacing: 7; rowSpacing: 1; Layout.fillWidth: true
                        Label { text: "A · left"; font.pixelSize: 11 } GainSlider { coefficient: al } GainValue { id: al }
                        Label { text: "A · right"; font.pixelSize: 11 } GainSlider { coefficient: ar } GainValue { id: ar; objectName: "gainAR" }
                        Label { text: "B · left"; font.pixelSize: 11 } GainSlider { coefficient: bl } GainValue { id: bl }
                        Label { text: "B · right"; font.pixelSize: 11 } GainSlider { coefficient: br } GainValue { id: br }
                        Label { text: "C · left"; font.pixelSize: 11 } GainSlider { coefficient: cl } GainValue { id: cl }
                        Label { text: "C · right"; font.pixelSize: 11 } GainSlider { coefficient: cr } GainValue { id: cr }
                        Rectangle { Layout.columnSpan: 3; Layout.fillWidth: true; implicitHeight: 1; color: "#626b75"; Layout.topMargin: 4; Layout.bottomMargin: 4 }
                        Label { text: "Preamp"; font.pixelSize: 11 } GainSlider { coefficient: preamp } GainValue { id: preamp }
                    }
                    RowLayout {
                        Label { text: "Stereo"; font.pixelSize: 11 }
                        RetroComboBox { id: stereo; objectName: "mixerStereo"; Layout.fillWidth: true; implicitHeight: 26; model: ["Custom","Mono","ABC","ACB","BAC","BCA","CAB","CBA"]; onActivated: index => { if(index>0)mixer.pan(currentText) } }
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 205; spacing: 8
                MixerGroup {
                    title: "Sound chip"; Layout.fillWidth: true
                    RetroComboBox { id: chip; width: parent.width; implicitHeight: 26; model: ["AY-3-8910","YM2149F"] }
                }
                MixerGroup {
                    title: "Master clock · Hz"; Layout.fillWidth: true
                    ColumnLayout {
                        anchors.fill: parent; spacing: 4
                        RetroSpinBox { id: clock; Layout.fillWidth: true; from: 1000000; to: 3546800; value: 1773400; editable: true; implicitHeight: 26 }
                        RetroComboBox {
                            Layout.fillWidth: true; implicitHeight: 26; model: ["Clock presets…","ZX Spectrum · 1773400","Pentagon · 1750000","Atari ST · 2000000","Amstrad · 1000000"]
                            onActivated: index => { if(index>0)clock.value=[1773400,1750000,2000000,1000000][index-1] }
                        }
                    }
                }
                MixerGroup {
                    title: "Interrupt · Hz"; Layout.fillWidth: true
                    RetroSpinBox {
                        id: interrupts; width: parent.width; implicitHeight: 26; from: 1000; to: 2000000; value: 50000; stepSize: 1000; editable: true
                        textFromValue: function(value, locale) { return (value/1000).toFixed(3) }
                        valueFromText: function(text, locale) { return Math.round(parseFloat(text)*1000) }
                        validator: DoubleValidator { bottom: 1; top: 2000; decimals: 3; notation: DoubleValidator.StandardNotation }
                    }
                }
                Item { Layout.fillHeight: true }
            }
            MixerGroup {
                objectName: "mixerOutput"; title: "Output & resampling"; Layout.columnSpan: 2; Layout.fillWidth: true
                GridLayout {
                    anchors.fill: parent; columns: 3; columnSpacing: 9; rowSpacing: 3
                    Label { text: "PCM · Hz"; font.pixelSize: 11 }
                    RetroComboBox { id: sampleRate; implicitWidth: 100; implicitHeight: 26; model: ["22050","44100","48000","96000"] }
                    RetroCheckBox { id: filtering; text: "FIR filter"; checked: true }
                    RetroCheckBox { id: fileTiming; Layout.columnSpan: 3; text: "Use interrupt rate stored in register logs"; checked: true }
                }
            }
        }
    }

    SkinDialog {
        id: about; title: "About C++Ay"; anchors.centerIn: parent; modal: true; standardButtons: Dialog.Close; width: 480
        Label { width: parent.width; wrapMode: Text.Wrap; text: "C++Ay · native C++20 / Qt Quick\n\nPT3, PSG and YM3 decoding and AY/YM rendering ported from Sergey Bulba’s AY_Emul source. Chip amplitude tables credited to Hacker KAY.\n\nThe supplied Flexo02 fixture renders bit-exact. Full AY_Emul format and application parity is still in progress." }
    }
}
