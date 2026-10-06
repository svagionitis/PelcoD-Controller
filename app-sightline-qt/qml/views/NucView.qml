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
                text: "THERMAL NON-UNIFORMITY CORRECTION (NUC)"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// 0x35 NUC Parameters · 0x36 Read/Write NUC · 0xA8 Dead Pixel"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow (placeholders until the 0xA1 / 0x35 replies are bound to the bridge)
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "FPA Sensor Temp"; value: "—"; unit: "°C"; accentColor: SightlineTheme.warning; iconText: "🌡️" }
            MetricCard { title: "Dead Pixels"; value: "—"; unit: "px"; accentColor: SightlineTheme.info; iconText: "🩹" }
            MetricCard { title: "NUC Table"; value: "—"; accentColor: SightlineTheme.textMuted; iconText: "📄" }
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

                // The former handlers sent a reporting-mode change and a save-parameters
                // command, neither of which performs a NUC. Actions stay disabled until the guided
                // NUC workflow (0x35 run modes / 0x36 file ops) is exposed by the bridge.
                Flow {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        id: nucRunBtn
                        text: "Run NUC"
                        enabled: false
                        implicitWidth: 220
                        implicitHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.warning; radius: 4; opacity: nucRunBtn.enabled ? 1.0 : 0.35 }
                    }

                    Button {
                        id: deadTableBtn
                        text: "Save Dead Pixel Table"
                        enabled: false
                        implicitWidth: 180
                        implicitHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4; opacity: deadTableBtn.enabled ? 1.0 : 0.35 }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: "NUC / DPR actions are disabled pending the guided NUC workflow."
                    color: SightlineTheme.textMuted
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
