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
                    Text { text: "Bilateral / Temporal Denoise"; color: "#f0f4fc"; font.pixelSize: 12 }
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

        // Section 2: Color & Tonal Grading
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: colorCol.implicitHeight + 20
            color: "#181b24"
            radius: 6
            border.color: "#2a2f40"

            ColumnLayout {
                id: colorCol
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Text {
                    text: "COLOR & TONAL GRADING"
                    color: "#b388ff"
                    font.pixelSize: 11
                    font.bold: true
                }

                // Brightness
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Brightness (" + (controller.brightness > 0 ? "+" : "") + controller.brightness + ")"
                            color: "#f0f4fc"
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "Reset"
                            font.pixelSize: 9
                            flat: true
                            onClicked: controller.brightness = 0
                        }
                    }
                    Slider {
                        Layout.fillWidth: true
                        from: -100
                        to: 100
                        stepSize: 1
                        value: controller.brightness
                        onMoved: controller.brightness = Math.round(value)
                    }
                }

                // Contrast
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Contrast (" + controller.contrast.toFixed(2) + "x)"
                            color: "#f0f4fc"
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "Reset"
                            font.pixelSize: 9
                            flat: true
                            onClicked: controller.contrast = 1.0
                        }
                    }
                    Slider {
                        Layout.fillWidth: true
                        from: 0.2
                        to: 3.0
                        stepSize: 0.05
                        value: controller.contrast
                        onMoved: controller.contrast = Math.round(value * 20) / 20
                    }
                }

                // Gamma
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Gamma (" + controller.gamma.toFixed(2) + ")"
                            color: "#f0f4fc"
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "Reset"
                            font.pixelSize: 9
                            flat: true
                            onClicked: controller.gamma = 1.0
                        }
                    }
                    Slider {
                        Layout.fillWidth: true
                        from: 0.2
                        to: 3.0
                        stepSize: 0.05
                        value: controller.gamma
                        onMoved: controller.gamma = Math.round(value * 20) / 20
                    }
                }

                // Saturation / Color Enhance
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Saturation Boost (" + controller.colorEnhanceFactor.toFixed(2) + "x)"
                            color: "#f0f4fc"
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "Reset"
                            font.pixelSize: 9
                            flat: true
                            onClicked: controller.colorEnhanceFactor = 1.0
                        }
                    }
                    Slider {
                        Layout.fillWidth: true
                        from: 0.0
                        to: 3.0
                        stepSize: 0.05
                        value: controller.colorEnhanceFactor
                        onMoved: controller.colorEnhanceFactor = Math.round(value * 20) / 20
                    }
                }

                // Color Tone Mode
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "Color Tone Filter"; color: "#8894ab"; font.pixelSize: 10 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Normal (Full Color)", "Grayscale Monochrome", "Inverted Negative", "Vintage Sepia"]
                        currentIndex: controller.colorToneMode
                        onActivated: function(index) {
                            controller.colorToneMode = index;
                        }
                    }
                }

                // Color Tint Preset
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "Color Tint / Phosphor Cast"; color: "#8894ab"; font.pixelSize: 10 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Disabled", "Night-Vision Tactical Green", "Deep Marine Blue", "Warm Amber Sunset"]
                        currentIndex: controller.colorTintPreset
                        onActivated: function(index) {
                            controller.colorTintPreset = index;
                        }
                    }
                }

                // Histogram Equalization
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "Adaptive Histogram Equalization"; color: "#8894ab"; font.pixelSize: 10 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Disabled", "Standard Global Equalization", "Square-Root Anti-Saturation", "Sobel Feature-Based"]
                        currentIndex: controller.histogramEqMode
                        onActivated: function(index) {
                            controller.histogramEqMode = index;
                        }
                    }
                }

                // Vignette
                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: controller.vignetteEnabled
                        onToggled: controller.vignetteEnabled = checked
                    }
                    Text { text: "Perimeter Vignette Shade"; color: "#f0f4fc"; font.pixelSize: 12 }
                }

                // Binary Threshold
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        Switch {
                            checked: controller.thresholdEnabled
                            onToggled: controller.thresholdEnabled = checked
                        }
                        Text { text: "Binary Threshold Mask"; color: "#f0f4fc"; font.pixelSize: 12 }
                    }
                    Slider {
                        Layout.fillWidth: true
                        visible: controller.thresholdEnabled
                        from: 0
                        to: 255
                        stepSize: 1
                        value: controller.thresholdValue
                        onMoved: controller.thresholdValue = Math.round(value)
                    }
                }
            }
        }

        // Section 3: Thermal & False Color
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

        // Section 4: Motion & Computer Vision Analytics
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

        // Section 5: Tactical Overlays & OSD
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

                // Stream Health Watchdog OSD
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        Switch {
                            checked: controller.streamHealthOsdEnabled
                            onToggled: controller.streamHealthOsdEnabled = checked
                        }
                        Text { text: "Live Stream Health Watchdog (● LIVE)"; color: "#f0f4fc"; font.pixelSize: 12 }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: controller.streamHealthOsdEnabled
                        spacing: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: "Badge Style"; color: "#8894ab"; font.pixelSize: 10 }
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["Tactical Pill", "Minimal Beacon", "Full Telemetry"]
                                currentIndex: controller.streamHealthOsdStyle
                                onActivated: function(index) {
                                    controller.streamHealthOsdStyle = index;
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: "Corner Position"; color: "#8894ab"; font.pixelSize: 10 }
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["Top Left", "Top Right", "Bottom Left", "Bottom Right"]
                                currentIndex: controller.streamHealthOsdPosition
                                onActivated: function(index) {
                                    controller.streamHealthOsdPosition = index;
                                }
                            }
                        }
                    }
                }

                // Custom Text Banner
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        Switch {
                            checked: controller.textOverlayEnabled
                            onToggled: controller.textOverlayEnabled = checked
                        }
                        Text { text: "Custom Operator Banner"; color: "#f0f4fc"; font.pixelSize: 12 }
                    }
                    TextField {
                        Layout.fillWidth: true
                        visible: controller.textOverlayEnabled
                        placeholderText: "Enter banner text..."
                        text: controller.textOverlayString
                        color: "#f0f4fc"
                        background: Rectangle {
                            color: "#12151e"
                            radius: 4
                            border.color: "#2a2f40"
                        }
                        onEditingFinished: controller.textOverlayString = text
                    }
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

                // Tactical Mini-Map Inset (OpenCV Rasterizer)
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Switch {
                            checked: controller.mapRasterizerEnabled
                            onToggled: controller.mapRasterizerEnabled = checked
                        }
                        Text { text: "Tactical Mini-Map / Radar Inset"; color: "#00e5ff"; font.pixelSize: 12; font.bold: true }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        visible: controller.mapRasterizerEnabled
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: "Corner Placement"; color: "#8894ab"; font.pixelSize: 10 }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: ["Bottom Right", "Bottom Left", "Top Right", "Top Left"]
                                    currentIndex: controller.mapRasterizerCorner
                                    onActivated: function(index) {
                                        controller.mapRasterizerCorner = index;
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Inset Opacity"; color: "#8894ab"; font.pixelSize: 10 }
                                Item { Layout.fillWidth: true }
                                Text { text: Math.round(controller.mapRasterizerOpacity * 100) + "%"; color: "#00e5ff"; font.pixelSize: 10; font.family: "Monospace" }
                            }
                            Slider {
                                Layout.fillWidth: true
                                from: 0.1
                                to: 1.0
                                stepSize: 0.05
                                value: controller.mapRasterizerOpacity
                                onMoved: controller.mapRasterizerOpacity = value
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Map Zoom Level"; color: "#8894ab"; font.pixelSize: 10 }
                                Item { Layout.fillWidth: true }
                                Text { text: "Z" + controller.mapRasterizerZoom; color: "#00e5ff"; font.pixelSize: 10; font.family: "Monospace" }
                            }
                            Slider {
                                Layout.fillWidth: true
                                from: 1
                                to: 18
                                stepSize: 1
                                value: controller.mapRasterizerZoom
                                onMoved: controller.mapRasterizerZoom = Math.round(value)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            CheckBox {
                                checked: controller.mapRasterizerShowFrustum
                                onToggled: controller.mapRasterizerShowFrustum = checked
                                contentItem: Text { text: "Show Optical Frustum Footprint"; color: "#f0f4fc"; font.pixelSize: 11; leftPadding: 24 }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            CheckBox {
                                checked: controller.mapRasterizerShowHeading
                                onToggled: controller.mapRasterizerShowHeading = checked
                                contentItem: Text { text: "Show Platform Heading Vector"; color: "#f0f4fc"; font.pixelSize: 11; leftPadding: 24 }
                            }
                        }
                    }
                }
            }
        }
    }
}
