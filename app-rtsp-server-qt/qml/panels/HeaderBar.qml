import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Rectangle {
    id: headerRoot
    required property var controller
    signal toastRequested(string message)

    height: 52
    color: "#141720"
    border.color: "#242938"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 14

        StatusIndicator {
            stateCode: headerRoot.controller.serverState
            text: headerRoot.controller.serverStateString
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 20
            color: "#2a3040"
        }

        ColumnLayout {
            spacing: 1
            Text {
                text: "PELCOD RTSP STREAMING SERVER"
                color: "#f8fafc"
                font.pixelSize: 12
                font.bold: true
                font.letterSpacing: 1.2
            }
            Text {
                text: "LIVE555 CORE // STANAG 4609 & H.264 EGRESS"
                color: "#64748b"
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 0.8
            }
        }

        Item { Layout.fillWidth: true }

        // Primary RTSP URL Display Box
        Rectangle {
            Layout.preferredWidth: Math.min(360, parent.width * 0.35)
            Layout.preferredHeight: 32
            color: "#0a0c10"
            border.color: "#242938"
            border.width: 1
            radius: 4

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 6
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: headerRoot.controller.primaryUrl
                    color: headerRoot.controller.serverState === 2 ? "#00e5ff" : "#64748b"
                    font.pixelSize: 11
                    font.family: "Monospace"
                    elide: Text.ElideMiddle
                }

                TacticalButton {
                    text: "COPY"
                    accentColor: "#00e5ff"
                    Layout.preferredWidth: 54
                    Layout.preferredHeight: 22
                    onClicked: {
                        headerRoot.controller.copyUrlToClipboard()
                        headerRoot.toastRequested("RTSP URL copied to clipboard")
                    }
                }
            }
        }

        // Active Clients Counter Badge
        Rectangle {
            Layout.preferredHeight: 32
            Layout.preferredWidth: 90
            color: "#10141d"
            border.color: headerRoot.controller.activeClients > 0 ? "#00e676" : "#242938"
            border.width: 1
            radius: 4

            RowLayout {
                anchors.centerIn: parent
                spacing: 6
                Text {
                    text: headerRoot.controller.activeClients.toString()
                    color: headerRoot.controller.activeClients > 0 ? "#00e676" : "#94a3b8"
                    font.pixelSize: 13
                    font.bold: true
                    font.family: "Monospace"
                }
                Text {
                    text: "CLIENTS"
                    color: "#64748b"
                    font.pixelSize: 9
                    font.bold: true
                }
            }
        }

        // Uptime Display
        Rectangle {
            Layout.preferredHeight: 32
            Layout.preferredWidth: 84
            color: "#10141d"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Text {
                anchors.centerIn: parent
                text: headerRoot.controller.serverUptime
                color: "#94a3b8"
                font.pixelSize: 11
                font.family: "Monospace"
            }
        }

        // Master Start / Stop Button
        TacticalButton {
            Layout.preferredWidth: 105
            Layout.preferredHeight: 32
            text: headerRoot.controller.serverState === 2 ? "STOP SERVER" : "START SERVER"
            accentColor: headerRoot.controller.serverState === 2 ? "#ff1744" : "#00e676"
            isPrimary: true
            onClicked: {
                headerRoot.controller.toggleServer()
            }
        }
    }
}
