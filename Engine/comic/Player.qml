import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: engine.canvasWidth; height: engine.canvasHeight
    visible: true; title: engine.title
    color: engine.theme.background || "#20232b"
    function setting(key, fallback) { return engine.theme[key] === undefined ? fallback : engine.theme[key] }
    font.family: setting("font_family", "Sans Serif")
    font.pixelSize: setting("font_size", 18)
    FrameAnimation { running: window.visible && engine.animated; onTriggered: engine.tick() }
    Item {
        id: stage
        width: engine.canvasWidth; height: engine.canvasHeight
        scale: Math.min(window.width/width, window.height/height)
        anchors.centerIn: parent
        Image { anchors.fill: parent; source: engine.background; fillMode: Image.PreserveAspectCrop; cache: true; asynchronous: true }
        Repeater {
            model: engine.layers
            delegate: Image {
                required property var modelData
                source: modelData.source
                cache: true; smooth: true; asynchronous: true
                opacity: modelData.opacity; z: modelData.z
                transform: Matrix4x4 { matrix: modelData.transform }
            }
        }
    }
    ColumnLayout {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: 20; spacing: 10
        Rectangle {
            Layout.fillWidth: true; implicitHeight: Math.min(220, Math.max(80, dialogueText.implicitHeight+32))
            color: window.setting("bubble_fill", "#ee202938")
            border.color: window.setting("bubble_edge", "#aebbd0")
            border.width: window.setting("bubble_border", 2)
            radius: window.setting("bubble_radius", 12)
            visible: engine.dialogue.length > 0
            ScrollView {
                anchors.fill: parent; anchors.margins: 16; clip: true
                TextArea {
                    id: dialogueText; text: engine.dialogue; readOnly: true; wrapMode: Text.Wrap
                    color: window.setting("text", "#ffffff")
                    font: window.font; background: null; selectByMouse: true
                }
            }
        }
        Flow {
            Layout.fillWidth: true; spacing: 8
            Repeater {
                model: engine.sceneData.buttons || []
                delegate: Button {
                    required property var modelData
                    text: modelData.label; enabled: !engine.busy
                    font: window.font
                    contentItem: Text { text: parent.text; color: window.setting("button_text", "white"); font: window.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: parent.down ? window.setting("button_pressed", "#46506a") : (parent.hovered ? window.setting("button_hover", "#626d87") : window.setting("button_fill", "#3d4860")); radius: window.setting("button_radius", 6); border.color: window.setting("button_edge", "#9aa8c0") }
                    onClicked: engine.action(modelData.action)
                }
            }
        }
        RowLayout {
            visible: engine.chatEnabled; Layout.fillWidth: true
            ComboBox {
                model: engine.actorNames; textRole: "name"; valueRole: "id"
                enabled: !engine.busy
                onActivated: engine.selectActor(currentValue)
            }
            TextField {
                id: input; Layout.fillWidth: true; placeholderText: engine.busy ? "Generating… You can draft your reply." : "Message"
                onAccepted: if (!engine.busy && text.trim().length) { engine.send(text); text = "" }
            }
            Button { text: "Send"; enabled: !engine.busy && input.text.trim().length > 0; onClicked: { engine.send(input.text); input.text = ""; input.forceActiveFocus() } }
        }
    }
}
