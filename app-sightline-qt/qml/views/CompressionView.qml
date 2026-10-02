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
                text: "H.264 / H.265 COMPRESSION & STREAMING"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Modules 0x32, 0x40 RTSP & Encoding"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "Video Codec"; value: "H.264 Main"; accentColor: SightlineTheme.primary; iconText: "🎬" }
            MetricCard { title: "Target Bitrate"; value: "4.0"; unit: "Mbps"; accentColor: SightlineTheme.info; iconText: "📶" }
            MetricCard { title: "GOP Interval"; value: "30"; unit: "frames"; accentColor: SightlineTheme.warning; iconText: "⏱️" }
            MetricCard { title: "RTSP Transport"; value: "ACTIVE"; accentColor: SightlineTheme.success; iconText: "📡" }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: compCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: compCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "ENCODER BITRATE & STREAMING CONTROLS"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Output Stream:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: streamCombo
                        model: ["Stream 0 (Primary RTSP)", "Stream 1 (Secondary RTSP)"]
                        Layout.preferredWidth: 260
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Bitrate Rate:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider {
                        id: bitrateSlider
                        from: 500
                        to: 15000
                        value: 4000
                        stepSize: 500
                        Layout.preferredWidth: 200
                    }
                    Text { text: (bitrateSlider.value / 1000).toFixed(1) + " Mbps (" + bitrateSlider.value + " kbps)"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "GOP Length:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider {
                        id: gopSlider
                        from: 10
                        to: 120
                        value: 30
                        stepSize: 5
                        Layout.preferredWidth: 200
                    }
                    Text { text: Math.round(gopSlider.value) + " frames"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Rate Control Mode:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: rateCtrlCombo
                        model: ["CBR (Constant Bitrate)", "VBR (Variable Bitrate)", "CQP (Constant Quantization)"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Encoder"
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            if (bridge) {
                                bridge.setH264Params(
                                    streamCombo.currentIndex,
                                    Math.round(bitrateSlider.value),
                                    Math.round(gopSlider.value),
                                    28,
                                    rateCtrlCombo.currentIndex
                                );
                            }
                        }
                    }

                    Button {
                        text: "Start Stream"
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.success; radius: 4 }
                        onClicked: { if (bridge) bridge.streamingControl(streamCombo.currentIndex, 1); }
                    }

                    Button {
                        text: "Stop Stream"
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#ffffff"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.error; radius: 4 }
                        onClicked: { if (bridge) bridge.streamingControl(streamCombo.currentIndex, 0); }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
