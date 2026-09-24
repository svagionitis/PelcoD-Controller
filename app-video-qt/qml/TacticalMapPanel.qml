import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import VideoApp 1.0
import PelcoD.Mapping 1.0

Item {
    id: root

    required property VideoPlayerController controller

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // Panel Title & Status
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                width: 4
                height: 18
                color: "#00e5ff"
                radius: 2
            }

            Text {
                text: "TACTICAL SITUATIONAL MAP"
                color: "#f0f4fc"
                font.pixelSize: 13
                font.bold: true
                font.letterSpacing: 1.2
            }

            Item { Layout.fillWidth: true }

            // Offline Indicator Badge
            Rectangle {
                height: 20
                radius: 4
                implicitWidth: offlineLabel.implicitWidth + 12
                color: mapItem.offlineOnly ? "#261a10" : "#102618"
                border.color: mapItem.offlineOnly ? "#ff9100" : "#00e676"
                border.width: 1

                Text {
                    id: offlineLabel
                    anchors.centerIn: parent
                    text: mapItem.offlineOnly ? "OFFLINE ONLY" : "HYBRID ONLINE"
                    color: mapItem.offlineOnly ? "#ff9100" : "#00e676"
                    font.pixelSize: 9
                    font.bold: true
                }
            }
        }

        // Tactical Map Quick Item Container
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0a0c10"
            radius: 6
            border.color: "#242938"
            border.width: 1
            clip: true

            TacticalMap {
                id: mapItem
                anchors.fill: parent
                centerLatitude: controller.platformLatitude
                centerLongitude: controller.platformLongitude
                zoom: 13
                offlineOnly: false
                showFrustum: true
                showHeading: true
                platformLatitude: controller.platformLatitude
                platformLongitude: controller.platformLongitude
                platformHeading: controller.platformHeading

                onCoordinateClicked: function(lat, lon) {
                    clickToast.text = "TARGET SLEW: " + lat.toFixed(5) + ", " + lon.toFixed(5);
                    clickToast.opacity = 1.0;
                    toastHideTimer.restart();
                    controller.coordinateTargetPicked(lat, lon);
                }
            }

            // Quick Map Controls Overlay (Top-Right)
            ColumnLayout {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 8
                spacing: 4

                Button {
                    text: "+"
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    contentItem: Text {
                        text: parent.text
                        color: "#00e5ff"
                        font.pixelSize: 16
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#243248" : "#141720ee"
                        radius: 4
                        border.color: "#2a2f40"
                    }
                    onClicked: mapItem.zoom = Math.min(19, mapItem.zoom + 1)
                }

                Button {
                    text: "−"
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    contentItem: Text {
                        text: parent.text
                        color: "#00e5ff"
                        font.pixelSize: 16
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#243248" : "#141720ee"
                        radius: 4
                        border.color: "#2a2f40"
                    }
                    onClicked: mapItem.zoom = Math.max(1, mapItem.zoom - 1)
                }

                Button {
                    text: "⌖"
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    contentItem: Text {
                        text: parent.text
                        color: "#00e676"
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#243248" : "#141720ee"
                        radius: 4
                        border.color: "#2a2f40"
                    }
                    onClicked: {
                        mapItem.centerLatitude = controller.platformLatitude;
                        mapItem.centerLongitude = controller.platformLongitude;
                    }
                }
            }

            // Click-to-slew notification toast
            Rectangle {
                id: clickToastRect
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 8
                height: 24
                implicitWidth: clickToast.implicitWidth + 16
                radius: 4
                color: "#141720ee"
                border.color: "#00e5ff"
                opacity: clickToast.opacity

                Text {
                    id: clickToast
                    anchors.centerIn: parent
                    text: ""
                    color: "#00e5ff"
                    font.pixelSize: 10
                    font.family: "Monospace"
                    opacity: 0.0

                    Behavior on opacity {
                        NumberAnimation { duration: 200 }
                    }
                }

                Timer {
                    id: toastHideTimer
                    interval: 2500
                    onTriggered: clickToast.opacity = 0.0
                }
            }
        }

        // Telemetry & Control Group
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: telemetryCol.implicitHeight + 16
            color: "#181b24"
            radius: 6
            border.color: "#2a2f40"

            ColumnLayout {
                id: telemetryCol
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text {
                    text: "POSITION & SENSOR TELEMETRY"
                    color: "#00e5ff"
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.0
                }

                GridLayout {
                    columns: 2
                    Layout.fillWidth: true
                    rowSpacing: 4
                    columnSpacing: 12

                    Text { text: "Latitude:"; color: "#8894ab"; font.pixelSize: 11 }
                    Text {
                        text: controller.platformLatitude.toFixed(6) + "°"
                        color: "#f0f4fc"
                        font.pixelSize: 11
                        font.family: "Monospace"
                    }

                    Text { text: "Longitude:"; color: "#8894ab"; font.pixelSize: 11 }
                    Text {
                        text: controller.platformLongitude.toFixed(6) + "°"
                        color: "#f0f4fc"
                        font.pixelSize: 11
                        font.family: "Monospace"
                    }

                    Text { text: "Heading (HDG):"; color: "#8894ab"; font.pixelSize: 11 }
                    Text {
                        text: controller.platformHeading.toFixed(1) + "°"
                        color: "#f0f4fc"
                        font.pixelSize: 11
                        font.family: "Monospace"
                    }

                    Text { text: "Current Zoom:"; color: "#8894ab"; font.pixelSize: 11 }
                    Text {
                        text: "Level " + mapItem.zoom
                        color: "#f0f4fc"
                        font.pixelSize: 11
                        font.family: "Monospace"
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#242938" }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        id: offlineSwitch
                        checked: mapItem.offlineOnly
                        onToggled: mapItem.offlineOnly = checked
                    }
                    Text { text: "Strict Offline Map Mode"; color: "#f0f4fc"; font.pixelSize: 11 }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: mapItem.showFrustum
                        onToggled: mapItem.showFrustum = checked
                    }
                    Text { text: "Optical Frustum Footprint"; color: "#f0f4fc"; font.pixelSize: 11 }
                }
            }
        }
    }
}
