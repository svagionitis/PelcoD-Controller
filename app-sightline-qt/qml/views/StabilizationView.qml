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

        // Title
        RowLayout {
            spacing: 12
            Text {
                text: "⚖️ VIDEO STABILIZATION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x05 / 0x22"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics Row
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Shift X Compensation"
                value: "1.4"
                unit: "px"
                accentColor: SightlineTheme.primary
                iconText: "↔️"
            }
            MetricCard {
                title: "Shift Y Compensation"
                value: "-0.8"
                unit: "px"
                accentColor: SightlineTheme.primary
                iconText: "↕️"
            }
            MetricCard {
                title: "Roll Correction"
                value: "0.2"
                unit: "deg"
                accentColor: SightlineTheme.info
                iconText: "🔄"
            }
            MetricCard {
                title: "Confidence"
                value: "98"
                unit: "%"
                accentColor: SightlineTheme.success
                iconText: "✅"
            }
            Item { Layout.fillWidth: true }
        }

        // Settings Card
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
                    text: "STABILIZATION CONFIGURATION"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: stabCam
                        model: ["Camera 0", "Camera 1", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 200
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Stabilization Mode:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: stabMode
                        model: ["Disabled (0)", "Full 2D Stabilization (1)", "High-pass Pan/Tilt/Roll (2)"]
                        Layout.preferredWidth: 260
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Automatic Bias:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Switch {
                        id: autoBiasSwitch
                        checked: true
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Max Shift Limit:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Slider {
                        id: maxShiftSlider
                        from: 16
                        to: 128
                        value: 64
                        stepSize: 8
                        Layout.preferredWidth: 200
                    }
                    Text { text: Math.round(maxShiftSlider.value) + " px"; color: SightlineTheme.textPrimary; font.bold: true }
                }

                RowLayout {
                    spacing: 12
                    Layout.topMargin: 12

                    Button {
                        text: "Apply Parameters"
                        Layout.preferredWidth: 160
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            bridge.setStabilization(
                                stabCam.currentIndex,
                                stabMode.currentIndex,
                                autoBiasSwitch.checked ? 1 : 0,
                                Math.round(maxShiftSlider.value)
                            );
                        }
                    }

                    Button {
                        text: "Reset Stabilization"
                        Layout.preferredWidth: 160
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.cardBorder }
                        onClicked: {
                            bridge.resetStabilization(stabCam.currentIndex);
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
