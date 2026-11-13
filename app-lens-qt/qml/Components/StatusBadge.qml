import QtQuick

Rectangle {
    id: badge
    width: labelText.implicitWidth + 20
    height: 22
    radius: 3
    color: "#141720"
    border.color: beaconColor
    border.width: 1

    property string stateText: "IDLE"
    property bool active: false
    property color activeColor: "#00e676"
    property color inactiveColor: "#525c70"
    property bool showPulse: false

    readonly property color beaconColor: {
        if (!active) return inactiveColor;
        return activeColor;
    }

    Row {
        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            width: 7
            height: 7
            radius: 3.5
            color: badge.beaconColor
            anchors.verticalCenter: parent.verticalCenter

            SequentialAnimation on opacity {
                running: badge.showPulse && badge.active
                loops: Animation.Infinite
                PropertyAnimation { to: 0.3; duration: 500 }
                PropertyAnimation { to: 1.0; duration: 500 }
            }
        }

        Text {
            id: labelText
            text: badge.stateText.toUpperCase()
            color: badge.active ? "#f0f4fc" : "#808a9d"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 0.8
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}

