import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: comboRoot
    property string label: ""
    property alias model: comboBox.model
    property alias currentIndex: comboBox.currentIndex
    property alias currentText: comboBox.currentText
    property bool enabledState: true
    signal activated(int index)

    spacing: 4

    Text {
        text: comboRoot.label
        color: "#94a3b8"
        font.pixelSize: 11
        font.bold: true
        visible: comboRoot.label.length > 0
    }

    ComboBox {
        id: comboBox
        Layout.fillWidth: true
        Layout.preferredHeight: 34
        enabled: comboRoot.enabledState
        onActivated: function(idx) { comboRoot.activated(idx) }

        background: Rectangle {
            color: comboBox.activeFocus ? "#171c26" : "#0e1117"
            border.color: comboBox.activeFocus ? "#00e5ff" : "#242938"
            border.width: 1
            radius: 4
        }

        contentItem: Text {
            leftPadding: 10
            rightPadding: comboBox.indicator.width + 10
            text: comboBox.displayText
            font.pixelSize: 12
            font.family: "Monospace"
            color: comboRoot.enabledState ? "#f1f5f9" : "#64748b"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        popup: Popup {
            y: comboBox.height + 2
            width: comboBox.width
            implicitHeight: Math.min(contentItem.implicitHeight + 4, 200)
            padding: 2

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: comboBox.popup.visible ? comboBox.delegateModel : null
                currentIndex: comboBox.highlightedIndex

                ScrollIndicator.vertical: ScrollIndicator { }
            }

            background: Rectangle {
                color: "#141720"
                border.color: "#2e3748"
                border.width: 1
                radius: 4
            }
        }

        delegate: ItemDelegate {
            id: delegateItem
            width: comboBox.width
            height: 30
            highlighted: comboBox.highlightedIndex === index

            contentItem: Text {
                text: modelData
                color: delegateItem.highlighted ? "#00e5ff" : "#e2e8f0"
                font.pixelSize: 11
                font.family: "Monospace"
                verticalAlignment: Text.AlignVCenter
                leftPadding: 10
            }

            background: Rectangle {
                color: delegateItem.highlighted ? "#1a2130" : "transparent"
            }
        }
    }
}
