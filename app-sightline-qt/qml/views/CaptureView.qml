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
                text: "📹 VIDEO CAPTURE CONFIGURATION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x74 / Video Format & Resolution"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Active Input"
                value: "HDMI-1"
                accentColor: SightlineTheme.primary
                iconText: "🔌"
            }
            MetricCard {
                title: "Native Resolution"
                value: "1080p60"
                accentColor: SightlineTheme.info
                iconText: "📺"
            }
            MetricCard {
                title: "Frame Jitter"
                value: "< 0.4"
                unit: "ms"
                accentColor: SightlineTheme.success
                iconText: "⏱️"
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
                    text: "SENSOR INPUT HARDWARE INTERFACE"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: capCam
                        model: ["Camera 0 (Primary)", "Camera 1 (Secondary)", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 220
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Input Hardware Type:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: capType
                        model: ["0: HDMI / DVI Digital", "1: 3G-SDI Broadcast", "2: Sony FCB Block (LVDS)", "3: MIPI CSI-2 Camera", "4: Analog NTSC/PAL"]
                        Layout.preferredWidth: 260
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Capture Resolution:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: resCombo
                        model: ["1920 x 1080 (1080p)", "1280 x 720 (720p)", "640 x 512 (Thermal VGA)", "3840 x 2160 (4K UHD)"]
                        Layout.preferredWidth: 220
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Capture Frame Rate:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: fpsCombo
                        model: ["60 fps", "30 fps", "25 fps", "15 fps"]
                        Layout.preferredWidth: 140
                    }
                }

                Button {
                    text: "Configure Video Capture"
                    Layout.preferredWidth: 220
                    Layout.topMargin: 8
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                    onClicked: {
                        var w = 1920;
                        var h = 1080;
                        if (resCombo.currentIndex === 1) { w = 1280; h = 720; }
                        else if (resCombo.currentIndex === 2) { w = 640; h = 512; }
                        else if (resCombo.currentIndex === 3) { w = 3840; h = 2160; }
                        var fps = 60;
                        if (fpsCombo.currentIndex === 1) fps = 30;
                        else if (fpsCombo.currentIndex === 2) fps = 25;
                        else if (fpsCombo.currentIndex === 3) fps = 15;

                        bridge.setVideoParams(capCam.currentIndex, capType.currentIndex, w, h, fps);
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
