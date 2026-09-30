import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."

Rectangle {
    id: root

    property string text: "Module"
    property string iconText: "📦"
    property bool selected: false
    property string badgeText: ""
    signal clicked()

    Layout.fillWidth: true
    implicitHeight: 34
    radius: SightlineTheme.radiusSmall
    color: root.selected ? "#1e2838" : (mouseArea.containsMouse ? "#181f2a" : "transparent")
    border.color: root.selected ? SightlineTheme.primary : "transparent"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        Text {
            text: root.iconText
            font.pixelSize: 13
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            text: root.text
            color: root.selected ? SightlineTheme.primary : (mouseArea.containsMouse ? SightlineTheme.textPrimary : SightlineTheme.textSecondary)
            font.pixelSize: SightlineTheme.fontSizeNormal
            font.bold: root.selected
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            elide: Text.ElideRight
        }

        Rectangle {
            visible: root.badgeText.length > 0
            implicitWidth: badgeLabel.implicitWidth + 8
            implicitHeight: 16
            radius: 8
            color: root.selected ? SightlineTheme.primary : SightlineTheme.surfaceLight
            Layout.alignment: Qt.AlignVCenter

            Text {
                id: badgeLabel
                anchors.centerIn: parent
                text: root.badgeText
                color: root.selected ? "#0e1014" : SightlineTheme.textSecondary
                font.pixelSize: 9
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
