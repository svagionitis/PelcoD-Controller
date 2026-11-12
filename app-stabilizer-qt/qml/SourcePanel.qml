import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import StabilizerApp 1.0
import "Components"

Rectangle {
    id: root
    width: 320
    height: parent ? parent.height : 600
    color: "#12151d"
    border.color: "#1e2433"
    border.width: 1

    property var controller: null
    property int selectedMode: 0 // 0: File, 1: RTSP, 2: Device, 3: Synthetic

    FileDialog {
        id: fileDialog
        title: "Select Video Source File"
        nameFilters: ["Video Files (*.mp4 *.mkv *.ts *.avi *.mov *.flv)", "All Files (*)"]
        onAccepted: {
            var urlStr = selectedFile.toString();
            // Strip file:// prefix if present
            if (urlStr.indexOf("file:///") === 0) {
                urlStr = urlStr.substring(8);
            } else if (urlStr.indexOf("file://") === 0) {
                urlStr = urlStr.substring(7);
            }
            filePathField.text = urlStr;
            if (controller) {
                controller.sourceUri = urlStr;
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Header Title
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "SOURCE & TRANSPORT"
                color: "#f0f4fc"
                font.pixelSize: 12
                font.bold: true
                font.letterSpacing: 1.5
            }

            Item { Layout.fillWidth: true }

            StatusBadge {
                stateText: controller ? {
                    switch (controller.playbackState) {
                    case StabilizerPlayerController.Playing: return "PLAYING";
                    case StabilizerPlayerController.Opening: return "OPENING";
                    case StabilizerPlayerController.Paused:  return "PAUSED";
                    case StabilizerPlayerController.Error:   return "ERROR";
                    default: return "IDLE";
                    }
                } : "IDLE"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#22293a"
        }

        // Mode Selector Tabs
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: ["FILE", "NET", "DEVICE", "SYNTH"]
                delegate: Button {
                    Layout.fillWidth: true
                    height: 26
                    text: modelData
                    checkable: true
                    checked: root.selectedMode === index
                    onClicked: root.selectedMode = index

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 10
                        font.bold: true
                        color: parent.checked ? "#00e5ff" : "#8894ab"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.checked ? "#1a2538" : (parent.hovered ? "#161b26" : "#10131a")
                        border.color: parent.checked ? "#00e5ff" : "#242c3d"
                        border.width: 1
                        radius: 3
                    }
                }
            }
        }

        // Mode Content Container
        StackLayout {
            Layout.fillWidth: true
            currentIndex: root.selectedMode

            // 0: Local File
            ColumnLayout {
                spacing: 8

                Text {
                    text: "VIDEO FILE PATH"
                    color: "#8894ab"
                    font.pixelSize: 10
                    font.bold: true
                }

                TextField {
                    id: filePathField
                    Layout.fillWidth: true
                    height: 28
                    text: "sample-videos/dji-ir-subtitles.mp4"
                    color: "#f0f4fc"
                    font.pixelSize: 11
                    background: Rectangle {
                        color: "#181d28"
                        border.color: "#2a3244"
                        border.width: 1
                        radius: 3
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        height: 28
                        text: "BROWSE..."
                        onClicked: fileDialog.open()

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 10
                            font.bold: true
                            color: "#00e5ff"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? "#1a283d" : "#131e2e"
                            border.color: "#00e5ff"
                            border.width: 1
                            radius: 3
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        height: 28
                        text: "OPEN FILE"
                        onClicked: {
                            if (controller && filePathField.text.length > 0) {
                                controller.sourceUri = filePathField.text;
                            }
                        }

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 10
                            font.bold: true
                            color: "#00e676"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? "#133824" : "#0d2618"
                            border.color: "#00e676"
                            border.width: 1
                            radius: 3
                        }
                    }
                }
            }

            // 1: Network / RTSP Stream
            ColumnLayout {
                spacing: 8

                Text {
                    text: "RTSP / UDP STREAM URL"
                    color: "#8894ab"
                    font.pixelSize: 10
                    font.bold: true
                }

                TextField {
                    id: rtspUrlField
                    Layout.fillWidth: true
                    height: 28
                    text: "rtsp://127.0.0.1:8554/live"
                    color: "#f0f4fc"
                    font.pixelSize: 11
                    background: Rectangle {
                        color: "#181d28"
                        border.color: "#2a3244"
                        border.width: 1
                        radius: 3
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    TacticalSwitch {
                        text: "TCP Interleaved"
                        checked: true
                    }

                    Button {
                        Layout.fillWidth: true
                        height: 28
                        text: "CONNECT"
                        onClicked: {
                            if (controller && rtspUrlField.text.length > 0) {
                                controller.sourceUri = rtspUrlField.text;
                            }
                        }

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 10
                            font.bold: true
                            color: "#00e5ff"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? "#1a283d" : "#131e2e"
                            border.color: "#00e5ff"
                            border.width: 1
                            radius: 3
                        }
                    }
                }
            }

            // 2: Hardware Device
            ColumnLayout {
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "CAPTURE DEVICE"
                        color: "#8894ab"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        height: 20
                        text: "REFRESH"
                        onClicked: {
                            if (controller) {
                                deviceCombo.model = controller.availableHardwareDevices();
                            }
                        }
                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 9
                            color: "#8894ab"
                        }
                        background: Rectangle {
                            color: "#141720"
                            border.color: "#2a3244"
                            radius: 2
                        }
                    }
                }

                TacticalComboBox {
                    id: deviceCombo
                    Layout.fillWidth: true
                    model: controller ? controller.availableHardwareDevices() : []
                }

                Button {
                    Layout.fillWidth: true
                    height: 28
                    text: "OPEN DEVICE"
                    onClicked: {
                        if (controller && deviceCombo.currentText.length > 0) {
                            controller.sourceUri = "device://" + deviceCombo.currentText;
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 10
                        font.bold: true
                        color: "#00e676"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#133824" : "#0d2618"
                        border.color: "#00e676"
                        border.width: 1
                        radius: 3
                    }
                }
            }

            // 3: Synthetic Shaker
            ColumnLayout {
                spacing: 6

                Text {
                    text: "CALIBRATED SHAKER BENCHMARK"
                    color: "#ff9100"
                    font.pixelSize: 10
                    font.bold: true
                }

                TacticalSlider {
                    label: "Mast Frequency"
                    unit: "Hz"
                    from: 1.0
                    to: 30.0
                    value: 6.0
                    stepSize: 0.5
                    decimals: 1
                }

                TacticalSlider {
                    label: "Mast Amplitude"
                    unit: "px"
                    from: 0.0
                    to: 40.0
                    value: 12.0
                    stepSize: 1.0
                    decimals: 0
                }

                TacticalSwitch {
                    text: "Random-Walk Drift"
                    checked: true
                }

                Button {
                    Layout.fillWidth: true
                    height: 28
                    text: "START SHAKER STREAM"
                    onClicked: {
                        if (controller) {
                            controller.sourceUri = "synthetic://shaker";
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 10
                        font.bold: true
                        color: "#ff9100"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#3d2a13" : "#291c0d"
                        border.color: "#ff9100"
                        border.width: 1
                        radius: 3
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#22293a"
        }

        // Playback Transport Controls Section
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "PLAYBACK TRANSPORT"
                color: "#f0f4fc"
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 1.2
            }

            // Transport Buttons Row
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    Layout.fillWidth: true
                    height: 32
                    text: "|<"
                    onClicked: if (controller) controller.seek(0.0)
                    background: Rectangle { color: "#181d28"; border.color: "#2a3244"; radius: 3 }
                    contentItem: Text { text: parent.text; color: "#f0f4fc"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                Button {
                    Layout.fillWidth: true
                    height: 32
                    text: "STEP"
                    onClicked: if (controller) controller.stepFrame()
                    background: Rectangle { color: "#181d28"; border.color: "#2a3244"; radius: 3 }
                    contentItem: Text { text: parent.text; color: "#00e5ff"; font.bold: true; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 64
                    height: 32
                    text: (controller && controller.playbackState === StabilizerPlayerController.Playing) ? "PAUSE" : "PLAY"
                    onClicked: {
                        if (controller) {
                            if (controller.playbackState === StabilizerPlayerController.Playing) {
                                controller.pause();
                            } else {
                                controller.play();
                            }
                        }
                    }
                    background: Rectangle {
                        color: (controller && controller.playbackState === StabilizerPlayerController.Playing) ? "#133824" : "#1a2538"
                        border.color: (controller && controller.playbackState === StabilizerPlayerController.Playing) ? "#00e676" : "#00e5ff"
                        border.width: 1
                        radius: 3
                    }
                    contentItem: Text {
                        text: parent.text
                        color: (controller && controller.playbackState === StabilizerPlayerController.Playing) ? "#00e676" : "#00e5ff"
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    Layout.fillWidth: true
                    height: 32
                    text: "STOP"
                    onClicked: if (controller) controller.stop()
                    background: Rectangle { color: "#181d28"; border.color: "#2a3244"; radius: 3 }
                    contentItem: Text { text: parent.text; color: "#ff1744"; font.bold: true; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }

            // Playback Rate Multipliers
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: "RATE:"
                    color: "#8894ab"
                    font.pixelSize: 10
                    font.bold: true
                }

                Repeater {
                    model: [0.25, 0.5, 1.0, 2.0, 4.0]
                    delegate: Button {
                        Layout.fillWidth: true
                        height: 22
                        text: modelData + "x"
                        onClicked: {
                            if (controller) {
                                controller.setPlaybackRate(modelData);
                            }
                        }
                        background: Rectangle {
                            color: "#141720"
                            border.color: "#242938"
                            radius: 2
                        }
                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 9
                            color: "#c0c8d8"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}

