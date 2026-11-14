import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    property string label: "AXIS POSITION"
    property int value: 0
    property int minimumValue: 0
    property int maximumValue: 65535
    property color accentColor: "#00e676"
    property bool readOnly: false

    signal valueCommitted(int newValue)
    signal sliderMoved(int newValue)

    implicitWidth: 280
    implicitHeight: 60

    readonly property real span: Math.max(1, root.maximumValue - root.minimumValue)
    readonly property real normalizedValue: Math.max(0.0, Math.min(1.0, (root.value - root.minimumValue) / span))

    Column {
        anchors.fill: parent
        spacing: 4

        // Top Row: Label and Editable / Readout Box
        Row {
            width: parent.width
            height: 22

            Text {
                text: root.label.toUpperCase()
                color: "#808a9d"
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 0.8
                anchors.verticalCenter: parent.verticalCenter
            }

            Item { width: 1; height: 1; Layout.fillWidth: true }

            Rectangle {
                width: 75
                height: 20
                radius: 3
                color: "#0a0c10"
                border.color: editInput.activeFocus ? root.accentColor : "#242938"
                border.width: 1
                anchors.right: parent.right

                TextInput {
                    id: editInput
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    verticalAlignment: TextInput.AlignVCenter
                    horizontalAlignment: TextInput.AlignRight
                    color: "#f0f4fc"
                    font.pixelSize: 11
                    font.bold: true
                    font.family: "Monospace"
                    selectByMouse: true
                    readOnly: root.readOnly
                    text: root.value.toString()

                    validator: IntValidator {
                        bottom: root.minimumValue
                        top: root.maximumValue
                    }

                    onAccepted: {
                        var val = parseInt(text);
                        if (!isNaN(val)) {
                            var clamped = Math.max(root.minimumValue, Math.min(root.maximumValue, val));
                            root.valueCommitted(clamped);
                        }
                        editInput.focus = false;
                    }
                }
            }
        }

        // Slider Track & Handle
        Item {
            id: trackContainer
            width: parent.width
            height: 20

            // Background Groove
            Rectangle {
                id: groove
                width: parent.width
                height: 6
                radius: 3
                color: "#141720"
                border.color: "#242938"
                border.width: 1
                anchors.verticalCenter: parent.verticalCenter

                // Active Fill
                Rectangle {
                    width: parent.width * root.normalizedValue
                    height: parent.height
                    radius: 3
                    color: root.accentColor
                    opacity: 0.85
                }
            }

            // Draggable Thumb Handle
            Rectangle {
                id: handle
                width: 14
                height: 14
                radius: 7
                color: root.accentColor
                border.color: "#ffffff"
                border.width: 1
                x: Math.max(0, Math.min(trackContainer.width - width, (trackContainer.width * root.normalizedValue) - (width * 0.5)))
                anchors.verticalCenter: parent.verticalCenter

                // Glow ring
                Rectangle {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    radius: 11
                    color: root.accentColor
                    opacity: handleMouse.containsMouse || handleMouse.pressed ? 0.35 : 0.0
                }
            }

            MouseArea {
                id: handleMouse
                anchors.fill: parent
                enabled: !root.readOnly
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                function updateFromMouse(mouseX) {
                    var norm = Math.max(0.0, Math.min(1.0, mouseX / trackContainer.width));
                    var computed = Math.round(root.minimumValue + (norm * root.span));
                    root.sliderMoved(computed);
                    return computed;
                }

                onPositionChanged: function(mouse) {
                    if (pressed) {
                        updateFromMouse(mouse.x);
                    }
                }

                onPressed: function(mouse) {
                    updateFromMouse(mouse.x);
                }

                onReleased: function(mouse) {
                    var finalVal = updateFromMouse(mouse.x);
                    root.valueCommitted(finalVal);
                }
            }
        }

        // Bottom Limits Readout
        Row {
            width: parent.width
            height: 12

            Text {
                text: "MIN: " + root.minimumValue
                color: "#525c70"
                font.pixelSize: 9
                font.family: "Monospace"
            }

            Item { width: 1; height: 1; Layout.fillWidth: true }

            Text {
                text: "MAX: " + root.maximumValue
                color: "#525c70"
                font.pixelSize: 9
                font.family: "Monospace"
                anchors.right: parent.right
            }
        }
    }
}
