import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

ScrollView {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: parent.width - 32
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { width: 4; height: 18; color: SightlineTheme.primary; radius: 2 }
            Text {
                text: "OPTICAL FOCUS & MOTORIZED LENS CONTROL"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Modules 0x36, 0x41 Autofocus & Zoom"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            MetricCard { title: "Optical Zoom"; value: "18.4x"; accentColor: SightlineTheme.primary; iconText: "🔍" }
            MetricCard { title: "Focus Score"; value: "924"; unit: "pts"; accentColor: SightlineTheme.success; iconText: "🎯" }
            MetricCard { title: "Lens Barrel Temp"; value: "32.6"; unit: "°C"; accentColor: SightlineTheme.warning; iconText: "🌡️" }
            Item { Layout.fillWidth: true }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: lensCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: lensCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "MOTORIZED LENS ACTUATION & AUTOFOCUS"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: lensCam
                        model: ["Camera 0 (EO Zoom)", "Camera 1 (IR Lens)", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Optical Zoom:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Button { text: "Wide (Zoom Out)"; implicitHeight: 30; implicitWidth: 130; onClicked: { if (bridge) bridge.sendLensCommand(lensCam.currentIndex, 0x01, -50); } }
                    Button { text: "Stop"; implicitHeight: 30; implicitWidth: 70; onClicked: { if (bridge) bridge.sendLensCommand(lensCam.currentIndex, 0x01, 0); } }
                    Button { text: "Tele (Zoom In)"; implicitHeight: 30; implicitWidth: 130; onClicked: { if (bridge) bridge.sendLensCommand(lensCam.currentIndex, 0x01, 50); } }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Manual Focus:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Button { text: "Focus Near"; implicitHeight: 30; implicitWidth: 130; onClicked: { if (bridge) bridge.sendLensCommand(lensCam.currentIndex, 0x02, -50); } }
                    Button { text: "Stop"; implicitHeight: 30; implicitWidth: 70; onClicked: { if (bridge) bridge.sendLensCommand(lensCam.currentIndex, 0x02, 0); } }
                    Button { text: "Focus Far"; implicitHeight: 30; implicitWidth: 130; onClicked: { if (bridge) bridge.sendLensCommand(lensCam.currentIndex, 0x02, 50); } }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Autofocus Action:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Button {
                        text: "Trigger One-Push Autofocus"
                        implicitHeight: 32
                        implicitWidth: 200
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.success; radius: 4 }
                        onClicked: { if (bridge) bridge.sendLensCommand(lensCam.currentIndex, 0x03, 1); }
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
