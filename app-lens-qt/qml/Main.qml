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

        // Center Viewport
        LensVideoView {
            id: videoView
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: controller
        }

        // Right Quick Tactical Jog Panel
        Rectangle {
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            color: "#11141c"
            border.color: "#1f2432"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16

                Text {
                    text: "OPTICAL AXIS CONTROLS"
                    color: "#f0f4fc"
                    font.pixelSize: 12
                    font.bold: true
                    font.letterSpacing: 1.2
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#1f2432"
                }

                // Focus Axis Controls
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Text {
                            text: "FOCUS ACTUATION"
                            color: "#00e5ff"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: ("00000" + controller.currentFocus).slice(-5)
                            color: "#00e5ff"
                            font.family: "Monospace"
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            Layout.fillWidth: true
                            text: "NEAR (-50)"
                            onClicked: controller.moveFocusStep(-controller.configModel.microStepSize)
                            background: Rectangle {
                                implicitHeight: 32
                                color: "#161b26"
                                border.color: "#2a3449"
                                border.width: 1
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#f0f4fc"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "FAR (+50)"
                            onClicked: controller.moveFocusStep(controller.configModel.microStepSize)
                            background: Rectangle {
                                implicitHeight: 32
                                color: "#161b26"
                                border.color: "#2a3449"
                                border.width: 1
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#f0f4fc"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                // Zoom Axis Controls
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Text {
                            text: "ZOOM ACTUATION"
                            color: "#00e676"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: ("00000" + controller.currentZoom).slice(-5)
                            color: "#00e676"
                            font.family: "Monospace"
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            Layout.fillWidth: true
                            text: "WIDE (-50)"
                            onClicked: controller.moveZoomStep(-controller.configModel.microStepSize)
                            background: Rectangle {
                                implicitHeight: 32
                                color: "#161b26"
                                border.color: "#2a3449"
                                border.width: 1
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#f0f4fc"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "TELE (+50)"
                            onClicked: controller.moveZoomStep(controller.configModel.microStepSize)
                            background: Rectangle {
                                implicitHeight: 32
                                color: "#161b26"
                                border.color: "#2a3449"
                                border.width: 1
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#f0f4fc"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                // Halting Button
                Button {
                    Layout.fillWidth: true
                    text: "HALT MOTION"
                    onClicked: controller.stopMotion()
                    background: Rectangle {
                        implicitHeight: 32
                        color: "#261a1a"
                        border.color: "#5c2a2a"
                        border.width: 1
                        radius: 3
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#ff8a80"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Item { Layout.fillHeight: true }

                // Simulation Watermark info
                Rectangle {
                    Layout.fillWidth: true
                    height: 56
                    color: "#0a0c10"
                    border.color: "#1c2230"
                    border.width: 1
                    radius: 3

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2
                        Text {
                            text: "PHASE 1 SKELETON ACTIVE"
                            color: "#00e5ff"
                            font.pixelSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Text {
                            text: "MockLensDriver bound (Channels 0..3)"
                            color: "#808a9d"
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                        }
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

