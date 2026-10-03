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
                text: "NETWORK INTERFACES, TRAFFIC CONTROL & TELEMETRY"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// EAN-Encoding Modules 0x1C (Network), 0x66 (NIC List), 0x92 Key 13 (tc), 0x7E (CoT)"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
            Item { Layout.fillWidth: true }

            Button {
                text: "Query Network Info"
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
                        bridge.queryNetworkParams();
                    }
                }
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10

            MetricCard {
                title: "Hardware NICs"
                value: bridge && bridge.networkInterfaces.length > 0 ? bridge.networkInterfaces.join(", ") : "eth0"
                accentColor: SightlineTheme.success
                iconText: "🔌"
            }
            MetricCard {
                title: "Board IPv4"
                value: bridge ? bridge.boardIp : "192.168.0.107"
                accentColor: SightlineTheme.primary
                iconText: "🌐"
            }
            MetricCard {
                title: "IP Mode"
                value: bridge ? (bridge.boardDhcp ? "DHCP (Auto)" : "STATIC IP") : "STATIC IP"
                accentColor: bridge && bridge.boardDhcp ? SightlineTheme.info : SightlineTheme.accent
                iconText: "⚙️"
            }
            MetricCard {
                title: "Traffic Limiter"
                value: bridge ? (bridge.tcRateKbps > 0 ? bridge.tcRateKbps + " kbps" : "UNRESTRICTED") : "UNRESTRICTED"
                accentColor: bridge && bridge.tcRateKbps > 0 ? SightlineTheme.warning : SightlineTheme.textMuted
                iconText: "🚦"
            }
        }

        // Section: SLA Board Network Interface Configuration (Message ID 0x1C)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: netCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: netCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "SLA HARDWARE NETWORK CONFIGURATION (STATIC IP & DHCP)"
                        color: SightlineTheme.primary
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Message ID 0x1C (SLASetNetworkParameters)"
                        color: SightlineTheme.textMuted
                        font.pixelSize: 10
                        font.family: "Monospace"
                    }
                }

                Text {
                    text: "Configure the embedded SLA video processor's onboard Ethernet IP address, subnet mask, and default gateway per EAN-Encoding Section 3.5 & Figure 6."
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }

                // Mode: DHCP vs Static IP
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Addressing Mode:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    RadioButton {
                        id: staticRadio
                        text: "Static IPv4 Address"
                        checked: bridge ? !bridge.boardDhcp : true
                    }
                    RadioButton {
                        id: dhcpRadio
                        text: "DHCP (Auto Assign)"
                        checked: bridge ? bridge.boardDhcp : false
                    }
                    Item { Layout.fillWidth: true }
                }

                // IP Address
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    enabled: staticRadio.checked
                    opacity: staticRadio.checked ? 1.0 : 0.4
                    Text { text: "Board IP Address:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    TextField {
                        id: boardIpField
                        text: bridge ? bridge.boardIp : "192.168.0.107"
                        Layout.preferredWidth: 180
                        color: SightlineTheme.textPrimary
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                        }
                    }

                    Text { text: "Subnet Mask:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                    TextField {
                        id: boardMaskField
                        text: bridge ? bridge.boardNetmask : "255.255.0.0"
                        Layout.preferredWidth: 160
                        color: SightlineTheme.textPrimary
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // Gateway
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    enabled: staticRadio.checked
                    opacity: staticRadio.checked ? 1.0 : 0.4
                    Text { text: "Default Gateway:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    TextField {
                        id: boardGwField
                        text: bridge ? bridge.boardGateway : "192.168.0.1"
                        Layout.preferredWidth: 180
                        color: SightlineTheme.textPrimary
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // Quick Subnet Presets
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    enabled: staticRadio.checked
                    opacity: staticRadio.checked ? 1.0 : 0.4
                    Item { Layout.preferredWidth: 160 }
                    Text { text: "Quick Presets:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }

                    Button {
                        text: "EAN Fig 6 (192.168.0.107/16)"
                        Layout.preferredHeight: 24
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 3 }
                        contentItem: Text { text: parent.text; color: SightlineTheme.primary; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: {
                            boardIpField.text = "192.168.0.107";
                            boardMaskField.text = "255.255.0.0";
                            boardGwField.text = "192.168.0.1";
                        }
                    }

                    Button {
                        text: "Class C (192.168.1.100/24)"
                        Layout.preferredHeight: 24
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 3 }
                        contentItem: Text { text: parent.text; color: SightlineTheme.textSecondary; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: {
                            boardIpField.text = "192.168.1.100";
                            boardMaskField.text = "255.255.255.0";
                            boardGwField.text = "192.168.1.1";
                        }
                    }

                    Button {
                        text: "Tactical LAN (10.0.0.100/8)"
                        Layout.preferredHeight: 24
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 3 }
                        contentItem: Text { text: parent.text; color: SightlineTheme.textSecondary; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: {
                            boardIpField.text = "10.0.0.100";
                            boardMaskField.text = "255.0.0.0";
                            boardGwField.text = "10.0.0.1";
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // Apply Network Settings Button
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Hardware Network Config"
                        Layout.preferredWidth: 230
                        Layout.preferredHeight: 32
                        background: Rectangle {
                            color: parent.hovered ? "#33ebff" : SightlineTheme.primary
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#0e1014"
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.setBoardNetwork(
                                    boardIpField.text,
                                    boardMaskField.text,
                                    boardGwField.text,
                                    dhcpRadio.checked
                                );
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Section: Linux Traffic Control (tc) Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: tcCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: tcCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "LINUX TRAFFIC CONTROL (tc) BANDWIDTH LIMITER"
                        color: SightlineTheme.primary
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "System Value 13 (0x92 / 0x93 / Appendix A2 & A3)"
                        color: SightlineTheme.textMuted
                        font.pixelSize: 10
                        font.family: "Monospace"
                    }
                }

                Text {
                    text: "Configure kernel-level packet shaping on the SLA embedded Linux network stack to prevent RF link saturation during high-motion bursts."
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }

                // Rate Limit Slider & SpinBox
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Shaping Rate Limit:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    Slider {
                        id: tcRateSlider
                        from: 0
                        to: 30000
                        value: bridge ? bridge.tcRateKbps : 0
                        stepSize: 250
                        Layout.preferredWidth: 260
                    }

                    TextField {
                        id: tcRateInput
                        text: Math.round(tcRateSlider.value).toString()
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                        }
                        onEditingFinished: {
                            var val = parseInt(text) || 0;
                            val = Math.max(0, Math.min(100000, val));
                            tcRateSlider.value = val;
                        }
                    }

                    Text {
                        text: tcRateSlider.value === 0 ? "Unlimited (Shaping Disabled)" : "kbps (" + (tcRateSlider.value / 1000.0).toFixed(2) + " Mbps)"
                        color: tcRateSlider.value === 0 ? SightlineTheme.textMuted : SightlineTheme.warning
                        font.bold: tcRateSlider.value > 0
                        font.pixelSize: 12
                    }

                    Item { Layout.fillWidth: true }
                }

                // Quick Rate Presets
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Item { Layout.preferredWidth: 160 }
                    Repeater {
                        model: [
                            { label: "0 (Unlimited)", val: 0 },
                            { label: "500k", val: 500 },
                            { label: "1.0M", val: 1000 },
                            { label: "2.5M (Tactical Link)", val: 2500 },
                            { label: "5.0M", val: 5000 },
                            { label: "10.0M", val: 10000 }
                        ]
                        Button {
                            text: modelData.label
                            Layout.preferredHeight: 24
                            background: Rectangle {
                                color: tcRateSlider.value === modelData.val ? SightlineTheme.accent : SightlineTheme.surfaceLight
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: tcRateSlider.value === modelData.val ? "#0e1014" : SightlineTheme.textSecondary
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: { tcRateSlider.value = modelData.val; }
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                // Burst and MTU
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Token Bucket Parameters:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }

                    Text { text: "Burst Size (Bytes):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    SpinBox {
                        id: tcBurstSpin
                        from: 500
                        to: 65535
                        value: bridge && bridge.tcBurstBytes > 0 ? bridge.tcBurstBytes : 3000
                        stepSize: 500
                        Layout.preferredWidth: 110
                    }

                    Text { text: "MTU (Bytes):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                    SpinBox {
                        id: tcMtuSpin
                        from: 576
                        to: 9000
                        value: bridge && bridge.tcMtuBytes > 0 ? bridge.tcMtuBytes : 1500
                        stepSize: 100
                        Layout.preferredWidth: 110
                    }

                    Item { Layout.fillWidth: true }
                }

                // TC Actions
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Traffic Control"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 32
                        background: Rectangle {
                            color: parent.hovered ? "#33ebff" : SightlineTheme.primary
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#0e1014"
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.setTrafficControl(
                                    Math.round(tcRateSlider.value),
                                    tcBurstSpin.value,
                                    tcMtuSpin.value
                                );
                            }
                        }
                    }

                    Button {
                        text: "Reset (Disable Shaping)"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 32
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: SightlineTheme.cardBorder
                        }
                        contentItem: Text {
                            text: parent.text
                            color: SightlineTheme.textSecondary
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.resetTrafficControl();
                                tcRateSlider.value = 0;
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Section: Cursor-on-Target (CoT) XML Feed Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: cotCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: cotCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "CURSOR-ON-TARGET (CoT) XML TELEMETRY FEED"
                        color: SightlineTheme.primary
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Module 0x7E ATAK/TAK Interop"
                        color: SightlineTheme.textMuted
                        font.pixelSize: 10
                        font.family: "Monospace"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Enable CoT Stream:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    Switch { id: cotSwitch; checked: true }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Destination UDP Port:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: cotPortInput
                        text: "1870"
                        Layout.preferredWidth: 90
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: cotPortInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                    }
                    Text { text: "(Standard ATAK / WinTAK multicast/broadcast port)" ; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Platform Call Sign:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: cotUidInput
                        text: "SIGHTLINE-UAV-01"
                        Layout.preferredWidth: 220
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: cotUidInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "MIL-STD Entity Type:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: cotTypeInput
                        text: "a-f-A-M-F-Q"
                        Layout.preferredWidth: 220
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: cotTypeInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                    }
                    Text { text: "(Air Military Fixed-Wing Reconnaissance)" ; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply CoT Parameters"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            if (bridge) {
                                bridge.setCursorOnTarget(
                                    cotSwitch.checked ? 1 : 0,
                                    parseInt(cotPortInput.text) || 1870,
                                    cotUidInput.text,
                                    cotTypeInput.text
                                );
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
