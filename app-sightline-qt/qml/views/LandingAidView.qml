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
                text: "🛬 PRECISION LANDING AID"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x77 / Fiducial & Visual Servo"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Guidance Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Relative X"
                value: "+0.12"
                unit: "m"
                accentColor: SightlineTheme.primary
                iconText: "↔️"
            }
            MetricCard {
                title: "Relative Y"
                value: "-0.04"
                unit: "m"
                accentColor: SightlineTheme.primary
                iconText: "↕️"
            }
            MetricCard {
                title: "Descent Range (Z)"
                value: "14.8"
                unit: "m"
                accentColor: SightlineTheme.info
                iconText: "📏"
            }
            MetricCard {
                title: "Fiducial Lock"
                value: "LOCKED"
                accentColor: SightlineTheme.success
                iconText: "🎯"
            }
            Item { Layout.fillWidth: true }
        }

        // Configuration Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 320
            color: SightlineTheme.surface
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16

                Text {
                    text: "LANDING AID GUIDANCE CONFIGURATION"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: landCam
                        model: ["Camera 0", "Camera 1", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 200
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Target Marker Type:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: markerCombo
                        model: ["Sightline Proprietary Multi-Ring", "AprilTag 36h11", "ArUco 4x4", "High-Contrast H-Pad"]
                        Layout.preferredWidth: 280
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Marker Physical Size:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    TextField {
                        id: markerDimInput
                        text: "0.50"
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 }
                    }
                    Text { text: "meters width"; color: SightlineTheme.textMuted }
                }

                RowLayout {
                    spacing: 12
                    Layout.topMargin: 8

                    Button {
                        text: "Arm Landing Guidance"
                        Layout.preferredWidth: 180
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.success; radius: 4 }
                        onClicked: {
                            bridge.startTracking(landCam.currentIndex, 960, 540, 120, 120, 0x04);
                        }
                    }

                    Button {
                        text: "Disarm Guidance"
                        Layout.preferredWidth: 180
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.error; radius: 4 }
                        onClicked: {
                            bridge.stopTracking(landCam.currentIndex, 0xFF);
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
