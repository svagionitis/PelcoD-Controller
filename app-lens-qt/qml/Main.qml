import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import LensApp 1.0
import "Components"

ApplicationWindow {
    id: root
    visible: true
    width: 1440
    height: 860
    minimumWidth: 960
    minimumHeight: 640
    title: "PelcoD Motorized Lens Controller & AF Testbench"
    color: "#0e1014"

    LensPlayerController {
        id: controller
    }

    // Top Tactical Header Bar
    header: Rectangle {
        width: parent.width
        height: 48
        color: "#141720"
        border.color: "#242938"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            // Tactical Pulse Heartbeat Beacon
            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: "#00e676"

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    PropertyAnimation { to: 0.25; duration: 600 }
                    PropertyAnimation { to: 1.0; duration: 600 }
                }
            }

            Text {
                text: "LENS CONTROLLER TESTBENCH"
                color: "#f0f4fc"
                font.pixelSize: 13
                font.bold: true
                font.letterSpacing: 1.5
            }

            Rectangle {
                width: 1
                height: 20
                color: "#2a2f40"
            }

            // Camera Channel Selector
            RowLayout {
                spacing: 4

                Repeater {
                    model: [
                        controller.configModel.channel0Name,
                        controller.configModel.channel1Name,
                        controller.configModel.channel2Name,
                        controller.configModel.channel3Name
                    ]

                    Button {
                        id: chBtn
                        text: modelData
                        checkable: true
                        checked: controller.activeChannel === index
                        onClicked: controller.setActiveChannel(index)

                        contentItem: Text {
                            text: chBtn.text
                            color: chBtn.checked ? "#00e5ff" : "#808a9d"
                            font.pixelSize: 11
                            font.bold: chBtn.checked
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            implicitWidth: 120
                            implicitHeight: 28
                            color: chBtn.checked ? "#1f2738" : "#141720"
                            border.color: chBtn.checked ? "#00e5ff" : "#242938"
                            border.width: chBtn.checked ? 1.5 : 1
                            radius: 3
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Operating Mode Indicator Badge
            StatusBadge {
                active: true
                activeColor: controller.lensMode === 0 ? "#00e5ff" : "#00e676"
                stateText: {
                    switch (controller.lensMode) {
                    case 0: return "MANUAL JOG";
                    case 1: return "AF ONCE";
                    case 2: return "AF CONT";
                    case 3: return "ZOOM TRACK";
                    case 4: return "CALIBRATING";
                    default: return "IDLE";
                    }
                }
            }

            // Emergency Stop Button
            Button {
                id: eStopBtn
                text: "E-STOP"
                onClicked: controller.emergencyStop()

                contentItem: Text {
                    text: eStopBtn.text
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.0
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    implicitWidth: 80
                    implicitHeight: 28
                    color: eStopBtn.down ? "#b71c1c" : "#d32f2f"
                    border.color: "#ff5252"
                    border.width: 1
                    radius: 3
                }
            }
        }
    }

    // Main Central Workspace
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Left Source Panel
        SourcePanel {
            id: sourcePanel
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            controller: controller
        }

        // Center Viewport
        LensVideoView {
            id: videoView
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: controller
        }

        // Right Panel Deck with Tabbed Navigation
        Rectangle {
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            color: "#141720"
            border.color: "#242938"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Tab Header Bar
                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    color: "#0e1014"
                    border.color: "#242938"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        spacing: 2

                        Button {
                            id: tabAxisBtn
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            checkable: true
                            checked: rightPanelStack.currentIndex === 0
                            text: "AXIS CONTROL"
                            onClicked: rightPanelStack.currentIndex = 0

                            contentItem: Text {
                                text: tabAxisBtn.text
                                color: tabAxisBtn.checked ? "#00e676" : "#808a9d"
                                font.pixelSize: 10
                                font.bold: tabAxisBtn.checked
                                font.letterSpacing: 0.5
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                color: tabAxisBtn.checked ? "#141720" : "#0e1014"
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: 2
                                    color: tabAxisBtn.checked ? "#00e676" : "transparent"
                                }
                            }
                        }

                        Button {
                            id: tabAfBtn
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            checkable: true
                            checked: rightPanelStack.currentIndex === 1
                            text: "AUTOFOCUS LAB"
                            onClicked: rightPanelStack.currentIndex = 1

                            contentItem: Text {
                                text: tabAfBtn.text
                                color: tabAfBtn.checked ? "#00e5ff" : "#808a9d"
                                font.pixelSize: 10
                                font.bold: tabAfBtn.checked
                                font.letterSpacing: 0.5
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                color: tabAfBtn.checked ? "#141720" : "#0e1014"
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: 2
                                    color: tabAfBtn.checked ? "#00e5ff" : "transparent"
                                }
                            }
                        }
                    }
                }

                // StackLayout hosting Axis Deck vs AF Deck
                StackLayout {
                    id: rightPanelStack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: 0

                    LensControlPanel {
                        id: controlPanel
                        controller: controller
                    }

                    AutoFocusPanel {
                        id: autoFocusPanel
                        controller: controller
                    }
                }
            }
        }
    }

    // Bottom Tactical Status Bar
    footer: Rectangle {
        width: parent.width
        height: 32
        color: "#141720"
        border.color: "#242938"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            Text {
                text: "STATUS: " + controller.statusMessage
                color: "#808a9d"
                font.pixelSize: 10
                font.bold: true
            }

            Rectangle {
                width: 1
                height: 14
                color: "#2a2f40"
            }

            Text {
                text: "LIMITS:"
                color: "#808a9d"
                font.pixelSize: 10
                font.bold: true
            }

            StatusBadge {
                stateText: "NEAR"
                active: (controller.limitFlags & 0x01) !== 0
                activeColor: "#ff1744"
                inactiveColor: "#2a3449"
            }

            StatusBadge {
                stateText: "FAR"
                active: (controller.limitFlags & 0x02) !== 0
                activeColor: "#ff1744"
                inactiveColor: "#2a3449"
            }

            StatusBadge {
                stateText: "WIDE"
                active: (controller.limitFlags & 0x04) !== 0
                activeColor: "#ff1744"
                inactiveColor: "#2a3449"
            }

            StatusBadge {
                stateText: "TELE"
                active: (controller.limitFlags & 0x08) !== 0
                activeColor: "#ff1744"
                inactiveColor: "#2a3449"
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "CONTROL TICK: 30 HZ"
                color: "#00e676"
                font.family: "Monospace"
                font.pixelSize: 10
                font.bold: true
            }
        }
    }
}

