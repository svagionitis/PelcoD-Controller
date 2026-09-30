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
                text: "🔬 REAL-TIME PROTOCOL TRAFFIC INSPECTOR"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Byte-level SLA Frame Auditing & Diagnostic Injection"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Top Injection Bar Card
        Rectangle {
            Layout.fillWidth: true
            height: 64
            color: SightlineTheme.surface
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Text {
                    text: "RAW HEX INJECTION:"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                TextField {
                    id: hexInput
                    text: "51 AC 02 01 53"
                    Layout.fillWidth: true
                    placeholderText: "Enter space or hyphen separated hex bytes (e.g., 51 AC 02 01 53)"
                    font.family: "Monospace, Courier"
                    color: SightlineTheme.textPrimary
                    background: Rectangle {
                        color: SightlineTheme.surfaceLight
                        radius: SightlineTheme.radiusSmall
                        border.color: hexInput.activeFocus ? SightlineTheme.primary : SightlineTheme.cardBorder
                    }
                }

                Button {
                    text: "Send Raw Hex"
                    implicitHeight: 34
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                    onClicked: {
                        if (hexInput.text.length > 0) {
                            bridge.sendRawHex(hexInput.text);
                        }
                    }
                }

                Button {
                    text: "Clear Log"
                    implicitHeight: 34
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.cardBorder }
                    onClicked: bridge.trafficLogModel.clearLog()
                }
            }
        }

        // Traffic Log Table
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: SightlineTheme.surface
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            clip: true

            ListView {
                id: logListView
                anchors.fill: parent
                anchors.margins: 8
                model: bridge.trafficLogModel
                spacing: 4

                header: Rectangle {
                    width: logListView.width
                    height: 28
                    color: SightlineTheme.surfaceLight
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        Text { text: "Time"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 80 }
                        Text { text: "Dir"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 40 }
                        Text { text: "Message Name"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 200 }
                        Text { text: "Len"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 40 }
                        Text { text: "CRC"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 50 }
                        Text { text: "Hex Payload"; color: SightlineTheme.textMuted; font.bold: true; Layout.fillWidth: true }
                    }
                }

                delegate: Rectangle {
                    width: logListView.width
                    height: 30
                    color: index % 2 === 0 ? SightlineTheme.surface : SightlineTheme.surfaceLight
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        Text {
                            text: model.timestamp
                            color: SightlineTheme.textSecondary
                            font.family: "Monospace, Courier"
                            font.pixelSize: 11
                            Layout.preferredWidth: 80
                        }

                        Rectangle {
                            Layout.preferredWidth: 40
                            height: 18
                            radius: 9
                            color: model.isTx ? Qt.rgba(0, 0.75, 1.0, 0.2) : Qt.rgba(0, 0.9, 0.46, 0.2)

                            Text {
                                anchors.centerIn: parent
                                text: model.isTx ? "TX" : "RX"
                                color: model.isTx ? SightlineTheme.primary : SightlineTheme.success
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        Text {
                            text: model.messageName
                            color: SightlineTheme.textPrimary
                            font.bold: true
                            font.pixelSize: 12
                            Layout.preferredWidth: 200
                            elide: Text.ElideRight
                        }

                        Text {
                            text: model.length.toString()
                            color: SightlineTheme.textMuted
                            font.pixelSize: 11
                            Layout.preferredWidth: 40
                        }

                        Text {
                            text: model.crcOk ? "OK" : "ERR"
                            color: model.crcOk ? SightlineTheme.success : SightlineTheme.error
                            font.bold: true
                            font.pixelSize: 11
                            Layout.preferredWidth: 50
                        }

                        Text {
                            text: model.hexPayload
                            color: SightlineTheme.textSecondary
                            font.family: "Monospace, Courier"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }
}
