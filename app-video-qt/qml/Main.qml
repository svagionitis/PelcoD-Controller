import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import VideoApp 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 760
    minimumWidth: 800
    minimumHeight: 600
    title: "PelcoD Tactical Video Player"
    color: "#0e1014"

    VideoPlayerController {
        id: controller
    }

    // Top Tactical Header Bar
    header: Rectangle {
        height: 48
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
                    case VideoPlayerController.Playing: return "#00e676";
                    case VideoPlayerController.Opening: return "#ff9100";
                    case VideoPlayerController.Paused: return "#00e5ff";
                    case VideoPlayerController.Error: return "#ff1744";
                    default: return "#525c70";
                    }
                }
                SequentialAnimation on opacity {
                    running: controller.playbackState === VideoPlayerController.Playing
                             || controller.playbackState === VideoPlayerController.Opening
                    loops: Animation.Infinite
                    PropertyAnimation { to: 0.3; duration: 600 }
                    PropertyAnimation { to: 1.0; duration: 600 }
                }
            }

            Text {
                text: "PELCOD TACTICAL VIDEO DISPLAY"
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

            // Quick Status Pill
            Text {
                text: {
                    var b = controller.availableBackends.length > controller.backendIndex && controller.backendIndex >= 0
                          ? controller.availableBackends[controller.backendIndex] : "None";
                    return b.toUpperCase() + " // " + (controller.frameWidth > 0 ? (controller.frameWidth + "x" + controller.frameHeight) : "NO SIGNAL");
                }
                color: "#8894ab"
                font.pixelSize: 11
                font.family: "Monospace"
            }

            Item { Layout.fillWidth: true }

            // Snapshot Notification Toast Label
            Text {
                id: toastLabel
                opacity: 0.0
                text: ""
                color: "#00e676"
                font.pixelSize: 11
                font.bold: true

                Behavior on opacity {
                    NumberAnimation { duration: 250 }
                }

                Timer {
                    id: toastTimer
                    interval: 3500
                    onTriggered: toastLabel.opacity = 0.0
                }
            }

            // Toggle Sidebar Button
            Button {
                id: drawerToggleBtn
                Layout.preferredHeight: 32
                contentItem: RowLayout {
                    spacing: 6
                    Text {
                        text: rightPanel.visible ? "HIDE CONTROLS ❯" : "❮ SHOW CONTROLS"
                        color: "#00e5ff"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
                background: Rectangle {
                    color: parent.hovered ? "#243248" : "#1b202e"
                    radius: 4
                    border.color: "#2a2f40"
                }
                onClicked: rightPanel.visible = !rightPanel.visible
            }
        }
    }

    // Main Content Split Layout
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Center Video Viewport Area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            VideoItem {
                id: videoItem
                anchors.fill: parent

                Component.onCompleted: {
                    controller.attachVideoItem(videoItem);
                }
            }

            // Tactical Corner HUD Brackets
            Item {
                anchors.fill: parent
                anchors.margins: 12
                opacity: 0.4
                enabled: false

                // Top-Left Corner
                Rectangle { x: 0; y: 0; width: 24; height: 2; color: "#00e5ff" }
                Rectangle { x: 0; y: 0; width: 2; height: 24; color: "#00e5ff" }

                // Top-Right Corner
                Rectangle { anchors.top: parent.top; anchors.right: parent.right; width: 24; height: 2; color: "#00e5ff" }
                Rectangle { anchors.top: parent.top; anchors.right: parent.right; width: 2; height: 24; color: "#00e5ff" }

                // Bottom-Left Corner
                Rectangle { anchors.bottom: parent.bottom; anchors.left: parent.left; width: 24; height: 2; color: "#00e5ff" }
                Rectangle { anchors.bottom: parent.bottom; anchors.left: parent.left; width: 2; height: 24; color: "#00e5ff" }

                // Bottom-Right Corner
                Rectangle { anchors.bottom: parent.bottom; anchors.right: parent.right; width: 24; height: 2; color: "#00e5ff" }
                Rectangle { anchors.bottom: parent.bottom; anchors.right: parent.right; width: 2; height: 24; color: "#00e5ff" }
            }

            // Bottom Transport Controls Bar
            ControlsBar {
                id: controlsBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 16
                controller: controller
                videoItem: videoItem

                onSnapshotSaved: function(path) {
                    toastLabel.text = "SNAPSHOT SAVED: " + path.replace(/^.*[\\\/]/, '');
                    toastLabel.opacity = 1.0;
                    toastTimer.restart();
                }

                onToggleFullscreen: {
                    if (root.visibility === Window.FullScreen) {
                        root.showNormal();
                    } else {
                        root.showFullScreen();
                    }
                }
            }
        }

        // Right Sidebar Drawer
        Rectangle {
            id: rightPanel
            Layout.preferredWidth: 350
            Layout.fillHeight: true
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            visible: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Tab Selector
                Rectangle {
                    Layout.fillWidth: true
                    height: 40
                    color: "#181b24"
                    border.color: "#242938"

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        Repeater {
                            model: ["Source", "Filters", "Stats"]
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: tabStack.currentIndex === index ? "#1f2433" : "transparent"

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: 2
                                    color: tabStack.currentIndex === index ? "#00e5ff" : "transparent"
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: tabStack.currentIndex === index ? "#f0f4fc" : "#8894ab"
                                    font.pixelSize: 12
                                    font.bold: tabStack.currentIndex === index
                                    font.letterSpacing: 0.8
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: tabStack.currentIndex = index
                                }
                            }
                        }
                    }
                }

                // Tab Content Stack
                StackLayout {
                    id: tabStack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: 0

                    SourcePanel {
                        controller: controller
                    }

                    FilterPanel {
                        controller: controller
                    }

                    StatsPanel {
                        controller: controller
                    }
                }
            }
        }
    }
}
