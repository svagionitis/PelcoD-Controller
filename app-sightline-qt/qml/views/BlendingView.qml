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
    property int warpIndex: 0
    property int fixedIndex: 1
    property int blendMode: 1
    property int blendAmt: 128
    property int blendHue: 0
    property int blendFlags: 0
    property int hotStart: 0
    property int coldEnd: 0
    property int vertOffset: 0
    property int horizOffset: 0
    property int rotOffset: 0
    property int zoomScale: 128
    property int hzoomScale: 128
    property bool usePresetAlign: false
    property int presetAlignIndex: 0

    // Fine-tune alignment (0xB9)
    property int alignSlot: 0
    property int alignVertical: 0
    property int alignHorizontal: 0
    property int alignRotateDeg: 0
    property real alignZoomRatio: 1.0
    property real alignHzoomRatio: 1.0

    // Four-point alignment (0x95)
    property int fourAlignSlot: 0
    property var fourPoints: [
        { "leftCol": 0, "leftRow": 0, "rightCol": 0, "rightRow": 0 },
        { "leftCol": 640, "leftRow": 0, "rightCol": 640, "rightRow": 0 },
        { "leftCol": 640, "leftRow": 480, "rightCol": 640, "rightRow": 480 },
        { "leftCol": 0, "leftRow": 480, "rightCol": 0, "rightRow": 480 }
    ]

    // Telemetry feedback status
    property bool hasTelemetry: false
    property int telemUp: 0
    property int telemRight: 0
    property int telemDown: 0
    property int telemLeft: 0
    property int telemMode: 1
    property int telemAmt: 128

    readonly property var modeNames: [
        "No Change (0)",
        "Frame Blend: Warped EO + Fixed IR (1)",
        "Thermal False Color: Warped EO + Fixed IR (2)",
        "Night Blend: Warped EO + Fixed IR (3)",
        "Color Blend: Warped EO + Fixed IR (4)",
        "Reserved (5)",
        "Frame Blend: Fixed EO + Warped IR (6)",
        "Thermal False Color: Fixed EO + Warped IR (7)",
        "Night Blend: Fixed EO + Warped IR (8)",
        "Color Blend: Fixed EO + Warped IR (9)",
        "Color IR Blend: Fixed EO + Warped IR (10)",
        "Color IR Blend: Warped EO + Fixed IR (11)",
        "Dual Color Blend: EO + EO (12)"
    ]

    readonly property var modeValues: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]

    function modeToComboIndex(m) {
        for (var i = 0; i < modeValues.length; ++i) {
            if (modeValues[i] === m) return i;
        }
        return 1;
    }

    Connections {
        target: bridge

        function onCurrentBlendParamsReceived(params) {
            root.hasTelemetry = true;
            if (params.warpIndex !== undefined) root.warpIndex = params.warpIndex;
            if (params.fixedIndex !== undefined) root.fixedIndex = params.fixedIndex;
            if (params.mode !== undefined) {
                root.blendMode = params.mode;
                modeCombo.currentIndex = root.modeToComboIndex(params.mode);
            }
            if (params.amt !== undefined) {
                root.blendAmt = params.amt;
                amtSlider.value = params.amt;
            }
            if (params.hue !== undefined) {
                root.blendHue = params.hue;
                hueSlider.value = params.hue;
            }
            if (params.flags !== undefined) {
                root.blendFlags = params.flags;
                hueFlagCheck.checked = (params.flags & 0x02) !== 0;
                histEqCheck.checked = (params.flags & 0x01) !== 0;
            }
            if (params.hotStart !== undefined) {
                root.hotStart = params.hotStart;
                hotSlider.value = params.hotStart;
            }
            if (params.coldEnd !== undefined) {
                root.coldEnd = params.coldEnd;
                coldSlider.value = params.coldEnd;
            }
            if (params.up !== undefined) root.telemUp = params.up;
            if (params.right !== undefined) root.telemRight = params.right;
            if (params.down !== undefined) root.telemDown = params.down;
            if (params.left !== undefined) root.telemLeft = params.left;
            if (params.mode !== undefined) root.telemMode = params.mode;
            if (params.amt !== undefined) root.telemAmt = params.amt;
        }

        function onBlendParametersReceived(params) {
            root.hasTelemetry = true;
            if (params.mode !== undefined) {
                root.blendMode = params.mode;
                modeCombo.currentIndex = root.modeToComboIndex(params.mode);
            }
            if (params.amt !== undefined) {
                root.blendAmt = params.amt;
                amtSlider.value = params.amt;
            }
        }

        function onBlendAlignReceived(align) {
            if (align.index !== undefined) root.alignSlot = align.index;
            if (align.vertical !== undefined) {
                root.alignVertical = align.vertical;
                vertSpin.value = align.vertical;
            }
            if (align.horizontal !== undefined) {
                root.alignHorizontal = align.horizontal;
                horizSpin.value = align.horizontal;
            }
            if (align.rotate !== undefined) {
                root.alignRotateDeg = Math.round(align.rotate / 128.0);
                rotSpin.value = root.alignRotateDeg;
            }
            if (align.zoom !== undefined) {
                root.alignZoomRatio = align.zoom / 4096.0;
                zoomSlider.value = align.zoom / 4096.0;
            }
            if (align.hzoom !== undefined) {
                root.alignHzoomRatio = align.hzoom / 4096.0;
                hzoomSlider.value = align.hzoom / 4096.0;
            }
        }

        function onFourAlignPointsReceived(index, points) {
            root.fourAlignSlot = index;
            if (points && points.length >= 4) {
                root.fourPoints = points;
            }
        }
    }

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        spacing: 16

        Item { Layout.preferredHeight: 4 }

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle { width: 4; height: 22; color: SightlineTheme.primary; radius: 2 }

            Text {
                text: "DUAL-SENSOR FUSION & MULTI-CAMERA REGISTRATION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }

            Text {
                text: "// IDD 3.11 Modules: 0x2F / 0x4D / 0xB9 / 0x95"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                height: 24
                radius: 4
                color: root.hasTelemetry ? "#122a1f" : "#2a1f12"
                border.color: root.hasTelemetry ? SightlineTheme.success : SightlineTheme.warning
                border.width: 1
                implicitWidth: statusText.implicitWidth + 16

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        width: 6; height: 6; radius: 3
                        color: root.hasTelemetry ? SightlineTheme.success : SightlineTheme.warning
                    }
                    Text {
                        id: statusText
                        text: root.hasTelemetry ? "HARDWARE TELEMETRY ACTIVE (0x4D)" : "LOCAL CACHE (WAITING SYNC)"
                        color: root.hasTelemetry ? SightlineTheme.success : SightlineTheme.warning
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }

            Button {
                text: "Refresh (0x30 / 0xB9)"
                Layout.preferredHeight: 26
                contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.pixelSize: 11; font.bold: true }
                background: Rectangle { color: SightlineTheme.surfaceCard; radius: 4; border.color: SightlineTheme.cardBorder; border.width: 1 }
                onClicked: {
                    bridge.getBlendParameters();
                    bridge.getBlendAlign(root.alignSlot);
                    bridge.getFourAlignPoints(root.fourAlignSlot);
                }
            }
        }

        // Metrics Summary Row
        Flow {
            Layout.fillWidth: true
            spacing: 12

            MetricCard {
                title: "Warped Camera"
                value: "Cam " + root.warpIndex + " (Warp)"
                accentColor: SightlineTheme.primary
                iconText: "🎥"
            }

            MetricCard {
                title: "Fixed Reference"
                value: "Cam " + root.fixedIndex + " (Fixed)"
                accentColor: SightlineTheme.warning
                iconText: "📐"
            }

            MetricCard {
                title: "Active Algorithm"
                value: "Mode " + root.blendMode
                accentColor: SightlineTheme.info
                iconText: "✨"
            }

            MetricCard {
                title: "Blend Mix (Alpha)"
                value: Math.round(root.blendAmt / 2.55) + "% (" + root.blendAmt + ")"
                accentColor: SightlineTheme.success
                iconText: "⚖️"
            }

            MetricCard {
                title: "Preset Align Slot"
                value: "Slot " + root.alignSlot
                accentColor: SightlineTheme.accent
                iconText: "🎯"
            }
        }

        // Card 1: Sensor Fusion & Blending Parameters (0x2F)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: card1Col.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: card1Col
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "MULTI-SENSOR VIDEO BLENDING CONFIGURATION (MESSAGE 0x2F)"
                        color: SightlineTheme.primary
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Conforms to EAN-Blending & IDD v3.11"
                        color: SightlineTheme.textMuted
                        font.pixelSize: 10
                    }
                }

                // Sensor Pairing
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text { text: "Sensor Routing:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }

                    Text { text: "Warp Camera (Index):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    ComboBox {
                        id: warpCombo
                        model: ["Cam 0", "Cam 1", "Cam 2", "Cam 3"]
                        currentIndex: root.warpIndex
                        Layout.preferredWidth: 110
                        onActivated: root.warpIndex = currentIndex
                    }

                    Text { text: "Fixed Camera (Index):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 12 }
                    ComboBox {
                        id: fixedCombo
                        model: ["Cam 0", "Cam 1", "Cam 2", "Cam 3"]
                        currentIndex: root.fixedIndex
                        Layout.preferredWidth: 110
                        onActivated: root.fixedIndex = currentIndex
                    }

                    Item { Layout.fillWidth: true }
                }

                // Fusion Algorithm
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text { text: "Fusion Algorithm:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }

                    ComboBox {
                        id: modeCombo
                        model: root.modeNames
                        currentIndex: root.modeToComboIndex(root.blendMode)
                        Layout.preferredWidth: 380
                        onActivated: root.blendMode = root.modeValues[currentIndex]
                    }

                    Item { Layout.fillWidth: true }
                }

                // Alpha Mix Slider
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text { text: "Alpha Mix (0..255):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }

                    Slider {
                        id: amtSlider
                        from: 0
                        to: 255
                        value: root.blendAmt
                        stepSize: 1
                        Layout.preferredWidth: 260
                        onMoved: root.blendAmt = Math.round(value)
                    }

                    Text {
                        text: Math.round(amtSlider.value) + " (" + Math.round(amtSlider.value / 2.55) + "% Fixed Sensor)"
                        color: SightlineTheme.textPrimary
                        font.bold: true
                        font.pixelSize: 12
                    }

                    Item { Layout.fillWidth: true }
                }

                // Hue Rotation Slider
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text { text: "Color Hue (0..255):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }

                    Slider {
                        id: hueSlider
                        from: 0
                        to: 255
                        value: root.blendHue
                        stepSize: 1
                        Layout.preferredWidth: 260
                        onMoved: root.blendHue = Math.round(value)
                    }

                    Text {
                        text: Math.round(hueSlider.value) + " / 255"
                        color: SightlineTheme.textPrimary
                        font.bold: true
                        font.pixelSize: 12
                    }

                    Item { Layout.fillWidth: true }
                }

                // Flags & Options
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 20

                    Text { text: "Feature Flags:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }

                    CheckBox {
                        id: hueFlagCheck
                        text: "Use Hue For Color (Bit 1)"
                        checked: (root.blendFlags & 0x02) !== 0
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.pixelSize: 11; leftPadding: 22 }
                    }

                    CheckBox {
                        id: histEqCheck
                        text: "IR Histogram Equalization (Bit 0)"
                        checked: (root.blendFlags & 0x01) !== 0
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.pixelSize: 11; leftPadding: 22 }
                    }

                    CheckBox {
                        id: presetCheck
                        text: "Enable Alignment Preset"
                        checked: root.usePresetAlign
                        onToggled: root.usePresetAlign = checked
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.pixelSize: 11; leftPadding: 22 }
                    }

                    Item { Layout.fillWidth: true }
                }

                // Thermal False-Color Thresholds (Hot Start / Cold End)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text { text: "Thermal Thresholds:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }

                    Text { text: "Hot Start:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    Slider {
                        id: hotSlider
                        from: 0; to: 255; value: root.hotStart; stepSize: 1
                        Layout.preferredWidth: 140
                        onMoved: root.hotStart = Math.round(value)
                    }
                    Text { text: Math.round(hotSlider.value); color: SightlineTheme.warning; font.bold: true; font.pixelSize: 11 }

                    Text { text: "Cold End:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 12 }
                    Slider {
                        id: coldSlider
                        from: 0; to: 255; value: root.coldEnd; stepSize: 1
                        Layout.preferredWidth: 140
                        onMoved: root.coldEnd = Math.round(value)
                    }
                    Text { text: Math.round(coldSlider.value); color: SightlineTheme.info; font.bold: true; font.pixelSize: 11 }

                    Item { Layout.fillWidth: true }
                }

                // Apply Action Buttons
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 6

                    Button {
                        text: "Apply Blending Parameters (0x2F)"
                        Layout.preferredWidth: 240
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            var flags = (histEqCheck.checked ? 0x01 : 0) | (hueFlagCheck.checked ? 0x02 : 0);
                            bridge.setBlendParameters(
                                warpCombo.currentIndex,
                                fixedCombo.currentIndex,
                                root.blendMode,
                                Math.round(amtSlider.value),
                                Math.round(hueSlider.value),
                                flags,
                                Math.round(hotSlider.value),
                                Math.round(coldSlider.value)
                            );
                        }
                    }

                    Button {
                        text: "Query Active Blend (0x30)"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.surfaceHeader; radius: 4; border.color: SightlineTheme.cardBorder; border.width: 1 }
                        onClicked: bridge.getBlendParameters()
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Card 2: Fine-Tune Alignment & Registration Offsets (0xB9)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: card2Col.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: card2Col
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "FINE-TUNE REGISTRATION & HOMOGRAPHY OFFSETS (MESSAGE 0xB9)"
                        color: SightlineTheme.warning
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Slots 0..4 (11-Byte Payload)"
                        color: SightlineTheme.textMuted
                        font.pixelSize: 10
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text { text: "Alignment Preset Slot:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }

                    ComboBox {
                        id: slotCombo
                        model: ["Slot 0", "Slot 1", "Slot 2", "Slot 3", "Slot 4"]
                        currentIndex: root.alignSlot
                        Layout.preferredWidth: 120
                        onActivated: {
                            root.alignSlot = currentIndex;
                            bridge.getBlendAlign(currentIndex);
                        }
                    }

                    Button {
                        text: "Load Slot (Query 0xB9)"
                        Layout.preferredHeight: 28
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.pixelSize: 11; font.bold: true }
                        background: Rectangle { color: SightlineTheme.surfaceHeader; radius: 4; border.color: SightlineTheme.cardBorder; border.width: 1 }
                        onClicked: bridge.getBlendAlign(slotCombo.currentIndex)
                    }

                    Item { Layout.fillWidth: true }
                }

                // Vertical & Horizontal Offsets
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text { text: "Pixel Offsets (V / H):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }

                    Text { text: "Vertical (px):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    SpinBox {
                        id: vertSpin
                        from: -1000; to: 1000; value: root.alignVertical
                        stepSize: 1
                        editable: true
                        Layout.preferredWidth: 110
                    }

                    Text { text: "Horizontal (px):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 12 }
                    SpinBox {
                        id: horizSpin
                        from: -1000; to: 1000; value: root.alignHorizontal
                        stepSize: 1
                        editable: true
                        Layout.preferredWidth: 110
                    }

                    Item { Layout.fillWidth: true }
                }

                // Rotation and Scaling
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text { text: "Rotation & Zoom:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }

                    Text { text: "Rotation (deg):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    SpinBox {
                        id: rotSpin
                        from: -180; to: 180; value: root.alignRotateDeg
                        stepSize: 1
                        editable: true
                        Layout.preferredWidth: 100
                    }

                    Text { text: "V-Zoom:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                    Slider {
                        id: zoomSlider
                        from: 0.5; to: 3.0; value: root.alignZoomRatio; stepSize: 0.05
                        Layout.preferredWidth: 120
                    }
                    Text { text: zoomSlider.value.toFixed(2) + "x"; color: SightlineTheme.textPrimary; font.pixelSize: 11; font.bold: true }

                    Text { text: "H-Zoom:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                    Slider {
                        id: hzoomSlider
                        from: 0.5; to: 3.0; value: root.alignHzoomRatio; stepSize: 0.05
                        Layout.preferredWidth: 120
                    }
                    Text { text: hzoomSlider.value.toFixed(2) + "x"; color: SightlineTheme.textPrimary; font.pixelSize: 11; font.bold: true }

                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Alignment Offsets (0xB9)"
                        Layout.preferredWidth: 220
                        Layout.preferredHeight: 30
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.warning; radius: 4 }
                        onClicked: {
                            var rotRaw = Math.round(rotSpin.value * 128);
                            var zoomRaw = Math.round(zoomSlider.value * 4096);
                            var hzoomRaw = Math.round(hzoomSlider.value * 4096);
                            bridge.setBlendAlign(
                                slotCombo.currentIndex,
                                vertSpin.value,
                                horizSpin.value,
                                rotRaw,
                                zoomRaw,
                                hzoomRaw
                            );
                        }
                    }

                    Button {
                        text: "Reset Slot to Zero"
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 30
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.surfaceHeader; radius: 4; border.color: SightlineTheme.cardBorder; border.width: 1 }
                        onClicked: {
                            vertSpin.value = 0;
                            horizSpin.value = 0;
                            rotSpin.value = 0;
                            zoomSlider.value = 1.0;
                            hzoomSlider.value = 1.0;
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Card 3: Live Hardware Telemetry Monitor (0x4D)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: card3Col.implicitHeight + 24
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: card3Col
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "INBOUND TELEMETRY AUDIT LOG (MESSAGE 0x4D / 19-BYTE WIRE FORMAT)"
                        color: SightlineTheme.info
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.hasTelemetry ? "Synchronized" : "Listening..."
                        color: root.hasTelemetry ? SightlineTheme.success : SightlineTheme.textMuted
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 24

                    ColumnLayout {
                        spacing: 2
                        Text { text: "Directional Offsets (0x4D Wire):"; color: SightlineTheme.textMuted; font.pixelSize: 10 }
                        Text {
                            text: "Up: " + root.telemUp + " px | Right: " + root.telemRight + " px | Down: " + root.telemDown + " px | Left: " + root.telemLeft + " px"
                            color: SightlineTheme.textPrimary; font.pixelSize: 12; font.family: "Monospace"; font.bold: true
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        Text { text: "Calculated Shift (Vertical / Horizontal):"; color: SightlineTheme.textMuted; font.pixelSize: 10 }
                        Text {
                            text: "V: " + (root.telemUp - root.telemDown) + " px | H: " + (root.telemRight - root.telemLeft) + " px"
                            color: SightlineTheme.accent; font.pixelSize: 12; font.family: "Monospace"; font.bold: true
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        Text { text: "Telemetry Mode / Mix:"; color: SightlineTheme.textMuted; font.pixelSize: 10 }
                        Text {
                            text: "Mode " + root.telemMode + " | " + root.telemAmt + " / 255"
                            color: SightlineTheme.textPrimary; font.pixelSize: 12; font.family: "Monospace"; font.bold: true
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
