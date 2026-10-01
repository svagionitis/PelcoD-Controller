import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."

Rectangle {
    id: root

    property int activeCamera: 0
    property int frameWidth: 1920
    property int frameHeight: 1080
    property int selectedCol: 960
    property int selectedRow: 540
    property int gateWidth: 80
    property int gateHeight: 80

    property bool showCrosshair: true
    property bool showGrid: true
    property bool showTelemetry: true
    property real zoomLevel: 1.0

    color: "#080a0f"
    border.color: SightlineTheme.cardBorder
    border.width: 1
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. Top HUD Status Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#0f121a"
            border.color: SightlineTheme.cardBorder
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                // Camera Badge & Mode
                Rectangle {
                    implicitWidth: 110
                    implicitHeight: 24
                    color: root.activeCamera === 1 ? "#3d1414" : "#14253d"
                    radius: 3
                    border.color: root.activeCamera === 1 ? SightlineTheme.accent : SightlineTheme.primary
                    border.width: 1

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        Rectangle {
                            width: 6; height: 6; radius: 3
                            color: root.activeCamera === 1 ? SightlineTheme.accent : SightlineTheme.primary
                        }
                        Text {
                            text: root.activeCamera === 1 ? "CAM 1: IR THERMAL" : "CAM 0: EO DAYLIGHT"
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                // Telemetry Tape Indicators
                Text {
                    text: "RES: 1920x1080p60"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                    font.family: "Monospace"
                }

                Text {
                    text: "FOV: 2.1° TELE"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 10
                    font.family: "Monospace"
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "RETICLE: (" + root.selectedCol + ", " + root.selectedRow + ")"
                    color: SightlineTheme.primary
                    font.pixelSize: 10
                    font.bold: true
                    font.family: "Monospace"
                }

                Rectangle {
                    implicitWidth: 70
                    implicitHeight: 22
                    color: bridge && bridge.isConnected ? "#0f2e1a" : "#2e1214"
                    radius: 3
                    border.color: bridge && bridge.isConnected ? SightlineTheme.success : SightlineTheme.error
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: bridge && bridge.isConnected ? "LIVE FEED" : "STANDBY"
                        color: bridge && bridge.isConnected ? SightlineTheme.success : SightlineTheme.error
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }
        }

        // 2. Central Video Stream & Tactical Reticle Canvas
        Item {
            id: canvasArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // Base Simulated Video / Sensor Texture Background
            Rectangle {
                anchors.fill: parent
                color: root.activeCamera === 1 ? "#120a0a" : "#0a0e14"

                // Synthetic scanline gradient simulation
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: root.activeCamera === 1 ? "#1a0f0f" : "#0d131c" }
                        GradientStop { position: 0.5; color: root.activeCamera === 1 ? "#0f0808" : "#080c12" }
                        GradientStop { position: 1.0; color: root.activeCamera === 1 ? "#140c0c" : "#0a0f16" }
                    }
                }
            }

            // Grid Lines Overlay (Optional)
            Item {
                anchors.fill: parent
                visible: root.showGrid
                opacity: 0.25

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width; height: 1
                    color: SightlineTheme.primary
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 1; height: parent.height
                    color: SightlineTheme.primary
                }

                // Intermediate grid lines
                Repeater {
                    model: 4
                    Rectangle {
                        x: (index + 1) * (canvasArea.width / 5)
                        y: 0
                        width: 1; height: canvasArea.height
                        color: SightlineTheme.textMuted
                        opacity: 0.2
                    }
                }
                Repeater {
                    model: 4
                    Rectangle {
                        x: 0
                        y: (index + 1) * (canvasArea.height / 5)
                        width: canvasArea.width; height: 1
                        color: SightlineTheme.textMuted
                        opacity: 0.2
                    }
                }
            }

            // Central Tactical Crosshair & Mil-Dots (Optional)
            Item {
                anchors.centerIn: parent
                visible: root.showCrosshair

                // Horizontal crosshair lines
                Rectangle {
                    x: -120; y: 0; width: 100; height: 1
                    color: SightlineTheme.primary
                }
                Rectangle {
                    x: 20; y: 0; width: 100; height: 1
                    color: SightlineTheme.primary
                }

                // Vertical crosshair lines
                Rectangle {
                    x: 0; y: -120; width: 1; height: 100
                    color: SightlineTheme.primary
                }
                Rectangle {
                    x: 0; y: 20; width: 1; height: 100
                    color: SightlineTheme.primary
                }

                // Center Circle
                Rectangle {
                    anchors.centerIn: parent
                    width: 20; height: 20; radius: 10
                    color: "transparent"
                    border.color: SightlineTheme.primary
                    border.width: 1
                }
            }

            // Active Primary Target Tracking Gate
            Rectangle {
                id: targetGate
                x: (root.selectedCol / root.frameWidth) * canvasArea.width - width / 2
                y: (root.selectedRow / root.frameHeight) * canvasArea.height - height / 2
                width: Math.max(20, (root.gateWidth / root.frameWidth) * canvasArea.width)
                height: Math.max(20, (root.gateHeight / root.frameHeight) * canvasArea.height)
                color: "transparent"
                border.color: SightlineTheme.primary
                border.width: 2

                // Target Gate Corner Brackets
                Rectangle { x: -1; y: -1; width: 5; height: 5; color: SightlineTheme.primary }
                Rectangle { x: parent.width - 4; y: -1; width: 5; height: 5; color: SightlineTheme.primary }
                Rectangle { x: -1; y: parent.height - 4; width: 5; height: 5; color: SightlineTheme.primary }
                Rectangle { x: parent.width - 4; y: parent.height - 4; width: 5; height: 5; color: SightlineTheme.primary }

                // Center cross dot
                Rectangle {
                    anchors.centerIn: parent
                    width: 4; height: 4; radius: 2
                    color: SightlineTheme.primary
                }

                // Target Label Banner
                Rectangle {
                    y: -18
                    anchors.horizontalCenter: parent.horizontalCenter
                    implicitWidth: trkLabel.implicitWidth + 8
                    implicitHeight: 14
                    color: "#cc080a0f"
                    border.color: SightlineTheme.primary
                    border.width: 1

                    Text {
                        id: trkLabel
                        anchors.centerIn: parent
                        text: "TRK #01 [LOCK]"
                        color: SightlineTheme.primary
                        font.pixelSize: 8
                        font.bold: true
                        font.family: "Monospace"
                    }
                }
            }

            // Point-and-Click Video Acquisition Interaction
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.CrossCursor

                function designateTarget(mouse) {
                    var col = Math.max(0, Math.min(root.frameWidth, Math.round((mouse.x / canvasArea.width) * root.frameWidth)));
                    var row = Math.max(0, Math.min(root.frameHeight, Math.round((mouse.y / canvasArea.height) * root.frameHeight)));
                    root.selectedCol = col;
                    root.selectedRow = row;

                    if (bridge) {
                        bridge.startTracking(root.activeCamera, col, row, root.gateWidth, root.gateHeight, 0x01);
                    }
                }

                onClicked: designateTarget(mouse)
                onPositionChanged: {
                    if (pressed) {
                        designateTarget(mouse);
                    }
                }
            }
        }

        // 3. Bottom Viewport Control Toolbar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#0f121a"
            border.color: SightlineTheme.cardBorder
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                // Camera Switcher Buttons
                Text {
                    text: "SENSOR:"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }

                Button {
                    text: "EO (Cam 0)"
                    implicitHeight: 28
                    implicitWidth: 80
                    checked: root.activeCamera === 0
                    checkable: true
                    onClicked: root.activeCamera = 0
                }

                Button {
                    text: "IR (Cam 1)"
                    implicitHeight: 28
                    implicitWidth: 80
                    checked: root.activeCamera === 1
                    checkable: true
                    onClicked: root.activeCamera = 1
                }

                Rectangle { width: 1; height: 20; color: SightlineTheme.cardBorder }

                // HUD Display Toggles
                CheckBox {
                    text: "Crosshair"
                    checked: root.showCrosshair
                    onCheckedChanged: root.showCrosshair = checked
                }

                CheckBox {
                    text: "Grid"
                    checked: root.showGrid
                    onCheckedChanged: root.showGrid = checked
                }

                Item { Layout.fillWidth: true }

                // Quick Nudge Buttons
                Text {
                    text: "NUDGE:"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }

                Button {
                    text: "◄"
                    implicitWidth: 28; implicitHeight: 28
                    onClicked: if (bridge) bridge.nudgeTracking(root.activeCamera, -5, 0)
                }
                Button {
                    text: "▲"
                    implicitWidth: 28; implicitHeight: 28
                    onClicked: if (bridge) bridge.nudgeTracking(root.activeCamera, 0, -5)
                }
                Button {
                    text: "▼"
                    implicitWidth: 28; implicitHeight: 28
                    onClicked: if (bridge) bridge.nudgeTracking(root.activeCamera, 0, 5)
                }
                Button {
                    text: "►"
                    implicitWidth: 28; implicitHeight: 28
                    onClicked: if (bridge) bridge.nudgeTracking(root.activeCamera, 5, 0)
                }

                Button {
                    text: "Center"
                    implicitHeight: 28
                    onClicked: {
                        root.selectedCol = Math.round(root.frameWidth / 2);
                        root.selectedRow = Math.round(root.frameHeight / 2);
                        if (bridge) {
                            bridge.startTracking(root.activeCamera, root.selectedCol, root.selectedRow, root.gateWidth, root.gateHeight, 0x01);
                        }
                    }
                }
            }
        }
    }
}
