import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import RtspServerApp 1.0

import "components"
import "panels"

ApplicationWindow {
    id: root
    visible: true
    width: 1100
    height: 780
    minimumWidth: 840
    minimumHeight: 620
    title: "PelcoD Tactical RTSP Streaming Server"
    color: "#0e1014"

    RtspServerController {
        id: serverCtrl
    }

    // Top Header
    header: HeaderBar {
        controller: serverCtrl
        onToastRequested: function(msg) { root.showToast(msg) }
    }

    // Main Content
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // Tactical Navigation Tab Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            RowLayout {
                anchors.fill: parent
                anchors.margins: 3
                spacing: 6

                Repeater {
                    model: ["SERVER CONFIGURATION", "MEDIA INGEST SOURCE", "TELEMETRY & PROFILES"]

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 3
                        color: mainStack.currentIndex === index ? "#1e2636" : (tabMouse.containsMouse ? "#171c26" : "transparent")
                        border.color: mainStack.currentIndex === index ? "#00e5ff" : "transparent"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: mainStack.currentIndex === index ? "#00e5ff" : (tabMouse.containsMouse ? "#f1f5f9" : "#94a3b8")
                            font.pixelSize: 11
                            font.bold: true
                            font.letterSpacing: 0.8
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: mainStack.currentIndex = index
                        }
                    }
                }
            }
        }

        // Tab Content Stack
        StackLayout {
            id: mainStack
            Layout.fillWidth: true
            Layout.preferredHeight: 400
            currentIndex: 0

            ServerConfigPanel {
                controller: serverCtrl
            }

            MediaSourcePanel {
                controller: serverCtrl
            }

            TelemetryPanel {
                controller: serverCtrl
                onToastRequested: function(msg) { root.showToast(msg) }
            }
        }

        // Live Event Log Drawer / Panel
        EventLogPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: serverCtrl
        }
    }

    // Floating Toast Notification
    Rectangle {
        id: toastBox
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        implicitWidth: toastText.implicitWidth + 32
        implicitHeight: 36
        radius: 18
        color: "#141720"
        border.color: "#00e676"
        border.width: 1
        opacity: 0.0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        RowLayout {
            anchors.centerIn: parent
            spacing: 8
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: "#00e676"
            }
            Text {
                id: toastText
                text: ""
                color: "#f8fafc"
                font.pixelSize: 12
                font.bold: true
            }
        }

        Timer {
            id: toastTimer
            interval: 2500
            onTriggered: toastBox.opacity = 0.0
        }
    }

    function showToast(msg) {
        toastText.text = msg
        toastBox.opacity = 1.0
        toastTimer.restart()
    }
}
