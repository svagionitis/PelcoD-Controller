import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import StabilizerApp 1.0

Rectangle {
    id: root
    height: 40
    color: "#11141c"
    border.color: "#1e2230"
    border.width: 1

    property StabilizerQuickItem targetView: null

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Text {
            text: "VIEW MODE:"
            color: "#6c7a96"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 1.0
        }

        // Mode Segmented Buttons
        Repeater {
            model: [
                { name: "SPLIT WIPE", mode: StabilizerQuickItem.SplitWipe },
                { name: "SIDE BY SIDE", mode: StabilizerQuickItem.SideBySide },
                { name: "TOGGLE (SPACE)", mode: StabilizerQuickItem.SingleToggle },
                { name: "HEATMAP", mode: StabilizerQuickItem.DifferenceHeatmap }
            ]

            Rectangle {
                id: modeBtn
                width: modeText.implicitWidth + 16
                height: 26
                radius: 3
                color: (targetView && targetView.presentationMode === modelData.mode) ? "#1a3644" : (modeMouse.containsMouse ? "#1e2330" : "#141722")
                border.color: (targetView && targetView.presentationMode === modelData.mode) ? "#00e5ff" : "#283042"
                border.width: 1

                Text {
                    id: modeText
                    anchors.centerIn: parent
                    text: modelData.name
                    color: (targetView && targetView.presentationMode === modelData.mode) ? "#00e5ff" : (modeMouse.containsMouse ? "#f0f4fc" : "#8894ab")
                    font.pixelSize: 10
                    font.bold: true
                }

                MouseArea {
                    id: modeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (targetView) {
                            targetView.presentationMode = modelData.mode;
                        }
                    }
                }
            }
        }

        // Split Orientation Toggle (Visible when SplitWipe)
        Rectangle {
            visible: targetView && (targetView.presentationMode === StabilizerQuickItem.SplitWipe)
            width: orientText.implicitWidth + 14
            height: 26
            radius: 3
            color: orientMouse.containsMouse ? "#1e2330" : "#141722"
            border.color: "#283042"
            border.width: 1

            Text {
                id: orientText
                anchors.centerIn: parent
                text: (targetView && targetView.splitOrientation === StabilizerQuickItem.Vertical) ? "ORIENTATION: VERT" : "ORIENTATION: HORIZ"
                color: "#00e5ff"
                font.pixelSize: 10
                font.bold: true
            }

            MouseArea {
                id: orientMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (targetView) {
                        targetView.splitOrientation = (targetView.splitOrientation === StabilizerQuickItem.Vertical)
                            ? StabilizerQuickItem.Horizontal : StabilizerQuickItem.Vertical;
                    }
                }
            }
        }

        Rectangle {
            width: 1
            height: 18
            color: "#242938"
        }

        Text {
            text: "OVERLAYS:"
            color: "#6c7a96"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 1.0
        }

        // Overlay Toggle Buttons
        Repeater {
            model: [
                { name: "LANDMARKS", prop: "showLandmarks" },
                { name: "QUIVER", prop: "showQuiver" },
                { name: "CROP BOX", prop: "showCropMargin" },
                { name: "RETICLE", prop: "showReticle" }
            ]

            Rectangle {
                width: ovText.implicitWidth + 14
                height: 26
                radius: 3
                color: (targetView && targetView[modelData.prop]) ? "#133327" : (ovMouse.containsMouse ? "#1e2330" : "#141722")
                border.color: (targetView && targetView[modelData.prop]) ? "#00e676" : "#283042"
                border.width: 1

                Text {
                    id: ovText
                    anchors.centerIn: parent
                    text: modelData.name
                    color: (targetView && targetView[modelData.prop]) ? "#00e676" : (ovMouse.containsMouse ? "#f0f4fc" : "#6c7a96")
                    font.pixelSize: 10
                    font.bold: true
                }

                MouseArea {
                    id: ovMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (targetView) {
                            targetView[modelData.prop] = !targetView[modelData.prop];
                        }
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Zoom & Reset Indicator
        Rectangle {
            visible: targetView && (targetView.zoomFactor > 1.001)
            width: zoomText.implicitWidth + 14
            height: 24
            radius: 3
            color: "#262014"
            border.color: "#ff9100"
            border.width: 1

            Text {
                id: zoomText
                anchors.centerIn: parent
                text: "ZOOM: " + (targetView ? targetView.zoomFactor.toFixed(1) : "1.0") + "x"
                color: "#ff9100"
                font.pixelSize: 10
                font.bold: true
            }
        }

        Rectangle {
            width: resetText.implicitWidth + 14
            height: 26
            radius: 3
            color: resetMouse.containsMouse ? "#2a1c22" : "#1b1419"
            border.color: "#802b3e"
            border.width: 1

            Text {
                id: resetText
                anchors.centerIn: parent
                text: "RESET VIEW"
                color: "#ff5252"
                font.pixelSize: 10
                font.bold: true
            }

            MouseArea {
                id: resetMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (targetView) {
                        targetView.resetZoomAndPan();
                    }
                }
            }
        }
    }
}

