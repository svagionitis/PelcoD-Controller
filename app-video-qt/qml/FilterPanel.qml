import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import VideoApp 1.0

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    required property VideoPlayerController controller

    ColumnLayout {
        width: parent.width - 20
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 16

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                width: 4
                height: 18
                color: "#00e5ff"
                radius: 2
            }
            Text {
                text: "REAL-TIME DSP FILTERS"
                color: "#f0f4fc"
                font.pixelSize: 13
                font.bold: true
                font.letterSpacing: 1.2
            }
        }

        // Section 1: Enhancement
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: enhanceCol.implicitHeight + 20
            color: "#181b24"
            radius: 6
            border.color: "#2a2f40"

            ColumnLayout {
                id: enhanceCol
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Text {
                    text: "IMAGE ENHANCEMENT"
                    color: "#00e5ff"
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.dehazeEnabled
                        onToggled: controller.dehazeEnabled = checked
                    }
                    Text { text: "Dehaze (Dark Channel Prior)"; color: "#f0f4fc"; font.pixelSize: 12 }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.claheEnabled
                        onToggled: controller.claheEnabled = checked
                    }
                    Text { text: "CLAHE (Adaptive Histogram)"; color: "#f0f4fc"; font.pixelSize: 12 }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.stabilizationEnabled
                        onToggled: controller.stabilizationEnabled = checked
                    }
                    Text { text: "EIS (Electronic Stabilization)"; color: "#f0f4fc"; font.pixelSize: 12 }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.denoiseEnabled
                        onToggled: controller.denoiseEnabled = checked
                    }
                    Text { text: "Bilateral Noise Reduction"; color: "#f0f4fc"; font.pixelSize: 12 }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.sharpenEnabled
                        onToggled: controller.sharpenEnabled = checked
                    }
                    Text { text: "Unsharp Mask / Sharpen"; color: "#f0f4fc"; font.pixelSize: 12 }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.edgeDetectEnabled
                        onToggled: controller.edgeDetectEnabled = checked
                    }
                    Text { text: "Edge Detection (Sobel)"; color: "#f0f4fc"; font.pixelSize: 12 }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "White Balance Algorithm"; color: "#8894ab"; font.pixelSize: 10 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Disabled", "Gray World Correction", "White Patch Retinex"]
                        currentIndex: controller.whiteBalanceMode
                        onActivated: function(index) {
                            controller.whiteBalanceMode = index;
                        }
                    }
                }
            }
        }

        // Section 2: Thermal & False Color
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: thermalCol.implicitHeight + 20
            color: "#181b24"
            radius: 6
            border.color: "#2a2f40"

            ColumnLayout {
                id: thermalCol
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Text {
                    text: "THERMAL & FALSE COLOR"
                    color: "#ff9100"
                    font.pixelSize: 11
                    font.bold: true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "False Color Palette"; color: "#8894ab"; font.pixelSize: 10 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["None (RGB)", "White Hot", "Black Hot", "Iron256 (FLIR)", "Jet", "Turbo", "Rainbow", "Ocean", "Inferno"]
                        currentIndex: controller.falseColorPalette
                        onActivated: function(index) {
                            controller.falseColorPalette = index;
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "Isotherm Temperature Highlight"; color: "#8894ab"; font.pixelSize: 10 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Off", "Human Body Band (36-38°C)", "High Heat Alert (>60°C)", "Dynamic Midtones"]
                        currentIndex: controller.isothermPreset
                        onActivated: function(index) {
                            controller.isothermPreset = index;
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.hotspotTrackerEnabled
                        onToggled: controller.hotspotTrackerEnabled = checked
                    }
                    Text { text: "Auto Hotspot / Max Heat Tracker"; color: "#f0f4fc"; font.pixelSize: 12 }
                }
            }
        }

        // Section 3: Motion & Computer Vision Analytics
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: motionCol.implicitHeight + 20
            color: "#181b24"
            radius: 6
            border.color: "#2a2f40"

            ColumnLayout {
                id: motionCol
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Text {
                    text: "MOTION & CV ANALYTICS"
                    color: "#00e676"
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.mtiMotionEnabled
                        onToggled: controller.mtiMotionEnabled = checked
                    }
                    Text { text: "Moving Target Indication (MTI)"; color: "#f0f4fc"; font.pixelSize: 12 }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "Optical Flow Tracking"; color: "#8894ab"; font.pixelSize: 10 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Off", "Motion Vector Grid", "Directional Color Flow (Farneback)"]
                        currentIndex: controller.opticalFlowMode
                        onActivated: function(index) {
                            controller.opticalFlowMode = index;
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.heatmapEnabled
                        onToggled: controller.heatmapEnabled = checked
                    }
                    Text { text: "Motion Density Heatmap"; color: "#f0f4fc"; font.pixelSize: 12 }
                }
            }
        }

        // Section 4: Tactical Overlays
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: overlayCol.implicitHeight + 20
            color: "#181b24"
            radius: 6
            border.color: "#2a2f40"

            ColumnLayout {
                id: overlayCol
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Text {
                    text: "TACTICAL HUD & OVERLAYS"
                    color: "#00e5ff"
                    font.pixelSize: 11
                    font.bold: true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "Reticle / Crosshair HUD Style"; color: "#8894ab"; font.pixelSize: 10 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["None", "Precision Crosshair", "Mil-Dot Tactical", "Stadiametric Rangefinder", "Corner Brackets"]
                        currentIndex: controller.reticleStyle
                        onActivated: function(index) {
                            controller.reticleStyle = index;
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.tripwireEnabled
                        onToggled: controller.tripwireEnabled = checked
                    }
                    Text { text: "Perimeter Tripwire Line"; color: "#f0f4fc"; font.pixelSize: 12 }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "Privacy Masking Area"; color: "#8894ab"; font.pixelSize: 10 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Disabled", "Blackout Solid Mask", "Gaussian Blur", "Pixelated Mosaic"]
                        currentIndex: controller.privacyMaskMode
                        onActivated: function(index) {
                            controller.privacyMaskMode = index;
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.watermarkEnabled
                        onToggled: controller.watermarkEnabled = checked
                    }
                    Text { text: "Burn-in Timestamp & Watermark"; color: "#f0f4fc"; font.pixelSize: 12 }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.telemetryOsdEnabled
                        onToggled: controller.telemetryOsdEnabled = checked
                    }
                    Text { text: "Tactical Telemetry OSD Header"; color: "#f0f4fc"; font.pixelSize: 12 }
                }
            }
        }
    }
}
