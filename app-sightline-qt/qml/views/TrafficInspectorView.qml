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

    readonly property int minContentWidth: 640

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        spacing: 14

        Item { Layout.preferredHeight: 2 }

    // Section Title
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Rectangle { width: 4; height: 18; color: SightlineTheme.primary; radius: 2 }
        Text {
            text: "REAL-TIME PROTOCOL TRAFFIC INSPECTOR"
            color: SightlineTheme.textPrimary
            font.pixelSize: SightlineTheme.fontSizeMedium
            font.bold: true
            font.letterSpacing: 1.0
        }
        Text {
            text: "// SLA Frame Auditing & Diagnostic Raw Hex Injection"
            color: SightlineTheme.textMuted
            font.pixelSize: SightlineTheme.fontSizeSmall
            font.family: "Monospace"
        }
    }

    // Top Injection Bar Card
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 52
        color: SightlineTheme.surfaceCard
        radius: SightlineTheme.radiusMedium
        border.color: SightlineTheme.cardBorder
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            Text {
                text: "RAW HEX:"
                color: SightlineTheme.textSecondary
                font.pixelSize: 10
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 32
                color: SightlineTheme.surfaceLight
                radius: 4
                border.color: hexInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                Layout.alignment: Qt.AlignVCenter

                TextInput {
                    id: hexInput
                    anchors.fill: parent
                    anchors.margins: 8
                    text: "51 AC 02 01 53"
                    color: SightlineTheme.textPrimary
                    font.family: "Monospace"
                    font.pixelSize: 11
                    selectByMouse: true
                }
            }

            Button {
                text: "Send Hex"
                implicitHeight: 32
                implicitWidth: 95
                Layout.alignment: Qt.AlignVCenter
                contentItem: Text {
                    text: parent.text
                    color: "#0e1014"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: SightlineTheme.primary
                    radius: 4
                }
                onClicked: {
                    if (bridge && hexInput.text.length > 0) {
                        bridge.sendRawHex(hexInput.text);
                    }
                }
            }

            Button {
                text: "Clear Log"
                implicitHeight: 32
                implicitWidth: 80
                Layout.alignment: Qt.AlignVCenter
                contentItem: Text {
                    text: parent.text
                    color: SightlineTheme.textPrimary
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: SightlineTheme.surfaceLight
                    radius: 4
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
                Layout.alignment: Qt.AlignVCenter
                contentItem: Text {
                    text: autoScrollCb.text
                    font.pixelSize: 11
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
                text: "▼ Latest"
                implicitHeight: 32
                implicitWidth: 70
                Layout.alignment: Qt.AlignVCenter
                contentItem: Text {
                    text: parent.text
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: SightlineTheme.surfaceLight
                    radius: 4
                    border.color: SightlineTheme.cardBorder
                }
                onClicked: {
                    logListView.positionViewAtEnd();
                }
            }
        }
    }

    // Traffic Log Table
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 480
        color: SightlineTheme.surfaceCard
        radius: SightlineTheme.radiusMedium
        border.color: SightlineTheme.cardBorder
        border.width: 1
        clip: true

        ListView {
            id: logListView
            anchors.fill: parent
            anchors.margins: 8
            model: bridge ? bridge.trafficLogModel : null
            spacing: 3
            clip: true
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

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

            header: Rectangle {
                width: logListView.width
                height: 26
                color: SightlineTheme.surfaceLight
                radius: 4

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    Text { text: "Time"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 80 }
                    Text { text: "Dir"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 40 }
                    Text { text: "Message Name"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 190 }
                    Text { text: "Len"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 40 }
                    Text { text: "CRC"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.preferredWidth: 45 }
                    Text { text: "Hex Payload"; color: SightlineTheme.textMuted; font.bold: true; font.pixelSize: 10; Layout.fillWidth: true }
                }
            }

            delegate: Rectangle {
                width: logListView.width
                height: 28
                color: index % 2 === 0 ? SightlineTheme.surfaceCard : SightlineTheme.surfaceLight
                radius: 3

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    Text {
                        text: model.timestamp
                        color: SightlineTheme.textSecondary
                        font.family: "Monospace"
                        font.pixelSize: 10
                        Layout.preferredWidth: 80
                    }

                    Rectangle {
                        Layout.preferredWidth: 36
                        height: 16
                        radius: 8
                        color: model.isTx ? Qt.rgba(0, 0.9, 1.0, 0.15) : Qt.rgba(0, 0.9, 0.46, 0.15)

                        Text {
                            anchors.centerIn: parent
                            text: model.isTx ? "TX" : "RX"
                            color: model.isTx ? SightlineTheme.primary : SightlineTheme.success
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }

                    Text {
                        text: model.messageName
                        color: SightlineTheme.textPrimary
                        font.bold: true
                        font.pixelSize: 11
                        Layout.preferredWidth: 190
                        elide: Text.ElideRight
                    }

                    Text {
                        text: model.frameLength.toString()
                        color: SightlineTheme.textMuted
                        font.pixelSize: 10
                        font.family: "Monospace"
                        Layout.preferredWidth: 40
                    }

                    Text {
                        text: model.crcOk ? "OK" : "ERR"
                        color: model.crcOk ? SightlineTheme.success : SightlineTheme.error
                        font.bold: true
                        font.pixelSize: 10
                        Layout.preferredWidth: 45
                    }

                    Text {
                        text: model.hexPayload
                        color: SightlineTheme.textSecondary
                        font.family: "Monospace"
                        font.pixelSize: 10
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Item { Layout.preferredHeight: 16 }
    }
}
