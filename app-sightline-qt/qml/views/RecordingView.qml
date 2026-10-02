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
    property bool isRecording: false

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
                text: "ONBOARD MEDIA STORAGE & RECORDING"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Modules 0x70, 0x69 SD Card & Snapshots"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "Storage Free"; value: "118.4"; unit: "GB"; accentColor: SightlineTheme.info; iconText: "💽" }
            MetricCard { title: "Recorder State"; value: root.isRecording ? "RECORDING" : "STANDBY"; accentColor: root.isRecording ? SightlineTheme.error : SightlineTheme.success; iconText: root.isRecording ? "🔴" : "⏸️" }
            MetricCard { title: "Stored Clips"; value: "28"; unit: "files"; accentColor: SightlineTheme.primary; iconText: "📁" }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: recCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: recCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "VIDEO RECORDING & STILL FRAME CAPTURE"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Camera Source:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: recCam
                        model: ["Camera 0 (EO Video)", "Camera 1 (IR Video)", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Filename Prefix:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: prefixInput
                        text: "flight_rec"
                        Layout.preferredWidth: 180
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: prefixInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: root.isRecording ? "Stop Video Recording" : "Start Video Recording"
                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 32
                        contentItem: Text {
                            text: parent.text
                            color: root.isRecording ? "#ffffff" : "#0e1014"
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: root.isRecording ? SightlineTheme.error : SightlineTheme.success
                            radius: 4
                        }
                        onClicked: {
                            if (root.isRecording) {
                                if (bridge) bridge.setSDRecording(0, recCam.currentIndex, prefixInput.text);
                                root.isRecording = false;
                            } else {
                                if (bridge) bridge.setSDRecording(1, recCam.currentIndex, prefixInput.text);
                                root.isRecording = true;
                            }
                        }
                    }

                    Button {
                        text: "Capture Still Snapshot"
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: { if (bridge) bridge.setSDRecording(2, recCam.currentIndex, prefixInput.text); }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
