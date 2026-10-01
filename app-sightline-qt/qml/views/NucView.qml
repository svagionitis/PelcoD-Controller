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
    clip: true

    ColumnLayout {
        width: parent.width - 32
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { width: 4; height: 18; color: SightlineTheme.primary; radius: 2 }
            Text {
                text: "THERMAL NON-UNIFORMITY CORRECTION (NUC)"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Module 0x67 FFC Shutter & Bad Pixel Replacement"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "FPA Sensor Temp"; value: "38.2"; unit: "°C"; accentColor: SightlineTheme.warning; iconText: "🌡️" }
            MetricCard { title: "Bad Pixels Replaced"; value: "14"; unit: "px"; accentColor: SightlineTheme.info; iconText: "🩹" }
            MetricCard { title: "Calibration Table"; value: "VALID"; accentColor: SightlineTheme.success; iconText: "✅" }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: nucCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: nucCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "FLAT-FIELD CALIBRATION (FFC) & MAP REPLACEMENT"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Thermal Camera:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: nucCam
                        model: ["Camera 1 (LWIR Thermal)", "Camera 2 (MWIR Cooled)", "Camera 0"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Calibration Mode:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: nucMode
                        model: ["1-Point Shutter Flat-Field (Quick)", "2-Point Blackbody (High Dynamic)", "Scene-Based Dynamic (Continuous)"]
                        Layout.preferredWidth: 280
                    }
                    Item { Layout.fillWidth: true }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Actuate Mechanical Shutter NUC"
                        implicitWidth: 220
                        implicitHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.warning; radius: 4 }
                        onClicked: bridge.setReportingMode(nucCam.currentIndex, 100, 0x10)
                    }

                    Button {
                        text: "Update Bad Pixel Table"
                        implicitWidth: 180
                        implicitHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: bridge.saveParameters(0x02)
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
