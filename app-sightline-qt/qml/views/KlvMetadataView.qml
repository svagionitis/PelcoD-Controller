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
                text: "🏷️ MISB 0601 / 0102 KLV METADATA INJECTION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x73 / UAS Geospatial Telemetry"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Platform Latitude"
                value: parseFloat(latInput.text).toFixed(4)
                unit: "°N"
                accentColor: SightlineTheme.primary
                iconText: "🌐"
            }
            MetricCard {
                title: "Platform Longitude"
                value: parseFloat(lonInput.text).toFixed(4)
                unit: "°E"
                accentColor: SightlineTheme.primary
                iconText: "🌐"
            }
            MetricCard {
                title: "Altitude MSL"
                value: altInput.text
                unit: "m"
                accentColor: SightlineTheme.info
                iconText: "⛰️"
            }
            MetricCard {
                title: "Heading / Yaw"
                value: headingInput.text
                unit: "°"
                accentColor: SightlineTheme.warning
                iconText: "🧭"
            }
            Item { Layout.fillWidth: true }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: SightlineTheme.surface
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16

                Text {
                    text: "UAS NAVIGATION & ATTITUDE STATE"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Geodetic Position:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Text { text: "Lat:"; color: SightlineTheme.textMuted }
                    TextField { id: latInput; text: "37.7749"; Layout.preferredWidth: 100; color: SightlineTheme.textPrimary; background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 } }
                    Text { text: "Lon:"; color: SightlineTheme.textMuted }
                    TextField { id: lonInput; text: "-122.4194"; Layout.preferredWidth: 100; color: SightlineTheme.textPrimary; background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 } }
                    Text { text: "Alt (m):"; color: SightlineTheme.textMuted }
                    TextField { id: altInput; text: "1500.0"; Layout.preferredWidth: 90; color: SightlineTheme.textPrimary; background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 } }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Platform Attitude:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Text { text: "Heading:"; color: SightlineTheme.textMuted }
                    TextField { id: headingInput; text: "180.0"; Layout.preferredWidth: 80; color: SightlineTheme.textPrimary; background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 } }
                    Text { text: "Pitch:"; color: SightlineTheme.textMuted }
                    TextField { id: pitchInput; text: "-15.0"; Layout.preferredWidth: 80; color: SightlineTheme.textPrimary; background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 } }
                    Text { text: "Roll:"; color: SightlineTheme.textMuted }
                    TextField { id: rollInput; text: "0.0"; Layout.preferredWidth: 80; color: SightlineTheme.textPrimary; background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 } }
                }

                RowLayout {
                    spacing: 12
                    Layout.topMargin: 12

                    Button {
                        text: "Transmit Single KLV Frame"
                        Layout.preferredWidth: 220
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            bridge.setMetadata(
                                parseFloat(latInput.text),
                                parseFloat(lonInput.text),
                                parseFloat(altInput.text),
                                parseFloat(headingInput.text),
                                parseFloat(pitchInput.text),
                                parseFloat(rollInput.text)
                            );
                        }
                    }

                    CheckBox {
                        id: autoStreamKlv
                        text: "Auto-stream 10 Hz simulated flight telemetry"
                        checked: false
                    }
                }

                Timer {
                    id: klvSimTimer
                    interval: 100
                    running: autoStreamKlv.checked
                    repeat: true
                    onTriggered: {
                        var h = (parseFloat(headingInput.text) + 0.2) % 360.0;
                        headingInput.text = h.toFixed(1);
                        bridge.setMetadata(
                            parseFloat(latInput.text),
                            parseFloat(lonInput.text),
                            parseFloat(altInput.text),
                            h,
                            parseFloat(pitchInput.text),
                            parseFloat(rollInput.text)
                        );
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }
}
