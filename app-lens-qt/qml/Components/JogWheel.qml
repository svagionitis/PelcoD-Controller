import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    property string label: "FOCUS JOG"
    property color accentColor: "#00e676"  // Emerald for Focus, Cyan for Zoom
    property int stepMultiplier: 10
    property bool enabled: true

    signal stepTriggered(int steps)
    signal jogStarted()
    signal jogFinished()

    implicitWidth: 160
    implicitHeight: 185

    property real currentAngleDeg: 0.0
    property real lastMouseAngleRad: 0.0
    property real accumulatedAngleRad: 0.0
    property real radiansPerStep: 0.087266  // ~5 degrees per base step
    property int lastDirection: 0
    property int sessionSteps: 0

    Column {
        anchors.fill: parent
        spacing: 6
        anchors.horizontalCenter: parent.horizontalCenter

        // Header Label
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label.toUpperCase()
            color: root.enabled ? root.accentColor : "#525c70"
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 1.0
        }

        // Rotary Dial Container
        Item {
            id: dialContainer
            width: 130
            height: 130
            anchors.horizontalCenter: parent.horizontalCenter

            // Outer Ring
            Rectangle {
                anchors.fill: parent
                radius: width * 0.5
                color: "#141720"
                border.color: root.enabled ? "#242938" : "#1a1e29"
                border.width: 2

                // Tick Ring
                Repeater {
                    model: 36
                    Rectangle {
                        id: tick
                        width: (index % 3 === 0) ? 2 : 1
                        height: (index % 3 === 0) ? 7 : 4
                        color: (index % 3 === 0) ? root.accentColor : "#3a4459"
                        opacity: root.enabled ? 0.7 : 0.2
                        x: dialContainer.width * 0.5 - width * 0.5
                        y: 3
                        transformOrigin: Item.Bottom
                        transform: [
                            Translate { y: dialContainer.height * 0.5 - 3 },
                            Rotation { angle: index * 10 }
                        ]
                    }
                }

                // Rotating Disc
                Rectangle {
                    id: rotatingDisc
                    width: parent.width - 24
                    height: parent.height - 24
                    radius: width * 0.5
                    anchors.centerIn: parent
                    color: "#10131a"
                    border.color: root.enabled ? "#2e364a" : "#1c212d"
                    border.width: 1
                    rotation: root.currentAngleDeg

                    // Perimeter Indicator Dot
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: root.enabled ? root.accentColor : "#525c70"
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 6

                        // Subtle glow halo
                        Rectangle {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            radius: 7
                            color: root.accentColor
                            opacity: 0.35
                            visible: root.enabled
                        }
                    }
                }

                // Center Hub with Readout
                Rectangle {
                    width: 58
                    height: 58
                    radius: 29
                    anchors.centerIn: parent
                    color: "#0a0c10"
                    border.color: root.enabled ? root.accentColor : "#242938"
                    border.width: 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 1

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: {
                                if (root.lastDirection > 0) return "CW ▶";
                                if (root.lastDirection < 0) return "◀ CCW";
                                return "HOLD";
                            }
                            color: root.enabled ? (root.lastDirection !== 0 ? root.accentColor : "#606b82") : "#404859"
                            font.pixelSize: 8
                            font.bold: true
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: (root.sessionSteps >= 0 ? "+" : "") + root.sessionSteps
                            color: "#f0f4fc"
                            font.pixelSize: 10
                            font.bold: true
                            font.family: "Monospace"
                        }
                    }
                }
            }

            // Mouse Area for Rotary Tracking
            MouseArea {
                id: mouseArea
                anchors.fill: parent
                enabled: root.enabled
                hoverEnabled: true

                onPressed: function(mouse) {
                    var cx = dialContainer.width * 0.5;
                    var cy = dialContainer.height * 0.5;
                    root.lastMouseAngleRad = Math.atan2(mouse.y - cy, mouse.x - cx);
                    root.accumulatedAngleRad = 0.0;
                    root.sessionSteps = 0;
                    root.jogStarted();
                }

                onPositionChanged: function(mouse) {
                    if (!pressed) return;
                    var cx = dialContainer.width * 0.5;
                    var cy = dialContainer.height * 0.5;
                    var angle = Math.atan2(mouse.y - cy, mouse.x - cx);
                    var delta = angle - root.lastMouseAngleRad;

                    // Unwrap branch cut
                    if (delta > Math.PI) {
                        delta -= 2.0 * Math.PI;
                    } else if (delta < -Math.PI) {
                        delta += 2.0 * Math.PI;
                    }

                    root.lastMouseAngleRad = angle;
                    root.currentAngleDeg += (delta * 180.0 / Math.PI);
                    root.accumulatedAngleRad += delta;

                    // Step generation
                    var stepThreshold = root.radiansPerStep;
                    while (Math.abs(root.accumulatedAngleRad) >= stepThreshold) {
                        var dir = (root.accumulatedAngleRad > 0.0) ? 1 : -1;
                        root.lastDirection = dir;
                        var stepCount = dir * root.stepMultiplier;
                        root.sessionSteps += stepCount;
                        root.stepTriggered(stepCount);
                        root.accumulatedAngleRad -= (dir * stepThreshold);
                    }
                }

                onReleased: function() {
                    root.jogFinished();
                }
            }
        }

        // Multiplier Selector Row
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 3

            Repeater {
                model: [1, 10, 100, 500]

                Rectangle {
                    width: 32
                    height: 20
                    radius: 3
                    color: root.stepMultiplier === modelData ? root.accentColor : "#141720"
                    border.color: root.stepMultiplier === modelData ? root.accentColor : "#242938"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "×" + modelData
                        color: root.stepMultiplier === modelData ? "#0a0c10" : "#808a9d"
                        font.pixelSize: 9
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.stepMultiplier = modelData;
                        }
                    }
                }
            }
        }
    }
}
