import QtQuick
import QtQuick.Controls

Item {
    id: btnRoot
    property string text: ""
    property color accentColor: "#00e5ff"
    property bool isPrimary: false
    property bool isDestructive: false
    property bool enabledState: true
    signal clicked()

    implicitWidth: 100
    implicitHeight: 34

    Rectangle {
        id: bgRect
        anchors.fill: parent
        radius: 4
        color: {
            if (!btnRoot.enabledState) return "#1e222d"
            if (mouseArea.pressed) return Qt.darker(accentColorResolved, 1.4)
            if (mouseArea.containsMouse) return Qt.darker(accentColorResolved, 1.15)
            return btnRoot.isPrimary ? accentColorResolved : "#161b24"
        }
        border.color: {
            if (!btnRoot.enabledState) return "#2a3040"
            if (btnRoot.isPrimary) return accentColorResolved
            if (mouseArea.containsMouse) return accentColorResolved
            return "#2e3748"
        }
        border.width: 1

        readonly property color accentColorResolved: {
            if (btnRoot.isDestructive) return "#ff1744"
            return btnRoot.accentColor
        }

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }

        Text {
            anchors.centerIn: parent
            text: btnRoot.text
            color: {
                if (!btnRoot.enabledState) return "#556175"
                if (btnRoot.isPrimary) return "#000000"
                if (btnRoot.isDestructive) return "#ff5252"
                return "#f1f5f9"
            }
            font.pixelSize: 12
            font.bold: true
            font.letterSpacing: 0.5
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            enabled: btnRoot.enabledState
            cursorShape: btnRoot.enabledState ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: btnRoot.clicked()
        }
    }
}
