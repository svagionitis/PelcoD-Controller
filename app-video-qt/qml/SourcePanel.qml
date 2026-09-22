import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import VideoApp 1.0

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    required property VideoPlayerController controller

    // Internal source mode: 0 = Stream, 1 = File, 2 = Hardware Device
    property int sourceMode: 0

    FileDialog {
        id: fileDialog
        title: "Select Video File"
        nameFilters: ["Video Files (*.mp4 *.mkv *.avi *.mov *.ts *.flv *.webm *.m4v)", "All Files (*)"]
        onAccepted: {
            var path = selectedFile.toString();
            if (path.startsWith("file://")) {
                path = path.substring(7);
            }
            filePathField.text = path;
            controller.sourceUri = path;
        }
    }

    ColumnLayout {
        width: parent.width - 20
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 16

        // Header
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
                text: "SOURCE CONFIGURATION"
                color: "#f0f4fc"
                font.pixelSize: 13
                font.bold: true
                font.letterSpacing: 1.2
            }
        }

        // Source Type Segmented Switch
        Rectangle {
            Layout.fillWidth: true
            height: 38
            color: "#181b24"
            radius: 6
            border.color: "#2a2f40"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 3
                spacing: 4

                Repeater {
                    model: ["Stream", "Local File", "V4L2 Device"]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 4
                        color: root.sourceMode === index ? "#00e5ff" : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: root.sourceMode === index ? "#0e1014" : "#8894ab"
                            font.pixelSize: 12
                            font.bold: root.sourceMode === index
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.sourceMode = index;
                                if (index === 0) {
                                    controller.sourceUri = streamUriField.text;
                                } else if (index === 1) {
                                    controller.sourceUri = filePathField.text;
                                } else if (index === 2 && controller.availableDevices.length > 0) {
                                    var dev = controller.availableDevices[deviceCombo.currentIndex];
                                    if (dev && dev.path) {
                                        controller.sourceUri = dev.path;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Stream Mode Input
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.sourceMode === 0
            spacing: 6

            Text {
                text: "STREAM URL / RTSP / URI"
                color: "#8894ab"
                font.pixelSize: 11
                font.bold: true
            }

            Rectangle {
                Layout.fillWidth: true
                height: 38
                color: "#1e2230"
                radius: 4
                border.color: streamUriField.activeFocus ? "#00e5ff" : "#2d3446"

                TextInput {
                    id: streamUriField
                    anchors.fill: parent
                    anchors.margins: 10
                    color: "#f0f4fc"
                    font.pixelSize: 12
                    font.family: "Monospace"
                    text: controller.sourceUri
                    selectByMouse: true
                    onTextChanged: {
                        if (root.sourceMode === 0) {
                            controller.sourceUri = text;
                        }
                    }
                }
            }

            // Presets
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    Layout.fillWidth: true
                    text: "Mock Pattern"
                    contentItem: Text {
                        text: parent.text
                        color: "#00e5ff"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#283344" : "#1e2230"
                        radius: 4
                        border.color: "#2d3446"
                    }
                    onClicked: {
                        streamUriField.text = "mock://test";
                        controller.sourceUri = "mock://test";
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: "Local RTSP"
                    contentItem: Text {
                        text: parent.text
                        color: "#8894ab"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#283344" : "#1e2230"
                        radius: 4
                        border.color: "#2d3446"
                    }
                    onClicked: {
                        streamUriField.text = "rtsp://127.0.0.1:8554/live";
                        controller.sourceUri = "rtsp://127.0.0.1:8554/live";
                    }
                }
            }
        }

        // Local File Mode Input
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.sourceMode === 1
            spacing: 6

            Text {
                text: "VIDEO FILE PATH"
                color: "#8894ab"
                font.pixelSize: 11
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    height: 38
                    color: "#1e2230"
                    radius: 4
                    border.color: filePathField.activeFocus ? "#00e5ff" : "#2d3446"

                    TextInput {
                        id: filePathField
                        anchors.fill: parent
                        anchors.margins: 10
                        color: "#f0f4fc"
                        font.pixelSize: 12
                        font.family: "Monospace"
                        text: ""
                        selectByMouse: true
                        onTextChanged: {
                            if (root.sourceMode === 1) {
                                controller.sourceUri = text;
                            }
                        }
                    }
                }

                Button {
                    text: "Browse..."
                    Layout.preferredHeight: 38
                    contentItem: Text {
                        text: parent.text
                        color: "#00e5ff"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#283344" : "#1e2230"
                        radius: 4
                        border.color: "#00e5ff"
                    }
                    onClicked: fileDialog.open()
                }
            }
        }

        // V4L2 Device Mode Input
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.sourceMode === 2
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "CAPTURE DEVICE"
                    color: "#8894ab"
                    font.pixelSize: 11
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Scan"
                    contentItem: Text {
                        text: parent.text
                        color: "#00e5ff"
                        font.pixelSize: 11
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#283344" : "transparent"
                        radius: 3
                    }
                    onClicked: controller.refreshDevices()
                }
            }

            ComboBox {
                id: deviceCombo
                Layout.fillWidth: true
                model: controller.availableDevices
                textRole: "name"
                currentIndex: 0
                onCurrentIndexChanged: {
                    if (root.sourceMode === 2 && controller.availableDevices.length > currentIndex && currentIndex >= 0) {
                        var dev = controller.availableDevices[currentIndex];
                        if (dev && dev.path) {
                            controller.sourceUri = dev.path;
                        }
                    }
                }
                delegate: ItemDelegate {
                    width: deviceCombo.width
                    contentItem: ColumnLayout {
                        spacing: 2
                        Text {
                            text: modelData.name || "Unknown Device"
                            color: "#f0f4fc"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Text {
                            text: (modelData.path || "") + " (" + (modelData.busInfo || "") + ")"
                            color: "#8894ab"
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }
                    background: Rectangle {
                        color: highlighted ? "#283344" : "#1e2230"
                    }
                }
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#2a2f40"
        }

        // Backend Selection
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "DECODING BACKEND"
                color: "#8894ab"
                font.pixelSize: 11
                font.bold: true
            }

            ComboBox {
                id: backendCombo
                Layout.fillWidth: true
                model: controller.availableBackends
                currentIndex: controller.backendIndex
                onActivated: function(index) {
                    controller.backendIndex = index;
                }
            }
        }

        // Acceleration Device Selection
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "HARDWARE ACCELERATION"
                color: "#8894ab"
                font.pixelSize: 11
                font.bold: true
            }

            ComboBox {
                id: hwAccelCombo
                Layout.fillWidth: true
                model: ["CPU (Software)", "CUDA (NVIDIA NVDEC)", "VAAPI (Intel/AMD)"]
                currentIndex: controller.deviceIndex
                onActivated: function(index) {
                    controller.deviceIndex = index;
                }
            }
        }

        // Playback Options
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Switch {
                id: loopSwitch
                checked: controller.isLoopPlayback
                onToggled: controller.isLoopPlayback = checked
            }

            Text {
                text: "Loop Playback (Files)"
                color: "#f0f4fc"
                font.pixelSize: 12
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#2a2f40"
        }

        // Play / Stop Action Button
        Button {
            id: actionBtn
            Layout.fillWidth: true
            Layout.preferredHeight: 44

            readonly property bool isRunning: controller.playbackState === VideoPlayerController.Playing
                                              || controller.playbackState === VideoPlayerController.Opening

            contentItem: RowLayout {
                anchors.centerIn: parent
                spacing: 8
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: actionBtn.isRunning ? "#ff1744" : "#00e676"
                }
                Text {
                    text: actionBtn.isRunning ? "DISCONNECT / STOP" : "START PLAYBACK"
                    color: actionBtn.isRunning ? "#ff1744" : "#0e1014"
                    font.pixelSize: 13
                    font.bold: true
                    font.letterSpacing: 1.0
                }
            }

            background: Rectangle {
                color: actionBtn.isRunning ? "#2d1620" : "#00e676"
                radius: 6
                border.color: actionBtn.isRunning ? "#ff1744" : "#00e676"
                border.width: 1
            }

            onClicked: {
                if (actionBtn.isRunning) {
                    controller.stopPlayback();
                } else {
                    controller.startPlayback();
                }
            }
        }

        // Status Line
        Rectangle {
            Layout.fillWidth: true
            height: 36
            color: "#181b24"
            radius: 4
            border.color: "#2a2f40"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    color: {
                        switch (controller.playbackState) {
                        case VideoPlayerController.Playing: return "#00e676";
                        case VideoPlayerController.Opening: return "#ff9100";
                        case VideoPlayerController.Paused: return "#00e5ff";
                        case VideoPlayerController.Error: return "#ff1744";
                        default: return "#525c70";
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: controller.statusMessage
                    color: "#8894ab"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }
    }
}
