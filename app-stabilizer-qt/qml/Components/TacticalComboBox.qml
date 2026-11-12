import QtQuick
import QtQuick.Controls

Item {
    id: control
    width: parent ? parent.width : 280
    height: 48

    property string label: ""
    property alias model: combo.model
    property alias currentIndex: combo.currentIndex
    property alias currentText: combo.currentText
    property alias textRole: combo.textRole

    signal activated(int index)

    Column {
        anchors.fill: parent
        spacing: 4

        Text {
            text: control.label
            color: "#c0c8d8"
            font.pixelSize: 11
            font.bold: true
            elide: Text.ElideRight
            width: parent.width
        }

        ComboBox {
            id: combo
            width: parent.width
            height: 26

            onActivated: (index) => {
                control.activated(index);
            }

            background: Rectangle {
                implicitWidth: 120
                implicitHeight: 26
                color: combo.pressed ? "#161b26" : (combo.hovered ? "#1f2636" : "#181d28")
                border.color: combo.activeFocus || combo.popup.visible ? "#00e5ff" : "#2a3244"
                border.width: 1
                radius: 3
            }

            contentItem: Text {
                leftPadding: 8
                rightPadding: combo.indicator.width + 8
                text: combo.displayText
                font.pixelSize: 11
                color: "#f0f4fc"
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
                    context.fillStyle = combo.popup.visible ? "#00e5ff" : "#718096";
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
                    color: "#141720"
                    border.color: "#00e5ff"
                    border.width: 1
                    radius: 3
                }
            }

            delegate: ItemDelegate {
                width: combo.width - 4
                height: 24

                contentItem: Text {
                    text: typeof modelData === "string" ? modelData : (model[combo.textRole] || "")
                    color: highlighted ? "#00e5ff" : "#d0d7e5"
                    font.pixelSize: 11
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    color: highlighted ? "#1a2538" : "transparent"
                    radius: 2
                }
            }
        }
    }
}

