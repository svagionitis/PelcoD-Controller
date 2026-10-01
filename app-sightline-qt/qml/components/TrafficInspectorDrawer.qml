import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."

Rectangle {
    id: root

    property bool isCollapsed: false
    property int expandedHeight: 200
    property int minHeight: 90
    property int maxHeight: 400

    implicitHeight: isCollapsed ? 36 : expandedHeight
    color: SightlineTheme.surface
    border.color: SightlineTheme.cardBorder
    border.width: 1

    Behavior on implicitHeight {
        NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. Top Resize Handle & Header Bar
        Rectangle {
            id: headerBar
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: SightlineTheme.surfaceLight
            border.color: SightlineTheme.cardBorder
            border.width: 1

            // Drag handle to resize when not collapsed
            MouseArea {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 6
                cursorShape: Qt.SizeVerCursor
                enabled: !root.isCollapsed

                property int startY: 0
                property int startH: 0

                onPressed: function(mouse) {
                    startY = mouse.y;
                    startH = root.expandedHeight;
                }

                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var delta = startY - mouse.y;
                        var newH = Math.max(root.minHeight, Math.min(root.maxHeight, startH + delta));
                        root.expandedHeight = newH;
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                // Collapse / Expand toggle button
                Rectangle {
                    width: 24
                    height: 24
                    radius: 3
                    color: toggleArea.containsMouse ? SightlineTheme.cardBorderHighlight : SightlineTheme.surfaceCard
                    border.color: SightlineTheme.cardBorder

                    Text {
                        anchors.centerIn: parent
                        text: root.isCollapsed ? "▲" : "▼"
                        color: SightlineTheme.primary
                        font.pixelSize: 11
                        font.bold: true
                    }

                    MouseArea {
                        id: toggleArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.isCollapsed = !root.isCollapsed;
                        }
                    }
                }

                // Section Indicator bar
                Rectangle {
                    width: 3
                    height: 16
                    color: SightlineTheme.primary
                    radius: 1
                }

                Text {
                    text: "PROTOCOL TRAFFIC INSPECTOR"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: SightlineTheme.fontSizeNormal
                    font.bold: true
                    font.letterSpacing: 0.8
                }

                Text {
                    text: "// SLA Frame Monitor & Hex Injection"
                    color: SightlineTheme.textMuted
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.family: "Monospace"
                }

                Item { Layout.fillWidth: true }

                // Quick Raw Hex Injection input & send
                RowLayout {
                    visible: !root.isCollapsed
                    spacing: 6

                    Text {
                        text: "HEX:"
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 10
                        font.bold: true
                    }

                    Rectangle {
                        implicitWidth: 160
                        implicitHeight: 24
                        color: SightlineTheme.surfaceCard
                        radius: 3
                        border.color: hexInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder

                        TextInput {
                            id: hexInput
                            anchors.fill: parent
                            anchors.margins: 4
                            text: "51 AC 02 01 53"
                            color: SightlineTheme.textPrimary
                            font.family: "Monospace"
                            font.pixelSize: 10
                            selectByMouse: true
                            onAccepted: sendBtn.clicked()
                        }
                    }

                    Button {
                        id: sendBtn
                        text: "Inject"
                        implicitHeight: 24
                        implicitWidth: 55
                        contentItem: Text {
                            text: parent.text
                            color: "#0e1014"
                            font.pixelSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: SightlineTheme.primary
                            radius: 3
                        }
                        onClicked: {
                            if (bridge && hexInput.text.length > 0) {
                                bridge.sendRawHex(hexInput.text);
                            }
                        }
                    }
                }

                Rectangle {
                    width: 1
                    height: 16
                    color: SightlineTheme.cardBorder
                    visible: !root.isCollapsed
                }

                Button {
                    text: "Clear Log"
                    implicitHeight: 24
                    implicitWidth: 65
                    contentItem: Text {
                        text: parent.text
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: SightlineTheme.surfaceCard
                        radius: 3
                        border.color: SightlineTheme.inputBorder
                    }
                    onClicked: {
                        if (bridge && bridge.trafficLogModel) {
                            bridge.trafficLogModel.clearLog();
                        }
                    }
                }

                CheckBox {
                    id: autoScrollCb
                    text: "Auto-scroll"
                    checked: true
                    visible: !root.isCollapsed
                    contentItem: Text {
                        text: autoScrollCb.text
                        font.pixelSize: 10
                        font.bold: true
                        color: autoScrollCb.checked ? SightlineTheme.primary : SightlineTheme.textSecondary
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: autoScrollCb.indicator.width + 4
                    }
                    onCheckedChanged: {
                        if (checked && logListView.count > 0) {
                            logListView.positionViewAtEnd();
                        }
                    }
                }

                Button {
                    id: jumpLatestBtn
                    text: "▼ Latest"
                    implicitHeight: 24
                    implicitWidth: 60
                    visible: !root.isCollapsed
                    contentItem: Text {
                        text: parent.text
                        color: SightlineTheme.primary
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: SightlineTheme.surfaceCard
                        radius: 3
                        border.color: SightlineTheme.cardBorder
                    }
                    onClicked: {
                        logListView.positionViewAtEnd();
                    }
                }
            }
        }

        // 2. Traffic Log Table Body (Hidden if collapsed)
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: SightlineTheme.surfaceCard
            visible: !root.isCollapsed
            clip: true

            ListView {
                id: logListView
                anchors.fill: parent
                anchors.margins: 4
                model: bridge ? bridge.trafficLogModel : null
                spacing: 2
                clip: true

                onCountChanged: {
                    if (autoScrollCb.checked && count > 0) {
                        logListView.positionViewAtEnd();
                    }
                }

                Component.onCompleted: {
                    if (count > 0) {
                        logListView.positionViewAtEnd();
                    }
                }

                headerPositioning: ListView.OverlayHeader
                header: Rectangle {
                    width: logListView.width
                    height: 22
                    color: SightlineTheme.surfaceLight
                    z: 2

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 6

                        Text { text: "TIME"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 75 }
                        Text { text: "DIR"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 35 }
                        Text { text: "MESSAGE NAME"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 180 }
                        Text { text: "LEN"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 35 }
                        Text { text: "CRC"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 38 }
                        Text { text: "HEX PAYLOAD"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.fillWidth: true }
                    }
                }

                delegate: Rectangle {
                    width: logListView.width
                    height: 22
                    color: index % 2 === 0 ? SightlineTheme.surfaceCard : SightlineTheme.surfaceLight
                    radius: 2

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 6

                        Text {
                            text: model.timestamp
                            color: SightlineTheme.textSecondary
                            font.family: "Monospace"
                            font.pixelSize: 9
                            Layout.preferredWidth: 75
                        }

                        Rectangle {
                            Layout.preferredWidth: 30
                            height: 14
                            radius: 2
                            color: model.isTx ? Qt.rgba(0, 0.9, 1.0, 0.15) : Qt.rgba(0, 0.9, 0.46, 0.15)

                            Text {
                                anchors.centerIn: parent
                                text: model.isTx ? "TX" : "RX"
                                color: model.isTx ? SightlineTheme.primary : SightlineTheme.success
                                font.pixelSize: 8
                                font.bold: true
                            }
                        }

                        Text {
                            text: model.messageName
                            color: SightlineTheme.textPrimary
                            font.bold: true
                            font.pixelSize: 10
                            Layout.preferredWidth: 180
                            elide: Text.ElideRight
                        }

                        Text {
                            text: model.frameLength.toString()
                            color: SightlineTheme.textMuted
                            font.pixelSize: 9
                            font.family: "Monospace"
                            Layout.preferredWidth: 35
                        }

                        Text {
                            text: model.crcOk ? "OK" : "ERR"
                            color: model.crcOk ? SightlineTheme.success : SightlineTheme.error
                            font.bold: true
                            font.pixelSize: 9
                            Layout.preferredWidth: 38
                        }

                        Text {
                            text: model.hexPayload
                            color: SightlineTheme.textSecondary
                            font.family: "Monospace"
                            font.pixelSize: 9
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    active: true
                    policy: ScrollBar.AsNeeded
                }
            }
        }
    }
}
