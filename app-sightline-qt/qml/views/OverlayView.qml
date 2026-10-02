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
                text: "GRAPHIC OVERLAYS & RETICLES"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Module 0x62 HUD & Symbology"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "Reticle Pattern"; value: "Mil-Dot Cross"; accentColor: SightlineTheme.primary; iconText: "🎯" }
            MetricCard { title: "Active Overlays"; value: "6 Badges"; accentColor: SightlineTheme.info; iconText: "📊" }
            MetricCard { title: "OSD Refresh"; value: "60"; unit: "Hz"; accentColor: SightlineTheme.success; iconText: "⚡" }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: overCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: overCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "ON-SCREEN DISPLAY (OSD) CONFIGURATION"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Reticle Symbology:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: reticleCombo
                        model: ["Standard Crosshair (0)", "Mil-Dot Tactical (1)", "Circle-Cross (2)", "Corner Frame Gate (3)", "Artificial Horizon (4)"]
                        Layout.preferredWidth: 260
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Telemetry Badges:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignTop; Layout.topMargin: 4 }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        CheckBox { id: showTime; text: "UTC Time"; checked: true }
                        CheckBox { id: showGps; text: "GPS Coords"; checked: true }
                        CheckBox { id: showTracks; text: "Track Boxes"; checked: true }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Symbology Color:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: colorCombo
                        model: ["Cyan Tactical (#00E5FF)", "Fluorescent Green (#00E676)", "Tactical White (#F0F4FC)", "High-Vis Amber (#FF9100)"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Update Reticles & Overlays"
                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: bridge.setReportingMode(0, 33, 0x07)
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
