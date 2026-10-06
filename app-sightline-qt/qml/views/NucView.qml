import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"
import "../components/blending"

// NucView.qml
// Guided NUC / DPR calibration (docs/protocols/Sightline/EAN-NUC-and-DPR.pdf).
// All commands go through bridge.nuc (SightlineNucController), which validates them against
// IDD v3.11 and the reported firmware before transmitting 0x35 / 0x36 / 0xA8 / 0x28.

ScrollView {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true
    contentWidth: Math.max(availableWidth, minContentWidth + 32)
    contentHeight: mainCol.implicitHeight + 32
    clip: true
    ScrollBar.vertical.policy: ScrollBar.AsNeeded
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    readonly property int minContentWidth: 620
    readonly property var nuc: bridge ? bridge.nuc : null
    readonly property var caps: nuc ? nuc.caps : ({})
    readonly property var dead: nuc ? nuc.deadStats : ({})
    readonly property var noise: nuc ? nuc.noiseStats : ({})
    readonly property var tables: nuc ? nuc.tables : ({})
    readonly property bool fwKnown: nuc && nuc.firmware.length > 0

    // Last action feedback
    property string feedback: ""
    property bool feedbackOk: true

    // Shows the result of a controller call ("" = success).
    function report(result, okText) {
        feedbackOk = (result === "")
        feedback = feedbackOk ? okText : result
        feedbackTimer.restart()
    }

    function fmt(map, key, digits) {
        if (!map || map[key] === undefined) return "—"
        return digits === undefined ? String(map[key]) : Number(map[key]).toFixed(digits)
    }

    Timer { id: feedbackTimer; interval: 6000; onTriggered: root.feedback = "" }

    // ---------------------------------------------------------------- inline components
    component FieldLabel: Text {
        color: SightlineTheme.textSecondary
        font.pixelSize: 11
        Layout.alignment: Qt.AlignVCenter
    }

    component NameField: TextField {
        Layout.fillWidth: true
        implicitHeight: 30
        font.pixelSize: 12
        font.family: "Monospace"
        selectByMouse: true
        // IDD: no extension, [A-Za-z0-9_-], fewer than 64 characters
        validator: RegularExpressionValidator { regularExpression: /[A-Za-z0-9_-]{0,63}/ }
    }

    component LimitSpin: ColumnLayout {
        property alias label: lbl.text
        property alias value: sp.value
        property alias from: sp.from
        property alias to: sp.to
        spacing: 3
        Layout.fillWidth: true
        Text { id: lbl; color: SightlineTheme.textSecondary; font.pixelSize: 10; font.bold: true }
        SpinBox { id: sp; editable: true; Layout.fillWidth: true; implicitHeight: 30 }
    }

    component StatCell: Rectangle {
        id: cell
        property string label: ""
        property string value: "—"
        property color accent: SightlineTheme.primary
        Layout.fillWidth: true
        implicitHeight: 46
        radius: SightlineTheme.radiusSmall
        color: SightlineTheme.surfaceLight
        border.color: SightlineTheme.cardBorder
        Column {
            anchors.centerIn: parent
            spacing: 2
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: cell.value
                color: cell.accent
                font.pixelSize: 14
                font.bold: true
                font.family: "Monospace"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: cell.label
                color: SightlineTheme.textMuted
                font.pixelSize: 9
                font.bold: true
            }
        }
    }

    component Banner: Rectangle {
        property alias text: bannerText.text
        property color accent: SightlineTheme.warning
        Layout.fillWidth: true
        implicitHeight: bannerText.implicitHeight + 20
        radius: SightlineTheme.radiusMedium
        color: Qt.rgba(accent.r, accent.g, accent.b, 0.10)
        border.color: Qt.rgba(accent.r, accent.g, accent.b, 0.55)
        Rectangle { width: 3; radius: 1.5; color: parent.accent; anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 6 } }
        Text {
            id: bannerText
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 18; rightMargin: 12 }
            color: SightlineTheme.textPrimary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        // ------------------------------------------------------------ header
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { implicitWidth: 4; implicitHeight: 18; color: SightlineTheme.primary; radius: 2 }
            Text {
                text: "THERMAL NON-UNIFORMITY CORRECTION (NUC)"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// 0x35 · 0x36 · 0xA1 · 0xA8 · 0xAF"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
            Item { Layout.fillWidth: true }
            FieldLabel { text: "Camera:" }
            ComboBox {
                id: nucCam
                model: ["Camera 0", "Camera 1", "Camera 2", "Camera 3"]
                currentIndex: root.nuc ? root.nuc.camera : 0
                Layout.preferredWidth: 130
                onActivated: function(index) {
                    if (root.nuc) {
                        root.nuc.camera = index
                        root.nuc.refresh()
                    }
                }
            }
            BlendButton {
                id: nucRefreshBtn
                text: "⟳ Refresh"
                enabled: root.nuc !== null && bridge.isConnected
                onClicked: root.report(root.nuc.refresh() ? "" : "Not connected", "State queried")
            }
        }

        // ------------------------------------------------------------ metrics
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard {
                title: "Firmware"
                value: root.fwKnown ? root.nuc.firmware : "unknown"
                accentColor: root.fwKnown ? SightlineTheme.success : SightlineTheme.warning
                iconText: "🧩"
            }
            MetricCard {
                title: "Dead Pixels"
                value: root.fmt(root.dead, "nDead")
                unit: "px"
                accentColor: SightlineTheme.info
                iconText: "🩹"
            }
            MetricCard {
                title: "Temporal Noise"
                value: root.fmt(root.noise, "noiseTemporal", 2)
                unit: "DN"
                accentColor: SightlineTheme.accent
                iconText: "〰️"
            }
            MetricCard {
                title: "Loaded NUC Table"
                value: root.tables.nuc !== undefined && root.tables.nuc.length > 0 ? root.tables.nuc : "—"
                accentColor: SightlineTheme.primary
                iconText: "📄"
            }
            MetricCard {
                title: "Board State"
                value: root.nuc && root.nuc.hasBoardState ? "SYNCED" : "NOT READ"
                accentColor: root.nuc && root.nuc.hasBoardState ? SightlineTheme.success : SightlineTheme.textMuted
                iconText: "🔗"
            }
        }

        Banner {
            visible: root.nuc !== null && root.nuc.stabilizationOn
            accent: SightlineTheme.error
            text: "Stabilization is ON for this camera. EAN-NUC-and-DPR requires stabilization to be off before calibrating; NUC recipes are blocked until it is disabled."
        }
        Banner {
            visible: !root.fwKnown
            text: "Firmware version not reported yet. Commands are limited to the 3.0 feature set (no DPR tail, auto-dead, shutter flatten, noise stats or table names) until a 0x40 reply arrives."
        }
        Banner {
            visible: root.feedback.length > 0
            accent: root.feedbackOk ? SightlineTheme.success : SightlineTheme.error
            text: root.feedback
        }

        // ------------------------------------------------------------ guided calibration
        BlendCard {
            title: "GUIDED CALIBRATION"
            subtitle: "0x35 run modes · EAN §3.4 / §4.6 / §4.7 / §5.5"
            accent: SightlineTheme.warning

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                FieldLabel { text: "Procedure:" }
                ComboBox {
                    id: recipeBox
                    Layout.preferredWidth: 260
                    enabled: root.nuc && !root.nuc.busy
                    textRole: "text"
                    model: [
                        { text: "2-Point (hot + cold source)", need: "" },
                        { text: "1-Point (uniform source)", need: "" },
                        { text: "Shutter Flatten", need: "dpr" },
                        { text: "3D Noise Statistics", need: "noise" },
                        { text: "Multi-NUC (unverified)", need: "multi" }
                    ]
                }
                FieldLabel { text: "Frames:" }
                SpinBox {
                    id: framesBox
                    from: 1; to: 255; value: 30
                    editable: true
                    enabled: root.nuc && !root.nuc.busy
                    implicitHeight: 30
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: recipeBox.currentIndex === 2
                spacing: 10
                FieldLabel { text: "Save as:" }
                NameField {
                    id: flattenName
                    placeholderText: root.caps.shutterSave ? "e.g. field_shutter_only (optional)" : "Requires firmware 3.11"
                    enabled: root.caps.shutterSave === true && root.nuc && !root.nuc.busy
                }
            }

            // Multi-NUC (IDD 0x35 nucName, FW 3.11) - sequence not confirmed on hardware
            ColumnLayout {
                Layout.fillWidth: true
                visible: recipeBox.currentIndex === 4
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: multiNote.implicitHeight + 16
                    radius: SightlineTheme.radiusMedium
                    color: Qt.rgba(SightlineTheme.warning.r, SightlineTheme.warning.g, SightlineTheme.warning.b, 0.12)
                    border.color: SightlineTheme.warning
                    Text {
                        id: multiNote
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 8 }
                        text: root.caps.multi
                              ? "Unverified on hardware: cold frames per name, hot frames per name, then a 2-point calculation per name. Requires the 0x35 board state (Refresh)."
                              : "Requires firmware 3.11."
                        color: SightlineTheme.textPrimary
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                FieldLabel { text: "Table names (one per line, [A-Za-z0-9_-], max 16):" }
                TextArea {
                    id: multiNames
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    enabled: root.caps.multi === true && root.nuc && !root.nuc.busy
                    placeholderText: "wide\nmedium\nnarrow"
                    font.pixelSize: 12
                    font.family: "Monospace"
                    color: SightlineTheme.textPrimary
                    selectByMouse: true
                    wrapMode: TextEdit.NoWrap
                    background: Rectangle {
                        radius: SightlineTheme.radiusMedium
                        color: SightlineTheme.surfaceLight
                        border.color: multiNames.activeFocus ? SightlineTheme.warning : SightlineTheme.cardBorder
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                    }
                }
            }

            // Progress / prompt panel
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: promptCol.implicitHeight + 24
                radius: SightlineTheme.radiusMedium
                color: SightlineTheme.surfaceLight
                border.color: root.nuc && root.nuc.busy ? SightlineTheme.warning : SightlineTheme.cardBorder
                Behavior on border.color { ColorAnimation { duration: 200 } }

                ColumnLayout {
                    id: promptCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Rectangle {
                            id: stageChip
                            readonly property var names: ["IDLE", "RUNNING", "DONE", "ABORTED", "FAILED"]
                            readonly property var colors: [SightlineTheme.textMuted, SightlineTheme.warning,
                                SightlineTheme.success, SightlineTheme.textSecondary, SightlineTheme.error]
                            readonly property int st: root.nuc ? root.nuc.stage : 0
                            implicitWidth: chipText.implicitWidth + 16
                            implicitHeight: 20
                            radius: 10
                            color: Qt.rgba(colors[st].r, colors[st].g, colors[st].b, 0.18)
                            border.color: colors[st]
                            Text {
                                id: chipText
                                anchors.centerIn: parent
                                text: stageChip.names[stageChip.st]
                                color: stageChip.colors[stageChip.st]
                                font.pixelSize: 9
                                font.bold: true
                                font.letterSpacing: 0.8
                            }
                            SequentialAnimation on opacity {
                                running: root.nuc !== null && root.nuc.busy
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.55; duration: 700 }
                                NumberAnimation { to: 1.0; duration: 700 }
                                onStopped: stageChip.opacity = 1.0
                            }
                        }
                        Text {
                            text: root.nuc && root.nuc.stepCount > 0
                                  ? "Step " + Math.min(root.nuc.stepIndex + 1, root.nuc.stepCount) + " / " + root.nuc.stepCount
                                  : ""
                            color: SightlineTheme.textSecondary
                            font.pixelSize: 11
                            font.family: "Monospace"
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Step progress bar
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 4
                        radius: 2
                        color: SightlineTheme.cardBorder
                        Rectangle {
                            height: parent.height
                            radius: 2
                            width: root.nuc && root.nuc.stepCount > 0
                                   ? parent.width * root.nuc.stepIndex / root.nuc.stepCount : 0
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: SightlineTheme.warning }
                                GradientStop { position: 1.0; color: SightlineTheme.success }
                            }
                            Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.nuc ? root.nuc.prompt : ""
                        color: SightlineTheme.textPrimary
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.nuc ? root.nuc.lastError : ""
                        color: SightlineTheme.error
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                BlendButton {
                    id: nucStartBtn
                    text: "▶ Start"
                    primary: true
                    accent: SightlineTheme.warning
                    enabled: root.nuc && !root.nuc.busy && !root.nuc.stabilizationOn
                             && (recipeBox.model[recipeBox.currentIndex].need === ""
                                 || root.caps[recipeBox.model[recipeBox.currentIndex].need] === true)
                    onClicked: root.report(recipeBox.currentIndex === 4
                                           ? root.nuc.startMulti(framesBox.value, multiNames.text.split("\n"))
                                           : root.nuc.start(recipeBox.currentIndex, framesBox.value,
                                                            recipeBox.currentIndex === 2 ? flattenName.text : ""),
                                           "Procedure ready — follow the prompt, then press Next")
                }
                BlendButton {
                    id: nucNextBtn
                    text: "Next ⏭"
                    primary: true
                    enabled: root.nuc && root.nuc.busy
                    onClicked: root.report(root.nuc.next(), "Step sent")
                }
                BlendButton {
                    id: nucAbortBtn
                    text: "■ Abort"
                    accent: SightlineTheme.error
                    enabled: root.nuc && root.nuc.busy
                    onClicked: { root.nuc.abort(); root.report("", "Aborted — added frames stay on the board until cleared") }
                }
                Item { Layout.fillWidth: true }
                FieldLabel { text: "Display:" }
                ComboBox {
                    id: showBox
                    Layout.preferredWidth: 170
                    model: ["Uncorrected", "NUC + DPR", "NUC only", "DPR only", "Gain image", "Offset image", "Dead pixel image"]
                    currentIndex: root.nuc && root.nuc.board.nucShow !== undefined ? root.nuc.board.nucShow : 1
                }
                BlendButton {
                    id: nucShowBtn
                    text: "Apply"
                    enabled: root.nuc && !root.nuc.busy
                    onClicked: root.report(root.nuc.setShow(showBox.currentIndex), "Display mode set")
                }
            }
        }

        // ------------------------------------------------------------ table management
        BlendCard {
            title: "TABLE MANAGEMENT"
            subtitle: "0x36 Read/Write NUC · microSD"
            accent: SightlineTheme.primary

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 10
                rowSpacing: 8
                StatCell { label: "LOADED NUC"; value: root.tables.nuc || "—" }
                StatCell { label: "LOADED DEAD"; value: root.tables.dead || "—" }
                StatCell { label: "BOOT NUC"; value: root.tables.defaultNuc || "—"; accent: SightlineTheme.accent }
                StatCell { label: "BOOT DEAD"; value: root.tables.defaultDead || "—"; accent: SightlineTheme.accent }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                FieldLabel { text: "Table name:" }
                NameField { id: tableName; placeholderText: "e.g. field_20C (no extension)" }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8
                BlendButton { id: saveNucBtn; text: "Save NUC"; primary: true; onClicked: root.report(root.nuc.tableOp(1, 0, tableName.text, "", 0), "NUC table saved") }
                BlendButton { id: loadNucBtn; text: "Load NUC"; onClicked: root.report(root.nuc.tableOp(2, 0, tableName.text, "", 0), "NUC table loaded") }
                BlendButton { id: saveDeadBtn; text: "Save Dead"; primary: true; onClicked: root.report(root.nuc.tableOp(3, 0, tableName.text, "", 0), "Dead table saved") }
                BlendButton { id: loadDeadBtn; text: "Load Dead"; onClicked: root.report(root.nuc.tableOp(4, 0, tableName.text, "", 0), "Dead table loaded") }
                BlendButton { id: defNucBtn; text: "Boot NUC ★"; accent: SightlineTheme.accent; onClicked: root.report(root.nuc.tableOp(0, 1, tableName.text, "", 0), "Boot NUC table set") }
                BlendButton { id: defDeadBtn; text: "Boot Dead ★"; accent: SightlineTheme.accent; onClicked: root.report(root.nuc.tableOp(0, 2, tableName.text, "", 0), "Boot dead table set") }
                BlendButton { id: clrNucBtn; text: "Clear Boot NUC"; accent: SightlineTheme.error; onClicked: root.report(root.nuc.tableOp(0, 3, "", "", 0), "Boot NUC table cleared") }
                BlendButton { id: clrDeadBtn; text: "Clear Boot Dead"; accent: SightlineTheme.error; onClicked: root.report(root.nuc.tableOp(0, 4, "", "", 0), "Boot dead table cleared") }
            }

            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: SightlineTheme.cardBorder }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                FieldLabel { text: "Secondary:" }
                NameField { id: secondaryName; placeholderText: "second table (interpolate) or [NUC]_shutter_only" }
                FieldLabel { text: "Ratio " + ratioSlider.value.toFixed(0) }
                Slider { id: ratioSlider; from: 0; to: 255; stepSize: 1; value: 128; Layout.preferredWidth: 140 }
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                BlendButton {
                    id: interpBtn
                    text: "Load Interpolated"
                    enabled: root.caps.destripe === true
                    onClicked: root.report(root.nuc.tableOp(5, 0, tableName.text, secondaryName.text, ratioSlider.value),
                                           "Interpolated NUC loaded")
                }
                BlendButton {
                    id: flattenLoadBtn
                    text: "Load with Shutter Flatten"
                    enabled: root.caps.shutterSave === true
                    onClicked: root.report(root.nuc.tableOp(6, 0, tableName.text, secondaryName.text, 0),
                                           "NUC loaded with shutter flatten")
                }
                Text {
                    text: "Interpolation: FW 3.9 · Shutter-flatten tables: FW 3.11"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                }
            }
        }

        // ------------------------------------------------------------ DPR
        BlendCard {
            title: "DEAD PIXEL REPLACEMENT"
            subtitle: "0x35 run 9 / 10 / 11 · 0xA1 · 0xA8"
            accent: SightlineTheme.info

            Text {
                Layout.fillWidth: true
                text: "Limits default to the EAN §3.5 worked example (≈4σ of the gain/offset images); tune them per camera. Run a 2-point NUC first."
                color: SightlineTheme.textMuted
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 10
                rowSpacing: 6
                LimitSpin { id: minGain; label: "MIN GAIN %"; from: 0; to: 999; value: 76 }
                LimitSpin { id: maxGain; label: "MAX GAIN %"; from: 0; to: 999; value: 124 }
                LimitSpin { id: minVal; label: "MIN VALUE"; from: 0; to: 65535; value: 0 }
                LimitSpin { id: maxVal; label: "MAX VALUE"; from: 0; to: 65535; value: 65535 }
                LimitSpin { id: minOff; label: "MIN OFFSET"; from: -999999; to: 999999; value: -520 }
                LimitSpin { id: maxOff; label: "MAX OFFSET"; from: -999999; to: 999999; value: 520 }
                LimitSpin { id: maxStd; label: "MAX STD DEV"; from: 0; to: 65535; value: 65535 }
                LimitSpin { id: maxNum; label: "MAX # DEAD"; from: 0; to: 1000000; value: 0 }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8
                BlendButton {
                    id: calcDeadBtn
                    text: "Calculate Dead Pixels"
                    primary: true
                    accent: SightlineTheme.info
                    enabled: root.nuc && !root.nuc.busy
                    onClicked: root.report(root.nuc.calcDead({
                        minGain: minGain.value, maxGain: maxGain.value, minVal: minVal.value, maxVal: maxVal.value,
                        minOff: minOff.value, maxOff: maxOff.value, maxStdDev: maxStd.value, maxNumDead: maxNum.value
                    }), "Dead pixels calculated — statistics requested")
                }
                BlendButton {
                    id: calcRepBtn
                    text: "Recalc Replacement"
                    enabled: root.nuc && !root.nuc.busy
                    onClicked: root.report(root.nuc.calcReplace(), "Replacement pixels recalculated")
                }
                BlendButton {
                    id: autoDeadBtn
                    text: "Auto Detect (frame)"
                    enabled: root.caps.dpr === true && root.nuc && !root.nuc.busy
                    onClicked: root.report(root.nuc.autoDead(), "Automatic dead pixel detection run")
                }
                BlendButton {
                    id: dprDefaultsBtn
                    text: "Reset Limits"
                    onClicked: {
                        const d = root.nuc.dprDefaults()
                        minGain.value = d.minGain; maxGain.value = d.maxGain
                        minVal.value = d.minVal; maxVal.value = d.maxVal
                        minOff.value = d.minOff; maxOff.value = d.maxOff
                        maxStd.value = d.maxStdDev; maxNum.value = d.maxNumDead
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 8
                rowSpacing: 8
                StatCell { label: "GAIN LOW"; value: root.fmt(root.dead, "nGainLo") }
                StatCell { label: "GAIN HIGH"; value: root.fmt(root.dead, "nGainHi") }
                StatCell { label: "AVG LOW"; value: root.fmt(root.dead, "nAvgLo") }
                StatCell { label: "AVG HIGH"; value: root.fmt(root.dead, "nAvgHi") }
                StatCell { label: "OFFSET LOW"; value: root.fmt(root.dead, "nOffLo"); accent: SightlineTheme.accent }
                StatCell { label: "OFFSET HIGH"; value: root.fmt(root.dead, "nOffHi"); accent: SightlineTheme.accent }
                StatCell { label: "STD DEV HIGH"; value: root.fmt(root.dead, "nDevHi"); accent: SightlineTheme.accent }
                StatCell { label: "TOTAL DEAD"; value: root.fmt(root.dead, "nDead"); accent: SightlineTheme.success }
            }

            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: SightlineTheme.cardBorder }

            // Replacement method + dynamic filter (FW 3.3)
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                enabled: root.caps.dpr === true
                FieldLabel { text: "Replace:" }
                ComboBox {
                    id: repMethod
                    model: ["Nearest", "Average", "Median"]
                    currentIndex: root.nuc && root.nuc.board.deadReplace !== undefined ? root.nuc.board.deadReplace : 0
                    Layout.preferredWidth: 110
                }
                FieldLabel { text: "N:" }
                SpinBox { id: repN; from: 1; to: 8; value: root.nuc && root.nuc.board.numReplace !== undefined ? root.nuc.board.numReplace : 5; implicitHeight: 30 }
                FieldLabel { text: "Filter:" }
                ComboBox {
                    id: repFilter
                    readonly property var codes: [255, 0, 1, 2]
                    model: ["Keep current", "None", "Min/Max", "Near/Far"]
                    Layout.preferredWidth: 130
                }
                FieldLabel { text: "Thresh:" }
                SpinBox { id: repThresh; from: 0; to: 255; value: 64; editable: true; implicitHeight: 30 }
                Item { Layout.fillWidth: true }
                BlendButton {
                    id: repApplyBtn
                    text: "Apply"
                    onClicked: root.report(root.nuc.setReplace(repMethod.currentIndex, repN.value,
                                                               repFilter.codes[repFilter.currentIndex], repThresh.value),
                                           "Replacement settings applied")
                }
            }

            // Destripe (FW 3.9, needs board state)
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                enabled: root.caps.destripe === true
                FieldLabel { text: "Destripe " + destripeSlider.value.toFixed(0) }
                Slider {
                    id: destripeSlider
                    from: 0; to: 255; stepSize: 1
                    value: root.nuc && root.nuc.board.destripeAmount !== undefined ? root.nuc.board.destripeAmount : 0
                    Layout.fillWidth: true
                }
                FieldLabel { text: "Sections:" }
                SpinBox {
                    id: destripeSections
                    from: 1; to: 255
                    value: root.nuc && root.nuc.board.destripeSections !== undefined ? root.nuc.board.destripeSections : 1
                    implicitHeight: 30
                }
                BlendButton {
                    id: destripeBtn
                    text: "Apply"
                    enabled: root.nuc && root.nuc.hasBoardState
                    onClicked: root.report(root.nuc.setDestripe(destripeSlider.value, destripeSections.value), "Destripe applied")
                }
            }

            // Manual + dynamic dead pixel editing (0xA8, FW 3.3)
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                enabled: root.caps.dpr === true
                FieldLabel { text: "Pixel X:" }
                SpinBox { id: pixX; from: 0; to: 65535; editable: true; implicitHeight: 30 }
                FieldLabel { text: "Y:" }
                SpinBox { id: pixY; from: 0; to: 65535; editable: true; implicitHeight: 30 }
                BlendButton { id: addPixBtn; text: "+ Add"; onClicked: root.report(root.nuc.addDeadPixel(pixX.value, pixY.value), "Pixel added to dead list") }
                BlendButton {
                    id: removePixBtn
                    text: "− Remove"
                    accent: SightlineTheme.error
                    ToolTip.visible: hovered
                    ToolTip.text: "Only remove pixels that were added manually (EAN §5.3)"
                    onClicked: root.report(root.nuc.removeDeadPixel(pixX.value, pixY.value), "Pixel removed from dead list")
                }
                Item { Layout.fillWidth: true }
                FieldLabel { text: "Kernel:" }
                SpinBox { id: dynKernel; from: 0; to: 255; value: 0; implicitHeight: 30 }
                FieldLabel { text: "Max Δ:" }
                SpinBox { id: dynDiff; from: 0; to: 255; value: 0; implicitHeight: 30 }
                BlendButton { id: dynBtn; text: "Dynamic Detect"; onClicked: root.report(root.nuc.dynamicDead(dynKernel.value, dynDiff.value), "Dynamic detection run") }
            }
        }

        // ------------------------------------------------------------ noise + warnings
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            BlendCard {
                title: "3D NOISE STATISTICS"
                subtitle: "0xAF · grey levels (raw / 256)"
                accent: SightlineTheme.accent
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 1

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 8
                    rowSpacing: 8
                    StatCell { label: "σ T"; value: root.fmt(root.noise, "sigT", 2); accent: SightlineTheme.accent }
                    StatCell { label: "σ V"; value: root.fmt(root.noise, "sigV", 2); accent: SightlineTheme.accent }
                    StatCell { label: "σ H"; value: root.fmt(root.noise, "sigH", 2); accent: SightlineTheme.accent }
                    StatCell { label: "σ VH"; value: root.fmt(root.noise, "sigVh", 2); accent: SightlineTheme.accent }
                    StatCell { label: "σ TV"; value: root.fmt(root.noise, "sigTv", 2); accent: SightlineTheme.accent }
                    StatCell { label: "σ TH"; value: root.fmt(root.noise, "sigTh", 2); accent: SightlineTheme.accent }
                    StatCell { label: "σ TVH"; value: root.fmt(root.noise, "sigTvh", 2); accent: SightlineTheme.accent }
                    StatCell { label: "TEMPORAL"; value: root.fmt(root.noise, "noiseTemporal", 2); accent: SightlineTheme.success }
                }
            }

            BlendCard {
                title: "BOARD WARNINGS"
                subtitle: "0x86 User Warning"
                accent: SightlineTheme.textSecondary
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 1

                ListView {
                    id: warnList
                    Layout.fillWidth: true
                    implicitHeight: 96
                    clip: true
                    model: root.nuc ? root.nuc.warnings : []
                    delegate: Text {
                        width: warnList.width
                        text: "› " + modelData
                        color: index === 0 ? SightlineTheme.textPrimary : SightlineTheme.textSecondary
                        font.pixelSize: 11
                        font.family: "Monospace"
                        elide: Text.ElideRight
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: warnList.count === 0
                        text: "No warnings"
                        color: SightlineTheme.textMuted
                        font.pixelSize: 11
                    }
                }
                BlendButton {
                    id: clearWarnBtn
                    text: "Clear"
                    enabled: warnList.count > 0
                    onClicked: root.nuc.clearWarnings()
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
