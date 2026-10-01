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
                text: "AUTOMATED TARGET DETECTION & MTI"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Modules 0x0A, 0x48"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "Detected Blobs"; value: "4"; accentColor: SightlineTheme.info; iconText: "🎯" }
            MetricCard { title: "Detection Frame Rate"; value: "30"; unit: "Hz"; accentColor: SightlineTheme.success; iconText: "⚡" }
            MetricCard { title: "Clutter Rejection"; value: "96"; unit: "%"; accentColor: SightlineTheme.primary; iconText: "🛡️" }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: detectCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: detectCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "DETECTION SENSITIVITY & MTI FILTERING"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: detectCam
                        model: ["Camera 0 (EO Visible)", "Camera 1 (IR Thermal)", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Detection Algorithm:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: detectMode
                        model: ["Motion Detection (MTI)", "Blob / Hotspot", "Maritime Horizon", "Drone / Small Target"]
                        Layout.preferredWidth: 260
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Sensitivity Threshold:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider {
                        id: threshSlider
                        from: 1
                        to: 100
                        value: 45
                        Layout.preferredWidth: 200
                    }
                    Text { text: Math.round(threshSlider.value) + "%"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Target Size Bounds (px):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: minSizeInput
                        text: "8"
                        Layout.preferredWidth: 70
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: minSizeInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                    }
                    Text { text: "to"; color: SightlineTheme.textMuted; font.pixelSize: 12; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: maxSizeInput
                        text: "250"
                        Layout.preferredWidth: 70
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: maxSizeInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Detection Settings"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            if (bridge) {
                                bridge.setDetectionParams(
                                    detectCam.currentIndex,
                                    detectMode.currentIndex,
                                    Math.round(threshSlider.value),
                                    parseInt(minSizeInput.text),
                                    parseInt(maxSizeInput.text)
                                );
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
