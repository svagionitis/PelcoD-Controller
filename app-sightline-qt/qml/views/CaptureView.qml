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
                text: "VIDEO CAPTURE & HARDWARE INPUTS"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Module 0x74 Video Format & Acquisition"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "Active Input Port"; value: "HDMI-1"; accentColor: SightlineTheme.primary; iconText: "🔌" }
            MetricCard { title: "Input Resolution"; value: "1080p60"; accentColor: SightlineTheme.info; iconText: "📺" }
            MetricCard { title: "Capture Jitter"; value: "< 0.4"; unit: "ms"; accentColor: SightlineTheme.success; iconText: "⏱️" }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: capCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: capCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "SENSOR INPUT HARDWARE INTERFACE"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: capCam
                        model: ["Camera 0 (Primary Input)", "Camera 1 (Secondary Input)", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Hardware Interface:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: capType
                        model: ["0: HDMI / DVI Digital", "1: 3G-SDI Broadcast", "2: Sony FCB Block (LVDS)", "3: MIPI CSI-2 Camera", "4: Analog NTSC/PAL"]
                        Layout.preferredWidth: 260
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Capture Resolution:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: resCombo
                        model: ["1920 x 1080 (1080p)", "1280 x 720 (720p)", "640 x 512 (Thermal VGA)", "3840 x 2160 (4K UHD)"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Frame Rate:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: fpsCombo
                        model: ["60 fps", "30 fps", "25 fps", "15 fps"]
                        Layout.preferredWidth: 160
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Capture Settings"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            var w = 1920; var h = 1080;
                            if (resCombo.currentIndex === 1) { w = 1280; h = 720; }
                            else if (resCombo.currentIndex === 2) { w = 640; h = 512; }
                            else if (resCombo.currentIndex === 3) { w = 3840; h = 2160; }
                            var fps = 60;
                            if (fpsCombo.currentIndex === 1) fps = 30;
                            else if (fpsCombo.currentIndex === 2) fps = 25;
                            else if (fpsCombo.currentIndex === 3) fps = 15;
                            if (bridge) {
                                bridge.setVideoParams(capCam.currentIndex, capType.currentIndex, w, h, fps);
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
