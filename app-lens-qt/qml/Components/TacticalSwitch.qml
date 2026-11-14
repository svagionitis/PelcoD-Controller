import QtQuick 2.15

Item {
    id: root

    property string label: "TOGGLE"
    property bool checked: false
    property color accentColor: "#00e676"
    property bool enabled: true

    signal toggled(bool isChecked)

    implicitWidth: 160
    implicitHeight: 24

    Row {
        anchors.fill: parent
        spacing: 8

        Text {
            text: root.label.toUpperCase()
            color: root.enabled ? "#f0f4fc" : "#525c70"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 0.8
            anchors.verticalCenter: parent.verticalCenter
        }

        Item { width: 1; height: 1; Layout.fillWidth: true }

        Rectangle {
            id: track
            width: 36
            height: 18
            radius: 9
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            color: root.checked ? "#1a2a22" : "#141720"
            border.color: root.checked ? root.accentColor : "#242938"
            border.width: 1

            Rectangle {
                id: thumb
                width: 12
                height: 12
                radius: 6
                anchors.verticalCenter: parent.verticalCenter
                x: root.checked ? track.width - width - 3 : 3
                color: root.checked ? root.accentColor : "#525c70"

                Behavior on x {
                    NumberAnimation { duration: 120 }
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: root.enabled
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.checked = !root.checked;
                    root.toggled(root.checked);
                }
            }
        }
    }
}
