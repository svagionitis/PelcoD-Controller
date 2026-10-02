import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

ScrollView {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true
    contentWidth: availableWidth
    contentHeight: mainCol.implicitHeight + 32
    clip: true
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(0, root.availableWidth - 32)
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { width: 4; height: 18; color: SightlineTheme.primary; radius: 2 }
            Text {
                text: "TELEMETRY REPORTING & STREAMING MASKS"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Modules 0x0B, 0x43 Reporting Periods"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "Telemetry Rate"; value: "30"; unit: "Hz"; accentColor: SightlineTheme.primary; iconText: "⚡" }
            MetricCard { title: "Reporting Mask"; value: "0x003F"; accentColor: SightlineTheme.info; iconText: "🎭" }
            MetricCard { title: "Packet Bandwidth"; value: "48.2"; unit: "KB/s"; accentColor: SightlineTheme.success; iconText: "📊" }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: telemCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: telemCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "PERIODIC TELEMETRY STREAMING CONFIGURATION"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: telemCam
                        model: ["Camera 0", "Camera 1", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Reporting Interval:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider {
                        id: periodSlider
                        from: 16
                        to: 500
                        value: 33
                        stepSize: 1
                        Layout.preferredWidth: 200
                    }
                    Text { text: Math.round(periodSlider.value) + " ms (" + (1000.0 / periodSlider.value).toFixed(1) + " Hz)"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Message Masks:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    CheckBox { id: maskTrack; text: "Tracking (0x43)"; checked: true }
                    CheckBox { id: maskStatus; text: "Status (0x3B)"; checked: true }
                    CheckBox { id: maskWarn; text: "Warnings (0x40)"; checked: true }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Telemetry Rate"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            var flags = 0;
                            if (maskTrack.checked) flags |= 0x01;
                            if (maskStatus.checked) flags |= 0x02;
                            if (maskWarn.checked) flags |= 0x04;
                            bridge.setReportingMode(telemCam.currentIndex, Math.round(periodSlider.value), flags);
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
