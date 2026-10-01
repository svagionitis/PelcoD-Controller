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
                text: "DUAL-SENSOR BLENDING & FUSION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Module 0x68 EO + IR Pixel Fusion"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "Base Sensor"; value: "Visible EO"; accentColor: SightlineTheme.primary; iconText: "☀️" }
            MetricCard { title: "Overlay Sensor"; value: "LWIR Thermal"; accentColor: SightlineTheme.warning; iconText: "🌡️" }
            MetricCard { title: "Thermal Mix"; value: Math.round(alphaSlider.value / 2.55) + "% IR"; accentColor: SightlineTheme.info; iconText: "⚖️" }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: blendCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: blendCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "SENSOR FUSION & REGISTRATION PARAMETERS"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Sensor Pairing:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: pairCombo
                        model: ["Cam 0 (EO) + Cam 1 (IR)", "Cam 1 (IR) + Cam 0 (EO)", "Cam 2 + Cam 3"]
                        Layout.preferredWidth: 260
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Fusion Algorithm:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: modeCombo
                        model: ["Alpha Crossfade (0)", "Max Intensity (1)", "Edge Overlay / High-Pass (2)", "Adaptive Thermography (3)"]
                        Layout.preferredWidth: 280
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Alpha Weight (0-255):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider {
                        id: alphaSlider
                        from: 0
                        to: 255
                        value: 128
                        stepSize: 1
                        Layout.preferredWidth: 200
                    }
                    Text { text: Math.round(alphaSlider.value) + " (" + Math.round(alphaSlider.value / 2.55) + "%)"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Video Blending"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: bridge.setBlendParams(0, 1, modeCombo.currentIndex, Math.round(alphaSlider.value))
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
