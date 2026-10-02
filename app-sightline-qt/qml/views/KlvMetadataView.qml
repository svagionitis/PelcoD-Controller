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

    readonly property int minContentWidth: 520

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { width: 4; height: 18; color: SightlineTheme.primary; radius: 2 }
            Text {
                text: "MISB 0601 / 0102 KLV METADATA INJECTION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Module 0x73 UAS Geospatial Telemetry"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "Aircraft Latitude"; value: parseFloat(latInput.text).toFixed(4); unit: "°N"; accentColor: SightlineTheme.primary; iconText: "🌐" }
            MetricCard { title: "Aircraft Longitude"; value: parseFloat(lonInput.text).toFixed(4); unit: "°E"; accentColor: SightlineTheme.primary; iconText: "🌐" }
            MetricCard { title: "Altitude MSL"; value: altInput.text; unit: "m"; accentColor: SightlineTheme.info; iconText: "⛰️" }
            MetricCard { title: "Platform Heading"; value: headingInput.text; unit: "°"; accentColor: SightlineTheme.warning; iconText: "🧭" }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: klvCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: klvCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "UAS NAVIGATION & ATTITUDE STATE INJECTION"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text { text: "Geodetic Position:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 130; Layout.alignment: Qt.AlignTop; Layout.topMargin: 4 }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        RowLayout {
                            spacing: 4
                            Text { text: "Lat:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            TextField {
                                id: latInput; text: "37.7749"; implicitWidth: 80; color: SightlineTheme.textPrimary
                                background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: latInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                            }
                        }
                        RowLayout {
                            spacing: 4
                            Text { text: "Lon:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            TextField {
                                id: lonInput; text: "-122.4194"; implicitWidth: 85; color: SightlineTheme.textPrimary
                                background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: lonInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                            }
                        }
                        RowLayout {
                            spacing: 4
                            Text { text: "Alt(m):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            TextField {
                                id: altInput; text: "1500.0"; implicitWidth: 70; color: SightlineTheme.textPrimary
                                background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: altInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text { text: "Platform Attitude:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 130; Layout.alignment: Qt.AlignTop; Layout.topMargin: 4 }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        RowLayout {
                            spacing: 4
                            Text { text: "Heading:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            TextField {
                                id: headingInput; text: "180.0"; implicitWidth: 65; color: SightlineTheme.textPrimary
                                background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: headingInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                            }
                        }
                        RowLayout {
                            spacing: 4
                            Text { text: "Pitch:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            TextField {
                                id: pitchInput; text: "-15.0"; implicitWidth: 65; color: SightlineTheme.textPrimary
                                background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: pitchInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                            }
                        }
                        RowLayout {
                            spacing: 4
                            Text { text: "Roll:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                            TextField {
                                id: rollInput; text: "0.0"; implicitWidth: 65; color: SightlineTheme.textPrimary
                                background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: rollInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Layout.topMargin: 4

                    Button {
                        text: "Transmit Single KLV Frame"
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            if (bridge) {
                                bridge.setMetadata(
                                    parseFloat(latInput.text),
                                    parseFloat(lonInput.text),
                                    parseFloat(altInput.text),
                                    parseFloat(headingInput.text),
                                    parseFloat(pitchInput.text),
                                    parseFloat(rollInput.text)
                                );
                            }
                        }
                    }

                    CheckBox {
                        id: autoStreamKlv
                        text: "Simulate 10 Hz flight telemetry"
                        checked: false
                    }

                    Item { Layout.fillWidth: true }
                }

                Timer {
                    id: klvSimTimer
                    interval: 100
                    running: autoStreamKlv.checked
                    repeat: true
                    onTriggered: {
                        var h = (parseFloat(headingInput.text) + 0.2) % 360.0;
                        headingInput.text = h.toFixed(1);
                        if (bridge) {
                            bridge.setMetadata(
                                parseFloat(latInput.text),
                                parseFloat(lonInput.text),
                                parseFloat(altInput.text),
                                h,
                                parseFloat(pitchInput.text),
                                parseFloat(rollInput.text)
                            );
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
