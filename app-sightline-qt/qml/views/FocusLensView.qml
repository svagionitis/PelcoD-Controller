import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        RowLayout {
            spacing: 12
            Text {
                text: "🔭 OPTICAL FOCUS & LENS CONTROL"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x36 / 0x41 / Zoom & Autofocus"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Optical Zoom"
                value: "18.4x"
                accentColor: SightlineTheme.primary
                iconText: "🔍"
            }
            MetricCard {
                title: "Focus Metric"
                value: "924"
                unit: "pts"
                accentColor: SightlineTheme.success
                iconText: "🎯"
            }
            MetricCard {
                title: "Lens Temperature"
                value: "32.6"
                unit: "°C"
                accentColor: SightlineTheme.warning
                iconText: "🌡️"
            }
            Item { Layout.fillWidth: true }
        }

        // Lens Controls Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 340
            color: SightlineTheme.surface
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16

                Text {
                    text: "MOTORIZED LENS ACTUATION"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: lensCam
                        model: ["Camera 0", "Camera 1", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 200
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Optical Zoom:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Button {
                        text: "Wide (Zoom Out)"
                        Layout.preferredWidth: 140
                        onClicked: bridge.sendLensCommand(lensCam.currentIndex, 0x01, -50)
                    }
                    Button {
                        text: "Stop Zoom"
                        Layout.preferredWidth: 100
                        onClicked: bridge.sendLensCommand(lensCam.currentIndex, 0x01, 0)
                    }
                    Button {
                        text: "Tele (Zoom In)"
                        Layout.preferredWidth: 140
                        onClicked: bridge.sendLensCommand(lensCam.currentIndex, 0x01, 50)
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Manual Focus:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Button {
                        text: "Focus Near"
                        Layout.preferredWidth: 140
                        onClicked: bridge.sendLensCommand(lensCam.currentIndex, 0x02, -50)
                    }
                    Button {
                        text: "Stop Focus"
                        Layout.preferredWidth: 100
                        onClicked: bridge.sendLensCommand(lensCam.currentIndex, 0x02, 0)
                    }
                    Button {
                        text: "Focus Far"
                        Layout.preferredWidth: 140
                        onClicked: bridge.sendLensCommand(lensCam.currentIndex, 0x02, 50)
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Autofocus Trigger:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Button {
                        text: "Trigger One-Push Autofocus"
                        Layout.preferredWidth: 220
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.success; radius: 4 }
                        onClicked: bridge.sendLensCommand(lensCam.currentIndex, 0x03, 1)
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
