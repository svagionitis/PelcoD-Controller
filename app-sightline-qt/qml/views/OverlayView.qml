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
                text: "📐 GRAPHIC OVERLAYS & RETICLES"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x62 / HUD & Symbology"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Active Reticle"
                value: "Mil-Dot Cross"
                accentColor: SightlineTheme.primary
                iconText: "🎯"
            }
            MetricCard {
                title: "HUD Elements"
                value: "6 Active"
                accentColor: SightlineTheme.info
                iconText: "📊"
            }
            MetricCard {
                title: "Symbology Refresh"
                value: "60"
                unit: "Hz"
                accentColor: SightlineTheme.success
                iconText: "⚡"
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
                    text: "ON-SCREEN DISPLAY (OSD) CONFIGURATION"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Reticle Pattern:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: reticleCombo
                        model: ["Standard Crosshair (0)", "Mil-Dot Sniper (1)", "Circle-Cross (2)", "Corner Frame Gate (3)", "Artificial Horizon (4)"]
                        Layout.preferredWidth: 260
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "OSD Telemetry Badges:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    CheckBox { id: showTime; text: "UTC Timestamp"; checked: true }
                    CheckBox { id: showGps; text: "GPS Coords"; checked: true }
                    CheckBox { id: showTracks; text: "Target ID Gates"; checked: true }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Color & Contrast:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: colorCombo
                        model: ["Fluorescent Green (#00E676)", "Tactical White (#FFFFFF)", "High-Vis Amber (#FFB300)", "Inverted Auto-Contrast"]
                        Layout.preferredWidth: 260
                    }
                }

                Button {
                    text: "Update Reticles & Overlays"
                    Layout.preferredWidth: 220
                    Layout.topMargin: 8
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                    onClicked: {
                        bridge.setReportingMode(0, 33, 0x07);
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
