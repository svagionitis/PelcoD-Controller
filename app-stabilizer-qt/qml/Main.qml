import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import StabilizerApp 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 1440
    height: 860
    minimumWidth: 960
    minimumHeight: 640
    title: "PelcoD Digital Video Stabilizer Workbench"
    color: "#0e1014"

    property bool showSourcePanel: true
    property bool showRightPanel: true
    property int rightPanelMode: 0 // 0: Tuning, 1: Diagnostics
    property bool showTrajectoryChart: true

    StabilizerPlayerController {
        id: controller
    }

    // Top Tactical Header Bar
    header: Column {
        width: parent.width

        // Primary Header
        Rectangle {
            width: parent.width
            height: 44
            color: "#141720"
            border.color: "#242938"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                // Tactical Pulse Beacon
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: {
                        switch (controller.playbackState) {
                        case StabilizerPlayerController.Playing: return "#00e676";
                        case StabilizerPlayerController.Opening: return "#ff9100";
                        case StabilizerPlayerController.Paused:  return "#00e5ff";
                        case StabilizerPlayerController.Error:   return "#ff1744";
                        default: return "#525c70";
                        }
                    }
                    SequentialAnimation on opacity {
                        running: controller.playbackState === StabilizerPlayerController.Playing
                                 || controller.playbackState === StabilizerPlayerController.Opening
                        loops: Animation.Infinite
                        PropertyAnimation { to: 0.3; duration: 600 }
                        PropertyAnimation { to: 1.0; duration: 600 }
                    }
                }

                Text {
                    text: "VIDEO STABILIZER TESTBENCH"
                    color: "#f0f4fc"
                    font.pixelSize: 13
                    font.bold: true
                    font.letterSpacing: 1.5
                }

                Rectangle {
                    width: 1
                    height: 18
                    color: "#2a2f40"
                }

                Text {
                    text: controller.statusMessage.toUpperCase()
                    color: "#8894ab"
                    font.pixelSize: 11
                    Layout.maximumWidth: 260
                    elide: Text.ElideRight
                }

                Item { Layout.fillWidth: true }

                // Presets Quick-Access Toolbar
                Text {
                    text: "PRESETS:"
                    color: "#8894ab"
                    font.pixelSize: 10
                    font.bold: true
                }

                Repeater {
                    model: [
                        { name: "Drone Gimbal", label: "DRONE (1)" },
                        { name: "Pole Vibration", label: "POLE (2)" },
                        { name: "Handheld Action Cam", label: "ACTION (3)" },
                        { name: "Rolling Shutter", label: "RS (4)" },
                        { name: "Bypass", label: "BYPASS (5)" }
                    ]
                    delegate: Button {
                        height: 24
                        text: modelData.label
                        onClicked: controller.loadPreset(modelData.name)
                        background: Rectangle {
                            color: "#181d28"
                            border.color: "#2a3244"
                            border.width: 1
                            radius: 2
                        }
                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 9
                            font.bold: true
                            color: "#00e5ff"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Rectangle {
                    width: 1
                    height: 18
                    color: "#2a2f40"
                }

                // Panel Toggles
                Button {
                    height: 24
                    text: "SOURCES [S]"
                    checkable: true
                    checked: root.showSourcePanel
                    onClicked: root.showSourcePanel = !root.showSourcePanel
                    background: Rectangle {
                        color: parent.checked ? "#1a2538" : "#141720"
                        border.color: parent.checked ? "#00e5ff" : "#242c3d"
                        radius: 2
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 9
                        font.bold: true
                        color: parent.checked ? "#00e5ff" : "#8894ab"
                    }
                }

                Button {
                    height: 24
                    text: "TUNING [C]"
                    checkable: true
                    checked: root.showRightPanel && root.rightPanelMode === 0
                    onClicked: {
                        if (root.showRightPanel && root.rightPanelMode === 0) {
                            root.showRightPanel = false;
                        } else {
                            root.showRightPanel = true;
                            root.rightPanelMode = 0;
                        }
                    }
                    background: Rectangle {
                        color: parent.checked ? "#1a2538" : "#141720"
                        border.color: parent.checked ? "#00e5ff" : "#242c3d"
                        radius: 2
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 9
                        font.bold: true
                        color: parent.checked ? "#00e5ff" : "#8894ab"
                    }
                }

                Button {
                    height: 24
                    text: "DIAGS [D]"
                    checkable: true
                    checked: root.showRightPanel && root.rightPanelMode === 1
                    onClicked: {
                        if (root.showRightPanel && root.rightPanelMode === 1) {
                            root.showRightPanel = false;
                        } else {
                            root.showRightPanel = true;
                            root.rightPanelMode = 1;
                        }
                    }
                    background: Rectangle {
                        color: parent.checked ? "#1a2538" : "#141720"
                        border.color: parent.checked ? "#00e676" : "#242c3d"
                        radius: 2
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 9
                        font.bold: true
                        color: parent.checked ? "#00e676" : "#8894ab"
                    }
                }

                Button {
                    height: 24
                    text: "CHART [T]"
                    checkable: true
                    checked: root.showTrajectoryChart
                    onClicked: root.showTrajectoryChart = !root.showTrajectoryChart
                    background: Rectangle {
                        color: parent.checked ? "#1a2538" : "#141720"
                        border.color: parent.checked ? "#ff9100" : "#242c3d"
                        radius: 2
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 9
                        font.bold: true
                        color: parent.checked ? "#ff9100" : "#8894ab"
                    }
                }
            }
        }

        // Secondary Comparison Controls Toolbar
        ComparisonControls {
            width: parent.width
            targetView: videoView
        }
    }

    // Central Multi-Panel Layout
    Item {
        anchors.fill: parent
        focus: true

        Keys.onSpacePressed: {
            if (videoView.presentationMode === StabilizerQuickItem.SingleToggle) {
                videoView.toggleSourceView();
            } else {
                if (controller.playbackState === StabilizerPlayerController.Playing) {
                    controller.pause();
                } else {
                    controller.play();
                }
            }
        }

        Keys.onLeftPressed: controller.stepFrame()
        Keys.onRightPressed: controller.stepFrame()

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_T) {
                root.showTrajectoryChart = !root.showTrajectoryChart;
                event.accepted = true;
            } else if (event.key === Qt.Key_S) {
                root.showSourcePanel = !root.showSourcePanel;
                event.accepted = true;
            } else if (event.key === Qt.Key_C) {
                root.showRightPanel = true;
                root.rightPanelMode = 0;
                event.accepted = true;
            } else if (event.key === Qt.Key_D) {
                root.showRightPanel = true;
                root.rightPanelMode = 1;
                event.accepted = true;
            } else if (event.key === Qt.Key_1) {
                controller.loadPreset("Drone Gimbal");
                event.accepted = true;
            } else if (event.key === Qt.Key_2) {
                controller.loadPreset("Pole Vibration");
                event.accepted = true;
            } else if (event.key === Qt.Key_3) {
                controller.loadPreset("Handheld Action Cam");
                event.accepted = true;
            } else if (event.key === Qt.Key_4) {
                controller.loadPreset("Rolling Shutter");
                event.accepted = true;
            } else if (event.key === Qt.Key_5) {
                controller.loadPreset("Bypass");
                event.accepted = true;
            }
        }

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // Collapsible Left Drawer: Source Management
            SourcePanel {
                Layout.fillHeight: true
                Layout.preferredWidth: 320
                visible: root.showSourcePanel
                controller: controller
            }

            // Center Viewport & Bottom Strip-Chart
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                StabilizerQuickItem {
                    id: videoView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    controller: controller
                    splitPosition: 0.5
                    showGrid: true
                    showLandmarks: true
                    showQuiver: true
                    showCropMargin: true
                    showReticle: true
                }

                TrajectoryChart {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 140
                    visible: root.showTrajectoryChart
                    controller: controller
                }
            }

            // Collapsible Right Drawer: Tuning & Diagnostics
            StackLayout {
                Layout.fillHeight: true
                Layout.preferredWidth: 360
                visible: root.showRightPanel
                currentIndex: root.rightPanelMode

                StabilizerControlPanel {
                    controller: controller
                }

                DiagnosticsPanel {
                    controller: controller
                }
            }
        }
    }

    // Bottom Status Bar
    footer: Rectangle {
        height: 28
        color: "#11141c"
        border.color: "#1e2230"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 16

            Text {
                text: "FPS: " + controller.fps.toFixed(1)
                color: "#8894ab"
                font.pixelSize: 11
            }

            Text {
                text: "DIMENSIONS: " + (controller.videoWidth > 0 ? (controller.videoWidth + "x" + controller.videoHeight) : "NO SIGNAL")
                color: "#8894ab"
                font.pixelSize: 11
            }

            Text {
                text: {
                    switch (videoView.presentationMode) {
                    case StabilizerQuickItem.SplitWipe: return "MODE: SPLIT WIPE (" + Math.round(videoView.splitPosition * 100) + "%)";
                    case StabilizerQuickItem.SideBySide: return "MODE: SIDE-BY-SIDE DUAL";
                    case StabilizerQuickItem.SingleToggle: return "MODE: " + (videoView.showStabilizedInToggle ? "STABILIZED" : "RAW UNSTABILIZED");
                    case StabilizerQuickItem.DifferenceHeatmap: return "MODE: DIFFERENCE HEATMAP";
                    default: return "MODE: UNKNOWN";
                    }
                }
                color: "#00e5ff"
                font.pixelSize: 11
                font.bold: true
            }

            Text {
                text: "INLIERS: " + controller.inlierCount + " (" + (controller.inlierRatio * 100).toFixed(0) + "%)"
                color: "#00e676"
                font.pixelSize: 11
            }

            Text {
                text: "LATENCY: " + controller.totalLatencyMs.toFixed(2) + " ms"
                color: "#8894ab"
                font.pixelSize: 11
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "PelcoD Digital Video Stabilizer"
                color: "#525c70"
                font.pixelSize: 10
            }
        }
    }
}
