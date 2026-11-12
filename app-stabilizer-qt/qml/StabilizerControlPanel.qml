import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Components"

Rectangle {
    id: root
    width: 360
    height: parent ? parent.height : 600
    color: "#12151d"
    border.color: "#1e2433"
    border.width: 1

    property var controller: null
    readonly property var configModel: controller ? controller.configModel : null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Header
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "STABILIZER TUNING"
                color: "#f0f4fc"
                font.pixelSize: 12
                font.bold: true
                font.letterSpacing: 1.5
            }

            Item { Layout.fillWidth: true }

            Button {
                height: 22
                text: "RESET ALL"
                onClicked: if (configModel) configModel.resetAll()
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 9
                    font.bold: true
                    color: "#ff9100"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: "#24180d"
                    border.color: "#ff9100"
                    border.width: 1
                    radius: 2
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#22293a"
        }

        // Scrollable Accordion
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            Column {
                width: parent.width
                spacing: 6

                // Section 1: Core Mode & Supervisory
                AccordionSection {
                    title: "1. CORE MODE & SUPERVISORY"
                    isExpanded: true
                    sectionIndex: 0
                    configModel: root.configModel

                    TacticalComboBox {
                        label: "Stabilization Mode"
                        model: ["Disabled", "Translation Only (2-DOF)", "Translation + Rotation (3-DOF)", "Similarity (4-DOF)", "Affine (6-DOF)", "Rolling Shutter Comp"]
                        currentIndex: configModel ? configModel.stabilizationMode : 4
                        onActivated: (index) => { if (configModel) configModel.stabilizationMode = index; }
                    }

                    TacticalComboBox {
                        label: "Fallback Policy"
                        model: ["Identity Hold", "Velocity Extrapolate", "Bypass Warp"]
                        currentIndex: configModel ? configModel.fallbackPolicy : 1
                        onActivated: (index) => { if (configModel) configModel.fallbackPolicy = index; }
                    }

                    TacticalSlider {
                        label: "Min Tracked Features"
                        from: 5; to: 200; stepSize: 1; decimals: 0
                        value: configModel ? configModel.minTrackedFeatures : 20
                        onValueModified: (v) => { if (configModel) configModel.minTrackedFeatures = v; }
                    }

                    TacticalSlider {
                        label: "Replenish Interval"
                        unit: "frm"
                        from: 1; to: 60; stepSize: 1; decimals: 0
                        value: configModel ? configModel.replenishInterval : 15
                        onValueModified: (v) => { if (configModel) configModel.replenishInterval = v; }
                    }

                    TacticalSlider {
                        label: "Max Degraded Frames"
                        unit: "frm"
                        from: 1; to: 120; stepSize: 1; decimals: 0
                        value: configModel ? configModel.maxDegradedFrames : 30
                        onValueModified: (v) => { if (configModel) configModel.maxDegradedFrames = v; }
                    }

                    TacticalSlider {
                        label: "Min Inlier Ratio"
                        from: 0.05; to: 0.95; stepSize: 0.01; decimals: 2
                        value: configModel ? configModel.minInlierRatio : 0.25
                        onValueModified: (v) => { if (configModel) configModel.minInlierRatio = v; }
                    }
                }

                // Section 2: Trajectory Smoothing & Filter
                AccordionSection {
                    title: "2. TRAJECTORY SMOOTHING & FILTER"
                    sectionIndex: 1
                    configModel: root.configModel

                    TacticalComboBox {
                        label: "Smoothing Algorithm"
                        model: ["Kalman Filter (EKF)", "Gaussian FIR", "L1 Optimization"]
                        currentIndex: configModel ? configModel.smoothingMethod : 0
                        onActivated: (index) => { if (configModel) configModel.smoothingMethod = index; }
                    }

                    TacticalSlider {
                        label: "Max Crop Margin"
                        from: 0.0; to: 0.30; stepSize: 0.005; decimals: 3
                        value: configModel ? configModel.maxCropMargin : 0.08
                        onValueModified: (v) => { if (configModel) configModel.maxCropMargin = v; }
                    }

                    TacticalSlider {
                        label: "Soft Margin Ratio"
                        from: 0.10; to: 0.99; stepSize: 0.01; decimals: 2
                        value: configModel ? configModel.softMarginRatio : 0.80
                        onValueModified: (v) => { if (configModel) configModel.softMarginRatio = v; }
                    }

                    TacticalSlider {
                        label: "Process Noise Base (qa)"
                        from: 0.000001; to: 0.1; stepSize: 0.00001; decimals: 5
                        value: configModel ? configModel.processNoiseBase : 0.0001
                        onValueModified: (v) => { if (configModel) configModel.processNoiseBase = v; }
                    }

                    TacticalSlider {
                        label: "Measurement Noise (R)"
                        from: 0.0001; to: 1.0; stepSize: 0.001; decimals: 4
                        value: configModel ? configModel.measurementNoise : 0.01
                        onValueModified: (v) => { if (configModel) configModel.measurementNoise = v; }
                    }

                    TacticalSlider {
                        label: "PTZ Feedforward Gain"
                        from: 0.0; to: 1.50; stepSize: 0.05; decimals: 2
                        value: configModel ? configModel.ptzGain : 0.95
                        onValueModified: (v) => { if (configModel) configModel.ptzGain = v; }
                    }

                    TacticalSlider {
                        label: "PTZ Slew Deadband"
                        unit: "deg/s"
                        from: 0.0; to: 5.00; stepSize: 0.05; decimals: 2
                        value: configModel ? configModel.ptzDeadband : 0.15
                        onValueModified: (v) => { if (configModel) configModel.ptzDeadband = v; }
                    }

                    TacticalSlider {
                        label: "FIR/L1 Window Size"
                        unit: "frm"
                        from: 3; to: 63; stepSize: 2; decimals: 0
                        value: configModel ? configModel.windowSize : 31
                        onValueModified: (v) => { if (configModel) configModel.windowSize = v; }
                    }
                }

                // Section 3: Feature Detector (Shi-Tomasi)
                AccordionSection {
                    title: "3. FEATURE DETECTOR (SHI-TOMASI)"
                    sectionIndex: 2
                    configModel: root.configModel

                    TacticalSlider {
                        label: "Quality Threshold"
                        from: 0.0001; to: 0.5000; stepSize: 0.001; decimals: 4
                        value: configModel ? configModel.qualityThreshold : 0.01
                        onValueModified: (v) => { if (configModel) configModel.qualityThreshold = v; }
                    }

                    TacticalSlider {
                        label: "Outer Border Margin"
                        unit: "px"
                        from: 3; to: 64; stepSize: 1; decimals: 0
                        value: configModel ? configModel.borderMargin : 8
                        onValueModified: (v) => { if (configModel) configModel.borderMargin = v; }
                    }

                    TacticalSlider {
                        label: "Max Total Features"
                        from: 10; to: 2048; stepSize: 10; decimals: 0
                        value: configModel ? configModel.maxTotalFeatures : 300
                        onValueModified: (v) => { if (configModel) configModel.maxTotalFeatures = v; }
                    }

                    TacticalSlider {
                        label: "Grid Bucketing Cols"
                        from: 1; to: 32; stepSize: 1; decimals: 0
                        value: configModel ? configModel.gridCols : 8
                        onValueModified: (v) => { if (configModel) configModel.gridCols = v; }
                    }

                    TacticalSlider {
                        label: "Grid Bucketing Rows"
                        from: 1; to: 32; stepSize: 1; decimals: 0
                        value: configModel ? configModel.gridRows : 6
                        onValueModified: (v) => { if (configModel) configModel.gridRows = v; }
                    }

                    TacticalSlider {
                        label: "Max Features Per Cell"
                        from: 1; to: 64; stepSize: 1; decimals: 0
                        value: configModel ? configModel.maxFeaturesPerCell : 8
                        onValueModified: (v) => { if (configModel) configModel.maxFeaturesPerCell = v; }
                    }

                    TacticalSlider {
                        label: "Min Separation Distance"
                        unit: "px"
                        from: 0.0; to: 64.0; stepSize: 0.5; decimals: 1
                        value: configModel ? configModel.minFeatureDistance : 8.0
                        onValueModified: (v) => { if (configModel) configModel.minFeatureDistance = v; }
                    }
                }

                // Section 4: Optical Flow Tracker (Pyramidal LK)
                AccordionSection {
                    title: "4. OPTICAL FLOW TRACKER (LK)"
                    sectionIndex: 3
                    configModel: root.configModel

                    TacticalSlider {
                        label: "Integration Window (half-width)"
                        unit: "px"
                        from: 2; to: 16; stepSize: 1; decimals: 0
                        value: configModel ? configModel.flowWindowHalfWidth : 5
                        onValueModified: (v) => { if (configModel) configModel.flowWindowHalfWidth = v; }
                    }

                    TacticalSlider {
                        label: "Pyramid Levels"
                        from: 1; to: 3; stepSize: 1; decimals: 0
                        value: configModel ? configModel.numPyramidLevels : 3
                        onValueModified: (v) => { if (configModel) configModel.numPyramidLevels = v; }
                    }

                    TacticalSlider {
                        label: "Max Iterations / Level"
                        from: 1; to: 100; stepSize: 1; decimals: 0
                        value: configModel ? configModel.maxFlowIterations : 25
                        onValueModified: (v) => { if (configModel) configModel.maxFlowIterations = v; }
                    }

                    TacticalSlider {
                        label: "Min Epsilon"
                        unit: "px"
                        from: 0.001; to: 0.200; stepSize: 0.001; decimals: 3
                        value: configModel ? configModel.minEpsilon : 0.01
                        onValueModified: (v) => { if (configModel) configModel.minEpsilon = v; }
                    }

                    TacticalSlider {
                        label: "Max Fwd-Bwd Error"
                        unit: "px"
                        from: 0.05; to: 5.00; stepSize: 0.05; decimals: 2
                        value: configModel ? configModel.maxFbError : 0.50
                        onValueModified: (v) => { if (configModel) configModel.maxFbError = v; }
                    }

                    TacticalSwitch {
                        text: "Bidirectional Verification"
                        checked: configModel ? configModel.enableBidirectional : true
                        onToggled: (chk) => { if (configModel) configModel.enableBidirectional = chk; }
                    }
                }

                // Section 5: Motion Model Estimator (MSAC)
                AccordionSection {
                    title: "5. MOTION MODEL ESTIMATOR (MSAC)"
                    sectionIndex: 4
                    configModel: root.configModel

                    TacticalSlider {
                        label: "Inlier Threshold"
                        unit: "px"
                        from: 0.1; to: 15.0; stepSize: 0.1; decimals: 1
                        value: configModel ? configModel.inlierThreshold : 2.0
                        onValueModified: (v) => { if (configModel) configModel.inlierThreshold = v; }
                    }

                    TacticalSlider {
                        label: "Max MSAC Iterations"
                        from: 10; to: 1000; stepSize: 10; decimals: 0
                        value: configModel ? configModel.maxMsacIterations : 200
                        onValueModified: (v) => { if (configModel) configModel.maxMsacIterations = v; }
                    }

                    TacticalSlider {
                        label: "Min MSAC Iterations"
                        from: 5; to: 500; stepSize: 5; decimals: 0
                        value: configModel ? configModel.minMsacIterations : 20
                        onValueModified: (v) => { if (configModel) configModel.minMsacIterations = v; }
                    }

                    TacticalSlider {
                        label: "Consensus Confidence"
                        from: 0.80; to: 0.999; stepSize: 0.001; decimals: 3
                        value: configModel ? configModel.msacConfidence : 0.99
                        onValueModified: (v) => { if (configModel) configModel.msacConfidence = v; }
                    }

                    TacticalSwitch {
                        text: "Inlier Least-Squares Refinement"
                        checked: configModel ? configModel.enableRefinement : true
                        onToggled: (chk) => { if (configModel) configModel.enableRefinement = chk; }
                    }
                }

                // Section 6: Warp Engine & Rolling Shutter
                AccordionSection {
                    title: "6. WARP ENGINE & ROLLING SHUTTER"
                    sectionIndex: 5
                    configModel: root.configModel

                    TacticalComboBox {
                        label: "Texture Interpolation"
                        model: ["Nearest Neighbor", "Bilinear", "Bicubic"]
                        currentIndex: configModel ? configModel.warpInterpolation : 1
                        onActivated: (index) => { if (configModel) configModel.warpInterpolation = index; }
                    }

                    TacticalComboBox {
                        label: "Border Handling Mode"
                        model: ["Constant Fill", "Replicate Edge", "Reflect Edge"]
                        currentIndex: configModel ? configModel.borderMode : 0
                        onActivated: (index) => { if (configModel) configModel.borderMode = index; }
                    }

                    TacticalSlider {
                        label: "Auto-Crop Zoom Ratio"
                        from: 0.0; to: 0.30; stepSize: 0.005; decimals: 3
                        value: configModel ? configModel.warpCropRatio : 0.05
                        onValueModified: (v) => { if (configModel) configModel.warpCropRatio = v; }
                    }

                    TacticalSwitch {
                        text: "Enable Dynamic Auto-Crop"
                        checked: configModel ? configModel.enableAutoCrop : true
                        onToggled: (chk) => { if (configModel) configModel.enableAutoCrop = chk; }
                    }

                    TacticalSwitch {
                        text: "Rolling Shutter Compensation"
                        checked: configModel ? configModel.enableRollingShutter : false
                        onToggled: (chk) => { if (configModel) configModel.enableRollingShutter = chk; }
                    }

                    TacticalSlider {
                        label: "CMOS Readout Ratio"
                        from: 0.0; to: 1.0; stepSize: 0.02; decimals: 2
                        value: configModel ? configModel.rsReadoutRatio : 0.80
                        onValueModified: (v) => { if (configModel) configModel.rsReadoutRatio = v; }
                    }

                    TacticalSwitch {
                        text: "Scan Top to Bottom"
                        checked: configModel ? configModel.rsScanTopToBottom : true
                        onToggled: (chk) => { if (configModel) configModel.rsScanTopToBottom = chk; }
                    }
                }
            }
        }
    }

    // Component for each Accordion Section
    component AccordionSection: Rectangle {
        id: sectionRoot
        width: parent ? parent.width : 336
        height: isExpanded ? (headerRow.height + contentCol.height + 16) : headerRow.height
        clip: true
        color: "#161a24"
        border.color: isExpanded ? "#00e5ff" : "#242c3d"
        border.width: 1
        radius: 3

        property string title: ""
        property bool isExpanded: false
        property int sectionIndex: 0
        property var configModel: null
        default property alias content: contentCol.data

        Behavior on height {
            NumberAnimation { duration: 150; easing.type: Easing.InOutQuad }
        }

        // Accordion Header Bar
        Rectangle {
            id: headerRow
            width: parent.width
            height: 32
            color: isExpanded ? "#1c2230" : "#141720"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Text {
                    text: isExpanded ? "v" : ">"
                    color: isExpanded ? "#00e5ff" : "#8894ab"
                    font.bold: true
                    font.pixelSize: 11
                }

                Text {
                    text: sectionRoot.title
                    color: isExpanded ? "#f0f4fc" : "#c0c8d8"
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.0
                    Layout.fillWidth: true
                }

                Button {
                    height: 20
                    text: "RESET"
                    visible: sectionRoot.isExpanded
                    onClicked: {
                        if (sectionRoot.configModel) {
                            sectionRoot.configModel.resetSection(sectionRoot.sectionIndex);
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 9
                        color: "#8894ab"
                    }
                    background: Rectangle {
                        color: "#10131a"
                        border.color: "#2a3244"
                        radius: 2
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: sectionRoot.isExpanded = !sectionRoot.isExpanded
            }
        }

        // Section Content
        Column {
            id: contentCol
            x: 8
            y: headerRow.height + 8
            width: parent.width - 16
            spacing: 6
            visible: sectionRoot.isExpanded
        }
    }
}

