import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: cardRoot
    default property alias content: innerContainer.data
    property string title: ""
    property string subtitle: ""
    property color headerColor: "#141720"
    property color bodyColor: "#10131a"
    property color borderColor: "#242938"

    color: bodyColor
    radius: 6
    border.color: borderColor
    border.width: 1
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Card Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: cardRoot.title.length > 0 ? 38 : 0
            visible: cardRoot.title.length > 0
            color: cardRoot.headerColor

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: cardRoot.borderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 8

                Rectangle {
                    width: 3
                    height: 14
                    color: "#00e5ff"
                    radius: 1
                }

                Text {
                    text: cardRoot.title.toUpperCase()
                    color: "#e2e8f0"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.0
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: cardRoot.subtitle
                    color: "#64748b"
                    font.pixelSize: 11
                    font.family: "Monospace"
                    visible: cardRoot.subtitle.length > 0
                }
            }
        }

        // Card Body
        Item {
            id: innerContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
