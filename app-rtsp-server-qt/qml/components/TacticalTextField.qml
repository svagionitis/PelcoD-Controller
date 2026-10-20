import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: fieldRoot
    property string label: ""
    property alias text: textInput.text
    property string placeholderText: ""
    property alias readOnly: textInput.readOnly
    property alias echoMode: textInput.echoMode
    property bool enabledState: true

    spacing: 4

    Text {
        text: fieldRoot.label
        color: "#94a3b8"
        font.pixelSize: 11
        font.bold: true
        visible: fieldRoot.label.length > 0
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 34
        color: textInput.activeFocus ? "#171c26" : "#0e1117"
        border.color: textInput.activeFocus ? "#00e5ff" : "#242938"
        border.width: 1
        radius: 4

        Behavior on border.color { ColorAnimation { duration: 150 } }
        Behavior on color { ColorAnimation { duration: 150 } }

        TextInput {
            id: textInput
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            verticalAlignment: TextInput.AlignVCenter
            color: fieldRoot.enabledState ? "#f1f5f9" : "#64748b"
            selectionColor: "#00e5ff"
            selectedTextColor: "#000000"
            font.pixelSize: 12
            font.family: "Monospace"
            enabled: fieldRoot.enabledState
            clip: true

            Text {
                text: fieldRoot.placeholderText
                color: "#475569"
                font: textInput.font
                visible: !textInput.text && !textInput.activeFocus
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
