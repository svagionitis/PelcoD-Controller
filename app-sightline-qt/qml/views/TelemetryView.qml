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
                text: "📡 TELEMETRY REPORTING MODES"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x0B / 0x43 / Tracking Positions & Rates"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Report Rate"
                value: "30"
                unit: "Hz"
                accentColor: SightlineTheme.primary
                iconText: "⚡"
            }
            MetricCard {
                title: "Telemetry Mask"
                value: "0x003F"
                accentColor: SightlineTheme.info
                iconText: "🎭"
            }
            MetricCard {
                title: "Packet Throughput"
                value: "48.2"
                unit: "KB/s"
                accentColor: SightlineTheme.success
                iconText: "📊"
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
                    text: "PERIODIC TELEMETRY STREAMING"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: telemCam
                        model: ["Camera 0", "Camera 1", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 200
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Reporting Period (ms):"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    Slider {
                        id: periodSlider
                        from: 16
                        to: 500
                        value: 33
                        stepSize: 1
                        Layout.preferredWidth: 220
                    }
                    Text { text: Math.round(periodSlider.value) + " ms (" + (1000.0 / periodSlider.value).toFixed(1) + " Hz)"; color: SightlineTheme.textPrimary; font.bold: true }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Message Masks:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    CheckBox { id: maskTrack; text: "Tracking Positions (0x43)"; checked: true }
                    CheckBox { id: maskStatus; text: "System Status (0x3B)"; checked: true }
                    CheckBox { id: maskWarn; text: "User Warnings (0x40)"; checked: true }
                }

                Button {
                    text: "Apply Telemetry Rate"
                    Layout.preferredWidth: 200
                    Layout.topMargin: 8
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                    onClicked: {
                        var flags = 0;
                        if (maskTrack.checked) flags |= 0x01;
                        if (maskStatus.checked) flags |= 0x02;
                        if (maskWarn.checked) flags |= 0x04;
                        bridge.setReportingMode(telemCam.currentIndex, Math.round(periodSlider.value), flags);
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
