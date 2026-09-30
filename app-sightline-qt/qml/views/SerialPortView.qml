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
                text: "🔌 RS-232 / RS-422 SERIAL COMMUNICATION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x24 / UART & Peripheral Protocol"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Serial Port 0"
                value: "115200"
                unit: "bps"
                accentColor: SightlineTheme.primary
                iconText: "📟"
            }
            MetricCard {
                title: "Serial Port 1"
                value: "Pelco-D"
                accentColor: SightlineTheme.info
                iconText: "🕹️"
            }
            MetricCard {
                title: "UART TX/RX"
                value: "NORMAL"
                accentColor: SightlineTheme.success
                iconText: "⚡"
            }
            Item { Layout.fillWidth: true }
        }

        // Settings Card
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
                    text: "UART HARDWARE & PROTOCOL SELECTION"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Physical UART Port:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: portCombo
                        model: ["Port 0 (RS-232 Main)", "Port 1 (RS-422/485 Gimbal)", "Port 2 (TTL Auxiliary)"]
                        Layout.preferredWidth: 260
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Baud Rate:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: baudCombo
                        model: ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"]
                        currentIndex: 4
                        Layout.preferredWidth: 180
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Attached Protocol:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: protoCombo
                        model: ["Sightline SLA Protocol", "Pelco D Gimbal Controller", "Sony VISCA Camera Block", "Transparent Network Tunnel"]
                        Layout.preferredWidth: 260
                    }
                }

                Button {
                    text: "Apply UART Settings"
                    Layout.preferredWidth: 200
                    Layout.topMargin: 8
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                    onClicked: {
                        bridge.saveParameters(0x01);
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
