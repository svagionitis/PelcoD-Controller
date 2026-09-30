import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Rectangle {
    id: root

    property string title: "Metric"
    property string value: "--"
    property string unit: ""
    property color accentColor: SightlineTheme.primary
    property string iconText: "📊"

    implicitWidth: 160
    implicitHeight: 80
    color: SightlineTheme.surface
    radius: SightlineTheme.radiusMedium
    border.color: SightlineTheme.cardBorder
    border.width: 1

    Row {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Rectangle {
            width: 38
            height: 38
            radius: 8
            color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.15)
            anchors.verticalCenter: parent.verticalCenter

            Text {
                anchors.centerIn: parent
                text: root.iconText
                font.pixelSize: 18
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: root.title
                color: SightlineTheme.textSecondary
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.weight: Font.Medium
            }

            Row {
                spacing: 4
                Text {
                    text: root.value
                    color: SightlineTheme.textPrimary
                    font.pixelSize: SightlineTheme.fontSizeLarge
                    font.weight: Font.Bold
                }

                Text {
                    visible: root.unit.length > 0
                    text: root.unit
                    color: SightlineTheme.textMuted
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                }
            }
        }
    }
}
