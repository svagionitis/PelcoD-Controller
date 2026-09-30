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
                text: "🖥️ DISPLAY & VIDEO OUTPUT"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x16 / PIP & Orientation"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Metrics
        RowLayout {
            spacing: 12
            MetricCard {
                title: "Output Pipeline"
                value: "HDMI-OUT"
                accentColor: SightlineTheme.primary
                iconText: "📺"
            }
            MetricCard {
                title: "Layout Mode"
                value: "PIP Top-Right"
                accentColor: SightlineTheme.info
                iconText: "🖼️"
            }
            MetricCard {
                title: "Thermal Palette"
                value: "White Hot"
                accentColor: SightlineTheme.warning
                iconText: "🎨"
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
                    text: "VIDEO RENDERER & LAYOUT"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Screen Layout:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: layoutCombo
                        model: ["Single Fullscreen (Cam 0)", "Single Fullscreen (Cam 1)", "Picture-in-Picture (Cam 0 + 1)", "Side-by-Side Split"]
                        Layout.preferredWidth: 260
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Image Rotation:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: rotCombo
                        model: ["0 Degrees (Normal)", "90 Degrees CW", "180 Degrees Inverted", "270 Degrees CCW"]
                        Layout.preferredWidth: 220
                    }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Image Mirror/Flip:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    CheckBox { id: flipH; text: "Horizontal Flip" }
                    CheckBox { id: flipV; text: "Vertical Flip" }
                }

                RowLayout {
                    spacing: 16
                    Text { text: "Color Palette:"; color: SightlineTheme.textSecondary; Layout.preferredWidth: 150 }
                    ComboBox {
                        id: paletteCombo
                        model: ["White-Hot (Greyscale)", "Black-Hot (Inverted)", "Rainbow High-Contrast", "Ironbow Thermal", "Amber Night-Vision"]
                        Layout.preferredWidth: 240
                    }
                }

                Button {
                    text: "Update Display Settings"
                    Layout.preferredWidth: 200
                    Layout.topMargin: 8
                    contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                    onClicked: {
                        // Triggers display layout modification
                        bridge.setVideoParams(0, layoutCombo.currentIndex, 1920, 1080, 60);
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
