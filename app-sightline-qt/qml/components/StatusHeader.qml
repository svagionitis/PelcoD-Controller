import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."

Rectangle {
    id: header
    height: SightlineTheme.headerHeight
    color: SightlineTheme.surface
    border.color: SightlineTheme.cardBorder
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 16

        // Logo & Title
        Row {
            spacing: 10
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                width: 32
                height: 32
                radius: 6
                color: SightlineTheme.primary
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: "S"
                    color: "#FFFFFF"
                    font.pixelSize: 18
                    font.bold: true
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    text: "SIGHTLINE SLA"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: 15
                    font.bold: true
                    font.letterSpacing: 1
                }
                Text {
                    text: "Protocol v3.11.6 Suite"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 11
                }
            }
        }

        // Connection Controls
        RowLayout {
            spacing: 8
            Layout.alignment: Qt.AlignVCenter

            TextField {
                id: hostInput
                implicitWidth: 120
                implicitHeight: 34
                text: bridge.host
                placeholderText: "Host IP"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeNormal
                background: Rectangle {
                    color: SightlineTheme.surfaceLight
                    radius: SightlineTheme.radiusSmall
                    border.color: hostInput.activeFocus ? SightlineTheme.primary : SightlineTheme.cardBorder
                }
            }

            TextField {
                id: cmdPortInput
                implicitWidth: 70
                implicitHeight: 34
                text: bridge.commandPort.toString()
                placeholderText: "Cmd"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeNormal
                background: Rectangle {
                    color: SightlineTheme.surfaceLight
                    radius: SightlineTheme.radiusSmall
                    border.color: cmdPortInput.activeFocus ? SightlineTheme.primary : SightlineTheme.cardBorder
                }
            }

            TextField {
                id: replyPortInput
                implicitWidth: 70
                implicitHeight: 34
                text: bridge.replyPort.toString()
                placeholderText: "Reply"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeNormal
                background: Rectangle {
                    color: SightlineTheme.surfaceLight
                    radius: SightlineTheme.radiusSmall
                    border.color: replyPortInput.activeFocus ? SightlineTheme.primary : SightlineTheme.cardBorder
                }
            }

            Button {
                id: connectBtn
                implicitHeight: 34
                text: bridge.isConnected ? "Disconnect" : "Connect UDP"
                font.pixelSize: SightlineTheme.fontSizeNormal
                font.bold: true

                contentItem: Text {
                    text: connectBtn.text
                    font: connectBtn.font
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: SightlineTheme.radiusSmall
                    color: bridge.isConnected ? SightlineTheme.error : SightlineTheme.primary
                    opacity: connectBtn.down ? 0.8 : (connectBtn.hovered ? 0.9 : 1.0)
                }

                onClicked: {
                    if (bridge.isConnected) {
                        bridge.disconnectDevice();
                    } else {
                        bridge.connectUdp(hostInput.text, parseInt(cmdPortInput.text), parseInt(replyPortInput.text));
                    }
                }
            }
        }

        // Status Pill
        Rectangle {
            implicitWidth: 110
            implicitHeight: 30
            radius: 15
            color: bridge.isConnected ? Qt.rgba(0, 0.9, 0.46, 0.15) : Qt.rgba(1, 0.32, 0.32, 0.15)
            border.color: bridge.isConnected ? SightlineTheme.success : SightlineTheme.error
            border.width: 1
            Layout.alignment: Qt.AlignVCenter

            Row {
                anchors.centerIn: parent
                spacing: 6
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: bridge.isConnected ? SightlineTheme.success : SightlineTheme.error
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: bridge.isConnected ? "CONNECTED" : "OFFLINE"
                    color: bridge.isConnected ? SightlineTheme.success : SightlineTheme.error
                    font.pixelSize: 11
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Live Health Telemetry Readouts
        RowLayout {
            spacing: 14
            Layout.alignment: Qt.AlignVCenter

            Column {
                Text {
                    text: "CPU LOAD"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }
                Text {
                    text: bridge.isConnected ? (bridge.cpuLoadPercent + "%") : "--"
                    color: bridge.cpuLoadPercent > 80 ? SightlineTheme.warning : SightlineTheme.textPrimary
                    font.pixelSize: 13
                    font.bold: true
                }
            }

            Rectangle { width: 1; height: 24; color: SightlineTheme.cardBorder }

            Column {
                Text {
                    text: "CORE TEMP"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }
                Text {
                    text: bridge.isConnected ? (bridge.coreTempC + " °C") : "--"
                    color: bridge.coreTempC > 75 ? SightlineTheme.warning : SightlineTheme.textPrimary
                    font.pixelSize: 13
                    font.bold: true
                }
            }

            Rectangle { width: 1; height: 24; color: SightlineTheme.cardBorder }

            Column {
                Text {
                    text: "UPTIME"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }
                Text {
                    text: bridge.isConnected ? (bridge.uptimeSeconds + "s") : "--"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: 13
                    font.bold: true
                }
            }

            Rectangle { width: 1; height: 24; color: SightlineTheme.cardBorder }

            Column {
                Text {
                    text: "FIRMWARE"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }
                Text {
                    text: bridge.softwareVersion
                    color: SightlineTheme.primary
                    font.pixelSize: 12
                    font.bold: true
                }
            }
        }
    }
}
