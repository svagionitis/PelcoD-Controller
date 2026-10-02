import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

ScrollView {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true
    contentWidth: Math.max(availableWidth, minContentWidth + 32)
    contentHeight: mainCol.implicitHeight + 32
    clip: true
    ScrollBar.vertical.policy: ScrollBar.AsNeeded
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    readonly property int minContentWidth: 640

    Connections {
        target: bridge
        function onDetectionParamsReceived(cam, detIdx, mode, sensMode, threshold, minSize, maxSize) {
            if (cam === detectCam.currentIndex && detIdx === detInstanceCombo.currentIndex) {
                detectMode.currentIndex = mode;
                sensModeCombo.currentIndex = sensMode;
                threshSlider.value = threshold;
                minSizeInput.text = minSize.toString();
                maxSizeInput.text = maxSize.toString();
            }
        }
        function onAdvDetectionReceived(cam, updateRate, surroundSize, blobDir, use8Bit, gasOriginal, gasColor, iouThresh, enableMtd, downsample) {
            if (cam === detectCam.currentIndex) {
                updateRateSlider.value = updateRate;
                surroundSlider.value = surroundSize;
                blobDirCombo.currentIndex = blobDir;
                use8BitCheck.checked = use8Bit;
                gasOriginalSlider.value = gasOriginal;
                gasColorCombo.currentIndex = gasColor;
                iouSlider.value = iouThresh;
                enableMtdCheck.checked = enableMtd;
                downsampleCombo.currentIndex = downsample;
            }
        }
        function onDetectionRoiReceived(cam, detIdx, roiIdx, geomMode, x1, y1, x2, y2, lineSide) {
            if (cam === detectCam.currentIndex && detIdx === detInstanceCombo.currentIndex && roiIdx === roiIndexCombo.currentIndex) {
                roiGeomCombo.currentIndex = geomMode;
                lineX1Input.text = x1.toString();
                lineY1Input.text = y1.toString();
                lineX2Input.text = x2.toString();
                lineY2Input.text = y2.toString();
                lineSideCombo.currentIndex = lineSide;
            }
        }
    }

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        // Section Title Header
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
                text: "// Modules 0x0A, 0x48, 0x76, 0x7C, 0xAB (EAN-Detection-Modes)"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Live Telemetry Cards
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard {
                title: "Active Algorithm"
                value: detectMode.currentText.split(" ")[0]
                accentColor: SightlineTheme.info
                iconText: "🎯"
            }
            MetricCard {
                title: "Detection Instance"
                value: detInstanceCombo.currentIndex === 0 ? "Primary (0)" : "Secondary (1)"
                accentColor: SightlineTheme.primary
                iconText: "🔢"
            }
            MetricCard {
                title: "Sensitivity Mode"
                value: sensModeCombo.currentIndex === 0 ? "Auto Adaptation" : "Manual Floor"
                accentColor: SightlineTheme.success
                iconText: "⚙️"
            }
            MetricCard {
                title: "Geometry Bounds"
                value: roiGeomCombo.currentText
                accentColor: SightlineTheme.accent
                iconText: "📐"
            }
        }

        // 1. Detection Mode & Core Parameters
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: coreDetectCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: coreDetectCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "CORE DETECTION MODE & SENSITIVITY CONFIGURATION"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: detectCam
                        model: ["Camera 0 (EO Visible)", "Camera 1 (IR Thermal)", "Camera 2 (Secondary EO)", "Camera 3 (Secondary IR)"]
                        Layout.preferredWidth: 260
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Detection Instance:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: detInstanceCombo
                        model: ["0: Primary Detection Instance", "1: Secondary Detection Instance (Dual-Pipeline)"]
                        Layout.preferredWidth: 320
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Algorithmic Mode:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: detectMode
                        model: [
                            "0: Off (Target Detection Disabled)",
                            "1: Vehicle MTI (10-100px Road & Ground Movement)",
                            "2: Drone MTI (Multi-Rotor & Fast Airborne Targets)",
                            "3: Staring MTI (Stationary Ground / Low Drift Camera)",
                            "4: Aerial MTI (Perspective Airborne Platform Flight)",
                            "5: Anomaly (Color & Intensity Signature Deviation)",
                            "6: Radiometric (Temperature & Gray Value Threshold)",
                            "7: Maritime (Horizon & Surface Marine Vessels)",
                            "8: Blob (Hotspot & Thermal Signatures)",
                            "9: Optical Gas Imaging (Absorption Band Visual)",
                            "10: Person MTI (Slow Irregular Human Movement)",
                            "11: AI Detection (Deep Learning Full-Frame Neural)"
                        ]
                        Layout.preferredWidth: 420
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Sensitivity Strategy:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: sensModeCombo
                        model: ["Auto (Adaptive Internal Noise Floor)", "Manual (Explicit Threshold & Watch Frames)"]
                        Layout.preferredWidth: 320
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Sensitivity Threshold:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    Slider {
                        id: threshSlider
                        from: 1
                        to: 100
                        value: 45
                        Layout.preferredWidth: 220
                    }
                    Text { text: Math.round(threshSlider.value) + "%"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Target Size Bounds (px):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
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

                // Manual sensitivity extended inputs
                RowLayout {
                    visible: sensModeCombo.currentIndex === 1
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Manual Tuning Floors:"; color: SightlineTheme.warning; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    Text { text: "Bkgd:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    TextField {
                        id: bkgdThreshInput
                        text: "15"
                        Layout.preferredWidth: 50
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Text { text: "Watch Frames:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    TextField {
                        id: watchFramesInput
                        text: "5"
                        Layout.preferredWidth: 50
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Text { text: "Suspicious Score:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    TextField {
                        id: suspScoreInput
                        text: "30"
                        Layout.preferredWidth: 50
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Detection Settings"
                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            if (bridge) {
                                bridge.setDetectionExtended(
                                    detectCam.currentIndex,
                                    detInstanceCombo.currentIndex,
                                    detectMode.currentIndex,
                                    sensModeCombo.currentIndex,
                                    Math.round(threshSlider.value),
                                    parseInt(minSizeInput.text),
                                    parseInt(maxSizeInput.text),
                                    parseInt(bkgdThreshInput.text),
                                    parseInt(watchFramesInput.text),
                                    parseInt(suspScoreInput.text)
                                );
                            }
                        }
                    }

                    Button {
                        text: "Query State"
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                        onClicked: {
                            if (bridge) {
                                bridge.queryDetection(detectCam.currentIndex, detInstanceCombo.currentIndex);
                            }
                        }
                    }

                    Button {
                        text: "📸 Capture Snapshot (0xAB)"
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: SightlineTheme.accent; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.accent }
                        onClicked: {
                            if (bridge) {
                                bridge.triggerDetectionSnapshot(detectCam.currentIndex, detInstanceCombo.currentIndex);
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // 2. Advanced Algorithmic Parameter Tuning (Collapsible Card)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: advCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: advCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "ADVANCED ALGORITHMIC TUNING (EAN SEC 2)"
                        color: SightlineTheme.primary
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Switch {
                        id: advExpandSwitch
                        text: "Show Advanced Options"
                        checked: false
                    }
                }

                ColumnLayout {
                    visible: advExpandSwitch.checked
                    Layout.fillWidth: true
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Blob Contrast Direction:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                        ComboBox {
                            id: blobDirCombo
                            model: ["0: Bright Objects Only", "1: Dark Objects Only", "2: Both Bright & Dark"]
                            currentIndex: 2
                            Layout.preferredWidth: 240
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Resolution Downsample:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                        ComboBox {
                            id: downsampleCombo
                            model: ["0: Full Resolution (1x)", "1: Half Downsample (2x)", "2: Quarter Downsample (4x)"]
                            Layout.preferredWidth: 240
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Detection Update Rate:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                        Slider {
                            id: updateRateSlider
                            from: 1
                            to: 60
                            value: 30
                            Layout.preferredWidth: 220
                        }
                        Text { text: Math.round(updateRateSlider.value) + " Hz"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Surround Context Size:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                        Slider {
                            id: surroundSlider
                            from: 5
                            to: 50
                            value: 20
                            Layout.preferredWidth: 220
                        }
                        Text { text: Math.round(surroundSlider.value) + " px"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Gas Optical Blend:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                        Slider {
                            id: gasOriginalSlider
                            from: 0
                            to: 255
                            value: 128
                            Layout.preferredWidth: 160
                        }
                        Text { text: Math.round(gasOriginalSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                        Text { text: "Color Palette:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: gasColorCombo
                            model: ["0: Default Grayscale", "1: False Color Jet", "2: False Color Thermal"]
                            Layout.preferredWidth: 180
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        CheckBox {
                            id: use8BitCheck
                            text: "Force 8-Bit Grayscale Processing"
                            checked: false
                        }
                        CheckBox {
                            id: enableMtdCheck
                            text: "Enable MTD / AI Co-existence"
                            checked: true
                        }
                        Text { text: "AI IOU Threshold:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        Slider {
                            id: iouSlider
                            from: 10
                            to: 95
                            value: 50
                            Layout.preferredWidth: 120
                        }
                        Text { text: Math.round(iouSlider.value) + "%"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Layout.topMargin: 4

                        Button {
                            text: "Apply Advanced Tuning"
                            Layout.preferredWidth: 190
                            Layout.preferredHeight: 32
                            contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                            onClicked: {
                                if (bridge) {
                                    bridge.setDetectionAdvanced(
                                        detectCam.currentIndex,
                                        Math.round(updateRateSlider.value),
                                        Math.round(surroundSlider.value),
                                        blobDirCombo.currentIndex,
                                        use8BitCheck.checked,
                                        Math.round(gasOriginalSlider.value),
                                        gasColorCombo.currentIndex,
                                        Math.round(iouSlider.value),
                                        enableMtdCheck.checked,
                                        downsampleCombo.currentIndex
                                    );
                                }
                            }
                        }

                        Button {
                            text: "Query Advanced Settings"
                            Layout.preferredWidth: 180
                            Layout.preferredHeight: 32
                            contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            onClicked: {
                                if (bridge) {
                                    bridge.queryAdvDetection(detectCam.currentIndex);
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        // 3. Region of Interest (ROI) & Geometric Line Filter
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: roiCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: roiCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "GEOMETRIC REGION OF INTEREST (ROI) & TRIPWIRE LINE (EAN SEC 3)"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "ROI Slot Index:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: roiIndexCombo
                        model: ["ROI Slot 0 (Default)", "ROI Slot 1", "ROI Slot 2", "ROI Slot 3"]
                        Layout.preferredWidth: 200
                    }
                    Text { text: "Geometry Type:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: roiGeomCombo
                        model: ["0: Full Frame / Box ROI", "1: Directional Detection Line", "2: 16x16 Masked Grid"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                // Line ROI Controls
                RowLayout {
                    visible: roiGeomCombo.currentIndex === 1
                    Layout.fillWidth: true
                    spacing: 10
                    Text { text: "Tripwire Line (X1,Y1) -> (X2,Y2):"; color: SightlineTheme.accent; font.pixelSize: 12; Layout.preferredWidth: 180; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: lineX1Input
                        text: "50"
                        placeholderText: "X1"
                        Layout.preferredWidth: 50
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    TextField {
                        id: lineY1Input
                        text: "200"
                        placeholderText: "Y1"
                        Layout.preferredWidth: 50
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Text { text: "to"; color: SightlineTheme.textMuted; font.pixelSize: 12; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: lineX2Input
                        text: "590"
                        placeholderText: "X2"
                        Layout.preferredWidth: 50
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    TextField {
                        id: lineY2Input
                        text: "200"
                        placeholderText: "Y2"
                        Layout.preferredWidth: 50
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Text { text: "Side:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: lineSideCombo
                        model: ["0: Both", "1: Above Line", "2: Below Line", "3: Left of Line", "4: Right of Line"]
                        Layout.preferredWidth: 140
                    }
                    Item { Layout.fillWidth: true }
                }

                // Grid ROI Controls
                RowLayout {
                    visible: roiGeomCombo.currentIndex === 2
                    Layout.fillWidth: true
                    spacing: 10
                    Text { text: "Grid Bitmasks (Hex):"; color: SightlineTheme.accent; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: gridMask0
                        text: "0000000000000000"
                        Layout.preferredWidth: 120
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    TextField {
                        id: gridMask1
                        text: "0000000000000000"
                        Layout.preferredWidth: 120
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    TextField {
                        id: gridMask2
                        text: "0000000000000000"
                        Layout.preferredWidth: 120
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    TextField {
                        id: gridMask3
                        text: "0000000000000000"
                        Layout.preferredWidth: 120
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    CheckBox {
                        id: showGridRegionsCheck
                        text: "Show Grid Overlay"
                        checked: true
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Region Geometry"
                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            if (!bridge) return;
                            if (roiGeomCombo.currentIndex === 1) {
                                bridge.setDetectionRoiLine(
                                    detectCam.currentIndex,
                                    detInstanceCombo.currentIndex,
                                    roiIndexCombo.currentIndex,
                                    parseInt(lineX1Input.text),
                                    parseInt(lineY1Input.text),
                                    parseInt(lineX2Input.text),
                                    parseInt(lineY2Input.text),
                                    lineSideCombo.currentIndex
                                );
                            } else if (roiGeomCombo.currentIndex === 2) {
                                bridge.setDetectionRoiGrid(
                                    detectCam.currentIndex,
                                    detInstanceCombo.currentIndex,
                                    roiIndexCombo.currentIndex,
                                    16,
                                    16,
                                    gridMask0.text,
                                    gridMask1.text,
                                    gridMask2.text,
                                    gridMask3.text,
                                    showGridRegionsCheck.checked
                                );
                            }
                        }
                    }

                    Button {
                        text: "Query ROI"
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                        onClicked: {
                            if (bridge) {
                                bridge.queryDetectionROI(detectCam.currentIndex, roiIndexCombo.currentIndex);
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
