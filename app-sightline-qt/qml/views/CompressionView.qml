import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
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

    // Internal state cache for stream settings
    property int selectedStream: 0
    property bool isRtpViolation: bridge ? (!bridge.isValidPort(protocolCombo.currentValue, parseInt(portInput.text) || 0)) : false

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        // Header Title
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { width: 4; height: 20; color: SightlineTheme.primary; radius: 2 }
            Text {
                text: "H.264 / H.265 ENCODING & ETHERNET STREAMING"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// EAN-Encoding Manual Modules 0x23, 0x29, 0x1A, 0x90"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
            Item { Layout.fillWidth: true }

            Button {
                text: "Refresh Settings"
                Layout.preferredHeight: 28
                background: Rectangle {
                    color: parent.hovered ? SightlineTheme.surfaceLight : SightlineTheme.surfaceCard
                    radius: 4
                    border.color: SightlineTheme.cardBorder
                }
                contentItem: Text {
                    text: parent.text
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (bridge) {
                        bridge.queryEncoderParams(streamChannelCombo.currentIndex);
                        bridge.queryDisplayParams(streamChannelCombo.currentIndex);
                    }
                }
            }
        }

        // Live Telemetry Metric Cards
        Flow {
            Layout.fillWidth: true
            spacing: 10

            MetricCard {
                title: "Target Bitrate"
                value: bridge ? (bridge.encBitrateKbps >= 1000 ? (bridge.encBitrateKbps / 1000.0).toFixed(1) + " Mbps" : bridge.encBitrateKbps + " kbps") : "4.0 Mbps"
                unit: ""
                accentColor: SightlineTheme.info
                iconText: "📶"
            }
            MetricCard {
                title: "GOP Interval"
                value: bridge ? (bridge.encGopInterval === 0 ? "Intra-Refresh" : bridge.encGopInterval.toString()) : "30"
                unit: bridge && bridge.encGopInterval > 0 ? "frames" : ""
                accentColor: SightlineTheme.warning
                iconText: "⏱️"
            }
            MetricCard {
                title: "Transport Port"
                value: bridge ? (bridge.netDisplayPort > 0 ? ":" + bridge.netDisplayPort : ":15004") : ":15004"
                unit: isRtpViolation ? "⚠️ PARITY ERR" : "OK"
                accentColor: isRtpViolation ? SightlineTheme.error : SightlineTheme.success
                iconText: "📡"
            }
            MetricCard {
                title: "Traffic Shaping"
                value: bridge ? (bridge.tcRateKbps > 0 ? bridge.tcRateKbps + " kbps" : "UNTHROTTLED") : "UNTHROTTLED"
                unit: ""
                accentColor: bridge && bridge.tcRateKbps > 0 ? SightlineTheme.accent : SightlineTheme.textMuted
                iconText: "🚦"
            }
        }

        // Tactical 1-Click Profile Banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: bannerRow.implicitHeight + 16
            color: Qt.rgba(0.0, 0.9, 1.0, 0.08)
            radius: SightlineTheme.radiusMedium
            border.color: Qt.rgba(0.0, 0.9, 1.0, 0.3)
            border.width: 1

            RowLayout {
                id: bannerRow
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                Text {
                    text: "⚡"
                    font.pixelSize: 20
                }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "TACTICAL PRESETS & BANDWIDTH OPTIMIZATION (EAN Section 8)"
                        color: SightlineTheme.primary
                        font.bold: true
                        font.pixelSize: 11
                    }
                    Text {
                        text: "Instant 1-click profiles optimized for contested RF links (100 kbps Constrained CBR) or tactical high-def reconnaissance (8.0 Mbps)."
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "🚀 Apply Low Bandwidth (100 kbps)"
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#00c4d6" : SightlineTheme.primary
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#0e1014"
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (bridge) {
                            bridge.applyLowBandwidth(streamChannelCombo.currentIndex);
                            bitrateSlider.value = 100;
                            gopSlider.value = 30;
                            rateCtrlCombo.currentIndex = 2; // Constrained CBR
                            profileCombo.currentIndex = 1; // Main
                            frameStepCombo.currentIndex = 1; // 1/2 rate (15 fps)
                            resolutionCombo.currentIndex = 1; // 720p
                        }
                    }
                }

                Button {
                    text: "🎬 Apply Broadcast HD (8 Mbps)"
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? SightlineTheme.surfaceLight : SightlineTheme.surfaceCard
                        radius: 4
                        border.color: SightlineTheme.primary
                    }
                    contentItem: Text {
                        text: parent.text
                        color: SightlineTheme.primary
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (bridge) {
                            bitrateSlider.value = 8000;
                            gopSlider.value = 30;
                            rateCtrlCombo.currentIndex = 1; // VBR
                            profileCombo.currentIndex = 2; // High
                            frameStepCombo.currentIndex = 0; // Full rate (30 fps)
                            resolutionCombo.currentIndex = 0; // Native
                        }
                    }
                }
            }
        }

        // Channel Selector Bar
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: chanRow.implicitHeight + 14
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            RowLayout {
                id: chanRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 12

                Text {
                    text: "ACTIVE NETWORK STREAM:"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 11
                    font.bold: true
                    Layout.alignment: Qt.AlignVCenter
                }

                ComboBox {
                    id: streamChannelCombo
                    model: [
                        "Stream 0 — Net0 (0x0002) Primary Output",
                        "Stream 1 — Net1 (0x0080) Secondary Output",
                        "Stream 2 — Net2 (0x0200) Tertiary Output (4100/4110)"
                    ]
                    Layout.preferredWidth: 360
                    Layout.preferredHeight: 30
                    currentIndex: root.selectedStream
                    onCurrentIndexChanged: {
                        root.selectedStream = currentIndex;
                        if (bridge) {
                            bridge.queryEncoderParams(currentIndex);
                            bridge.queryDisplayParams(currentIndex);
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "Hardware Multi-Channel Active"
                    color: SightlineTheme.success
                    font.pixelSize: 11
                    font.family: "Monospace"
                }
            }
        }

        // Section: Video Encoder Configuration Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: encCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: encCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "VIDEO COMPRESSION & RATE CONTROL ENGINE"
                        color: SightlineTheme.primary
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Message ID 0x23 / 0x56"
                        color: SightlineTheme.textMuted
                        font.pixelSize: 10
                        font.family: "Monospace"
                    }
                }

                // Bitrate Slider and Spinbox
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        text: "Target Bitrate:"
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 12
                        Layout.preferredWidth: 160
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Slider {
                        id: bitrateSlider
                        from: 100
                        to: 30000
                        value: 4000
                        stepSize: 100
                        Layout.preferredWidth: 260
                    }
                    TextField {
                        id: bitrateInput
                        text: Math.round(bitrateSlider.value).toString()
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                        }
                        onEditingFinished: {
                            var val = parseInt(text) || 100;
                            val = Math.max(100, Math.min(30000, val));
                            bitrateSlider.value = val;
                        }
                    }
                    Text {
                        text: "kbps (" + (bitrateSlider.value / 1000.0).toFixed(2) + " Mbps)"
                        color: SightlineTheme.textPrimary
                        font.bold: true
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                }

                // Bitrate Quick Selector Chips
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Item { Layout.preferredWidth: 160 }
                    Repeater {
                        model: [
                            { label: "100k (Tactical)", val: 100 },
                            { label: "500k", val: 500 },
                            { label: "1.5M", val: 1500 },
                            { label: "4.0M (Standard)", val: 4000 },
                            { label: "8.0M (HD)", val: 8000 },
                            { label: "15M", val: 15000 },
                            { label: "25M", val: 25000 }
                        ]
                        Button {
                            text: modelData.label
                            Layout.preferredHeight: 24
                            background: Rectangle {
                                color: bitrateSlider.value === modelData.val ? SightlineTheme.primary : SightlineTheme.surfaceLight
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: bitrateSlider.value === modelData.val ? "#0e1014" : SightlineTheme.textSecondary
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: { bitrateSlider.value = modelData.val; }
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                // Profile and Rate Control
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Profile & Rate Control:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    ComboBox {
                        id: profileCombo
                        model: ["H.264 Baseline Profile", "H.264 Main Profile", "H.264 High Profile"]
                        currentIndex: 2 // High profile default
                        Layout.preferredWidth: 200
                    }

                    ComboBox {
                        id: rateCtrlCombo
                        model: ["Legacy Constant (0)", "Variable Bitrate / VBR (1)", "Constrained CBR (2)", "Balanced (3)"]
                        currentIndex: 0
                        Layout.preferredWidth: 220
                    }

                    Item { Layout.fillWidth: true }
                }

                // GOP Interval / Intra-Refresh
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "GOP / Intra-Frame:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    Slider {
                        id: gopSlider
                        from: 0
                        to: 255
                        value: 30
                        stepSize: 1
                        Layout.preferredWidth: 260
                    }
                    Text {
                        text: gopSlider.value === 0 ? "0 (Intra-Refresh Mode)" : Math.round(gopSlider.value) + " frames"
                        color: gopSlider.value === 0 ? SightlineTheme.warning : SightlineTheme.textPrimary
                        font.bold: true
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                }

                // Quantization Range (Min QP / Max QP)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Quantization Range:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    Text { text: "Min QP:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    SpinBox {
                        id: minQpSpin
                        from: 0
                        to: 30
                        value: 0
                        Layout.preferredWidth: 90
                    }

                    Text { text: "Max QP:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                    SpinBox {
                        id: maxQpSpin
                        from: 0
                        to: 51
                        value: 28
                        Layout.preferredWidth: 90
                    }

                    Text {
                        text: "(0 = Hardware Auto Rate Adaptation)"
                        color: SightlineTheme.textMuted
                        font.pixelSize: 11
                    }
                    Item { Layout.fillWidth: true }
                }

                // Advanced Intra-Refresh: AIR MB and Slice Rows
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Intra-Refresh & Slicing:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    Text { text: "AIR MBs:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    SpinBox {
                        id: airMbSpin
                        from: 0
                        to: 255
                        value: 0
                        Layout.preferredWidth: 90
                    }

                    Text { text: "Slice Rows:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                    SpinBox {
                        id: sliceRowsSpin
                        from: 0
                        to: 255
                        value: 0
                        Layout.preferredWidth: 90
                    }

                    Text { text: "Deblocking:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                    ComboBox {
                        id: deblockCombo
                        model: ["Enabled", "Disabled", "No Slice Boundaries"]
                        Layout.preferredWidth: 160
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Section: Ethernet Display & Destination Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: dispCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: dispCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "ETHERNET STREAM DESTINATION & RFC 3550 RTP TRANSPORT"
                        color: SightlineTheme.primary
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Message ID 0x29 / 0x51"
                        color: SightlineTheme.textMuted
                        font.pixelSize: 10
                        font.family: "Monospace"
                    }
                }

                // Protocol Dropdown
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Streaming Protocol:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    ComboBox {
                        id: protocolCombo
                        textRole: "text"
                        valueRole: "val"
                        model: [
                            { text: "MPEG2-TS H.264 (Standard UDP)", val: 1 },
                            { text: "MPEG2-TS H.265 (HEVC High-Efficiency)", val: 8 },
                            { text: "RTP H.264 (RFC 6184 Low-Latency)", val: 5 },
                            { text: "RTP H.265 (RFC 7798 Low-Latency)", val: 9 },
                            { text: "RTP encapsulated MPEG2-TS H.264", val: 6 },
                            { text: "RTP encapsulated MPEG2-TS H.265", val: 10 },
                            { text: "KLV Metadata Only (No Video Payload)", val: 7 },
                            { text: "MJPEG (Legacy Motion JPEG)", val: 2 },
                            { text: "Raw Uncompressed Frame Video", val: 4 }
                        ]
                        currentIndex: 0
                        Layout.preferredWidth: 340
                    }

                    Item { Layout.fillWidth: true }
                }

                // Destination IP
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Destination IPv4:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    TextField {
                        id: ipInput
                        text: bridge ? bridge.host : "127.0.0.1"
                        Layout.preferredWidth: 160
                        color: SightlineTheme.textPrimary
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                        }
                    }

                    Button {
                        text: "Use My IP"
                        Layout.preferredHeight: 28
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 3 }
                        contentItem: Text { text: parent.text; color: SightlineTheme.textSecondary; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: { if (bridge) ipInput.text = bridge.host; }
                    }

                    Button {
                        text: "Use Multicast"
                        Layout.preferredHeight: 28
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 3 }
                        contentItem: Text { text: parent.text; color: SightlineTheme.textSecondary; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: { ipInput.text = "224.10.10.10"; }
                    }

                    Item { Layout.fillWidth: true }
                }

                // Destination Port & RFC 3550 Validation
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Destination Port:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    TextField {
                        id: portInput
                        text: "15004"
                        Layout.preferredWidth: 90
                        color: SightlineTheme.textPrimary
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: isRtpViolation ? SightlineTheme.error : (parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder)
                        }
                    }

                    Text {
                        text: isRtpViolation ? "❌ Port MUST be even for RTP (RFC 3550 Section 11)" : "✓ Compliant port"
                        color: isRtpViolation ? SightlineTheme.error : SightlineTheme.success
                        font.pixelSize: 11
                        font.bold: isRtpViolation
                    }

                    Button {
                        visible: isRtpViolation
                        text: "Auto-Fix to " + (parseInt(portInput.text) > 0 ? (parseInt(portInput.text) - 1) : 15004)
                        Layout.preferredHeight: 26
                        background: Rectangle { color: SightlineTheme.error; radius: 3 }
                        contentItem: Text { text: parent.text; color: "#ffffff"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: {
                            var p = parseInt(portInput.text) || 15005;
                            if (p % 2 !== 0) p -= 1;
                            portInput.text = p.toString();
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // RFC 3550 Compliance Warning Banner
                Rectangle {
                    visible: isRtpViolation
                    Layout.fillWidth: true
                    implicitHeight: 36
                    color: Qt.rgba(1.0, 0.1, 0.2, 0.15)
                    radius: 4
                    border.color: SightlineTheme.error
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8
                        Text { text: "⚠️"; font.pixelSize: 14 }
                        Text {
                            text: "RFC 3550 Standard Violation: RTP video destination ports MUST be even numbers. Odd port numbers are reserved exclusively for RTCP transmission."
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                    }
                }

                // Packet Sizes (MTU)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Network Packet Sizes:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    Text { text: "TS Max Packet:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    SpinBox {
                        id: maxPacketSpin
                        from: 512
                        to: 1500
                        value: 1400
                        stepSize: 188
                        Layout.preferredWidth: 110
                    }

                    Text { text: "Raw Max Packet:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                    SpinBox {
                        id: maxRawPacketSpin
                        from: 0
                        to: 1500
                        value: 1400
                        Layout.preferredWidth: 110
                    }

                    Text {
                        text: "(Standard MTU 1400 avoids network fragmentation)"
                        color: SightlineTheme.textMuted
                        font.pixelSize: 11
                    }
                    Item { Layout.fillWidth: true }
                }

                // Section 3.4 Advanced Flags: Remove TS Encapsulation & RTP Aggregate Packets
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    Text { text: "Advanced Framing:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    CheckBox {
                        id: removeTsCheck
                        text: "Remove TS Encapsulation"
                        checked: false
                    }

                    CheckBox {
                        id: rtpAggregateCheck
                        text: "RTP Aggregate Packets (Multi-Packet RTP)"
                        checked: false
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Section: Downsampling & Frame Rate (Ethernet Video)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: vidCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: vidCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "FRAME DECIMATION & DOWNSAMPLING (ETHERNET VIDEO)"
                        color: SightlineTheme.primary
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Message ID 0x1A"
                        color: SightlineTheme.textMuted
                        font.pixelSize: 10
                        font.family: "Monospace"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Frame Rate Decimation:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    ComboBox {
                        id: frameStepCombo
                        model: [
                            "Step 1 — Full Rate (30 fps / 60 fps)",
                            "Step 2 — Half Rate (15 fps / 30 fps)",
                            "Step 3 — 1/3 Rate (10 fps / 20 fps)",
                            "Step 4 — 1/4 Rate (7.5 fps / 15 fps)",
                            "Step 6 — 1/6 Rate (5 fps / 10 fps)"
                        ]
                        currentIndex: 0
                        Layout.preferredWidth: 280
                    }

                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Output Resolution:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    ComboBox {
                        id: resolutionCombo
                        model: [
                            "0 — Native Input Resolution (Out=In)",
                            "1 — 720p HD (1280 x 720)",
                            "2 — 480p SD (640 x 480)",
                            "3 — 240p Low (320 x 240)",
                            "5 — Downsample 2:1 Scale",
                            "4 — Custom (e.g. 1920x1096 for accurate VLC frame rate)"
                        ]
                        currentIndex: 0
                        Layout.preferredWidth: 360
                    }

                    Item { Layout.fillWidth: true }
                }

                // Custom Resolution Row (when Custom is selected)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: resolutionCombo.currentIndex === 5
                    Text { text: "Custom Dimensions:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    Text { text: "Width (px):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    TextField {
                        id: customWidthField
                        text: "1920"
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                    }

                    Text { text: "Height (px):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                    TextField {
                        id: customHeightField
                        text: "1096"
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                    }

                    Text {
                        text: "(EAN Section 4.1: 1920x1096 resolves VLC 720p60 frame rate reporting issue)"
                        color: SightlineTheme.info
                        font.pixelSize: 11
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Action Buttons Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Layout.topMargin: 4

            Button {
                text: "Apply Encoder & Destination"
                Layout.preferredWidth: 220
                Layout.preferredHeight: 36
                enabled: !root.isRtpViolation
                background: Rectangle {
                    color: root.isRtpViolation ? SightlineTheme.surfaceLight : (parent.hovered ? "#33ebff" : SightlineTheme.primary)
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: root.isRtpViolation ? SightlineTheme.textMuted : "#0e1014"
                    font.bold: true
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (bridge) {
                        const streamIdx = streamChannelCombo.currentIndex;
                        bridge.setH264ParamsEx(
                            streamIdx,
                            Math.round(bitrateSlider.value),
                            Math.round(gopSlider.value),
                            profileCombo.currentIndex,
                            rateCtrlCombo.currentIndex,
                            minQpSpin.value,
                            maxQpSpin.value,
                            deblockCombo.currentIndex,
                            airMbSpin.value,
                            sliceRowsSpin.value
                        );

                        bridge.setEthernetDisplay(
                            streamIdx,
                            protocolCombo.currentValue,
                            ipInput.text,
                            parseInt(portInput.text) || 15004,
                            maxPacketSpin.value,
                            maxRawPacketSpin.value
                        );

                        const stepMap = [1, 2, 3, 4, 6];
                        const step = stepMap[frameStepCombo.currentIndex] || 1;
                        const resMap = [0, 1, 2, 3, 5, 4];
                        const res = resMap[resolutionCombo.currentIndex] || 0;
                        const custW = (res === 4) ? (parseInt(customWidthField.text) || 1920) : 0;
                        const custH = (res === 4) ? (parseInt(customHeightField.text) || 1096) : 0;
                        bridge.setEthernetVideo(streamIdx, step, res, custW, custH, 80, 0);
                    }
                }
            }

            Button {
                text: "Start Stream"
                Layout.preferredWidth: 140
                Layout.preferredHeight: 36
                background: Rectangle {
                    color: parent.hovered ? "#33f08c" : SightlineTheme.success
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "#0e1014"
                    font.bold: true
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (bridge) bridge.streamingControl(streamChannelCombo.currentIndex, 1);
                }
            }

            Button {
                text: "Stop Stream"
                Layout.preferredWidth: 140
                Layout.preferredHeight: 36
                background: Rectangle {
                    color: parent.hovered ? "#ff4064" : SightlineTheme.error
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "#ffffff"
                    font.bold: true
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (bridge) bridge.streamingControl(streamChannelCombo.currentIndex, 0);
                }
            }

            Button {
                text: "Export SDP File..."
                Layout.preferredWidth: 150
                Layout.preferredHeight: 36
                background: Rectangle {
                    color: parent.hovered ? SightlineTheme.surfaceLight : SightlineTheme.surfaceCard
                    radius: 4
                    border.color: SightlineTheme.cardBorder
                }
                contentItem: Text {
                    text: parent.text
                    color: SightlineTheme.textPrimary
                    font.bold: true
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    saveSdpDialog.open();
                }
            }

            Item { Layout.fillWidth: true }
        }

        FileDialog {
            id: saveSdpDialog
            title: "Export Stream SDP File (EAN-RTSP Section 7)"
            fileMode: FileDialog.SaveFile
            nameFilters: ["SDP files (*.sdp)", "All files (*)"]
            defaultSuffix: "sdp"
            currentFile: "stream_net" + streamChannelCombo.currentIndex + ".sdp"
            onAccepted: {
                var path = selectedFile.toString().replace(/^(file:\/{3}|file:\/\/)/, "");
                if (Qt.platform.os === "windows" && path.length > 2 && path[0] === '/' && path[2] === ':') {
                    path = path.substring(1);
                }
                if (bridge) {
                    bridge.exportSdpFile(streamChannelCombo.currentIndex, path);
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
