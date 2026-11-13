import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LensApp 1.0

Item {
    id: root

    property alias quickItem: viewportItem
    required property LensPlayerController controller

    // Viewport item filling available space
    LensVideoQuickItem {
        id: viewportItem
        anchors.fill: parent
        controller: root.controller
        showGrid: true
        showReticle: true
        showRoi: true
    }

    // Top-Left Optical Telemetry HUD Overlay
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 16
        width: hudLayout.implicitWidth + 24
        height: hudLayout.implicitHeight + 16
        color: Qt.rgba(0.06, 0.07, 0.10, 0.85)
        border.color: "#242938"
        border.width: 1
        radius: 4

        ColumnLayout {
            id: hudLayout
            anchors.centerIn: parent
            spacing: 4

            RowLayout {
                spacing: 8
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: "#00e676"
                }
                Text {
                    text: {
                        switch (root.controller.activeChannel) {
                        case 0: return root.controller.configModel.channel0Name;
                        case 1: return root.controller.configModel.channel1Name;
                        case 2: return root.controller.configModel.channel2Name;
                        case 3: return root.controller.configModel.channel3Name;
                        default: return "Channel " + root.controller.activeChannel;
                        }
                    }
                    color: "#f0f4fc"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.0
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#1f2432"
            }

            RowLayout {
                spacing: 16
                Text {
                    text: "FOCUS (F): " + ("00000" + root.controller.currentFocus).slice(-5)
                    color: "#00e5ff"
                    font.family: "Monospace"
                    font.pixelSize: 11
                    font.bold: true
                }
                Text {
                    text: "ZOOM (Z): " + ("00000" + root.controller.currentZoom).slice(-5)
                    color: "#00e676"
                    font.family: "Monospace"
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            Text {
                text: "MODE: " + {
                    switch (root.controller.lensMode) {
                    case 0: return "MANUAL JOG";
                    case 1: return "AUTOFOCUS (ONCE)";
                    case 2: return "AUTOFOCUS (CONTINUOUS)";
                    case 3: return "ZOOM TRACK ONLY";
                    case 4: return "CALIBRATING";
                    default: return "IDLE";
                    }
                }
                color: "#808a9d"
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 0.5
            }
        }
    }

    // Top-Right Viewport Display Controls
    RowLayout {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 8

        Button {
            text: "GRID"
            checkable: true
            checked: viewportItem.showGrid
            onToggled: viewportItem.showGrid = checked
            contentItem: Text {
                text: parent.text
                color: parent.checked ? "#00e5ff" : "#808a9d"
                font.pixelSize: 10
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth: 50
                implicitHeight: 24
                color: "#141720"
                border.color: parent.checked ? "#00e5ff" : "#242938"
                border.width: 1
                radius: 3
            }
        }

        Button {
            text: "RETICLE"
            checkable: true
            checked: viewportItem.showReticle
            onToggled: viewportItem.showReticle = checked
            contentItem: Text {
                text: parent.text
                color: parent.checked ? "#00e5ff" : "#808a9d"
                font.pixelSize: 10
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth: 64
                implicitHeight: 24
                color: "#141720"
                border.color: parent.checked ? "#00e5ff" : "#242938"
                border.width: 1
                radius: 3
            }
        }

        Button {
            text: "ROI"
            checkable: true
            checked: viewportItem.showRoi
            onToggled: viewportItem.showRoi = checked
            contentItem: Text {
                text: parent.text
                color: parent.checked ? "#ff9100" : "#808a9d"
                font.pixelSize: 10
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth: 44
                implicitHeight: 24
                color: "#141720"
                border.color: parent.checked ? "#ff9100" : "#242938"
                border.width: 1
                radius: 3
            }
        }
    }
}

