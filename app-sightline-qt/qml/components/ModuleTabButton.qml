import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Rectangle {
    id: root

    property string text: "Module"
    property string iconText: "📦"
    property bool selected: false
    property string badgeText: ""
    signal clicked()

    implicitWidth: 240
    implicitHeight: 38
    radius: SightlineTheme.radiusSmall
    color: root.selected ? Qt.rgba(0, 0.75, 1.0, 0.15) : (mouseArea.containsMouse ? SightlineTheme.surfaceLight : "transparent")
    border.color: root.selected ? SightlineTheme.primary : "transparent"
    border.width: 1

    Row {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        Text {
            text: root.iconText
            font.pixelSize: 16
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: root.text
            color: root.selected ? SightlineTheme.primary : (mouseArea.containsMouse ? SightlineTheme.textPrimary : SightlineTheme.textSecondary)
            font.pixelSize: SightlineTheme.fontSizeNormal
            font.bold: root.selected
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            width: root.width - (root.badgeText.length > 0 ? 80 : 50)
        }

        Item { width: 1; height: 1 }

        Rectangle {
            visible: root.badgeText.length > 0
            implicitWidth: badgeLabel.implicitWidth + 10
            implicitHeight: 18
            radius: 9
            color: root.selected ? SightlineTheme.primary : SightlineTheme.surfaceLight
            anchors.verticalCenter: parent.verticalCenter

            Text {
                id: badgeLabel
                anchors.centerIn: parent
                text: root.badgeText
                color: root.selected ? "#FFFFFF" : SightlineTheme.textMuted
                font.pixelSize: 10
                font.bold: true
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
