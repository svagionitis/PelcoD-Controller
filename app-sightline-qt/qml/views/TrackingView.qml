import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

ScrollView {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true
    contentWidth: Math.max(availableWidth, minContentWidth + 32)
    contentHeight: mainCol.implicitHeight + 32
    clip: true
    ScrollBar.vertical.policy: ScrollBar.AsNeeded
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    readonly property int minContentWidth: 560

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        // Section Title
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                width: 4
                height: 18
                color: SightlineTheme.primary
                radius: 2
            }
            Text {
                text: "TARGET TRACKING & ACQUISITION"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Modules 0x08, 0x09, 0x13, 0x43"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Acquisition Parameters Card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: targetCol.implicitHeight + 24
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: targetCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text {
                    text: "ACQUISITION PARAMETERS"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        text: "Camera Channel:"
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 12
                        Layout.preferredWidth: 130
                        Layout.alignment: Qt.AlignVCenter
                    }
                    ComboBox {
                        id: camCombo
                        model: ["Camera 0 (EO Visible)", "Camera 1 (IR Thermal)", "Camera 2", "Camera 3"]
                        Layout.fillWidth: true
                        Layout.maximumWidth: 260
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        text: "Center (Col, Row):"
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 12
                        Layout.preferredWidth: 130
                        Layout.alignment: Qt.AlignVCenter
                    }
                    TextField {
                        id: colInput
                        text: coordPad.selectedCol.toString()
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: colInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                        }
                    }
                    TextField {
                        id: rowInput
                        text: coordPad.selectedRow.toString()
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: rowInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        text: "Gate Size (W x H):"
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 12
                        Layout.preferredWidth: 130
                        Layout.alignment: Qt.AlignVCenter
                    }
                    TextField {
                        id: widthInput
                        text: "80"
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: widthInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                        }
                    }
                    TextField {
                        id: heightInput
                        text: "80"
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: heightInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Acquire Target"
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 32
                        contentItem: Text {
                            text: parent.text
                            color: "#0e1014"
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: SightlineTheme.success
                            radius: 4
                        }
                        onClicked: {
                            if (bridge) {
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
                    }

                    Button {
                        text: "Stop All"
                        Layout.preferredWidth: 110
                        Layout.preferredHeight: 32
                        contentItem: Text {
                            text: parent.text
                            color: "#ffffff"
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: SightlineTheme.error
                            radius: 4
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.stopTracking(camCombo.currentIndex, 0xFF);
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Secondary Reticle Coordinate Touch Pad
        CoordinatePad {
            id: coordPad
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            onCoordinatePicked: function(col, row) {
                colInput.text = col.toString();
                rowInput.text = row.toString();
            }
            onNudgeRequested: function(dCol, dRow) {
                if (bridge) {
                    bridge.nudgeTracking(camCombo.currentIndex, dCol, dRow);
                }
            }
        }

        // Live Tracks Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                width: 4
                height: 16
                color: SightlineTheme.info
                radius: 2
            }
            Text {
                text: "ACTIVE TRACK LIST (" + (bridge && bridge.trackListModel ? bridge.trackListModel.rowCount() : 0) + " Active Targets)"
                color: SightlineTheme.textSecondary
                font.pixelSize: 11
                font.bold: true
            }
        }

        // Live Tracks Table
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Math.max(160, Math.min(300, (bridge && bridge.trackListModel ? bridge.trackListModel.rowCount() : 0) * 40 + 40))
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1
            clip: true

            ListView {
                id: trackListView
                anchors.fill: parent
                anchors.margins: 8
                model: bridge ? bridge.trackListModel : null
                spacing: 4
                clip: true
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                header: Rectangle {
                    width: trackListView.width
                    height: 28
                    color: SightlineTheme.surfaceLight
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 4
                        Text { text: "ID"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 32 }
                        Text { text: "Status"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 60 }
                        Text { text: "Center"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 85 }
                        Text { text: "Size"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 75 }
                        Text { text: "Conf"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 48 }
                        Item { Layout.fillWidth: true }
                        Text { text: "Actions"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 180 }
                    }
                }

                delegate: Rectangle {
                    width: trackListView.width
                    height: 32
                    color: model.isPrimary ? "#1b2535" : (index % 2 === 0 ? SightlineTheme.surfaceCard : SightlineTheme.surfaceLight)
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 4

                        Text { text: "#" + model.trackId; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 32 }
                        Rectangle {
                            Layout.preferredWidth: 60
                            height: 16
                            radius: 8
                            color: model.isCoasting ? SightlineTheme.warning : (model.isPrimary ? SightlineTheme.primary : "transparent")
                            border.color: (model.isCoasting || model.isPrimary) ? "transparent" : SightlineTheme.cardBorder
                            Text {
                                anchors.centerIn: parent
                                text: model.isCoasting ? "COAST" : (model.isPrimary ? "PRIMARY" : "TRACK")
                                color: (model.isCoasting || model.isPrimary) ? "#0e1014" : SightlineTheme.textMuted
                                font.pixelSize: 8
                                font.bold: true
                            }
                        }
                        Text { text: model.centerCol + ", " + model.centerRow; color: SightlineTheme.textPrimary; font.pixelSize: 10; Layout.preferredWidth: 85 }
                        Text { text: model.trackWidth + "x" + model.trackHeight; color: SightlineTheme.textSecondary; font.pixelSize: 10; Layout.preferredWidth: 75 }
                        Text { text: model.confidence + "%"; color: model.confidence > 70 ? SightlineTheme.success : SightlineTheme.warning; font.pixelSize: 10; font.bold: true; Layout.preferredWidth: 48 }
                        Item { Layout.fillWidth: true }

                        RowLayout {
                            spacing: 4
                            Button {
                                text: "Designate"
                                implicitHeight: 24
                                visible: !model.isPrimary
                                onClicked: bridge.designatePrimary(camCombo.currentIndex, model.trackId)
                            }
                            Button {
                                text: "Reinit"
                                implicitHeight: 24
                                onClicked: bridge.reinitTrack(camCombo.currentIndex, model.trackId)
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

        Item { Layout.preferredHeight: 16 }
    }
}
