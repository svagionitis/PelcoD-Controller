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
                text: "🔍 AUTOMATED DETECTION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x0A / 0x48"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Detected Candidates"
                value: "4"
                accentColor: SightlineTheme.info
                iconText: "🎯"
            }
            MetricCard {
                title: "Detection Rate"
                value: "30"
                unit: "Hz"
                accentColor: SightlineTheme.success
                iconText: "⚡"
            }
            MetricCard {
                title: "False Alarm Rejection"
                value: "96"
                unit: "%"
                accentColor: SightlineTheme.primary
                iconText: "🛡️"
            }
            Item { Layout.fillWidth: true }
        }

        // Controls Card
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
                    text: "DETECTION SENSITIVITY & FILTERS"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: detectCam
                        model: ["Camera 0", "Camera 1", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 200
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Detection Mode:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: detectMode
                        model: ["Motion Detection (MTI)", "Blob / Hotspot", "Maritime Horizon", "Drone / Small Target"]
                        Layout.preferredWidth: 240
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Threshold Sensitivity:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Slider {
                        id: threshSlider
                        from: 1
                        to: 100
                        value: 45
                        Layout.preferredWidth: 200
                    }
                    Text { text: Math.round(threshSlider.value) + " %"; color: SightlineTheme.textPrimary; font.bold: true }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Min / Max Size (px):"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    TextField {
                        id: minSizeInput
                        text: "8"
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 }
                    }
                    Text { text: "to"; color: SightlineTheme.textMuted }
                    TextField {
                        id: maxSizeInput
                        text: "250"
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 }
                    }
                }

                Button {
                    text: "Apply Detection Settings"
                    Layout.preferredWidth: 200
                    Layout.topMargin: 8
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                    onClicked: {
                        bridge.setDetectionParams(
                            detectCam.currentIndex,
                            detectMode.currentIndex,
                            Math.round(threshSlider.value),
                            parseInt(minSizeInput.text),
                            parseInt(maxSizeInput.text)
                        );
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
