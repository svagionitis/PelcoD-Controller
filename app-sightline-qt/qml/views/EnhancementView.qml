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
                text: "✨ VIDEO ENHANCEMENT & NOISE FILTER"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x65 / 0x4B / CLAHE & 3D Denoise"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "CLAHE State"
                value: "ACTIVE"
                accentColor: SightlineTheme.success
                iconText: "⚡"
            }
            MetricCard {
                title: "Denoise Filter"
                value: "3D Temporal"
                accentColor: SightlineTheme.info
                iconText: "🌊"
            }
            MetricCard {
                title: "Dynamic Range"
                value: "14-bit Linear"
                accentColor: SightlineTheme.primary
                iconText: "📊"
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

            ScrollView {
                anchors.fill: parent
                anchors.margins: 16
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 14

                    Text {
                        text: "IMAGE ADJUSTMENTS & CLAHE"
                        color: SightlineTheme.textSecondary
                        font.pixelSize: SightlineTheme.fontSizeSmall
                        font.bold: true
                    }

                    RowLayout {
                        spacing: 16
                        Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                        ComboBox {
                            id: enhCam
                            model: ["Camera 0", "Camera 1", "Camera 2", "Camera 3"]
                            Layout.preferredWidth: 200
                        }
                    }

                    RowLayout {
                        spacing: 16
                        Text { text: "Contrast:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                        Slider { id: contrastSlider; from: -50; to: 50; value: 0; Layout.preferredWidth: 220 }
                        Text { text: Math.round(contrastSlider.value); color: SightlineTheme.textPrimary; font.bold: true }
                    }

                    RowLayout {
                        spacing: 16
                        Text { text: "Brightness:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                        Slider { id: brightSlider; from: -50; to: 50; value: 0; Layout.preferredWidth: 220 }
                        Text { text: Math.round(brightSlider.value); color: SightlineTheme.textPrimary; font.bold: true }
                    }

                    RowLayout {
                        spacing: 16
                        Text { text: "Sharpening Level:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                        Slider { id: sharpSlider; from: 0; to: 100; value: 25; Layout.preferredWidth: 220 }
                        Text { text: Math.round(sharpSlider.value) + " %"; color: SightlineTheme.textPrimary; font.bold: true }
                    }

                    RowLayout {
                        spacing: 16
                        Text { text: "CLAHE Level:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                        Slider { id: claheSlider; from: 0; to: 100; value: 40; Layout.preferredWidth: 220 }
                        Text { text: Math.round(claheSlider.value) + " %"; color: SightlineTheme.textPrimary; font.bold: true }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: SightlineTheme.cardBorder }

                    Text {
                        text: "3D DIGITAL NOISE REDUCTION"
                        color: SightlineTheme.textSecondary
                        font.pixelSize: SightlineTheme.fontSizeSmall
                        font.bold: true
                    }

                    RowLayout {
                        spacing: 16
                        Text { text: "Enable 3D Denoise:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                        Switch { id: denoiseSwitch; checked: true }
                    }

                    RowLayout {
                        spacing: 16
                        Text { text: "Temporal Strength:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                        Slider { id: tempSlider; from: 0; to: 100; value: 50; Layout.preferredWidth: 220 }
                        Text { text: Math.round(tempSlider.value) + " %"; color: SightlineTheme.textPrimary; font.bold: true }
                    }

                    RowLayout {
                        spacing: 16
                        Text { text: "Spatial Strength:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                        Slider { id: spatSlider; from: 0; to: 100; value: 30; Layout.preferredWidth: 220 }
                        Text { text: Math.round(spatSlider.value) + " %"; color: SightlineTheme.textPrimary; font.bold: true }
                    }

                    RowLayout {
                        spacing: 12
                        Layout.topMargin: 10

                        Button {
                            text: "Apply Image Enhancement"
                            Layout.preferredWidth: 220
                            contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
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
                    }
                }
            }
        }
    }
}
