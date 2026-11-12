import QtQuick
import QtQuick.Controls

Item {
    id: control
    width: parent ? parent.width : 280
    height: 32

    property string text: ""
    property alias checked: switchControl.checked
    property alias enabled: switchControl.enabled

    signal toggled(bool isChecked)

    Row {
        anchors.fill: parent
        spacing: 8

        Text {
            text: control.text
            color: control.enabled ? "#c0c8d8" : "#555e70"
            font.pixelSize: 11
            font.bold: true
            elide: Text.ElideRight
            width: parent.width - switchControl.width - 8
            anchors.verticalCenter: parent.verticalCenter
        }

        Switch {
            id: switchControl
            anchors.verticalCenter: parent.verticalCenter

            onToggled: {
                control.toggled(checked);
            }

            indicator: Rectangle {
                implicitWidth: 38
                implicitHeight: 20
                radius: 10
                color: switchControl.checked ? "#103b2b" : "#1a202c"
                border.color: switchControl.checked ? "#00e676" : "#2d3748"
                border.width: 1

                Rectangle {
                    x: switchControl.checked ? parent.width - width - 2 : 2
                    y: 2
                    width: 16
                    height: 16
                    radius: 8
                    color: switchControl.checked ? "#00e676" : "#718096"

                    Behavior on x {
                        NumberAnimation { duration: 120; easing.type: Easing.InOutQuad }
                    }
                    Behavior on color {
                        ColorAnimation { duration: 120 }
                    }
                }
            }
        }
    }
}

