import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."

Rectangle {
    id: header
    implicitHeight: SightlineTheme.headerHeight
    height: implicitHeight
    color: SightlineTheme.surface
    border.color: SightlineTheme.cardBorder
    border.width: 1

    readonly property bool devConnected: Boolean(bridge && bridge.isConnected)

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        // Logo & Title
        RowLayout {
            spacing: 8
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                width: 24
                height: 24
                radius: 4
                color: SightlineTheme.primary
                Layout.alignment: Qt.AlignVCenter

                Text {
                    anchors.centerIn: parent
                    text: "S"
                    color: "#0e1014"
                    font.pixelSize: 13
                    font.bold: true
                }
            }

            ColumnLayout {
                spacing: 0
                Layout.alignment: Qt.AlignVCenter

                Text {
                    text: "SIGHTLINE SLA"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: 12
                    font.bold: true
                    font.letterSpacing: 1.0
                }
                Text {
                    text: "Protocol v3.11.6"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 10
                }
            }
        }

        Rectangle { width: 1; height: 22; color: SightlineTheme.cardBorder; Layout.alignment: Qt.AlignVCenter }

        // Connection Controls
        RowLayout {
            spacing: 8
            Layout.alignment: Qt.AlignVCenter

            Text {
                text: "HOST:"
                color: SightlineTheme.textSecondary
                font.pixelSize: 10
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }

            Rectangle {
                implicitWidth: 110
                implicitHeight: 28
                color: SightlineTheme.surfaceLight
                radius: 4
                border.color: hostInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                Layout.alignment: Qt.AlignVCenter

                TextInput {
                    id: hostInput
                    anchors.fill: parent
                    anchors.margins: 6
                    text: bridge ? bridge.host : "127.0.0.1"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: 11
                    font.family: "Monospace"
                    selectByMouse: true
                }
            }

            Text {
                text: "CMD:"
                color: SightlineTheme.textSecondary
                font.pixelSize: 10
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }

            Rectangle {
                implicitWidth: 55
                implicitHeight: 28
                color: SightlineTheme.surfaceLight
                radius: 4
                border.color: cmdPortInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                Layout.alignment: Qt.AlignVCenter

                TextInput {
                    id: cmdPortInput
                    anchors.fill: parent
                    anchors.margins: 6
                    text: bridge ? bridge.commandPort.toString() : "14001"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: 11
                    font.family: "Monospace"
                    selectByMouse: true
                }
            }

            Text {
                text: "REPLY:"
                color: SightlineTheme.textSecondary
                font.pixelSize: 10
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }

            Rectangle {
                implicitWidth: 55
                implicitHeight: 28
                color: SightlineTheme.surfaceLight
                radius: 4
                border.color: replyPortInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                Layout.alignment: Qt.AlignVCenter

                TextInput {
                    id: replyPortInput
                    anchors.fill: parent
                    anchors.margins: 6
                    text: bridge ? bridge.replyPort.toString() : "14002"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: 11
                    font.family: "Monospace"
                    selectByMouse: true
                }
            }

            Button {
                id: connectBtn
                implicitHeight: 28
                implicitWidth: 85
                Layout.alignment: Qt.AlignVCenter
                text: header.devConnected ? "Disconnect" : "Connect"
                contentItem: Text {
                    text: connectBtn.text
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 4
                    color: header.devConnected ? SightlineTheme.error : SightlineTheme.primary
                    opacity: connectBtn.down ? 0.8 : (connectBtn.hovered ? 0.9 : 1.0)
                }
                onClicked: {
                    if (!bridge) return;
                    if (header.devConnected) {
                        bridge.disconnectDevice();
                    } else {
                        bridge.connectUdp(hostInput.text, parseInt(cmdPortInput.text), parseInt(replyPortInput.text));
                    }
                }
            }
        }

        // Status Pill
        Rectangle {
            implicitWidth: 90
            implicitHeight: 24
            radius: 12
            color: header.devConnected ? Qt.rgba(0, 0.9, 0.46, 0.15) : Qt.rgba(1, 0.09, 0.27, 0.15)
            border.color: header.devConnected ? SightlineTheme.success : SightlineTheme.error
            Layout.alignment: Qt.AlignVCenter

            RowLayout {
                anchors.centerIn: parent
                spacing: 6
                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    color: header.devConnected ? SightlineTheme.success : SightlineTheme.error
                }
                Text {
                    text: header.devConnected ? "ONLINE" : "OFFLINE"
                    color: header.devConnected ? SightlineTheme.success : SightlineTheme.error
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Live Health Telemetry Readouts
        RowLayout {
            spacing: 12
            Layout.alignment: Qt.AlignVCenter

            ColumnLayout {
                spacing: 1
                Layout.alignment: Qt.AlignVCenter
                Text { text: "CPU"; color: SightlineTheme.textMuted; font.pixelSize: 9; font.bold: true }
                Text {
                    text: header.devConnected ? (bridge.cpuLoadPercent + "%") : "--"
                    color: (bridge && bridge.cpuLoadPercent > 80) ? SightlineTheme.warning : SightlineTheme.textPrimary
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            Rectangle { width: 1; height: 18; color: SightlineTheme.cardBorder; Layout.alignment: Qt.AlignVCenter }

            ColumnLayout {
                spacing: 1
                Layout.alignment: Qt.AlignVCenter
                Text { text: "TEMP"; color: SightlineTheme.textMuted; font.pixelSize: 9; font.bold: true }
                Text {
                    text: header.devConnected ? (bridge.coreTempC + "°C") : "--"
                    color: (bridge && bridge.coreTempC > 75) ? SightlineTheme.warning : SightlineTheme.textPrimary
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            Rectangle { width: 1; height: 18; color: SightlineTheme.cardBorder; Layout.alignment: Qt.AlignVCenter }

            ColumnLayout {
                spacing: 1
                Layout.alignment: Qt.AlignVCenter
                Text { text: "UPTIME"; color: SightlineTheme.textMuted; font.pixelSize: 9; font.bold: true }
                Text {
                    text: header.devConnected ? (bridge.uptimeSeconds + "s") : "--"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            Rectangle { width: 1; height: 18; color: SightlineTheme.cardBorder; Layout.alignment: Qt.AlignVCenter }

            ColumnLayout {
                spacing: 1
                Layout.alignment: Qt.AlignVCenter
                Text { text: "FIRMWARE"; color: SightlineTheme.textMuted; font.pixelSize: 9; font.bold: true }
                Text {
                    text: bridge ? bridge.softwareVersion : "Disconnected"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }
    }
}
