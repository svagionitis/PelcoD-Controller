import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // Header Title
        RowLayout {
            spacing: 12
            Text {
                text: "🎯 TARGET TRACKING"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeLarge
                font.bold: true
            }
            Text {
                text: "• Module 0x08 / 0x09 / 0x13 / 0x43"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeNormal
            }
        }

        // Top Control Grid
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            // Interactive Reticle & Viewport
            CoordinatePad {
                id: coordPad
                Layout.preferredWidth: 380
                Layout.preferredHeight: 260
                onCoordinatePicked: function(col, row) {
                    colInput.text = col.toString();
                    rowInput.text = row.toString();
                }
                onNudgeRequested: function(dCol, dRow) {
                    bridge.nudgeTracking(camCombo.currentIndex, dCol, dRow);
                }
            }

            // Target Acquisition Controls Card
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 260
                color: SightlineTheme.surface
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Text {
                        text: "ACQUISITION PARAMETERS"
                        color: SightlineTheme.textSecondary
                        font.pixelSize: SightlineTheme.fontSizeSmall
                        font.bold: true
                    }

                    RowLayout {
                        spacing: 12
                        Text { text: "Camera:"; color: SightlineTheme.textSecondary; font.pixelSize: SightlineTheme.fontSizeNormal }
                        ComboBox {
                            id: camCombo
                            model: ["Camera 0 (EO)", "Camera 1 (IR)", "Camera 2", "Camera 3"]
                            Layout.fillWidth: true
                        }
                    }

                    RowLayout {
                        spacing: 12
                        Text { text: "Center (X, Y):"; color: SightlineTheme.textSecondary; font.pixelSize: SightlineTheme.fontSizeNormal }
                        TextField {
                            id: colInput
                            text: coordPad.selectedCol.toString()
                            Layout.preferredWidth: 80
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 }
                        }
                        TextField {
                            id: rowInput
                            text: coordPad.selectedRow.toString()
                            Layout.preferredWidth: 80
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 }
                        }
                    }

                    RowLayout {
                        spacing: 12
                        Text { text: "Gate Size (W, H):"; color: SightlineTheme.textSecondary; font.pixelSize: SightlineTheme.fontSizeNormal }
                        TextField {
                            id: widthInput
                            text: "80"
                            Layout.preferredWidth: 80
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 }
                        }
                        TextField {
                            id: heightInput
                            text: "80"
                            Layout.preferredWidth: 80
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4 }
                        }
                    }

                    // Action Buttons Row
                    RowLayout {
                        spacing: 8
                        Layout.topMargin: 4

                        Button {
                            text: "Acquire Target"
                            Layout.fillWidth: true
                            contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: SightlineTheme.success; radius: 4 }
                            onClicked: {
                                bridge.startTracking(
                                    camCombo.currentIndex,
                                    parseInt(colInput.text),
                                    parseInt(rowInput.text),
                                    parseInt(widthInput.text),
                                    parseInt(heightInput.text),
                                    0x01
                                );
                            }
                        }

                        Button {
                            text: "Stop All"
                            Layout.fillWidth: true
                            contentItem: Text { text: parent.text; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: SightlineTheme.error; radius: 4 }
                            onClicked: {
                                bridge.stopTracking(camCombo.currentIndex, 0xFF);
                            }
                        }
                    }
                }
            }
        }

        // Live Tracks Table Header
        Text {
            text: "ACTIVE TRACK LIST (" + bridge.trackListModel.rowCount() + " Active Targets)"
            color: SightlineTheme.textSecondary
            font.pixelSize: SightlineTheme.fontSizeSmall
            font.bold: true
        }

        // Live Tracks ListView
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: SightlineTheme.surface
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            clip: true

            ListView {
                id: trackListView
                anchors.fill: parent
                anchors.margins: 8
                model: bridge.trackListModel
                spacing: 6

                header: Rectangle {
                    width: trackListView.width
                    height: 28
                    color: SightlineTheme.surfaceLight
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        Text { text: "ID"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 40 }
                        Text { text: "Primary"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 60 }
                        Text { text: "Position (X, Y)"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 120 }
                        Text { text: "Size (W x H)"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 100 }
                        Text { text: "Velocity (Vx, Vy)"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 120 }
                        Text { text: "Confidence"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 80 }
                        Item { Layout.fillWidth: true }
                        Text { text: "Actions"; color: SightlineTheme.textMuted; font.bold: true; Layout.preferredWidth: 120 }
                    }
                }

                delegate: Rectangle {
                    width: trackListView.width
                    height: 36
                    color: model.isPrimary ? Qt.rgba(0, 0.75, 1.0, 0.1) : (index % 2 === 0 ? SightlineTheme.surface : SightlineTheme.surfaceLight)
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        Text { text: "#" + model.trackId; color: SightlineTheme.textPrimary; font.bold: true; Layout.preferredWidth: 40 }
                        Rectangle {
                            Layout.preferredWidth: 60
                            height: 18
                            radius: 9
                            color: model.isPrimary ? SightlineTheme.primary : "transparent"
                            Text { anchors.centerIn: parent; text: model.isPrimary ? "PRIMARY" : "--"; color: "#FFFFFF"; font.pixelSize: 9; font.bold: true }
                        }
                        Text { text: model.centerCol + ", " + model.centerRow; color: SightlineTheme.textPrimary; Layout.preferredWidth: 120 }
                        Text { text: model.trackWidth + " x " + model.trackHeight; color: SightlineTheme.textSecondary; Layout.preferredWidth: 100 }
                        Text { text: model.velocityCol + ", " + model.velocityRow; color: SightlineTheme.textSecondary; Layout.preferredWidth: 120 }
                        Text { text: model.confidence + "%"; color: model.confidence > 70 ? SightlineTheme.success : SightlineTheme.warning; Layout.preferredWidth: 80 }
                        Item { Layout.fillWidth: true }

                        Row {
                            spacing: 4
                            Button {
                                text: "Designate"
                                implicitHeight: 24
                                visible: !model.isPrimary
                                onClicked: bridge.designatePrimary(camCombo.currentIndex, model.trackId)
                            }
                            Button {
                                text: "Drop"
                                implicitHeight: 24
                                onClicked: bridge.stopTracking(camCombo.currentIndex, model.trackId)
                            }
                        }
                    }
                }
            }
        }
    }
}
