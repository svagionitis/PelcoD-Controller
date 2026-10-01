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
                text: "VIDEO STABILIZATION & DRIFT COMPENSATION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Modules 0x05, 0x22"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard { title: "Shift X Offset"; value: "1.4"; unit: "px"; accentColor: SightlineTheme.primary; iconText: "↔️" }
            MetricCard { title: "Shift Y Offset"; value: "-0.8"; unit: "px"; accentColor: SightlineTheme.primary; iconText: "↕️" }
            MetricCard { title: "Roll Angle"; value: "0.2"; unit: "deg"; accentColor: SightlineTheme.info; iconText: "🔄" }
            MetricCard { title: "Confidence"; value: "98"; unit: "%"; accentColor: SightlineTheme.success; iconText: "✅" }
        }

        // Settings Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: stabCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: stabCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "ELECTRONIC STABILIZATION PARAMETERS"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: stabCam
                        model: ["Camera 0 (EO Visible)", "Camera 1 (IR Thermal)", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 240
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Stabilization Mode:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: stabMode
                        model: ["Disabled (0)", "Full 2D Stabilization (1)", "High-pass Pan/Tilt/Roll (2)"]
                        Layout.preferredWidth: 260
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Automatic Bias:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Switch { id: autoBiasSwitch; checked: true }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Max Shift Limit:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150; Layout.alignment: Qt.AlignVCenter }
                    Slider {
                        id: maxShiftSlider
                        from: 16
                        to: 128
                        value: 64
                        stepSize: 8
                        Layout.preferredWidth: 200
                    }
                    Text { text: Math.round(maxShiftSlider.value) + " px"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Parameters"
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            if (bridge) {
                                bridge.setStabilization(
                                    stabCam.currentIndex,
                                    stabMode.currentIndex,
                                    autoBiasSwitch.checked ? 1 : 0,
                                    Math.round(maxShiftSlider.value)
                                );
                            }
                        }
                    }

                    Button {
                        text: "Reset Stabilization"
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                        onClicked: { if (bridge) bridge.resetStabilization(stabCam.currentIndex); }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
