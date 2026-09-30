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
                text: "🌐 ETHERNET & CURSOR-ON-TARGET (CoT)"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x1C / 0x7E / IP Endpoints & CoT XML"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Ethernet Link"
                value: "1000 Mbps"
                accentColor: SightlineTheme.success
                iconText: "🔌"
            }
            MetricCard {
                title: "Active IP Address"
                value: bridge.host
                accentColor: SightlineTheme.primary
                iconText: "🌐"
            }
            MetricCard {
                title: "CoT Broadcast"
                value: cotSwitch.checked ? "ACTIVE" : "STANDBY"
                accentColor: cotSwitch.checked ? SightlineTheme.success : SightlineTheme.textMuted
                iconText: "📡"
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
                    text: "CURSOR-ON-TARGET (CoT) TAK/ATAK INTEGRATION"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Enable CoT XML Feed:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 160 }
                    Switch { id: cotSwitch; checked: true }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "CoT Destination Port:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 160 }
                    TextField {
                        id: cotPortInput
                        text: "1870"
                        Layout.preferredWidth: 100
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 }
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Platform Call Sign / UID:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 160 }
                    TextField {
                        id: cotUidInput
                        text: "SIGHTLINE-UAV-01"
                        Layout.preferredWidth: 200
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 }
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "MIL-STD Entity Type:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 160 }
                    TextField {
                        id: cotTypeInput
                        text: "a-f-A-M-F-Q"
                        Layout.preferredWidth: 200
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 }
                    }
                }

                Button {
                    text: "Update CoT Configuration"
                    Layout.preferredWidth: 220
                    Layout.topMargin: 8
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                    onClicked: {
                        bridge.setCursorOnTarget(
                            cotSwitch.checked ? 1 : 0,
                            parseInt(cotPortInput.text),
                            cotUidInput.text,
                            cotTypeInput.text
                        );
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
