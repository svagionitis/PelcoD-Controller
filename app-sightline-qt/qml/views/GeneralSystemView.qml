import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

ScrollView {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: parent.width - 32
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { width: 4; height: 18; color: SightlineTheme.primary; radius: 2 }
            Text {
                text: "GENERAL SYSTEM MANAGEMENT & FLASH MEMORY"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Modules 0x01, 0x3B, 0x40 SoC Health"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            MetricCard { title: "Firmware Build"; value: bridge ? bridge.softwareVersion : "Disconnected"; accentColor: SightlineTheme.primary; iconText: "💾" }
            MetricCard { title: "SoC Core Temp"; value: (bridge && bridge.isConnected) ? (bridge.coreTempC + "°C") : "--"; accentColor: (bridge && bridge.coreTempC > 75) ? SightlineTheme.warning : SightlineTheme.info; iconText: "🌡️" }
            MetricCard { title: "Processor Load"; value: (bridge && bridge.isConnected) ? (bridge.cpuLoadPercent + "%") : "--"; accentColor: (bridge && bridge.cpuLoadPercent > 80) ? SightlineTheme.warning : SightlineTheme.success; iconText: "⚡" }
            MetricCard { title: "System Uptime"; value: (bridge && bridge.isConnected) ? (bridge.uptimeSeconds + "s") : "--"; accentColor: SightlineTheme.primary; iconText: "⏱️" }
            Item { Layout.fillWidth: true }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: sysCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: sysCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 14

                Text {
                    text: "NON-VOLATILE MEMORY (FLASH) & HARDWARE CONTROL"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: "Link Status:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.alignment: Qt.AlignVCenter }
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: (bridge && bridge.isConnected) ? SightlineTheme.success : SightlineTheme.error
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Text {
                        text: (bridge && bridge.isConnected) ? "Hardware Online & Responsive" : "Hardware Disconnected"
                        color: (bridge && bridge.isConnected) ? SightlineTheme.success : SightlineTheme.error
                        font.pixelSize: 12
                        font.bold: true
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Button {
                        text: "Query Version"
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: { if (bridge) bridge.queryVersion(); }
                    }

                    Button {
                        text: "Save to Flash"
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.success; radius: 4 }
                        onClicked: { if (bridge) bridge.saveParameters(0); }
                    }

                    Button {
                        text: "Reset Defaults"
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#ffffff"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.error; radius: 4 }
                        onClicked: { if (bridge) bridge.resetParameters(0); }
                    }

                    Item { Layout.fillWidth: true }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: SightlineTheme.cardBorder }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "LATEST SYSTEM ALERT / WARNING MESSAGE:"
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 10
                        font.bold: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 42
                        radius: 4
                        color: SightlineTheme.surfaceLight
                        border.color: (bridge && bridge.lastWarningMessage && bridge.lastWarningMessage.length > 0) ? SightlineTheme.warning : SightlineTheme.cardBorder

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            elide: Text.ElideRight
                            text: (bridge && bridge.lastWarningMessage && bridge.lastWarningMessage.length > 0) ? bridge.lastWarningMessage : "System operational. No active hardware faults or warnings."
                            color: (bridge && bridge.lastWarningMessage && bridge.lastWarningMessage.length > 0) ? SightlineTheme.warning : SightlineTheme.textMuted
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
