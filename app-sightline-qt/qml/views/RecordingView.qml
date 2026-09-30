import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

Item {
    id: root

    property bool isRecording: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        RowLayout {
            spacing: 12
            Text {
                text: "💾 ONBOARD RECORDING & SNAPSHOT"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x70 / 0x69 / SD Card Media Storage"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Storage Status"
                value: "118.4"
                unit: "GB Free"
                accentColor: SightlineTheme.info
                iconText: "💽"
            }
            MetricCard {
                title: "Recording State"
                value: root.isRecording ? "RECORDING" : "STANDBY"
                accentColor: root.isRecording ? SightlineTheme.error : SightlineTheme.success
                iconText: root.isRecording ? "🔴" : "⏸️"
            }
            MetricCard {
                title: "Recorded Clips"
                value: "28"
                accentColor: SightlineTheme.primary
                iconText: "📁"
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
                    text: "VIDEO RECORDER CONTROLS"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Camera Source:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: recCam
                        model: ["Camera 0 (EO Video)", "Camera 1 (IR Video)", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 220
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "File Prefix:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    TextField {
                        id: prefixInput
                        text: "flight_rec"
                        Layout.preferredWidth: 200
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 }
                    }
                }

                RowLayout {
                    spacing: 12
                    Layout.topMargin: 12

                    Button {
                        text: root.isRecording ? "Stop Recording" : "Start Video Recording"
                        Layout.preferredWidth: 180
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: root.isRecording ? SightlineTheme.error : SightlineTheme.success; radius: 4 }
                        onClicked: {
                            if (root.isRecording) {
                                bridge.setSDRecording(0, recCam.currentIndex, prefixInput.text);
                                root.isRecording = false;
                            } else {
                                bridge.setSDRecording(1, recCam.currentIndex, prefixInput.text);
                                root.isRecording = true;
                            }
                        }
                    }

                    Button {
                        text: "Capture Still Snapshot"
                        Layout.preferredWidth: 180
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            bridge.setSDRecording(2, recCam.currentIndex, prefixInput.text);
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
