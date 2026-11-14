import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs 6.3
import "Components"

Dialog {
    id: root
    title: "HARDWARE DRIVER CONFIGURATION"
    modal: true
    anchors.centerIn: parent
    width: 440
    implicitHeight: contentCol.implicitHeight + 70

    property var controller: null
    property int selectedDriverIndex: 0
    property string selectedPort: "COM1"
    property int selectedBaud: 115200
    property int selectedAddress: 1
    property string selectedDllPath: ""

    background: Rectangle {
        color: "#141720"
        border.color: "#00e5ff"
        border.width: 1
        radius: 4
    }

    header: Rectangle {
        width: parent.width
        height: 36
        color: "#0e1014"
        radius: 4

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: "HARDWARE DRIVER & PORT BINDING"
            color: "#00e5ff"
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 1.0
        }
    }

    Column {
        id: contentCol
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Driver Selection
        TacticalComboBox {
            width: parent.width
            label: "Driver Type"
            model: ["Mock In-Memory Simulator", "Pelco-D RS-485 Motorized Lens", "Sony VISCA Camera Block", "Dynamic Vendor DLL Plugin"]
            currentIndex: root.selectedDriverIndex
            accentColor: "#00e5ff"
            onActivated: function(idx) {
                root.selectedDriverIndex = idx;
            }
        }

        // Port & Baud (only for physical / DLL drivers)
        Row {
            width: parent.width
            spacing: 12
            visible: root.selectedDriverIndex !== 0

            TacticalComboBox {
                width: parent.width * 0.5 - 6
                label: "Serial Port"
                model: (root.controller && root.controller.availablePorts.length > 0) ?
                    root.controller.availablePorts :
                    ["COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "/dev/ttyUSB0", "/dev/ttyUSB1"]
                currentIndex: 0
                accentColor: "#00e5ff"
                onActivated: function(idx) {
                    root.selectedPort = currentText;
                }
            }

            TacticalComboBox {
                width: parent.width * 0.5 - 6
                label: "Baud Rate"
                model: ["9600", "19200", "38400", "57600", "115200"]
                currentIndex: 4
                accentColor: "#00e5ff"
                onActivated: function(idx) {
                    root.selectedBaud = parseInt(currentText);
                }
            }
        }

        // Bus Address (Pelco-D or VISCA)
        Row {
            width: parent.width
            spacing: 12
            visible: root.selectedDriverIndex === 1 || root.selectedDriverIndex === 2

            Text {
                text: "CAMERA BUS ADDRESS:"
                color: "#808a9d"
                font.pixelSize: 10
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle {
                width: 60
                height: 26
                color: "#0a0c10"
                border.color: "#242938"
                border.width: 1
                radius: 3

                TextInput {
                    anchors.fill: parent
                    anchors.margins: 4
                    verticalAlignment: TextInput.AlignVCenter
                    horizontalAlignment: TextInput.AlignHCenter
                    color: "#f0f4fc"
                    font.pixelSize: 11
                    font.bold: true
                    font.family: "Monospace"
                    text: root.selectedAddress.toString()
                    onTextChanged: {
                        var parsed = parseInt(text);
                        if (!isNaN(parsed)) {
                            root.selectedAddress = parsed;
                        }
                    }
                }
            }
        }

        // DLL Plugin Path (Dynamic DLL Driver only)
        Column {
            width: parent.width
            spacing: 4
            visible: root.selectedDriverIndex === 3

            Text {
                text: "VENDOR SHARED LIBRARY PATH (.DLL / .SO):"
                color: "#808a9d"
                font.pixelSize: 10
                font.bold: true
            }

            Row {
                width: parent.width
                spacing: 6

                Rectangle {
                    width: parent.width - 70
                    height: 26
                    color: "#0a0c10"
                    border.color: "#242938"
                    border.width: 1
                    radius: 3

                    TextInput {
                        id: dllInput
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        verticalAlignment: TextInput.AlignVCenter
                        color: "#f0f4fc"
                        font.pixelSize: 10
                        font.family: "Monospace"
                        text: root.selectedDllPath
                        onTextChanged: {
                            root.selectedDllPath = text;
                        }
                    }
                }

                Rectangle {
                    width: 64
                    height: 26
                    radius: 3
                    color: "#1a2438"
                    border.color: "#00e5ff"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "BROWSE"
                        color: "#00e5ff"
                        font.pixelSize: 9
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: fileDialog.open()
                    }
                }
            }
        }

        // Action Buttons Row
        Row {
            width: parent.width
            height: 32
            spacing: 12
            layoutDirection: Qt.RightToLeft

            Rectangle {
                width: 90
                height: 28
                radius: 3
                color: "#00e5ff"

                Text {
                    anchors.centerIn: parent
                    text: "APPLY BIND"
                    color: "#0a0c10"
                    font.pixelSize: 10
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.controller) {
                            if (root.selectedDriverIndex === 0) {
                                root.controller.bindMockDriver();
                            } else if (root.selectedDriverIndex === 1) {
                                var pIdx = parseInt(root.selectedPort.replace(/[^0-9]/g, "")) || 1;
                                root.controller.bindPelcoDriver(pIdx, root.selectedBaud, root.selectedAddress);
                            } else if (root.selectedDriverIndex === 2) {
                                var vIdx = parseInt(root.selectedPort.replace(/[^0-9]/g, "")) || 1;
                                root.controller.bindViscaDriver(vIdx, root.selectedBaud, root.selectedAddress);
                            } else if (root.selectedDriverIndex === 3) {
                                var dIdx = parseInt(root.selectedPort.replace(/[^0-9]/g, "")) || 1;
                                root.controller.loadDriverPlugin(root.selectedDllPath, dIdx, root.selectedBaud);
                            }
                        }
                        root.accept();
                    }
                }
            }

            Rectangle {
                width: 70
                height: 28
                radius: 3
                color: "#1c212d"
                border.color: "#242938"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "CANCEL"
                    color: "#808a9d"
                    font.pixelSize: 10
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.reject()
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "Select Lens Driver Plugin"
        nameFilters: ["Shared Libraries (*.dll *.so)", "All files (*)"]
        onAccepted: {
            var path = selectedFile.toString();
            if (path.startsWith("file:///")) {
                path = path.substring(8);
            }
            root.selectedDllPath = path;
        }
    }
}
