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
                text: "🗜️ H.264 / H.265 COMPRESSION & STREAMING"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x32 / 0x40 / Bitrate & Stream Control"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Active Codec"
                value: "H.264 Baseline"
                accentColor: SightlineTheme.primary
                iconText: "🎬"
            }
            MetricCard {
                title: "Live Bitrate"
                value: "4.2"
                unit: "Mbps"
                accentColor: SightlineTheme.info
                iconText: "📶"
            }
            MetricCard {
                title: "GOP Length"
                value: "30"
                unit: "frames"
                accentColor: SightlineTheme.warning
                iconText: "⏱️"
            }
            MetricCard {
                title: "RTSP Status"
                value: "STREAMING"
                accentColor: SightlineTheme.success
                iconText: "📡"
            }
            Item { Layout.fillWidth: true }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 340
            color: SightlineTheme.surface
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 14

                Text {
                    text: "ENCODER BITRATE & GOP PARAMETERS"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Stream Index:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: streamCombo
                        model: ["Stream 0 (Primary RTSP)", "Stream 1 (Secondary RTSP)"]
                        Layout.preferredWidth: 240
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Target Bitrate:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Slider {
                        id: bitrateSlider
                        from: 500
                        to: 15000
                        value: 4000
                        stepSize: 500
                        Layout.preferredWidth: 220
                    }
                    Text { text: (bitrateSlider.value / 1000).toFixed(1) + " Mbps (" + bitrateSlider.value + " kbps)"; color: SightlineTheme.textPrimary; font.bold: true }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "GOP Length:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Slider {
                        id: gopSlider
                        from: 10
                        to: 120
                        value: 30
                        stepSize: 5
                        Layout.preferredWidth: 220
                    }
                    Text { text: Math.round(gopSlider.value) + " frames"; color: SightlineTheme.textPrimary; font.bold: true }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Rate Control:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: rateCtrlCombo
                        model: ["CBR (Constant Bitrate)", "VBR (Variable Bitrate)", "CQP (Constant Quantization)"]
                        Layout.preferredWidth: 240
                    }
                }

                RowLayout {
                    spacing: 12
                    Layout.topMargin: 8

                    Button {
                        text: "Configure Encoder"
                        Layout.preferredWidth: 180
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            bridge.setH264Params(
                                streamCombo.currentIndex,
                                Math.round(bitrateSlider.value),
                                Math.round(gopSlider.value),
                                28,
                                rateCtrlCombo.currentIndex
                            );
                        }
                    }

                    Button {
                        text: "Start Stream"
                        Layout.preferredWidth: 140
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.success; radius: 4 }
                        onClicked: bridge.streamingControl(streamCombo.currentIndex, 1)
                    }

                    Button {
                        text: "Stop Stream"
                        Layout.preferredWidth: 140
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.error; radius: 4 }
                        onClicked: bridge.streamingControl(streamCombo.currentIndex, 0)
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
