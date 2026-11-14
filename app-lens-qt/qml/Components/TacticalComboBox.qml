import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    property string label: ""
    property var model: []
    property int currentIndex: 0
    property color accentColor: "#00e5ff"
    property bool enabled: true

    readonly property string currentText: {
        if (!model) return "";
        if (Array.isArray(model)) {
            return (currentIndex >= 0 && currentIndex < model.length) ? model[currentIndex] : "";
        }
        return combo.currentText;
    }

    signal activated(int index)

    implicitWidth: 200
    implicitHeight: (label.length > 0) ? 46 : 28

    Column {
        anchors.fill: parent
        spacing: 3

        Text {
            visible: root.label.length > 0
            text: root.label.toUpperCase()
            color: "#808a9d"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 0.8
        }

        ComboBox {
            id: combo
            width: parent.width
            height: 28
            model: root.model
            currentIndex: root.currentIndex
            enabled: root.enabled

            onActivated: function(index) {
                root.currentIndex = index;
                root.activated(index);
            }

            background: Rectangle {
                color: "#141720"
                border.color: combo.activeFocus || combo.hovered ? root.accentColor : "#242938"
                border.width: 1
                radius: 3
            }

            contentItem: Text {
                leftPadding: 8
                rightPadding: 24
                text: combo.displayText
                font.pixelSize: 11
                font.bold: true
                color: root.enabled ? "#f0f4fc" : "#525c70"
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            indicator: Canvas {
                id: canvas
                x: combo.width - width - 8
                y: combo.topPadding + (combo.availableHeight - height) / 2
                width: 10
                height: 6
                contextType: "2d"

                Connections {
                    target: combo
                    function onPressedChanged() { canvas.requestPaint(); }
                }

                onPaint: {
                    context.reset();
                    context.moveTo(0, 0);
                    context.lineTo(width, 0);
                    context.lineTo(width / 2, height);
                    context.closePath();
                    context.fillStyle = root.enabled ? root.accentColor : "#525c70";
                    context.fill();
                }
            }

            popup: Popup {
                y: combo.height + 2
                width: combo.width
                implicitHeight: contentItem.implicitHeight + 4
                padding: 2

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight > 180 ? 180 : contentHeight
                    model: combo.popup.visible ? combo.delegateModel : null
                    currentIndex: combo.highlightedIndex

                    ScrollIndicator.vertical: ScrollIndicator { }
                }

                background: Rectangle {
                    color: "#0e1014"
                    border.color: "#242938"
                    border.width: 1
                    radius: 3
                }
            }

            delegate: ItemDelegate {
                width: combo.width - 4
                height: 24
                highlighted: combo.highlightedIndex === index

                contentItem: Text {
                    text: modelData
                    color: highlighted ? root.accentColor : "#f0f4fc"
                    font.pixelSize: 10
                    font.bold: highlighted
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 6
                }

                background: Rectangle {
                    color: highlighted ? "#1a2438" : "transparent"
                    radius: 2
                }
            }
        }
    }
}
