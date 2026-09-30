import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
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

    implicitWidth: 360
    implicitHeight: 250
    color: SightlineTheme.surfaceCard
    radius: SightlineTheme.radiusMedium
    border.color: SightlineTheme.cardBorder
    border.width: 1
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        // Viewport Header
        Text {
            text: "VIEWPORT RETICLE (" + root.selectedCol + ", " + root.selectedRow + ")"
            color: SightlineTheme.textSecondary
            font.pixelSize: 10
            font.bold: true
            Layout.fillWidth: true
        }

        // Viewport Canvas Area
        Rectangle {
            id: viewport
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            color: "#08090c"
            radius: SightlineTheme.radiusSmall
            border.color: SightlineTheme.cardBorder
            clip: true

            // Grid lines
            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: 1
                color: "#181b24"
            }
            Rectangle {
                anchors.centerIn: parent
                width: 1
                height: parent.height
                color: "#181b24"
            }

            // Target Gate Box
            Rectangle {
                id: targetGate
                x: (root.selectedCol / root.frameWidth) * viewport.width - width / 2
                y: (root.selectedRow / root.frameHeight) * viewport.height - height / 2
                width: Math.max(16, (root.trackBoxW / root.frameWidth) * viewport.width)
                height: Math.max(16, (root.trackBoxH / root.frameHeight) * viewport.height)
                color: "transparent"
                border.color: SightlineTheme.primary
                border.width: 2

                // Center cross dot
                Rectangle {
                    anchors.centerIn: parent
                    width: 4
                    height: 4
                    radius: 2
                    color: SightlineTheme.primary
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
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6

            Button {
                text: "◄"
                implicitWidth: 32
                implicitHeight: 26
                onClicked: root.nudgeRequested(-5, 0)
            }
            Button {
                text: "▲"
                implicitWidth: 32
                implicitHeight: 26
                onClicked: root.nudgeRequested(0, -5)
            }
            Button {
                text: "▼"
                implicitWidth: 32
                implicitHeight: 26
                onClicked: root.nudgeRequested(0, 5)
            }
            Button {
                text: "►"
                implicitWidth: 32
                implicitHeight: 26
                onClicked: root.nudgeRequested(5, 0)
            }
            Button {
                text: "Center"
                implicitHeight: 26
                onClicked: {
                    root.selectedCol = Math.round(root.frameWidth / 2);
                    root.selectedRow = Math.round(root.frameHeight / 2);
                    root.coordinatePicked(root.selectedCol, root.selectedRow);
                }
            }
        }
    }
}
