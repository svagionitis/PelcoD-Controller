import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import StabilizerApp 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 760
    minimumWidth: 800
    minimumHeight: 600
    title: "PelcoD Digital Video Stabilizer Workbench"
    color: "#0e1014"

    StabilizerPlayerController {
        id: controller
    }

    // Top Tactical Header Bar & Comparison Controls
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
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "PHASE 3 PRESENTATION ACTIVE"
                    color: "#00e676"
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }

        // Secondary Comparison & Overlays Toolbar
        ComparisonControls {
            width: parent.width
            targetView: videoView
        }
    }

    // Central Presentation Viewport with Native Mouse/Keyboard Interaction
    Item {
        anchors.fill: parent
        focus: true

        Keys.onSpacePressed: {
            videoView.toggleSourceView();
        }

        StabilizerQuickItem {
            id: videoView
            anchors.fill: parent
            controller: controller
            splitPosition: 0.5
            showGrid: true
            showLandmarks: true
            showQuiver: true
            showCropMargin: true
            showReticle: true
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
                text: "MODE: " + {
                    switch (videoView.presentationMode) {
                    case StabilizerQuickItem.SplitWipe: return "SPLIT WIPE (" + Math.round(videoView.splitPosition * 100) + "%)";
                    case StabilizerQuickItem.SideBySide: return "SIDE-BY-SIDE DUAL";
                    case StabilizerQuickItem.SingleToggle: return videoView.showStabilizedInToggle ? "STABILIZED" : "RAW UNSTABILIZED";
                    case StabilizerQuickItem.DifferenceHeatmap: return "DIFFERENCE HEATMAP";
                    default: return "UNKNOWN";
                    }
                }
                color: "#00e5ff"
                font.pixelSize: 11
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "PelcoD Stabilizer Subsystem"
                color: "#525c70"
                font.pixelSize: 10
            }
        }
    }
}
