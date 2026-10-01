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
                text: "IMAGE ENHANCEMENT & 3D NOISE REDUCTION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Modules 0x65, 0x4B CLAHE & Denoise"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "CLAHE Processing"; value: "ACTIVE"; accentColor: SightlineTheme.success; iconText: "⚡" }
            MetricCard { title: "Denoise Filter"; value: "3D Temporal"; accentColor: SightlineTheme.info; iconText: "🌊" }
            MetricCard { title: "Tonal Range"; value: "14-bit Linear"; accentColor: SightlineTheme.primary; iconText: "📊" }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: enhCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: enhCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "CONTRAST, BRIGHTNESS & CLAHE EQUALIZATION"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: enhCam
                        model: ["Camera 0 (EO Visible)", "Camera 1 (IR Thermal)", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Contrast Adjustment:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider { id: contrastSlider; from: -50; to: 50; value: 0; Layout.preferredWidth: 200 }
                    Text { text: Math.round(contrastSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Brightness Adjustment:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider { id: brightSlider; from: -50; to: 50; value: 0; Layout.preferredWidth: 200 }
                    Text { text: Math.round(brightSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Sharpening Level:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider { id: sharpSlider; from: 0; to: 100; value: 25; Layout.preferredWidth: 200 }
                    Text { text: Math.round(sharpSlider.value) + "%"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "CLAHE Level:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider { id: claheSlider; from: 0; to: 100; value: 40; Layout.preferredWidth: 200 }
                    Text { text: Math.round(claheSlider.value) + "%"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: SightlineTheme.cardBorder }

                Text {
                    text: "3D DIGITAL NOISE REDUCTION"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Enable 3D Denoise:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Switch { id: denoiseSwitch; checked: true }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Temporal Strength:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider { id: tempSlider; from: 0; to: 100; value: 50; Layout.preferredWidth: 200 }
                    Text { text: Math.round(tempSlider.value) + "%"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Spatial Strength:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider { id: spatSlider; from: 0; to: 100; value: 30; Layout.preferredWidth: 200 }
                    Text { text: Math.round(spatSlider.value) + "%"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Enhancement Settings"
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            bridge.setVideoEnhance(
                                enhCam.currentIndex,
                                Math.round(contrastSlider.value),
                                Math.round(brightSlider.value),
                                Math.round(sharpSlider.value),
                                Math.round(claheSlider.value)
                            );
                            bridge.setNoise3D(
                                enhCam.currentIndex,
                                denoiseSwitch.checked ? 1 : 0,
                                Math.round(tempSlider.value),
                                Math.round(spatSlider.value)
                            );
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
