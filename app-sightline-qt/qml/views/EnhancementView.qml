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

    UserPaletteDialog {
        id: paletteDialog
    }

    Dialog {
        id: savePresetDialog
        title: "Save Enhancement Preset"
        modal: true
        anchors.centerIn: parent
        width: 320
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: SightlineTheme.surface
            border.color: SightlineTheme.cardBorder
            radius: SightlineTheme.radiusMedium
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            Text {
                text: "Enter Preset Name:"
                color: SightlineTheme.textPrimary
                font.pixelSize: 12
            }
            TextField {
                id: presetNameField
                Layout.fillWidth: true
                placeholderText: "e.g. NightVision_HighContrast"
                color: SightlineTheme.textPrimary
                background: Rectangle {
                    color: SightlineTheme.surfaceLight
                    border.color: SightlineTheme.inputBorder
                    radius: 4
                }
            }
        }

        onAccepted: {
            if (presetNameField.text.trim().length > 0 && bridge) {
                var map = {
                    "mode": modeCombo.currentIndex,
                    "sharpen": Math.round(sharpSlider.value),
                    "blend": Math.round(blendSlider.value),
                    "enhanceParam": Math.round(strengthSlider.value),
                    "denoise": Math.round(denoiseSlider.value),
                    "aerialMask": aerialMaskCheck.checked,
                    "staringMask": staringMaskCheck.checked,
                    "featureHist": featureHistCheck.checked,
                    "sqrtHist": sqrtHistCheck.checked,
                    "histAve": Math.round(histAveSlider.value),
                    "histMaxPct": Math.round(histMaxPctSlider.value),
                    "roiRow": roiRowSpin.value,
                    "roiCol": roiColSpin.value,
                    "roiHigh": roiHighSpin.value,
                    "roiWide": roiWideSpin.value,
                    "gaussian": Math.round(gaussSlider.value),
                    "lapMin": lapMinSpin.value,
                    "colorEnhance": Math.round(colorEnhSlider.value),
                    "brightness": Math.round(brightSlider.value),
                    "contrast": Math.round(contrastSlider.value),
                    "scintillation": scintCombo.currentIndex,
                    "radius": radiusCombo.currentIndex + 1,
                    "falseColor": falseColorCombo.currentIndex
                };
                bridge.saveEnhancePreset(presetNameField.text.trim(), map);
                refreshPresets();
            }
        }
    }

    function refreshPresets() {
        if (bridge) {
            var list = bridge.getEnhancePresets();
            presetCombo.model = ["-- Select Preset --"].concat(list);
        }
    }

    function applyAllSettings() {
        if (!bridge) return;

        var flags = 0;
        if (aerialMaskCheck.checked) flags |= (1 << 0);
        if (featureHistCheck.checked) flags |= (1 << 1);
        if (sqrtHistCheck.checked) flags |= (1 << 2);
        if (staringMaskCheck.checked) flags |= (1 << 4);

        bridge.setEnhanceFull(
            camCombo.currentIndex,
            modeCombo.currentIndex,
            Math.round(sharpSlider.value),
            Math.round(blendSlider.value),
            Math.round(strengthSlider.value),
            Math.round(denoiseSlider.value),
            flags,
            Math.round(histAveSlider.value),
            Math.round(histMaxPctSlider.value),
            roiRowSpin.value,
            roiColSpin.value,
            roiHighSpin.value,
            roiWideSpin.value,
            Math.round(gaussSlider.value),
            lapMinSpin.value,
            Math.round(colorEnhSlider.value),
            Math.round(brightSlider.value),
            Math.round(contrastSlider.value),
            scintCombo.currentIndex,
            radiusCombo.currentIndex + 1
        );

        var palIdx = falseColorCombo.currentIndex;
        if (palIdx === falseColorCombo.model.length - 1) {
            palIdx = 127; // User Palette
        }
        bridge.setFalseColor(camCombo.currentIndex, palIdx);

        bridge.setNoise3D(
            camCombo.currentIndex,
            denoise3dSwitch.checked ? 1 : 0,
            Math.round(temp3dSlider.value),
            Math.round(spat3dSlider.value)
        );
    }

    function loadPresetValues(name) {
        if (!bridge || !name || name === "-- Select Preset --") return;
        var p = bridge.loadEnhancePreset(name);
        if (!p || Object.keys(p).length === 0) return;

        if (p.mode !== undefined) modeCombo.currentIndex = p.mode;
        if (p.sharpen !== undefined) sharpSlider.value = p.sharpen;
        if (p.blend !== undefined) blendSlider.value = p.blend;
        if (p.enhanceParam !== undefined) strengthSlider.value = p.enhanceParam;
        if (p.denoise !== undefined) denoiseSlider.value = p.denoise;
        if (p.aerialMask !== undefined) aerialMaskCheck.checked = p.aerialMask;
        if (p.staringMask !== undefined) staringMaskCheck.checked = p.staringMask;
        if (p.featureHist !== undefined) featureHistCheck.checked = p.featureHist;
        if (p.sqrtHist !== undefined) sqrtHistCheck.checked = p.sqrtHist;
        if (p.histAve !== undefined) histAveSlider.value = p.histAve;
        if (p.histMaxPct !== undefined) histMaxPctSlider.value = p.histMaxPct;
        if (p.roiRow !== undefined) roiRowSpin.value = p.roiRow;
        if (p.roiCol !== undefined) roiColSpin.value = p.roiCol;
        if (p.roiHigh !== undefined) roiHighSpin.value = p.roiHigh;
        if (p.roiWide !== undefined) roiWideSpin.value = p.roiWide;
        if (p.gaussian !== undefined) gaussSlider.value = p.gaussian;
        if (p.lapMin !== undefined) lapMinSpin.value = p.lapMin;
        if (p.colorEnhance !== undefined) colorEnhSlider.value = p.colorEnhance;
        if (p.brightness !== undefined) brightSlider.value = p.brightness;
        if (p.contrast !== undefined) contrastSlider.value = p.contrast;
        if (p.scintillation !== undefined) scintCombo.currentIndex = p.scintillation;
        if (p.radius !== undefined) radiusCombo.currentIndex = Math.max(0, p.radius - 1);
        if (p.falseColor !== undefined) falseColorCombo.currentIndex = p.falseColor;
    }

    function resetDefaults() {
        modeCombo.currentIndex = 0;
        strengthSlider.value = 25;
        blendSlider.value = 255;
        sharpSlider.value = 0;
        radiusCombo.currentIndex = 0;
        colorEnhSlider.value = 0;
        scintCombo.currentIndex = 0;
        denoiseSlider.value = 0;
        aerialMaskCheck.checked = false;
        staringMaskCheck.checked = false;
        featureHistCheck.checked = false;
        sqrtHistCheck.checked = false;
        histAveSlider.value = 0;
        histMaxPctSlider.value = 0;
        brightSlider.value = 128;
        contrastSlider.value = 128;
        gaussSlider.value = 0;
        lapMinSpin.value = 2;
        roiRowSpin.value = 0;
        roiColSpin.value = 0;
        roiHighSpin.value = 0;
        roiWideSpin.value = 0;
        falseColorCombo.currentIndex = 0;
        denoise3dSwitch.checked = false;
        temp3dSlider.value = 50;
        spat3dSlider.value = 30;
    }

    Component.onCompleted: {
        refreshPresets();
    }

    ColumnLayout {
        width: parent.width - 32
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        // Header Title
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { width: 4; height: 18; color: SightlineTheme.primary; radius: 2 }
            Text {
                text: "VIDEO ENHANCEMENT & ADAPTIVE CONTRAST (PANEL+ PARITY)"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Messages 0x21, 0x16, 0x72 & 0xAF"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard {
                title: "Contrast Mode"
                value: modeCombo.currentText.split(" ")[0]
                accentColor: modeCombo.currentIndex > 0 ? SightlineTheme.success : SightlineTheme.textMuted
                iconText: "⚡"
            }
            MetricCard {
                title: "Sharpen & Blend"
                value: sharpSlider.value > 0 ? ("Lv " + Math.round(sharpSlider.value) + " / " + Math.round((blendSlider.value / 255.0) * 100) + "%") : "OFF"
                accentColor: sharpSlider.value > 0 ? SightlineTheme.info : SightlineTheme.textMuted
                iconText: "✨"
            }
            MetricCard {
                title: "Denoise Rate"
                value: denoiseSlider.value > 0 ? (Math.round(denoiseSlider.value) + " frames") : "OFF"
                accentColor: denoiseSlider.value > 0 ? SightlineTheme.primary : SightlineTheme.textMuted
                iconText: "🌊"
            }
            MetricCard {
                title: "False Color Palette"
                value: falseColorCombo.currentIndex > 0 ? falseColorCombo.currentText : "Grayscale"
                accentColor: falseColorCombo.currentIndex > 0 ? SightlineTheme.warning : SightlineTheme.textMuted
                iconText: "🎨"
            }
        }

        // Top Toolbar: Channel Selector, Presets & Action Buttons
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: toolRow.implicitHeight + 16
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            RowLayout {
                id: toolRow
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; font.bold: true }
                ComboBox {
                    id: camCombo
                    model: ["Camera 0 (EO Visible)", "Camera 1 (IR Thermal)", "Camera 2", "Camera 3"]
                    currentIndex: 0
                    Layout.preferredWidth: 190
                    Layout.preferredHeight: 28
                }

                Rectangle { width: 1; height: 20; color: SightlineTheme.cardBorder }

                Text { text: "Preset:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                ComboBox {
                    id: presetCombo
                    model: ["-- Select Preset --"]
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 28
                    onActivated: {
                        if (currentIndex > 0) {
                            root.loadPresetValues(currentText);
                        }
                    }
                }

                Button {
                    text: "Save Preset..."
                    implicitHeight: 28
                    onClicked: savePresetDialog.open()
                }

                Button {
                    text: "Reset OEM"
                    implicitHeight: 28
                    onClicked: root.resetDefaults()
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "Apply All Settings"
                    implicitHeight: 30
                    implicitWidth: 160
                    contentItem: Text {
                        text: parent.text
                        color: "#0e1014"
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: SightlineTheme.primary
                        radius: 4
                    }
                    onClicked: root.applyAllSettings()
                }
            }
        }

        // Section 1: Main Enhancement Controls Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: sec1Col.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: sec1Col
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.primary; radius: 1 }
                    Text { text: "CONTRAST MODES & SHARPENING (MESSAGE 0x21)"; color: SightlineTheme.primary; font.pixelSize: 11; font.bold: true }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    rowSpacing: 10
                    columnSpacing: 14

                    // Contrast Mode
                    Text { text: "Contrast Mode:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 130 }
                    ComboBox {
                        id: modeCombo
                        model: [
                            "0: None (Linear)",
                            "1: CLAHE (Contrast-Limited Adaptive)",
                            "2: LAP (Linear Adaptive Processing)",
                            "3: CLAHE 9-Bit",
                            "4: CLAHE 10-Bit",
                            "5: Histogram Equalization",
                            "6: Gamma Correction",
                            "7: LAP 16-Bit",
                            "8: Hist Equalization + Gamma"
                        ]
                        currentIndex: 0
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 28
                    }

                    // Strength
                    Text { text: "Strength / Limit:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 120 }
                    RowLayout {
                        spacing: 8
                        Slider { id: strengthSlider; from: 0; to: 127; value: 25; Layout.preferredWidth: 160 }
                        Text { text: Math.round(strengthSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 35 }
                    }

                    // Alpha Blend
                    Text { text: "Alpha Blend (0=Orig):"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        spacing: 8
                        Slider { id: blendSlider; from: 0; to: 255; value: 255; Layout.preferredWidth: 200 }
                        Text { text: Math.round(blendSlider.value) + " (" + Math.round((blendSlider.value / 255.0) * 100) + "%)"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11 }
                    }

                    // Sharpening
                    Text { text: "Sharpening Level:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        spacing: 8
                        Slider { id: sharpSlider; from: 0; to: 15; stepSize: 1; value: 0; Layout.preferredWidth: 160 }
                        Text { text: Math.round(sharpSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 35 }
                    }

                    // Sharpen Radius
                    Text { text: "Sharpen Radius:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    ComboBox {
                        id: radiusCombo
                        model: ["1 Pixel (Fine)", "2 Pixels (Medium)", "3 Pixels (Wide)"]
                        currentIndex: 0
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 28
                    }

                    // Color Enhance
                    Text { text: "Color Enhancement:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        spacing: 8
                        Slider { id: colorEnhSlider; from: 0; to: 255; value: 0; Layout.preferredWidth: 160 }
                        Text { text: Math.round(colorEnhSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 35 }
                    }

                    // Scintillation Mitigation Preset
                    Text { text: "Scintillation Mode:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    ComboBox {
                        id: scintCombo
                        model: ["0: Manual", "1: Low Motion (Heavy Ave)", "2: High Motion (Light Ave)", "3: Infra-Red Optimization"]
                        currentIndex: 0
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 28
                    }
                }
            }
        }

        // Section 2: Denoise & Motion Masking Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: sec2Col.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: sec2Col
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.info; radius: 1 }
                    Text { text: "REGISTERED TEMPORAL DENOISE & MOTION MASKING"; color: SightlineTheme.info; font.pixelSize: 11; font.bold: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text { text: "Registered Denoise Rate:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 140 }
                    Slider { id: denoiseSlider; from: 0; to: 240; value: 0; Layout.preferredWidth: 220 }
                    Text { text: Math.round(denoiseSlider.value) + " (0=Off, 240=Max)"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11 }

                    Item { Layout.fillWidth: true }

                    CheckBox {
                        id: aerialMaskCheck
                        text: "Aerial Motion Mask"
                        checked: false
                    }

                    CheckBox {
                        id: staringMaskCheck
                        text: "Staring Motion Mask"
                        checked: false
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: SightlineTheme.cardBorder }

                // 3D Noise Reduction (Msg 0xAF)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Text { text: "3D Digital Noise Reduction:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 160 }
                    Switch { id: denoise3dSwitch; checked: false }

                    Text { text: "Temporal Strength:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    Slider { id: temp3dSlider; from: 0; to: 100; value: 50; Layout.preferredWidth: 120 }
                    Text { text: Math.round(temp3dSlider.value) + "%"; color: SightlineTheme.textPrimary; font.pixelSize: 11; Layout.preferredWidth: 35 }

                    Text { text: "Spatial Strength:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    Slider { id: spat3dSlider; from: 0; to: 100; value: 30; Layout.preferredWidth: 120 }
                    Text { text: Math.round(spat3dSlider.value) + "%"; color: SightlineTheme.textPrimary; font.pixelSize: 11; Layout.preferredWidth: 35 }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Section 3: Histogram Shaping & Filters Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: sec3Col.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: sec3Col
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.warning; radius: 1 }
                    Text { text: "HISTOGRAM EQUALIZATION SHAPING, GAUSSIAN BLUR & TONAL CONTROLS"; color: SightlineTheme.warning; font.pixelSize: 11; font.bold: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 20

                    CheckBox { id: featureHistCheck; text: "Feature-Based Histogram"; checked: false }
                    CheckBox { id: sqrtHistCheck; text: "Square Root Histogram"; checked: false }

                    Item { Layout.fillWidth: true }

                    Text { text: "Gaussian Blur (0-6):"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    Slider { id: gaussSlider; from: 0; to: 6; stepSize: 1; value: 0; Layout.preferredWidth: 100 }
                    Text { text: Math.round(gaussSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 20 }

                    Text { text: "LAP Min Diff:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    SpinBox { id: lapMinSpin; from: 0; to: 255; value: 2; implicitHeight: 28; implicitWidth: 80 }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    rowSpacing: 10
                    columnSpacing: 14

                    // Hist Ave Rate
                    Text { text: "Hist Ave Rate:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 120 }
                    RowLayout {
                        spacing: 8
                        Slider { id: histAveSlider; from: 0; to: 255; value: 0; Layout.preferredWidth: 180 }
                        Text { text: Math.round(histAveSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 35 }
                    }

                    // Hist Max Pct Bin
                    Text { text: "Hist Max Pct Bin:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 120 }
                    RowLayout {
                        spacing: 8
                        Slider { id: histMaxPctSlider; from: 0; to: 255; value: 0; Layout.preferredWidth: 180 }
                        Text { text: Math.round(histMaxPctSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 35 }
                    }

                    // Brightness (centered at 128)
                    Text { text: "Brightness (128=Norm):"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        spacing: 8
                        Slider { id: brightSlider; from: 0; to: 255; value: 128; Layout.preferredWidth: 180 }
                        Text { text: Math.round(brightSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 35 }
                    }

                    // Contrast (centered at 128)
                    Text { text: "Contrast (128=Norm):"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        spacing: 8
                        Slider { id: contrastSlider; from: 0; to: 255; value: 128; Layout.preferredWidth: 180 }
                        Text { text: Math.round(contrastSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 35 }
                    }
                }
            }
        }

        // Section 4: Region of Interest (ROI) & False Color Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: sec4Col.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: sec4Col
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.accent; radius: 1 }
                    Text { text: "REGION OF INTEREST (ROI) & FALSE COLOR PALETTES"; color: SightlineTheme.accent; font.pixelSize: 11; font.bold: true }
                }

                // ROI coordinates
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text { text: "ROI Bounding Box:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; font.bold: true }
                    Text { text: "Row:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    SpinBox { id: roiRowSpin; from: 0; to: 2160; value: 0; implicitHeight: 28; implicitWidth: 80 }

                    Text { text: "Col:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    SpinBox { id: roiColSpin; from: 0; to: 3840; value: 0; implicitHeight: 28; implicitWidth: 80 }

                    Text { text: "Height:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    SpinBox { id: roiHighSpin; from: 0; to: 2160; value: 0; implicitHeight: 28; implicitWidth: 80 }

                    Text { text: "Width:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    SpinBox { id: roiWideSpin; from: 0; to: 3840; value: 0; implicitHeight: 28; implicitWidth: 80 }

                    Button {
                        text: "Reset ROI (Full Frame)"
                        implicitHeight: 28
                        onClicked: {
                            roiRowSpin.value = 0;
                            roiColSpin.value = 0;
                            roiHighSpin.value = 0;
                            roiWideSpin.value = 0;
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: SightlineTheme.cardBorder }

                // False Color Palette & Designer Modal
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text { text: "Thermal False Color Palette:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; font.bold: true }
                    ComboBox {
                        id: falseColorCombo
                        model: [
                            "0: None (Grayscale)",
                            "1: None Alt",
                            "2: White Hot",
                            "3: Black Hot",
                            "4: Rainbow",
                            "5: Rainbow Inverted",
                            "6: Iron",
                            "7: Iron Inverted",
                            "8: Hot / Cold",
                            "9: Hot / Cold Inverted",
                            "10: Jet",
                            "11: Jet Inverted",
                            "12: Hot",
                            "13: Hot Inverted",
                            "14: HSV",
                            "15: HSV Inverted",
                            "16: CLR 470-S",
                            "17: CLR 470-S Inverted",
                            "18: Color 1",
                            "19: Color 1 Inverted",
                            "20: Color 2",
                            "21: Color 2 Inverted",
                            "22: Color 3",
                            "23: Color 3 Inverted",
                            "24: Hot Iron",
                            "25: Hot Iron Inverted",
                            "26: Ice Fire",
                            "27: Ice Fire Inverted",
                            "28: ID Def",
                            "29: ID Def Inverted",
                            "30: Iron 256",
                            "31: Iron 256 Inverted",
                            "32: Rain 256",
                            "33: Rain 256 Inverted",
                            "34: Volcano",
                            "35: Volcano Inverted",
                            "36: Red Monochrome",
                            "37: Red Inverted",
                            "38: Green Monochrome",
                            "39: Green Inverted",
                            "40: Blue Monochrome",
                            "41: Blue Inverted",
                            "127: User Custom Palette (LUT)"
                        ]
                        currentIndex: 0
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 28
                        onActivated: {
                            if (bridge) {
                                var palIdx = index;
                                if (palIdx === model.length - 1) palIdx = 127;
                                bridge.setFalseColor(camCombo.currentIndex, palIdx);
                            }
                        }
                    }

                    Button {
                        text: "🎨 Open User Palette Designer (Msg 0x72)..."
                        implicitHeight: 28
                        contentItem: Text {
                            text: parent.text
                            color: SightlineTheme.accent
                            font.bold: true
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            border.color: SightlineTheme.accent
                            radius: 4
                        }
                        onClicked: paletteDialog.open()
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Section 5: Custom Spatial Convolution Matrix Card
        ConvolutionGrid {
            id: convGrid
            Layout.fillWidth: true
            cameraIndex: camCombo.currentIndex
        }

        Item { Layout.preferredHeight: 16 }
    }
}
