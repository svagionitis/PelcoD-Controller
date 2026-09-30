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
                text: "🧠 AI CLASSIFICATION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x76 / Custom Deep Learning"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Active Model"
                value: "YOLO-v8n"
                accentColor: SightlineTheme.info
                iconText: "🧬"
            }
            MetricCard {
                title: "Inference Latency"
                value: "14.2"
                unit: "ms"
                accentColor: SightlineTheme.success
                iconText: "⚡"
            }
            MetricCard {
                title: "Classified Targets"
                value: "3"
                accentColor: SightlineTheme.primary
                iconText: "🏷️"
            }
            Item { Layout.fillWidth: true }
        }

        // Configuration Card
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
                    text: "NEURAL NETWORK INFERENCE PARAMETERS"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: aiCam
                        model: ["Camera 0", "Camera 1", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 200
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Deployed Model:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: modelCombo
                        model: ["0: Edge-YOLO-General", "1: Maritime Vessels", "2: Aerial / Drone Detector", "3: Thermal Pedestrian / Vehicle"]
                        Layout.preferredWidth: 280
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Confidence Threshold:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Slider {
                        id: confSlider
                        from: 10
                        to: 95
                        value: 50
                        Layout.preferredWidth: 200
                    }
                    Text { text: Math.round(confSlider.value) + " %"; color: SightlineTheme.textPrimary; font.bold: true }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "NMS IOU Threshold:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Slider {
                        id: nmsSlider
                        from: 10
                        to: 90
                        value: 45
                        Layout.preferredWidth: 200
                    }
                    Text { text: Math.round(nmsSlider.value) + " %"; color: SightlineTheme.textPrimary; font.bold: true }
                }

                Button {
                    text: "Deploy AI Classifier"
                    Layout.preferredWidth: 200
                    Layout.topMargin: 8
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                    onClicked: {
                        bridge.customAIDetect(
                            aiCam.currentIndex,
                            modelCombo.currentIndex,
                            Math.round(confSlider.value),
                            Math.round(nmsSlider.value)
                        );
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
