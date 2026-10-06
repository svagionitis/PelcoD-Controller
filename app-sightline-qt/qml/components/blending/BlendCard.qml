import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../.."

// BlendCard.qml
// Themed card frame with accent title bar and hover highlight.

Rectangle {
    id: root

    property string title: ""
    property string subtitle: ""
    property color accent: SightlineTheme.primary
    default property alias content: body.data

    Layout.fillWidth: true
    implicitHeight: frame.implicitHeight + 28
    color: SightlineTheme.surfaceCard
    radius: SightlineTheme.radiusMedium
    border.width: 1
    border.color: hover.hovered ? SightlineTheme.cardBorderHighlight : SightlineTheme.cardBorder

    Behavior on border.color { ColorAnimation { duration: 160 } }

    HoverHandler { id: hover }

    // Accent glow strip along the top edge
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 }
        height: 2
        radius: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: root.accent }
            GradientStop { position: 1.0; color: "transparent" }
        }
        opacity: hover.hovered ? 0.9 : 0.45
        Behavior on opacity { NumberAnimation { duration: 160 } }
    }

    ColumnLayout {
        id: frame
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle { implicitWidth: 3; implicitHeight: 14; radius: 1.5; color: root.accent }

            Text {
                text: root.title
                color: root.accent
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 0.6
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.subtitle
                visible: text.length > 0
                color: SightlineTheme.textMuted
                font.pixelSize: 10
                font.family: "Monospace"
            }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: 10
        }
    }
}
