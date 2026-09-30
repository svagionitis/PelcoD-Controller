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
                text: "AUTONOMOUS LANDING AID & VISUAL SERVOING"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Module 0x77 Fiducial & Heli-Deck Recovery"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Row
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            MetricCard { title: "Relative Lateral (X)"; value: "+0.12"; unit: "m"; accentColor: SightlineTheme.primary; iconText: "↔️" }
            MetricCard { title: "Relative Longitude (Y)"; value: "-0.04"; unit: "m"; accentColor: SightlineTheme.primary; iconText: "↕️" }
            MetricCard { title: "Descent Altitude (Z)"; value: "14.8"; unit: "m"; accentColor: SightlineTheme.info; iconText: "📏" }
            MetricCard { title: "Marker Tracking"; value: "LOCKED"; accentColor: SightlineTheme.success; iconText: "🎯" }
            Item { Layout.fillWidth: true }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: landCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: landCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "FIDUCIAL RECOVERY & DESCENT VECTORING"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: landCam
                        model: ["Camera 0 (Downward EO)", "Camera 1 (Downward IR)", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Fiducial Symbology:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: markerCombo
                        model: ["Sightline Proprietary Multi-Ring", "AprilTag 36h11", "ArUco 4x4", "High-Contrast H-Pad"]
                        Layout.preferredWidth: 280
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Physical Target Size:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: markerDimInput
                        text: "0.50"
                        Layout.preferredWidth: 70
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: markerDimInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                    }
                    Text { text: "meters width"; color: SightlineTheme.textMuted; font.pixelSize: 12; Layout.alignment: Qt.AlignVCenter }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Arm Landing Guidance"
                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.success; radius: 4 }
                        onClicked: { if (bridge) bridge.startTracking(landCam.currentIndex, 960, 540, 120, 120, 0x04); }
                    }

                    Button {
                        text: "Disarm Guidance"
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#ffffff"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.error; radius: 4 }
                        onClicked: { if (bridge) bridge.stopTracking(landCam.currentIndex, 0xFF); }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
