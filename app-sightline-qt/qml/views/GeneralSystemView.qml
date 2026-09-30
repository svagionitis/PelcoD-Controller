import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        RowLayout {
            spacing: 12
            Text {
                text: "⚙️ GENERAL SYSTEM MANAGEMENT"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x01 / 0x3B / 0x40 / Health & Parameters"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Firmware Build"
                value: bridge.softwareVersion
                accentColor: SightlineTheme.primary
                iconText: "💾"
            }
            MetricCard {
                title: "Processor Load"
                value: bridge.cpuLoadPercent.toString()
                unit: "%"
                accentColor: bridge.cpuLoadPercent > 80 ? SightlineTheme.warning : SightlineTheme.success
                iconText: "⚡"
            }
            MetricCard {
                title: "SoC Core Temp"
                value: bridge.coreTempC.toString()
                unit: "°C"
                accentColor: bridge.coreTempC > 75 ? SightlineTheme.warning : SightlineTheme.info
                iconText: "🌡️"
            }
            MetricCard {
                title: "Active Uptime"
                value: bridge.uptimeSeconds.toString()
                unit: "s"
                accentColor: SightlineTheme.primary
                iconText: "⏱️"
            }
            Item { Layout.fillWidth: true }
        }

        // System Configuration & Flash Memory Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 340
            color: SightlineTheme.surface
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16

                Text {
                    text: "NON-VOLATILE MEMORY (FLASH) & HARDWARE CONTROL"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                Text {
                    text: "Current Status: " + (bridge.isConnected ? "Hardware Online & Responsive" : "Hardware Offline")
                    color: bridge.isConnected ? SightlineTheme.success : SightlineTheme.error
                    font.bold: true
                }

                RowLayout {
                    spacing: 12
                    Layout.topMargin: 8

                    Button {
                        text: "Query Version & Capabilities"
                        Layout.preferredWidth: 220
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: bridge.queryVersion()
                    }

                    Button {
                        text: "Save Parameters to Flash"
                        Layout.preferredWidth: 200
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.success; radius: 4 }
                        onClicked: bridge.saveParameters(0)
                    }

                    Button {
                        text: "Reset to Factory Defaults"
                        Layout.preferredWidth: 200
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.error; radius: 4 }
                        onClicked: bridge.resetParameters(0)
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: SightlineTheme.cardBorder }

                Text {
                    text: "Last System Warning / Alert:"
                    color: SightlineTheme.textMuted
                    font.pixelSize: SightlineTheme.fontSizeSmall
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 50
                    radius: 4
                    color: SightlineTheme.surfaceLight
                    border.color: bridge.lastWarningMessage.length > 0 ? SightlineTheme.warning : SightlineTheme.cardBorder

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        text: bridge.lastWarningMessage.length > 0 ? bridge.lastWarningMessage : "No active warnings. System running normally."
                        color: bridge.lastWarningMessage.length > 0 ? SightlineTheme.warning : SightlineTheme.textSecondary
                        font.pixelSize: SightlineTheme.fontSizeNormal
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
