import QtQuick

Rectangle {
    id: badge
    width: labelText.implicitWidth + 24
    height: 22
    radius: 3
    color: "#141720"
    border.color: beaconColor
    border.width: 1

    property string stateText: "IDLE"
    property bool showPulse: true

    readonly property color beaconColor: {
        var s = stateText.toUpperCase();
        if (s.indexOf("TRACK") !== -1 || s.indexOf("PLAY") !== -1) return "#00e676";
        if (s.indexOf("INIT") !== -1 || s.indexOf("OPEN") !== -1) return "#ff9100";
        if (s.indexOf("DEGRAD") !== -1 || s.indexOf("WARN") !== -1) return "#ffd600";
        if (s.indexOf("PAUSE") !== -1) return "#00e5ff";
        if (s.indexOf("ERR") !== -1 || s.indexOf("FAIL") !== -1) return "#ff1744";
        return "#525c70";
    }

    Row {
        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: badge.beaconColor
            anchors.verticalCenter: parent.verticalCenter

            SequentialAnimation on opacity {
                running: badge.showPulse && badge.stateText.toUpperCase() !== "IDLE"
                loops: Animation.Infinite
                PropertyAnimation { to: 0.25; duration: 600 }
                PropertyAnimation { to: 1.0; duration: 600 }
            }
        }

        Text {
            id: labelText
            text: badge.stateText.toUpperCase()
            color: "#f0f4fc"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 1.0
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}

