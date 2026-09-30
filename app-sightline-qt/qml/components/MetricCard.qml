import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."

Rectangle {
    id: root

    property string title: "Metric"
    property string value: "--"
    property string unit: ""
    property color accentColor: SightlineTheme.primary
    property string iconText: "📊"

    implicitWidth: 160
    implicitHeight: 62
    color: SightlineTheme.surfaceCard
    radius: SightlineTheme.radiusMedium
    border.color: SightlineTheme.cardBorder
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Rectangle {
            implicitWidth: 32
            implicitHeight: 32
            radius: 6
            color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.15)
            Layout.alignment: Qt.AlignVCenter

            Text {
                anchors.centerIn: parent
                text: root.iconText
                font.pixelSize: 15
            }
        }

        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            Text {
                text: root.title
                color: SightlineTheme.textSecondary
                font.pixelSize: 10
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: 4
                Text {
                    text: root.value
                    color: SightlineTheme.textPrimary
                    font.pixelSize: 13
                    font.bold: true
                }

                Text {
                    visible: root.unit.length > 0
                    text: root.unit
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignBaseline
                }

                Item { Layout.fillWidth: true }
            }
        }
    }
}
