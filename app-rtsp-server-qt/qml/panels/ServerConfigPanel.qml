import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ScrollView {
    id: scrollRoot
    required property var controller
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: scrollRoot.availableWidth
        spacing: 14

        // Network & Binding Card
        TacticalCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            title: "Network & Endpoint Configuration"
            subtitle: "RTSP Transport Parameters"

            GridLayout {
                anchors.fill: parent
                anchors.margins: 14
                columns: 2
                rowSpacing: 10
                columnSpacing: 14

                TacticalTextField {
                    Layout.fillWidth: true
                    label: "Listening Port (0 for ephemeral test)"
                    text: scrollRoot.controller.port.toString()
                    enabledState: scrollRoot.controller.serverState !== 2
                    onTextChanged: {
                        var p = parseInt(text)
                        if (!isNaN(p)) scrollRoot.controller.port = p
                    }
                }

                TacticalTextField {
                    Layout.fillWidth: true
                    label: "Stream Mount Path (URL Subpath)"
                    text: scrollRoot.controller.streamPath
                    enabledState: scrollRoot.controller.serverState !== 2
                    onTextChanged: scrollRoot.controller.streamPath = text
                }

                TacticalTextField {
                    Layout.fillWidth: true
                    label: "SDP Stream Name"
                    text: scrollRoot.controller.streamName
                    enabledState: scrollRoot.controller.serverState !== 2
                    onTextChanged: scrollRoot.controller.streamName = text
                }

                TacticalTextField {
                    Layout.fillWidth: true
                    label: "SDP Session Info Description"
                    text: scrollRoot.controller.streamInfo
                    enabledState: scrollRoot.controller.serverState !== 2
                    onTextChanged: scrollRoot.controller.streamInfo = text
                }
            }
        }

        // Protocol & Capacity Card
        TacticalCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            title: "Protocol & Queue Sizing"
            subtitle: "Buffering & Client Limits"

            GridLayout {
                anchors.fill: parent
                anchors.margins: 14
                columns: 2
                rowSpacing: 10
                columnSpacing: 14

                TacticalComboBox {
                    Layout.fillWidth: true
                    label: "Encapsulation Stream Type"
                    model: ["RFC 6184 H.264 Elementary Stream", "RFC 2250 MPEG-2 TS (STANAG 4609)"]
                    currentIndex: scrollRoot.controller.streamType
                    enabledState: scrollRoot.controller.serverState !== 2
                    onActivated: function(idx) { scrollRoot.controller.streamType = idx }
                }

                TacticalComboBox {
                    Layout.fillWidth: true
                    label: "Client Transport Protocol Policy"
                    model: ["Permit All (UDP Unicast + TCP Interleaved)", "Restrict to TCP Interleaved (Firewall)", "Restrict to UDP Unicast (Low Latency)"]
                    currentIndex: scrollRoot.controller.transport
                    enabledState: scrollRoot.controller.serverState !== 2
                    onActivated: function(idx) { scrollRoot.controller.transport = idx }
                }

                TacticalTextField {
                    Layout.fillWidth: true
                    label: "Max Simultaneous Clients"
                    text: scrollRoot.controller.maxClients.toString()
                    enabledState: scrollRoot.controller.serverState !== 2
                    onTextChanged: {
                        var n = parseInt(text)
                        if (!isNaN(n)) scrollRoot.controller.maxClients = n
                    }
                }

                TacticalTextField {
                    Layout.fillWidth: true
                    label: "Ring Buffer Queue Capacity (Frames)"
                    text: scrollRoot.controller.queueCapacity.toString()
                    enabledState: scrollRoot.controller.serverState !== 2
                    onTextChanged: {
                        var n = parseInt(text)
                        if (!isNaN(n)) scrollRoot.controller.queueCapacity = n
                    }
                }
            }
        }

        // Security & Authentication Card
        TacticalCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 165
            title: "Access Control & Authentication"
            subtitle: "Digest / Basic Realm Security"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                CheckBox {
                    id: authCheckBox
                    text: "Enable RTSP Digest / Basic Client Authentication"
                    checked: scrollRoot.controller.authEnabled
                    enabled: scrollRoot.controller.serverState !== 2
                    onToggled: scrollRoot.controller.authEnabled = checked

                    contentItem: Text {
                        text: authCheckBox.text
                        font.pixelSize: 11
                        font.bold: true
                        color: authCheckBox.checked ? "#00e5ff" : "#94a3b8"
                        leftPadding: authCheckBox.indicator ? (authCheckBox.indicator.width + 8) : 24
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    TacticalTextField {
                        Layout.fillWidth: true
                        label: "Username"
                        text: scrollRoot.controller.username
                        enabledState: scrollRoot.controller.authEnabled && scrollRoot.controller.serverState !== 2
                        onTextChanged: scrollRoot.controller.username = text
                    }

                    TacticalTextField {
                        Layout.fillWidth: true
                        label: "Password"
                        text: scrollRoot.controller.password
                        echoMode: TextInput.Password
                        enabledState: scrollRoot.controller.authEnabled && scrollRoot.controller.serverState !== 2
                        onTextChanged: scrollRoot.controller.password = text
                    }

                    TacticalTextField {
                        Layout.fillWidth: true
                        label: "Authentication Realm"
                        text: scrollRoot.controller.realm
                        enabledState: scrollRoot.controller.authEnabled && scrollRoot.controller.serverState !== 2
                        onTextChanged: scrollRoot.controller.realm = text
                    }
                }
            }
        }

        // Multicast RTP Egress Card
        TacticalCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 165
            title: "Multicast RTP Egress"
            subtitle: "Direct UDP Group Broadcasting"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                CheckBox {
                    id: multicastCheckBox
                    text: "Enable Multicast RTP Egress Broadcast"
                    checked: scrollRoot.controller.enableMulticast
                    enabled: scrollRoot.controller.serverState !== 2
                    onToggled: scrollRoot.controller.enableMulticast = checked

                    contentItem: Text {
                        text: multicastCheckBox.text
                        font.pixelSize: 11
                        font.bold: true
                        color: multicastCheckBox.checked ? "#00e5ff" : "#94a3b8"
                        leftPadding: multicastCheckBox.indicator ? (multicastCheckBox.indicator.width + 8) : 24
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    TacticalTextField {
                        Layout.fillWidth: true
                        label: "Multicast Group IP"
                        text: scrollRoot.controller.multicastAddress
                        enabledState: scrollRoot.controller.enableMulticast && scrollRoot.controller.serverState !== 2
                        onTextChanged: scrollRoot.controller.multicastAddress = text
                    }

                    TacticalTextField {
                        Layout.fillWidth: true
                        label: "Multicast Port"
                        text: scrollRoot.controller.multicastPort.toString()
                        enabledState: scrollRoot.controller.enableMulticast && scrollRoot.controller.serverState !== 2
                        onTextChanged: {
                            var p = parseInt(text)
                            if (!isNaN(p)) scrollRoot.controller.multicastPort = p
                        }
                    }

                    TacticalTextField {
                        Layout.fillWidth: true
                        label: "TTL Hop Limit"
                        text: scrollRoot.controller.multicastTtl.toString()
                        enabledState: scrollRoot.controller.enableMulticast && scrollRoot.controller.serverState !== 2
                        onTextChanged: {
                            var t = parseInt(text)
                            if (!isNaN(t)) scrollRoot.controller.multicastTtl = t
                        }
                    }
                }
            }
        }
    }
}
