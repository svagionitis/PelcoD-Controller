import QtQuick
import QtQuick.Controls

Item {
    id: control
    width: parent ? parent.width : 280
    height: 48

    property string label: ""
    property string unit: ""
    property real from: 0.0
    property real to: 100.0
    property real value: 0.0
    property real stepSize: 1.0
    property int decimals: 0
    property bool isLogarithmic: false

    signal valueModified(real newValue)

    Column {
        anchors.fill: parent
        spacing: 4

        // Header: Label & Value readout
        Row {
            width: parent.width

            Text {
                text: control.label
                color: "#c0c8d8"
                font.pixelSize: 11
                font.bold: true
                elide: Text.ElideRight
                width: parent.width - valueBadge.width - 8
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle {
                id: valueBadge
                width: Math.max(54, valueText.implicitWidth + 12)
                height: 20
                radius: 3
                color: "#181d28"
                border.color: slider.pressed ? "#00e5ff" : "#2a3244"
                border.width: 1
                anchors.verticalCenter: parent.verticalCenter

                TextInput {
                    id: valueText
                    anchors.centerIn: parent
                    text: control.isLogarithmic
                          ? control.value.toExponential(2)
                          : control.value.toFixed(control.decimals) + (control.unit !== "" ? " " + control.unit : "")
                    color: "#00e5ff"
                    font.pixelSize: 11
                    font.family: "Monospace"
                    font.bold: true
                    selectByMouse: true
                    onEditingFinished: {
                        var parsed = parseFloat(text);
                        if (!isNaN(parsed)) {
                            var clamped = Math.max(control.from, Math.min(control.to, parsed));
                            control.value = clamped;
                            control.valueModified(clamped);
                        }
                    }
                }
            }
        }

        // Slider track
        Slider {
            id: slider
            width: parent.width
            height: 20
            from: control.from
            to: control.to
            stepSize: control.stepSize
            value: control.value

            onMoved: {
                control.value = slider.value;
                control.valueModified(slider.value);
            }

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 4
                width: slider.availableWidth
                height: implicitHeight
                radius: 2
                color: "#1c2230"

                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    color: "#00b4d8"
                    radius: 2
                }
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                implicitWidth: 14
                implicitHeight: 14
                radius: 7
                color: slider.pressed ? "#00e5ff" : (slider.hovered ? "#90e0ef" : "#48cae4")
                border.color: "#ffffff"
                border.width: slider.pressed ? 2 : 1

                Behavior on color {
                    ColorAnimation { duration: 100 }
                }
            }

            WheelHandler {
                onWheel: (event) => {
                    var delta = event.angleDelta.y > 0 ? control.stepSize : -control.stepSize;
                    var newVal = Math.max(control.from, Math.min(control.to, control.value + delta));
                    control.value = newVal;
                    control.valueModified(newVal);
                }
            }
        }
    }
}

