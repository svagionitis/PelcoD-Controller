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
                text: "🔀 DUAL-SENSOR VIDEO BLENDING & FUSION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x68 / EO + IR Pixel Blending"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Primary Sensor"
                value: "Visible EO"
                accentColor: SightlineTheme.primary
                iconText: "☀️"
            }
            MetricCard {
                title: "Secondary Sensor"
                value: "LWIR Thermal"
                accentColor: SightlineTheme.warning
                iconText: "🌡️"
            }
            MetricCard {
                title: "Blend Ratio"
                value: Math.round(alphaSlider.value / 2.55) + "% IR"
                accentColor: SightlineTheme.info
                iconText: "⚖️"
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
                spacing: 16

                Text {
                    text: "SENSOR FUSION PARAMETERS"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Sensor Pairing:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: pairCombo
                        model: ["Cam 0 (EO) + Cam 1 (IR)", "Cam 1 (IR) + Cam 0 (EO)", "Cam 2 + Cam 3"]
                        Layout.preferredWidth: 260
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Blending Mode:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: modeCombo
                        model: ["Alpha Crossfade (0)", "Max Intensity (1)", "Edge Overlay / High-Pass (2)", "Adaptive Thermography (3)"]
                        Layout.preferredWidth: 280
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Alpha Weight (0-255):"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Slider {
                        id: alphaSlider
                        from: 0
                        to: 255
                        value: 128
                        stepSize: 1
                        Layout.preferredWidth: 220
                    }
                    Text { text: Math.round(alphaSlider.value) + " (" + Math.round(alphaSlider.value / 2.55) + "%)"; color: SightlineTheme.textPrimary; font.bold: true }
                }

                Button {
                    text: "Apply Video Blending"
                    Layout.preferredWidth: 200
                    Layout.topMargin: 8
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                    onClicked: {
                        bridge.setBlendParams(0, 1, modeCombo.currentIndex, Math.round(alphaSlider.value));
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
