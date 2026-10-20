import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

TacticalCard {
    id: logRoot
    required property var controller
    title: "Real-time Event & Streaming Log"
    subtitle: "Diagnostics & Session Events"

    ListModel {
        id: logListModel
    }

    Connections {
        target: logRoot.controller
        function onLogMessageEmitted(timestamp, level, message) {
            logListModel.append({
                "timestamp": timestamp,
                "level": level,
                "message": message
            })
            if (logListModel.count > 300) {
                logListModel.remove(0)
            }
            logListView.positionViewAtEnd()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "TOTAL EVENTS: " + logListModel.count
                color: "#64748b"
                font.pixelSize: 10
                font.bold: true
                font.family: "Monospace"
            }

            Item { Layout.fillWidth: true }

            TacticalButton {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 24
                text: "CLEAR"
                onClicked: logListModel.clear()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0a0c10"
            border.color: "#242938"
            border.width: 1
            radius: 4

            ListView {
                id: logListView
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                model: logListModel

                delegate: RowLayout {
                    id: delegateRow
                    width: logListView.width
                    spacing: 8

                    required property string timestamp
                    required property string level
                    required property string message

                    Text {
                        text: delegateRow.timestamp
                        color: "#475569"
                        font.pixelSize: 10
                        font.family: "Monospace"
                    }

                    Rectangle {
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 18
                        radius: 2
                        color: {
                            switch (delegateRow.level) {
                            case "ERROR": return "#450a0a"
                            case "WARN": return "#451a03"
                            default: return "#082f49"
                            }
                        }
                        border.color: {
                            switch (delegateRow.level) {
                            case "ERROR": return "#ff1744"
                            case "WARN": return "#ff9100"
                            default: return "#00e5ff"
                            }
                        }
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: delegateRow.level
                            color: {
                                switch (delegateRow.level) {
                                case "ERROR": return "#ff5252"
                                case "WARN": return "#ffb74d"
                                default: return "#38bdf8"
                                }
                            }
                            font.pixelSize: 9
                            font.bold: true
                            font.family: "Monospace"
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: delegateRow.message
                        color: "#e2e8f0"
                        font.pixelSize: 11
                        font.family: "Monospace"
                        wrapMode: Text.WrapAnywhere
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    active: true
                }
            }
        }
    }
}
