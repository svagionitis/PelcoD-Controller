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
                text: "🌡️ NON-UNIFORMITY CORRECTION (NUC)"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x67 / Thermal FPA Calibration & BPR"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "FPA Temperature"
                value: "38.2"
                unit: "°C"
                accentColor: SightlineTheme.warning
                iconText: "🌡️"
            }
            MetricCard {
                title: "Bad Pixels Replaced"
                value: "14"
                unit: "px"
                accentColor: SightlineTheme.info
                iconText: "🩹"
            }
            MetricCard {
                title: "Calibration State"
                value: "VALID"
                accentColor: SightlineTheme.success
                iconText: "✅"
            }
            Item { Layout.fillWidth: true }
        }

        // Calibration Controls Card
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
                    text: "FLAT-FIELD CALIBRATION (FFC) & BAD PIXEL MAP"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Thermal Camera:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: nucCam
                        model: ["Camera 1 (LWIR Thermal)", "Camera 2 (MWIR Cooled)", "Camera 0"]
                        Layout.preferredWidth: 240
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Calibration Mode:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: nucMode
                        model: ["1-Point Shutter Flat-Field (Quick)", "2-Point Blackbody (High Dynamic)", "Scene-Based Dynamic (Continuous)"]
                        Layout.preferredWidth: 280
                    }
                }

                RowLayout {
                    spacing: 12
                    Layout.topMargin: 8

                    Button {
                        text: "Actuate Mechanical Shutter NUC"
                        Layout.preferredWidth: 240
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.warning; radius: 4 }
                        onClicked: {
                            bridge.setReportingMode(nucCam.currentIndex, 100, 0x10);
                        }
                    }

                    Button {
                        text: "Update Bad Pixel Table"
                        Layout.preferredWidth: 200
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            bridge.saveParameters(0x02);
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
