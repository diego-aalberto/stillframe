import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1280; height: 850; minimumWidth: 900; minimumHeight: 650
    visible: true; title: engine.title; color: "#f6f1fa"
    font.family: "Segoe UI"; font.pixelSize: 19
    property bool newSave: false
    function act(key, value) { engine.command(key, value || "") }
    FrameAnimation { running: window.visible && engine.animated; onTriggered: engine.tick() }
    Item {
        id: viewport; anchors.fill: parent; anchors.bottomMargin: composer.visible ? composer.height + 80 : 55; clip: true
        Item {
            width: engine.canvasWidth; height: engine.canvasHeight
            scale: Math.max(viewport.width / width, viewport.height / height); anchors.centerIn: parent
            Image { anchors.fill: parent; source: engine.background; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true }
            Repeater { model: engine.layers
                delegate: Image {
                    required property var modelData
                    source: modelData.source; opacity: modelData.opacity; z: modelData.z
                    asynchronous: true; cache: true; smooth: true
                    transform: Matrix4x4 { matrix: modelData.transform }
                }
            }
        }
    }
    component Pill: Button {
        font: window.font
        contentItem: Text { text: parent.text; font: parent.font; color: "#49394f"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 18; color: parent.down ? "#cab9de" : parent.hovered ? "#e3d4f0" : "#eee3f7"; opacity: parent.enabled ? 1 : .6; Behavior on color { ColorAnimation { duration: 120 } } }
    }
    Row {
        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 18; spacing: 10
        visible: !engine.menu
        Pill { text: "♡  Next paths"; onClicked: paths.open() }
        Pill { text: "History"; onClicked: { info.title="History"; infoText.text=engine.history; info.open() } }
        Pill { text: "Menu"; onClicked: act("menu") }
    }
    ColumnLayout {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 24; spacing: 10
        visible: !engine.menu && !engine.story.needs_name
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 190; radius: 26
            color: engine.theme.bubble_fill || "#cdf1eaf9"
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 18
                Label { text: engine.intro ? engine.story.title : engine.speaker; font.bold: true; color: "#49394f" }
                ScrollView { Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    TextArea { text: engine.richDialogue; textFormat: TextEdit.RichText; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; color: "#392e42"; font: window.font; background: null }
                }
            }
        }
        Label { Layout.fillWidth: true; text: engine.error || engine.story.note || "A little honesty goes a long way."; color: "#49394f"; wrapMode: Text.Wrap }
        RowLayout {
            id: composer; objectName: "conversationInput"; Layout.fillWidth: true; visible: engine.canChat
            ComboBox { model: engine.actorNames; textRole: "name"; valueRole: "id"; enabled: !engine.busy; onActivated: engine.selectActor(currentValue) }
            ScrollView { Layout.fillWidth: true; Layout.preferredHeight: Math.min(200, Math.max(46, input.contentHeight+20)); clip: true
                TextArea { id: input; wrapMode: TextEdit.Wrap; placeholderText: "What would you like to say?"; font: window.font
                    Keys.onPressed: function(event) { if ((event.modifiers & Qt.ControlModifier) && (event.key===Qt.Key_Return || event.key===Qt.Key_Enter) && !engine.busy) { engine.send(text); text=""; event.accepted=true } }
                }
            }
            Pill { text: "Speak"; enabled: !engine.busy && input.text.trim().length>0; onClicked: { engine.send(input.text); input.text="" } }
        }
        Flow {
            Layout.fillWidth: true; spacing: 10; enabled: !engine.busy
            Pill { visible: engine.intro; text: "Begin scene"; onClicked: act("begin") }
            Pill { objectName: "continueAction"; visible: !engine.intro && (engine.story.choice || engine.story.hub) && !engine.story.ended; text: engine.story.hub ? "Next day" : "Continue"; onClicked: act("advance") }
            Pill { objectName: "keepTalkingAction"; visible: engine.story.can_keep_talking && !engine.intro; text: "Keep talking"; onClicked: act("keep") }
            Repeater { model: engine.story.hub ? engine.visits : []
                delegate: Pill { required property var modelData; text: "Visit "+modelData.name; onClicked: act("visit",modelData.id) }
            }
            Pill { visible: engine.story.pending; text: "Retry response"; onClicked: act("retry") }
            Pill { visible: engine.story.ended; text: "New story"; onClicked: { window.newSave=true; nameDialog.open() } }
        }
    }
    Rectangle {
        visible: engine.menu; width: Math.min(470,parent.width*.48); height: parent.height; color: "#f6f1fa"
        ColumnLayout {
            anchors.centerIn: parent; width: parent.width-64; spacing: 24
            Label { text: "The Light\nWe Leave On"; font.pixelSize: 40; color: "#49394f" }
            Label { Layout.fillWidth: true; text: "A summer of late conversations, unexpected closeness, and finding your place."; wrapMode: Text.Wrap; color: "#49394f" }
            Pill { Layout.fillWidth: true; text: "Continue your story"; enabled: !engine.busy; onClicked: { if(engine.story.needs_name) { window.newSave=false; nameDialog.open() } else act("resume") } }
            Pill { Layout.fillWidth: true; text: "New story"; enabled: !engine.busy; onClicked: { window.newSave=true; nameInput.text=""; nameDialog.open() } }
            Pill { Layout.fillWidth: true; text: "How to play"; onClicked: { info.title="How to play"; infoText.text="Speak in your own words. Choose who you address. She offers Continue when an outcome is reached or she needs space. Keep talking preserves an agreed outcome, but a repeated boundary violation can end the conversation. Use Next paths for a gentle nudge. Progress saves automatically. Ctrl+Enter sends your message."; info.open() } }
            Pill { Layout.fillWidth: true; text: "Quit"; enabled: !engine.busy; onClicked: window.close() }
        }
    }
    Dialog {
        id: nameDialog; title: "What should they call you?"; modal: true; anchors.centerIn: parent; width: 440
        ColumnLayout { anchors.fill: parent
            TextField { id: nameInput; Layout.fillWidth: true; maximumLength: 32; placeholderText: "Your name" }
            Label { text: engine.error; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Pill { text: "Begin my story"; onClicked: { act(window.newSave ? "new" : "name",nameInput.text); if(!engine.error) nameDialog.close() } }
            Pill { text: "Back"; onClicked: nameDialog.close() }
        }
    }
    Popup {
        id: paths; objectName: "pathsPanel"; modal: true; anchors.centerIn: parent
        width: Math.min(620,window.width-48); height: Math.min(690,window.height-48); padding: 28
        background: Rectangle { radius: 28; color: "#fbf6fc"; border.color: "#e6d5e9"; border.width: 1 }
        Overlay.modal: Rectangle { color: "#70342d46" }
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 } }
        ColumnLayout {
            anchors.fill: parent; spacing: 14
            RowLayout {
                Layout.fillWidth: true
                Label { text: "A little further together"; font.pixelSize: 25; font.bold: true; color: "#513d60"; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Pill { text: "×"; onClicked: paths.close() }
            }
            Label { text: engine.story.title; color: "#957697"; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Label { text: "Your next stops, with a small nudge. Speak in your own words."; color: "#77667e"; Layout.fillWidth: true; wrapMode: Text.Wrap }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
                Column {
                    width: parent.width; spacing: 12
                    Repeater {
                        model: engine.story.paths
                        delegate: Rectangle {
                            required property var modelData
                            width: parent.width; height: details.implicitHeight+36; radius: 20; color: "#ffffff"
                            border.color: "#ecdfef"
                            Rectangle { x: 0; y: 20; width: 5; height: parent.height-40; radius: 2; color: modelData.color }
                            ColumnLayout {
                                id: details; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 18; spacing: 9
                                Label { text: modelData.title; color: "#503a5d"; font.bold: true; font.pixelSize: 20; Layout.fillWidth: true; wrapMode: Text.Wrap }
                                Label { text: modelData.hint; color: "#796881"; Layout.fillWidth: true; wrapMode: Text.Wrap }
                                Label { text: "→  "+modelData.destination; color: "#725584"; font.bold: true; font.pixelSize: 15; Layout.fillWidth: true; wrapMode: Text.Wrap }
                            }
                        }
                    }
                    Label { visible: engine.story.ended; text: "This chapter is complete. Your epilogue is waiting below."; width: parent.width; wrapMode: Text.Wrap; color: "#796881" }
                }
            }
            Pill { text: "Back to our conversation"; Layout.fillWidth: true; onClicked: paths.close() }
        }
    }
    Dialog {
        id: info; modal: true; anchors.centerIn: parent; width: Math.min(650,window.width-60); height: Math.min(650,window.height-60); standardButtons: Dialog.Close
        ScrollView { anchors.fill: parent; clip: true
            TextArea { id: infoText; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; textFormat: TextEdit.PlainText }
        }
    }
    Shortcut { sequence: "Escape"; onActivated: act(engine.menu ? "resume" : "menu") }
    onClosing: function(close) { if(engine.busy) { close.accepted=false; info.title="Finishing your conversation"; infoText.text="Wait for this response to save before closing."; info.open() } }
}
