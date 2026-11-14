import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Components"

ScrollView {
    id: root

    property var controller: null

    implicitWidth: 320
    implicitHeight: 720
    clip: true

    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    Column {
        width: root.width - 12
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12
        topPadding: 8
        bottomPadding: 16

        // =====================================================================
        // SECTION 1: FOCUS AXIS DECK
        // =====================================================================
        Rectangle {
            width: parent.width
            height: focusCol.implicitHeight + 20
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: focusCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 8

                // Header Row
                Row {
                    width: parent.width
                    height: 22

                    Text {
                        text: "FOCUS ACTUATION"
                        color: "#00e676"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Item { width: 1; height: 1; Layout.fillWidth: true }

                    Row {
                        spacing: 4
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter

                        StatusBadge {
                            stateText: "NEAR"
                            active: root.controller ? ((root.controller.limitFlags & 0x01) !== 0) : false
                            activeColor: "#ff1744"
                            inactiveColor: "#2e364a"
                        }

                        StatusBadge {
                            stateText: "FAR"
                            active: root.controller ? ((root.controller.limitFlags & 0x02) !== 0) : false
                            activeColor: "#ff1744"
                            inactiveColor: "#2e364a"
                        }
                    }
                }

                // Rotary Jog Wheel
                JogWheel {
                    anchors.horizontalCenter: parent.horizontalCenter
                    label: "FOCUS JOG"
                    accentColor: "#00e676"
                    stepMultiplier: 10
                    onStepTriggered: function(steps) {
                        if (root.controller) {
                            root.controller.moveFocusStep(steps);
                        }
                    }
                }

                // Micro-Stepping Quick Buttons
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 4

                    Repeater {
                        model: [-100, -10, -1, 1, 10, 100]
                        Rectangle {
                            width: 38
                            height: 22
                            radius: 3
                            color: "#10131a"
                            border.color: "#242938"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: (modelData > 0 ? "+" : "") + modelData
                                color: (modelData > 0) ? "#00e676" : "#808a9d"
                                font.pixelSize: 9
                                font.bold: true
                                font.family: "Monospace"
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.controller) {
                                        root.controller.moveFocusStep(modelData);
                                    }
                                }
                            }
                        }
                    }
                }

                // Position Range Slider
                TacticalSlider {
                    width: parent.width
                    label: "FOCUS POSITION"
                    value: root.controller ? root.controller.currentFocus : 0
                    minimumValue: root.controller ? root.controller.focusMin : 0
                    maximumValue: root.controller ? root.controller.focusMax : 65535
                    accentColor: "#00e676"
                    onValueCommitted: function(newPos) {
                        if (root.controller) {
                            root.controller.setFocusTarget(newPos);
                        }
                    }
                }

                // Speed Presets
                Row {
                    width: parent.width
                    height: 22
                    spacing: 4

                    Text {
                        text: "SPEED:"
                        color: "#525c70"
                        font.pixelSize: 9
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Repeater {
                        model: [
                            { name: "CREEP", spd: 32 },
                            { name: "NORM", spd: 127 },
                            { name: "FAST", spd: 200 },
                            { name: "TURBO", spd: 255 }
                        ]

                        Rectangle {
                            width: 50
                            height: 20
                            radius: 2
                            color: (root.controller && root.controller.focusSpeed === modelData.spd) ? "#00e676" : "#10131a"
                            border.color: (root.controller && root.controller.focusSpeed === modelData.spd) ? "#00e676" : "#242938"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: modelData.name
                                color: (root.controller && root.controller.focusSpeed === modelData.spd) ? "#0a0c10" : "#808a9d"
                                font.pixelSize: 8
                                font.bold: true
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.controller) {
                                        root.controller.setFocusSpeed(modelData.spd);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // SECTION 2: ZOOM AXIS DECK
        // =====================================================================
        Rectangle {
            width: parent.width
            height: zoomCol.implicitHeight + 20
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: zoomCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 8

                // Header Row
                Row {
                    width: parent.width
                    height: 22

                    Text {
                        text: "ZOOM ACTUATION"
                        color: "#00e5ff"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Item { width: 1; height: 1; Layout.fillWidth: true }

                    Row {
                        spacing: 4
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter

                        StatusBadge {
                            stateText: "WIDE"
                            active: root.controller ? ((root.controller.limitFlags & 0x04) !== 0) : false
                            activeColor: "#ff1744"
                            inactiveColor: "#2e364a"
                        }

                        StatusBadge {
                            stateText: "TELE"
                            active: root.controller ? ((root.controller.limitFlags & 0x08) !== 0) : false
                            activeColor: "#ff1744"
                            inactiveColor: "#2e364a"
                        }
                    }
                }

                // Rotary Jog Wheel
                JogWheel {
                    anchors.horizontalCenter: parent.horizontalCenter
                    label: "ZOOM JOG"
                    accentColor: "#00e5ff"
                    stepMultiplier: 10
                    onStepTriggered: function(steps) {
                        if (root.controller) {
                            root.controller.moveZoomStep(steps);
                        }
                    }
                }

                // Micro-Stepping Quick Buttons
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 4

                    Repeater {
                        model: [-100, -10, -1, 1, 10, 100]
                        Rectangle {
                            width: 38
                            height: 22
                            radius: 3
                            color: "#10131a"
                            border.color: "#242938"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: (modelData > 0 ? "+" : "") + modelData
                                color: (modelData > 0) ? "#00e5ff" : "#808a9d"
                                font.pixelSize: 9
                                font.bold: true
                                font.family: "Monospace"
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.controller) {
                                        root.controller.moveZoomStep(modelData);
                                    }
                                }
                            }
                        }
                    }
                }

                // Position Range Slider
                TacticalSlider {
                    width: parent.width
                    label: "ZOOM POSITION"
                    value: root.controller ? root.controller.currentZoom : 0
                    minimumValue: root.controller ? root.controller.zoomMin : 0
                    maximumValue: root.controller ? root.controller.zoomMax : 65535
                    accentColor: "#00e5ff"
                    onValueCommitted: function(newPos) {
                        if (root.controller) {
                            root.controller.setZoomTarget(newPos);
                        }
                    }
                }

                // Speed Presets
                Row {
                    width: parent.width
                    height: 22
                    spacing: 4

                    Text {
                        text: "SPEED:"
                        color: "#525c70"
                        font.pixelSize: 9
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Repeater {
                        model: [
                            { name: "CREEP", spd: 32 },
                            { name: "NORM", spd: 127 },
                            { name: "FAST", spd: 200 },
                            { name: "TURBO", spd: 255 }
                        ]

                        Rectangle {
                            width: 50
                            height: 20
                            radius: 2
                            color: (root.controller && root.controller.zoomSpeed === modelData.spd) ? "#00e5ff" : "#10131a"
                            border.color: (root.controller && root.controller.zoomSpeed === modelData.spd) ? "#00e5ff" : "#242938"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: modelData.name
                                color: (root.controller && root.controller.zoomSpeed === modelData.spd) ? "#0a0c10" : "#808a9d"
                                font.pixelSize: 8
                                font.bold: true
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.controller) {
                                        root.controller.setZoomSpeed(modelData.spd);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // SECTION 3: DRIVER & PORT BINDING
        // =====================================================================
        Rectangle {
            width: parent.width
            height: driverCol.implicitHeight + 16
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: driverCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 8

                Row {
                    width: parent.width
                    height: 20

                    Text {
                        text: "HARDWARE DRIVER"
                        color: "#808a9d"
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 0.8
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Item { width: 1; height: 1; Layout.fillWidth: true }

                    StatusBadge {
                        stateText: root.controller ? root.controller.driverName : "MOCK"
                        active: true
                        activeColor: (root.controller && root.controller.isPortConnected) ? "#00e676" : "#00e5ff"
                        anchors.right: parent.right
                    }
                }

                Row {
                    width: parent.width
                    height: 18

                    Text {
                        text: {
                            if (!root.controller || root.controller.activeDriverType === 0) {
                                return "PORT: IN-MEMORY (SIMULATION)";
                            }
                            return "PORT: " + root.controller.boundPortName + " @ " + root.controller.boundBaudRate + " BPS (ADDR " + root.controller.cameraAddress + ")";
                        }
                        color: "#525c70"
                        font.pixelSize: 9
                        font.family: "Monospace"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 26
                    radius: 3
                    color: "#1c212d"
                    border.color: "#00e5ff"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "CONFIGURE HARDWARE DRIVER..."
                        color: "#00e5ff"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            driverDialog.open();
                        }
                    }
                }
            }
        }

        // =====================================================================
        // SECTION 4: EMERGENCY STOP
        // =====================================================================
        Rectangle {
            id: eStopBtn
            width: parent.width
            height: 44
            radius: 4
            color: eStopMouse.pressed ? "#d50000" : (eStopMouse.containsMouse ? "#ff1744" : "#b71c1c")
            border.color: "#ff5252"
            border.width: 1

            Row {
                anchors.centerIn: parent
                spacing: 8

                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: "#ffffff"
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "EMERGENCY STOP (HALT ALL AXES)"
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.0
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: eStopMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (root.controller) {
                        root.controller.emergencyStop();
                    }
                }
            }
        }
    }

    DriverConfigDialog {
        id: driverDialog
        controller: root.controller
    }
}
