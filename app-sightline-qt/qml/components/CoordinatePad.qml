import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Rectangle {
    id: root

    property int frameWidth: 1920
    property int frameHeight: 1080
    property int selectedCol: 960
    property int selectedRow: 540
    property int trackBoxW: 80
    property int trackBoxH: 80

    signal coordinatePicked(int col, int row)
    signal nudgeRequested(int deltaCol, int deltaRow)

    implicitWidth: 380
    implicitHeight: 260
    color: SightlineTheme.surfaceLight
    radius: SightlineTheme.radiusMedium
    border.color: SightlineTheme.cardBorder
    border.width: 1
    clip: true

    Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Viewport Header
        Row {
            width: parent.width
            Text {
                text: "CAMERA VIEWPORT COORDINATES (" + root.selectedCol + ", " + root.selectedRow + ")"
                color: SightlineTheme.textSecondary
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.bold: true
            }
        }

        // Viewport Canvas Area
        Rectangle {
            id: viewport
            width: parent.width
            height: 160
            color: "#0B0C0E"
            radius: SightlineTheme.radiusSmall
            border.color: SightlineTheme.cardBorder
            clip: true

            // Grid lines
            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: 1
                color: "#1E222A"
            }
            Rectangle {
                anchors.centerIn: parent
                width: 1
                height: parent.height
                color: "#1E222A"
            }

            // Target Gate Box
            Rectangle {
                id: targetGate
                x: (root.selectedCol / root.frameWidth) * viewport.width - width / 2
                y: (root.selectedRow / root.frameHeight) * viewport.height - height / 2
                width: Math.max(16, (root.trackBoxW / root.frameWidth) * viewport.width)
                height: Math.max(16, (root.trackBoxH / root.frameHeight) * viewport.height)
                color: "transparent"
                border.color: SightlineTheme.success
                border.width: 2

                // Center dot
                Rectangle {
                    anchors.centerIn: parent
                    width: 4
                    height: 4
                    radius: 2
                    color: SightlineTheme.success
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.CrossCursor

                function updateCoord(mouse) {
                    var c = Math.max(0, Math.min(root.frameWidth, Math.round((mouse.x / viewport.width) * root.frameWidth)));
                    var r = Math.max(0, Math.min(root.frameHeight, Math.round((mouse.y / viewport.height) * root.frameHeight)));
                    root.selectedCol = c;
                    root.selectedRow = r;
                    root.coordinatePicked(c, r);
                }

                onClicked: updateCoord(mouse)
                onPositionChanged: {
                    if (pressed) {
                        updateCoord(mouse);
                    }
                }
            }
        }

        // Nudge and Center Controls
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8

            Button {
                text: "◄"
                implicitWidth: 36
                implicitHeight: 30
                onClicked: root.nudgeRequested(-5, 0)
            }
            Button {
                text: "▲"
                implicitWidth: 36
                implicitHeight: 30
                onClicked: root.nudgeRequested(0, -5)
            }
            Button {
                text: "▼"
                implicitWidth: 36
                implicitHeight: 30
                onClicked: root.nudgeRequested(0, 5)
            }
            Button {
                text: "►"
                implicitWidth: 36
                implicitHeight: 30
                onClicked: root.nudgeRequested(5, 0)
            }
            Button {
                text: "Center"
                implicitHeight: 30
                onClicked: {
                    root.selectedCol = root.frameWidth / 2;
                    root.selectedRow = root.frameHeight / 2;
                    root.coordinatePicked(root.selectedCol, root.selectedRow);
                }
            }
        }
    }
}
