import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LensApp 1.0

Rectangle {
    id: root

    required property LensPlayerController controller

    color: "#11141c"
    border.color: "#1f2432"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Header
        Text {
            text: "SOURCE & OPTICAL SIMULATION"
            color: "#f0f4fc"
            font.pixelSize: 12
            font.bold: true
            font.letterSpacing: 1.2
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#1f2432"
        }

        // Tab Selector
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: ["SYNTHETIC", "FILE", "STREAM", "DEVICE"]
                Button {
                    id: tabBtn
                    Layout.fillWidth: true
                    checkable: true
                    checked: tabStack.currentIndex === index
                    onClicked: tabStack.currentIndex = index

                    contentItem: Text {
                        text: modelData
                        color: tabBtn.checked ? "#00e5ff" : "#808a9d"
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        implicitHeight: 26
                        color: tabBtn.checked ? "#1f2738" : "#141720"
                        border.color: tabBtn.checked ? "#00e5ff" : "#242938"
                        border.width: tabBtn.checked ? 1.5 : 1
                        radius: 3
                    }
                }
            }
        }

        // Tab Content
        StackLayout {
            id: tabStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: 0

            // Tab 0: Synthetic Defocus Simulator
            ColumnLayout {
                spacing: 12

                Text {
                    text: "PROCEDURAL DEFOCUS SIMULATOR"
                    color: "#00e5ff"
                    font.pixelSize: 11
                    font.bold: true
                }

                Text {
                    text: "Generates optical point-spread function (PSF) blur proportional to motor focus distance |f - f_plane|."
                    color: "#808a9d"
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                // Pattern Picker
                Text {
                    text: "TEST PATTERN"
                    color: "#808a9d"
                    font.pixelSize: 10
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Repeater {
                        model: [
                            { name: "SIEMENS", id: 0 },
                            { name: "ISO 12233", id: 1 },
                            { name: "TEXTURE", id: 2 }
                        ]
                        Button {
                            id: patBtn
                            Layout.fillWidth: true
                            text: modelData.name
                            onClicked: {
                                root.controller.setSyntheticPattern(modelData.id)
                                root.controller.openSource("synthetic://" + modelData.name.toLowerCase())
                            }
                            contentItem: Text {
                                text: patBtn.text
                                color: "#f0f4fc"
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                            }
                            background: Rectangle {
                                implicitHeight: 24
                                color: "#161b26"
                                border.color: "#242938"
                                border.width: 1
                                radius: 3
                            }
                        }
                    }
                }

                // Target Plane Focus Slider
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "TARGET IN-FOCUS PLANE:"
                        color: "#f0f4fc"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: ("00000" + Math.round(planeSlider.value)).slice(-5)
                        color: "#00e676"
                        font.family: "Monospace"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Slider {
                    id: planeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 65535
                    value: root.controller.syntheticPlane
                    stepSize: 100
                    onMoved: root.controller.setSyntheticPlane(Math.round(value))
                }

                // Noise Level Slider
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "SENSOR NOISE SIGMA:"
                        color: "#f0f4fc"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: noiseSlider.value.toFixed(1)
                        color: "#ff9100"
                        font.family: "Monospace"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Slider {
                    id: noiseSlider
                    Layout.fillWidth: true
                    from: 0.0
                    to: 20.0
                    value: root.controller.syntheticNoise
                    stepSize: 0.5
                    onMoved: root.controller.setSyntheticNoise(value)
                }

                Button {
                    Layout.fillWidth: true
                    text: "RESET SIMULATOR DEFAULTS"
                    onClicked: {
                        root.controller.setSyntheticPlane(32768)
                        root.controller.setSyntheticNoise(1.5)
                        root.controller.openSource("synthetic://siemens-star")
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#808a9d"
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    background: Rectangle {
                        implicitHeight: 26
                        color: "#141720"
                        border.color: "#242938"
                        border.width: 1
                        radius: 3
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // Tab 1: Video File Ingestion
            ColumnLayout {
                spacing: 12

                Text {
                    text: "LOCAL VIDEO FILE"
                    color: "#00e5ff"
                    font.pixelSize: 11
                    font.bold: true
                }

                TextField {
                    id: filePathInput
                    Layout.fillWidth: true
                    placeholderText: "path/to/video.mp4"
                    text: "sample-videos/dji-ir-subtitles.mp4"
                    color: "#f0f4fc"
                    font.pixelSize: 11
                    background: Rectangle {
                        color: "#161b26"
                        border.color: "#242938"
                        border.width: 1
                        radius: 3
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        text: "LOAD FILE"
                        onClicked: root.controller.openSource(filePathInput.text)
                        contentItem: Text {
                            text: parent.text
                            color: "#00e5ff"
                            font.pixelSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        background: Rectangle {
                            implicitHeight: 28
                            color: "#1f2738"
                            border.color: "#00e5ff"
                            border.width: 1
                            radius: 3
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "LOAD SAMPLE"
                        onClicked: {
                            filePathInput.text = "sample-videos/dji-ir-subtitles.mp4"
                            root.controller.openSource("sample-videos/dji-ir-subtitles.mp4")
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#00e676"
                            font.pixelSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        background: Rectangle {
                            implicitHeight: 28
                            color: "#14251c"
                            border.color: "#00e676"
                            border.width: 1
                            radius: 3
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // Tab 2: RTSP Network Stream
            ColumnLayout {
                spacing: 12

                Text {
                    text: "LIVE NETWORK STREAM"
                    color: "#00e5ff"
                    font.pixelSize: 11
                    font.bold: true
                }

                TextField {
                    id: streamUrlInput
                    Layout.fillWidth: true
                    placeholderText: "rtsp://127.0.0.1:8554/live"
                    text: "rtsp://127.0.0.1:8554/live"
                    color: "#f0f4fc"
                    font.pixelSize: 11
                    background: Rectangle {
                        color: "#161b26"
                        border.color: "#242938"
                        border.width: 1
                        radius: 3
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: "CONNECT RTSP"
                    onClicked: root.controller.openSource(streamUrlInput.text)
                    contentItem: Text {
                        text: parent.text
                        color: "#00e5ff"
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    background: Rectangle {
                        implicitHeight: 28
                        color: "#1f2738"
                        border.color: "#00e5ff"
                        border.width: 1
                        radius: 3
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // Tab 3: Hardware Capture Device
            ColumnLayout {
                spacing: 12

                Text {
                    text: "HARDWARE CAPTURE DEVICE"
                    color: "#00e5ff"
                    font.pixelSize: 11
                    font.bold: true
                }

                TextField {
                    id: deviceInput
                    Layout.fillWidth: true
                    placeholderText: "device://0"
                    text: "device://0"
                    color: "#f0f4fc"
                    font.pixelSize: 11
                    background: Rectangle {
                        color: "#161b26"
                        border.color: "#242938"
                        border.width: 1
                        radius: 3
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: "OPEN CAPTURE DEVICE"
                    onClicked: root.controller.openSource(deviceInput.text)
                    contentItem: Text {
                        text: parent.text
                        color: "#00e5ff"
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    background: Rectangle {
                        implicitHeight: 28
                        color: "#1f2738"
                        border.color: "#00e5ff"
                        border.width: 1
                        radius: 3
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // Bottom Playback Transport Controls
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#1f2432"
        }

        Text {
            text: "PLAYBACK CONTROLS"
            color: "#808a9d"
            font.pixelSize: 10
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Button {
                Layout.fillWidth: true
                text: root.controller.playbackState === 2 ? "PAUSE" : "PLAY"
                onClicked: {
                    if (root.controller.playbackState === 2) {
                        root.controller.pause()
                    } else {
                        root.controller.play()
                    }
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.checked ? "#00e676" : "#00e5ff"
                    font.pixelSize: 10
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    implicitHeight: 28
                    color: "#161b26"
                    border.color: "#00e5ff"
                    border.width: 1
                    radius: 3
                }
            }

            Button {
                Layout.fillWidth: true
                text: "STOP"
                onClicked: root.controller.stop()
                contentItem: Text {
                    text: parent.text
                    color: "#ff1744"
                    font.pixelSize: 10
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    implicitHeight: 28
                    color: "#161b26"
                    border.color: "#ff1744"
                    border.width: 1
                    radius: 3
                }
            }

            Button {
                Layout.fillWidth: true
                text: "STEP"
                onClicked: root.controller.stepFrame()
                contentItem: Text {
                    text: parent.text
                    color: "#f0f4fc"
                    font.pixelSize: 10
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    implicitHeight: 28
                    color: "#161b26"
                    border.color: "#242938"
                    border.width: 1
                    radius: 3
                }
            }
        }

        // Speed Multiplier Chips
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: [0.5, 1.0, 2.0]
                Button {
                    id: speedBtn
                    Layout.fillWidth: true
                    text: modelData.toFixed(1) + "x"
                    onClicked: root.controller.setPlaybackRate(modelData)
                    contentItem: Text {
                        text: speedBtn.text
                        color: "#808a9d"
                        font.pixelSize: 9
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    background: Rectangle {
                        implicitHeight: 20
                        color: "#141720"
                        border.color: "#242938"
                        border.width: 1
                        radius: 2
                    }
                }
            }
        }
    }
}

