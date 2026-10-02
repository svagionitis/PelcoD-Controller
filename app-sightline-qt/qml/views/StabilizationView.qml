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
    contentHeight: mainCol.implicitHeight + 40
    clip: true
    ScrollBar.vertical.policy: ScrollBar.AsNeeded
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    readonly property int minContentWidth: 980

    // Internal reactive properties synchronized with hardware telemetry
    property int currentCam: 0
    property bool isStabActive: true
    property int stabDriftRate: 50
    property int stabAngleLimit: 5
    property int stabMaxStabOff: 0
    property int stabMaxDispOffset: 0
    property int stabModeFlags: 1
    property int edgeModeIndex: 1
    property int edgeY: 16
    property int edgeU: 128
    property int edgeV: 128

    property int regMaxTrans: 0
    property int regMaxRot: 5
    property int regMaxZoom: 0
    property bool regLowDrift: false
    property int regLeft: 0
    property int regRight: 0
    property int regTop: 0
    property int regBottom: 0
    property bool regUserMode: false
    property bool showIgnoredEdges: false

    property bool autoBiasEnabled: true
    property int biasUpdateRate: 50
    property int manualColBias: 0
    property int manualRowBias: 0

    // Gimbal Bias Calculator properties (EAN-Stabilization Section 3.1 & 3.2)
    property real calcPanRate: 2.0
    property real calcTiltRate: 0.0
    property int calcHRes: 1920
    property int calcVRes: 1080
    property real calcHFov: 8.0
    property real calcVFov: 4.5
    property real calcFps: 30.0
    property int liveCalculatedColBias: 16
    property int liveCalculatedRowBias: 0

    function updateCalculator() {
        if (calcHFov > 0.01 && calcFps > 0.01) {
            liveCalculatedColBias = Math.round((calcPanRate * calcHRes) / (calcHFov * calcFps))
        } else {
            liveCalculatedColBias = 0
        }
        if (calcVFov > 0.01 && calcFps > 0.01) {
            liveCalculatedRowBias = Math.round((calcTiltRate * calcVRes) / (calcVFov * calcFps))
        } else {
            liveCalculatedRowBias = 0
        }
    }

    Connections {
        target: bridge
        function onStabilizationChanged(cam, mode, rate, maxDispOffset, maxAngle, maxStabOff) {
            if (cam === currentCam) {
                isStabActive = (mode & 0x01) !== 0
                stabDriftRate = rate
                stabMaxDispOffset = maxDispOffset
                stabAngleLimit = maxAngle
                stabMaxStabOff = maxStabOff
                stabModeFlags = mode
            }
        }
        function onRegistrationChanged(cam, maxTranslation, maxRotation, zoomRange, left, right, top, bottom, updateRate) {
            if (cam === currentCam) {
                regMaxTrans = maxTranslation
                regMaxRot = maxRotation
                regMaxZoom = zoomRange
                regLeft = left
                regRight = right
                regTop = top
                regBottom = bottom
                regLowDrift = (updateRate <= 30)
            }
        }
        function onStabilizationBiasChanged(cam, biasCol, biasRow, autoBias, updateRate) {
            if (cam === currentCam) {
                manualColBias = biasCol
                manualRowBias = biasRow
                autoBiasEnabled = (autoBias !== 0)
                biasUpdateRate = updateRate
            }
        }
    }

    ColumnLayout {
        id: mainCol
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        x: 16
        spacing: 16

        Item { Layout.preferredHeight: 4 }

        // Header Title
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { width: 4; height: 22; color: SightlineTheme.primary; radius: 2 }
            ColumnLayout {
                spacing: 2
                Text {
                    text: "VIDEO STABILIZATION & FRAME REGISTRATION"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: SightlineTheme.fontSizeMedium
                    font.bold: true
                    font.letterSpacing: 1.0
                }
                Text {
                    text: "Sightline EAN-Stabilization Compliance • Messages 0x02, 0x04, 0x9E, 0x9F"
                    color: SightlineTheme.textMuted
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.family: "Monospace"
                }
            }
            Item { Layout.fillWidth: true }

            // Active Camera Selector
            RowLayout {
                spacing: 6
                Text { text: "Sensor:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                ComboBox {
                    id: cameraSelector
                    model: ["Cam 0 (EO Visible)", "Cam 1 (IR Thermal)", "Cam 2 (Aux)", "Cam 3 (Network)"]
                    currentIndex: currentCam
                    Layout.preferredWidth: 180
                    onActivated: {
                        currentCam = index
                        if (bridge) {
                            bridge.queryModuleParameters(1)
                        }
                    }
                }
            }
        }

        // Live Telemetry Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard {
                title: "Stabilization"
                value: isStabActive ? "ACTIVE" : "OFF"
                unit: ""
                accentColor: isStabActive ? SightlineTheme.success : SightlineTheme.textMuted
                iconText: "🎯"
            }
            MetricCard {
                title: "Col Bias (H)"
                value: (manualColBias >= 0 ? "+" : "") + manualColBias
                unit: "px"
                accentColor: SightlineTheme.primary
                iconText: "↔️"
            }
            MetricCard {
                title: "Row Bias (V)"
                value: (manualRowBias >= 0 ? "+" : "") + manualRowBias
                unit: "px"
                accentColor: SightlineTheme.primary
                iconText: "↕️"
            }
            MetricCard {
                title: "Drift Recentering"
                value: stabDriftRate.toString()
                unit: "rate"
                accentColor: SightlineTheme.info
                iconText: "⚡"
            }
            MetricCard {
                title: "Max Roll Angle"
                value: stabAngleLimit.toString()
                unit: "deg"
                accentColor: stabAngleLimit > 0 ? SightlineTheme.info : SightlineTheme.textMuted
                iconText: "🔄"
            }
            MetricCard {
                title: "Auto Bias Mode"
                value: autoBiasEnabled ? "ENABLED" : "MANUAL"
                unit: ""
                accentColor: autoBiasEnabled ? SightlineTheme.accent : SightlineTheme.warning
                iconText: "🧭"
            }
        }

        // Operational Profiles Presets Toolbar (EAN-Stabilization Section 2.2)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: presetCol.implicitHeight + 24
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: presetCol
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.accent; radius: 1 }
                    Text {
                        text: "OPERATIONAL PROFILE PRESETS (EAN-STABILIZATION §2.2)"
                        color: SightlineTheme.accent
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    // 1. Airborne Preset
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        contentItem: ColumnLayout {
                            spacing: 1
                            anchors.centerIn: parent
                            Text {
                                text: "✈️ AIRBORNE GIMBAL (§2.2.1)"
                                color: SightlineTheme.textPrimary
                                font.bold: true
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: "Trans:0, Rot:5, Drift:50, LowDrift:OFF, AutoBias:ON"
                                color: SightlineTheme.textMuted
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                        background: Rectangle {
                            color: parent.hovered ? SightlineTheme.surfaceLight : "#161b22"
                            border.color: SightlineTheme.cardBorder
                            border.width: 1
                            radius: 4
                        }
                        onClicked: {
                            if (bridge) bridge.applyStabilizationPreset(currentCam, 0)
                            regMaxTrans = 0; regMaxRot = 5; regMaxZoom = 0; regLowDrift = false
                            regLeft = 0; regRight = 0; regTop = 0; regBottom = 0
                            stabDriftRate = 50; stabAngleLimit = 5; stabMaxStabOff = 0
                            autoBiasEnabled = true; biasUpdateRate = 50
                        }
                    }

                    // 2. Fixed / Ground PTZ Preset
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        contentItem: ColumnLayout {
                            spacing: 1
                            anchors.centerIn: parent
                            Text {
                                text: "🔭 FIXED / GROUND PTZ (§2.2.2)"
                                color: SightlineTheme.textPrimary
                                font.bold: true
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: "Trans:50, Rot:0, Drift:20, LowDrift:ON, MaxStab:32"
                                color: SightlineTheme.textMuted
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                        background: Rectangle {
                            color: parent.hovered ? SightlineTheme.surfaceLight : "#161b22"
                            border.color: SightlineTheme.cardBorder
                            border.width: 1
                            radius: 4
                        }
                        onClicked: {
                            if (bridge) bridge.applyStabilizationPreset(currentCam, 1)
                            regMaxTrans = 50; regMaxRot = 0; regMaxZoom = 0; regLowDrift = true
                            regLeft = 0; regRight = 0; regTop = 0; regBottom = 0
                            stabDriftRate = 20; stabAngleLimit = 0; stabMaxStabOff = 32
                            autoBiasEnabled = false; biasUpdateRate = 50
                        }
                    }

                    // 3. Moving Vehicle Preset
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        contentItem: ColumnLayout {
                            spacing: 1
                            anchors.centerIn: parent
                            Text {
                                text: "🚗 MOVING VEHICLE (§2.2.3)"
                                color: SightlineTheme.textPrimary
                                font.bold: true
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: "Trans:0, Rot:0, MaxStab:50, IgnoreEdges:100/50"
                                color: SightlineTheme.textMuted
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                        background: Rectangle {
                            color: parent.hovered ? SightlineTheme.surfaceLight : "#161b22"
                            border.color: SightlineTheme.cardBorder
                            border.width: 1
                            radius: 4
                        }
                        onClicked: {
                            if (bridge) bridge.applyStabilizationPreset(currentCam, 2)
                            regMaxTrans = 0; regMaxRot = 0; regMaxZoom = 0; regLowDrift = false
                            regLeft = 100; regRight = 100; regTop = 50; regBottom = 0
                            stabDriftRate = 50; stabAngleLimit = 0; stabMaxStabOff = 50
                            autoBiasEnabled = true; biasUpdateRate = 50
                        }
                    }
                }
            }
        }

        // Section 1: Video Stabilization Settings (Message ID 0x02)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: stabSectionCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: stabSectionCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.primary; radius: 1 }
                    Text {
                        text: "VIDEO STABILIZATION PARAMETERS (MESSAGE 0x02)"
                        color: SightlineTheme.primary
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                // Stabilization Switches
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 24

                    RowLayout {
                        spacing: 8
                        Switch {
                            id: stabOnSwitch
                            checked: isStabActive
                            onToggled: isStabActive = checked
                        }
                        Text { text: "Enable Video Stabilization"; color: SightlineTheme.textPrimary; font.pixelSize: 12 }
                    }

                    RowLayout {
                        spacing: 8
                        Switch {
                            id: rollCompSwitch
                            checked: stabAngleLimit > 0
                            onToggled: stabAngleLimit = checked ? 5 : 0
                        }
                        Text { text: "Roll Axis Compensation"; color: SightlineTheme.textPrimary; font.pixelSize: 12 }
                    }

                    RowLayout {
                        spacing: 8
                        CheckBox {
                            id: disableAllCheck
                            text: "Disable All Processing"
                            checked: (stabModeFlags & 0x02) !== 0
                            onToggled: {
                                if (checked) stabModeFlags |= 0x02
                                else stabModeFlags &= ~0x02
                            }
                        }
                    }

                    RowLayout {
                        spacing: 8
                        CheckBox {
                            id: disableRegCheck
                            text: "Disable Registration"
                            checked: (stabModeFlags & 0x40) !== 0
                            onToggled: {
                                if (checked) stabModeFlags |= 0x40
                                else stabModeFlags &= ~0x40
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // Grid of Numerical Tunables
                GridLayout {
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Text { text: "Drift Re-centering Rate:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    RowLayout {
                        SpinBox {
                            id: driftRateSpin
                            from: 0; to: 255; value: stabDriftRate
                            onValueModified: stabDriftRate = value
                        }
                        Text { text: (stabDriftRate / 10).toFixed(1) + "% / frame"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    }

                    Text { text: "Max Stabilization Offset:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: maxStabOffSpin
                        from: 0; to: 255; value: stabMaxStabOff
                        onValueModified: stabMaxStabOff = value
                    }

                    Text { text: "Max Display Grey Offset:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: maxDispOffSpin
                        from: 0; to: 255; value: stabMaxDispOffset
                        onValueModified: stabMaxDispOffset = value
                    }

                    Text { text: "Max Rotation Limit (°):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: angleLimitSpin
                        from: 0; to: 180; value: stabAngleLimit
                        onValueModified: stabAngleLimit = value
                    }
                }

                // Edge Mode and Border Fill
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text { text: "Background Edge Mode:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        id: edgeModeCombo
                        model: ["Blur & Fade to Color (0)", "Solid Background Color (1)", "Previous Images No Fade (2)"]
                        currentIndex: edgeModeIndex
                        onActivated: edgeModeIndex = index
                    }

                    Text { text: "Border Fill (Y, U, V):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox { id: edgeYSpin; from: 0; to: 255; value: edgeY; onValueModified: edgeY = value; Layout.preferredWidth: 80 }
                    SpinBox { id: edgeUSpin; from: 0; to: 255; value: edgeU; onValueModified: edgeU = value; Layout.preferredWidth: 80 }
                    SpinBox { id: edgeVSpin; from: 0; to: 255; value: edgeV; onValueModified: edgeV = value; Layout.preferredWidth: 80 }

                    Button {
                        text: "Default Grey"
                        Layout.preferredHeight: 28
                        onClicked: { edgeY = 16; edgeU = 128; edgeV = 128 }
                    }

                    Item { Layout.fillWidth: true }
                }

                // Filter Reset Controls (Message ID 0x04)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Text { text: "Smoothing Filter Reset:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        id: resetTypeCombo
                        model: ["0: Reset All Filters (Default)", "1: Display Filter Only", "2: Auto Bias Filter Only"]
                        currentIndex: 0
                        Layout.preferredWidth: 230
                    }

                    Button {
                        text: "Execute Filter Reset (0x04)"
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 30
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                        onClicked: {
                            if (bridge) bridge.resetStabilization(currentCam, resetTypeCombo.currentIndex)
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Section 2: Image Registration Parameters (Message ID 0x9E)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: regSectionCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: regSectionCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.info; radius: 1 }
                    Text {
                        text: "FRAME-TO-FRAME REGISTRATION PARAMETERS (MESSAGE 0x9E)"
                        color: SightlineTheme.info
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                GridLayout {
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Text { text: "Maximum Translation (px):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    RowLayout {
                        SpinBox {
                            id: regTransSpin
                            from: 0; to: 500; value: regMaxTrans
                            onValueModified: regMaxTrans = value
                        }
                        Text { text: regMaxTrans === 0 ? "(0 = 1/4 height default)" : ""; color: SightlineTheme.textMuted; font.pixelSize: 10 }
                    }

                    Text { text: "Maximum Rotation (°/frame):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: regRotSpin
                        from: 0; to: 10; value: regMaxRot
                        onValueModified: regMaxRot = value
                    }

                    Text { text: "Max Zoom Range (%/frame):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: regZoomSpin
                        from: 0; to: 10; value: regMaxZoom
                        onValueModified: regMaxZoom = value
                    }

                    Text { text: "Stationary Camera Low Drift:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    RowLayout {
                        CheckBox {
                            id: lowDriftCheck
                            checked: regLowDrift
                            onToggled: regLowDrift = checked
                        }
                        Text {
                            text: regLowDrift ? "Checked (10% update rate - Staring Lock)" : "Unchecked (100% update rate - Moving Cam)"
                            color: regLowDrift ? SightlineTheme.accent : SightlineTheme.textMuted
                            font.pixelSize: 11
                        }
                    }
                }

                // Ignore Edges Band for Registration (EAN §2.2.4 & §2.2.5)
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: ignoreEdgesCol.implicitHeight + 16
                    color: "#161b22"
                    radius: 4
                    border.color: SightlineTheme.cardBorder

                    ColumnLayout {
                        id: ignoreEdgesCol
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        RowLayout {
                            spacing: 8
                            Text {
                                text: "IGNORE EDGE PIXELS (FOR CAMERA OSD OVERLAYS / VIGNETTING):"
                                color: SightlineTheme.textSecondary
                                font.bold: true
                                font.pixelSize: 11
                            }
                            Item { Layout.fillWidth: true }
                            Switch {
                                id: ignoredOverlaySwitch
                                checked: showIgnoredEdges
                                onToggled: {
                                    showIgnoredEdges = checked
                                    if (bridge) bridge.setIgnoredEdgesOverlay(currentCam, checked)
                                }
                            }
                            Text {
                                text: "Show Ignored Edges Yellow Overlay (0x06)"
                                color: showIgnoredEdges ? SightlineTheme.warning : SightlineTheme.textMuted
                                font.pixelSize: 11
                            }
                        }

                        RowLayout {
                            spacing: 16
                            Text { text: "Left (px):"; color: SightlineTheme.textMuted; font.pixelSize: 12 }
                            SpinBox { from: 0; to: 1000; value: regLeft; onValueModified: regLeft = value; Layout.preferredWidth: 90 }

                            Text { text: "Right (px):"; color: SightlineTheme.textMuted; font.pixelSize: 12 }
                            SpinBox { from: 0; to: 1000; value: regRight; onValueModified: regRight = value; Layout.preferredWidth: 90 }

                            Text { text: "Top (px):"; color: SightlineTheme.textMuted; font.pixelSize: 12 }
                            SpinBox { from: 0; to: 1000; value: regTop; onValueModified: regTop = value; Layout.preferredWidth: 90 }

                            Text { text: "Bottom (px):"; color: SightlineTheme.textMuted; font.pixelSize: 12 }
                            SpinBox { from: 0; to: 1000; value: regBottom; onValueModified: regBottom = value; Layout.preferredWidth: 90 }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }

        // Section 3: Manual & Automatic Stabilization Bias (Message ID 0x9F)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: biasSectionCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: biasSectionCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.success; radius: 1 }
                    Text {
                        text: "STABILIZATION BIAS & GIMBAL INTEGRATION (MESSAGE 0x9F & EAN §3)"
                        color: SightlineTheme.success
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 24

                    RowLayout {
                        spacing: 8
                        Switch {
                            id: autoBiasSw
                            checked: autoBiasEnabled
                            onToggled: autoBiasEnabled = checked
                        }
                        Text { text: "Automatic Stabilization Bias"; color: SightlineTheme.textPrimary; font.pixelSize: 12 }
                    }

                    RowLayout {
                        spacing: 8
                        Text { text: "Auto Bias Update Rate:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        SpinBox {
                            id: biasRateSpin
                            from: 0; to: 255; value: biasUpdateRate
                            onValueModified: biasUpdateRate = value
                        }
                    }

                    RowLayout {
                        spacing: 8
                        Text { text: "Manual Col Bias (px):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        SpinBox {
                            from: -2000; to: 2000; value: manualColBias
                            onValueModified: manualColBias = value
                            Layout.preferredWidth: 100
                        }
                    }

                    RowLayout {
                        spacing: 8
                        Text { text: "Manual Row Bias (px):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        SpinBox {
                            from: -2000; to: 2000; value: manualRowBias
                            onValueModified: manualRowBias = value
                            Layout.preferredWidth: 100
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // Gimbal Feedforward Bias Calculator Box (EAN §3.1 & §3.2)
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: calcBoxCol.implicitHeight + 16
                    color: "#161b22"
                    radius: 4
                    border.color: SightlineTheme.cardBorder

                    ColumnLayout {
                        id: calcBoxCol
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Text {
                            text: "FEEDFORWARD GIMBAL BIAS CALCULATOR (§3.1): biasCol = (panLeft * HRes)/(HFov * fps)"
                            color: SightlineTheme.accent
                            font.bold: true
                            font.pixelSize: 11
                        }

                        GridLayout {
                            columns: 6
                            columnSpacing: 14
                            rowSpacing: 8
                            Layout.fillWidth: true

                            Text { text: "Pan Rate (°/s, +Left):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            TextField {
                                text: calcPanRate.toString()
                                onTextChanged: { calcPanRate = parseFloat(text) || 0.0; updateCalculator() }
                                Layout.preferredWidth: 70
                            }

                            Text { text: "Tilt Rate (°/s, +Up):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            TextField {
                                text: calcTiltRate.toString()
                                onTextChanged: { calcTiltRate = parseFloat(text) || 0.0; updateCalculator() }
                                Layout.preferredWidth: 70
                            }

                            Text { text: "Frame Rate (fps):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            TextField {
                                text: calcFps.toString()
                                onTextChanged: { calcFps = parseFloat(text) || 30.0; updateCalculator() }
                                Layout.preferredWidth: 70
                            }

                            Text { text: "Horiz FOV (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            TextField {
                                text: calcHFov.toString()
                                onTextChanged: { calcHFov = parseFloat(text) || 8.0; updateCalculator() }
                                Layout.preferredWidth: 70
                            }

                            Text { text: "Vert FOV (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            TextField {
                                text: calcVFov.toString()
                                onTextChanged: { calcVFov = parseFloat(text) || 4.5; updateCalculator() }
                                Layout.preferredWidth: 70
                            }

                            Text { text: "Resolution (H x V):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            Text { text: calcHRes + " × " + calcVRes + " px"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11 }
                        }

                        RowLayout {
                            spacing: 16
                            Layout.topMargin: 4
                            Text {
                                text: "Calculated Bias:  Col = " + (liveCalculatedColBias >= 0 ? "+" : "") + liveCalculatedColBias + " px/frame  |  Row = " + (liveCalculatedRowBias >= 0 ? "+" : "") + liveCalculatedRowBias + " px/frame"
                                color: SightlineTheme.success
                                font.bold: true
                                font.pixelSize: 12
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                text: "Send Calculated Gimbal Bias"
                                Layout.preferredHeight: 30
                                contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                background: Rectangle { color: SightlineTheme.accent; radius: 4 }
                                onClicked: {
                                    if (bridge) {
                                        bridge.setGimbalFeedforwardBias(
                                            currentCam, calcPanRate, calcTiltRate, calcHRes, calcVRes, calcHFov, calcVFov, calcFps)
                                        manualColBias = liveCalculatedColBias
                                        manualRowBias = liveCalculatedRowBias
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Action Buttons Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Layout.topMargin: 4

            Button {
                text: "Apply All Parameters (0x02, 0x9E, 0x9F)"
                Layout.preferredWidth: 260
                Layout.preferredHeight: 36
                contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                onClicked: {
                    if (bridge) {
                        // 1. Send Stabilization (0x02)
                        let modeByte = isStabActive ? 1 : 0
                        if (disableAllCheck.checked) modeByte |= 0x02
                        if (edgeModeIndex === 1) modeByte |= 0x04
                        else if (edgeModeIndex === 2) modeByte |= 0x10
                        if (disableRegCheck.checked) modeByte |= 0x40

                        bridge.setStabilizationFull(
                            currentCam, modeByte, stabDriftRate, stabMaxDispOffset,
                            stabAngleLimit, stabMaxStabOff, edgeY, edgeU, edgeV)

                        // 2. Send Registration (0x9E)
                        let updateRateVal = regLowDrift ? 10 : 100
                        let regFlags = regUserMode ? 2 : 0
                        bridge.setRegistration(
                            currentCam, regMaxTrans, regMaxRot, regMaxZoom,
                            regLeft, regRight, regTop, regBottom, updateRateVal, regFlags)

                        // 3. Send Bias (0x9F)
                        bridge.setStabilizationBias(
                            currentCam, manualColBias, manualRowBias, autoBiasEnabled ? 1 : 0, biasUpdateRate)
                    }
                }
            }

            Button {
                text: "Request Parameters From Device"
                Layout.preferredWidth: 240
                Layout.preferredHeight: 36
                contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                onClicked: {
                    if (bridge) bridge.queryModuleParameters(1)
                }
            }

            Item { Layout.fillWidth: true }
        }

        Item { Layout.preferredHeight: 24 }
    }

    Component.onCompleted: {
        updateCalculator()
    }
}
