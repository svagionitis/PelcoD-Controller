import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import VideoApp 1.0

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    required property VideoPlayerController controller

    /// Video surface providing decode→display latency (optional; card shows "--" when unset).
    property VideoItem videoItem: null

    readonly property bool latencyMeasured: videoItem !== null && videoItem.presentedFrames > 0

    function latencyColor(ms) {
        if (!latencyMeasured) return "#525c70";
        return ms <= 50.0 ? "#00e676" : (ms <= 100.0 ? "#ff9100" : "#ff1744");
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
                text: "TELEMETRY & DIAGNOSTICS"
                color: "#f0f4fc"
                font.pixelSize: 13
                font.bold: true
                font.letterSpacing: 1.2
            }
        }

        // Live Health Cards Grid
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 10
            columnSpacing: 10

            // FPS Card
            Rectangle {
                Layout.fillWidth: true
                height: 75
                color: "#181b24"
                radius: 6
                border.color: "#2a2f40"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2
                    Text {
                        text: "FRAME RATE"
                        color: "#8894ab"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    RowLayout {
                        spacing: 4
                        Text {
                            text: controller.fps.toFixed(1)
                            color: controller.fps >= 24 ? "#00e676" : (controller.fps >= 10 ? "#ff9100" : "#ff1744")
                            font.pixelSize: 22
                            font.bold: true
                            font.family: "Monospace"
                        }
                        Text {
                            text: "FPS"
                            color: "#8894ab"
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignBottom
                        }
                    }
                }
            }

            // Decode Latency Card
            Rectangle {
                Layout.fillWidth: true
                height: 75
                color: "#181b24"
                radius: 6
                border.color: "#2a2f40"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2
                    Text {
                        text: "DECODE TIME"
                        color: "#8894ab"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    RowLayout {
                        spacing: 4
                        Text {
                            text: controller.avgDecodeTimeMs.toFixed(2)
                            color: controller.avgDecodeTimeMs <= 10 ? "#00e5ff" : (controller.avgDecodeTimeMs <= 30 ? "#ff9100" : "#ff1744")
                            font.pixelSize: 22
                            font.bold: true
                            font.family: "Monospace"
                        }
                        Text {
                            text: "ms"
                            color: "#8894ab"
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignBottom
                        }
                    }
                }
            }

            // Bitrate Card
            Rectangle {
                Layout.fillWidth: true
                height: 75
                color: "#181b24"
                radius: 6
                border.color: "#2a2f40"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2
                    Text {
                        text: "EST. BITRATE"
                        color: "#8894ab"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    RowLayout {
                        spacing: 4
                        Text {
                            text: controller.bitrateKbps > 0 ? (controller.bitrateKbps / 1000.0).toFixed(2) : "0.0"
                            color: "#f0f4fc"
                            font.pixelSize: 22
                            font.bold: true
                            font.family: "Monospace"
                        }
                        Text {
                            text: "Mbps"
                            color: "#8894ab"
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignBottom
                        }
                    }
                }
            }

            // Dropped Frames Card
            Rectangle {
                Layout.fillWidth: true
                height: 75
                color: "#181b24"
                radius: 6
                border.color: "#2a2f40"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2
                    Text {
                        text: "DROPPED FRAMES"
                        color: "#8894ab"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    RowLayout {
                        spacing: 4
                        Text {
                            text: controller.droppedFrames.toString()
                            color: controller.droppedFrames > 0 ? "#ff1744" : "#00e676"
                            font.pixelSize: 22
                            font.bold: true
                            font.family: "Monospace"
                        }
                        Text {
                            text: "pkts"
                            color: "#8894ab"
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignBottom
                        }
                    }
                }
            }

            // Display Latency Card (decode → QQuickWindow::frameSwapped)
            Rectangle {
                id: displayLatencyCard
                Layout.fillWidth: true
                Layout.columnSpan: 2
                height: 104
                color: "#181b24"
                radius: 6
                border.color: root.latencyMeasured ? Qt.darker(root.latencyColor(root.videoItem.displayLatencyMs), 2.2) : "#2a2f40"
                Behavior on border.color { ColorAnimation { duration: 300 } }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "DISPLAY LATENCY"
                            color: "#8894ab"
                            font.pixelSize: 10
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        Text {
                            text: "DECODE → SCREEN"
                            color: "#525c70"
                            font.pixelSize: 9
                            font.letterSpacing: 0.8
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: root.latencyMeasured ? root.videoItem.displayLatencyMs.toFixed(1) : "--"
                            color: root.latencyColor(root.latencyMeasured ? root.videoItem.displayLatencyMs : 0)
                            font.pixelSize: 22
                            font.bold: true
                            font.family: "Monospace"
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                        Text {
                            text: "ms avg"
                            color: "#8894ab"
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignBottom
                        }
                        Item { Layout.fillWidth: true }
                        Repeater {
                            model: [
                                { label: "LAST", key: "displayLatencyLastMs" },
                                { label: "MIN", key: "displayLatencyMinMs" },
                                { label: "MAX", key: "displayLatencyMaxMs" }
                            ]
                            delegate: ColumnLayout {
                                spacing: 0
                                Layout.leftMargin: 10
                                Text {
                                    text: modelData.label
                                    color: "#525c70"
                                    font.pixelSize: 9
                                    font.bold: true
                                    Layout.alignment: Qt.AlignRight
                                }
                                Text {
                                    text: root.latencyMeasured ? root.videoItem[modelData.key].toFixed(1) : "--"
                                    color: "#f0f4fc"
                                    font.pixelSize: 12
                                    font.family: "Monospace"
                                    Layout.alignment: Qt.AlignRight
                                }
                            }
                        }
                    }

                    // Budget bar: average latency on a 0–100 ms scale, marker at one 30 FPS frame
                    Rectangle {
                        Layout.fillWidth: true
                        height: 4
                        radius: 2
                        color: "#212530"

                        Rectangle {
                            height: parent.height
                            radius: 2
                            width: root.latencyMeasured
                                   ? parent.width * Math.min(1.0, root.videoItem.displayLatencyMs / 100.0)
                                   : 0
                            color: root.latencyColor(root.latencyMeasured ? root.videoItem.displayLatencyMs : 0)
                            Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }

                        // 1-frame marker at 33.3 ms
                        Rectangle {
                            x: parent.width * 0.333
                            width: 1
                            height: parent.height + 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#8894ab"
                            opacity: 0.6
                        }
                    }
                }

                HoverHandler { id: latencyHover }
                ToolTip.visible: latencyHover.hovered
                ToolTip.delay: 400
                ToolTip.text: "Time from decode completion to QQuickWindow::frameSwapped\n"
                              + "Rolling window of 120 frames. Bar: 0–100 ms, marker = 1 frame @30 FPS.\n"
                              + "Measured frames: " + (root.videoItem ? root.videoItem.presentedFrames : 0)

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: if (root.videoItem) root.videoItem.resetLatency()
                }
            }
        }

        // Detailed Metadata Table
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: detailsCol.implicitHeight + 20
            color: "#181b24"
            radius: 6
            border.color: "#2a2f40"

            ColumnLayout {
                id: detailsCol
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Text {
                    text: "STREAM PARAMETERS"
                    color: "#00e5ff"
                    font.pixelSize: 11
                    font.bold: true
                }

                // Resolution
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Native Resolution"; color: "#8894ab"; font.pixelSize: 12; Layout.fillWidth: true }
                    Text {
                        text: controller.frameWidth > 0 ? (controller.frameWidth + " × " + controller.frameHeight) : "N/A"
                        color: "#f0f4fc"
                        font.pixelSize: 12
                        font.family: "Monospace"
                    }
                }

                // Codec
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Video Codec"; color: "#8894ab"; font.pixelSize: 12; Layout.fillWidth: true }
                    Text {
                        text: controller.codecName.length > 0 ? controller.codecName : "None"
                        color: "#f0f4fc"
                        font.pixelSize: 12
                        font.family: "Monospace"
                    }
                }

                // Pixel Format
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Pixel Format"; color: "#8894ab"; font.pixelSize: 12; Layout.fillWidth: true }
                    Text {
                        text: controller.pixelFormat.length > 0 ? controller.pixelFormat : "None"
                        color: "#f0f4fc"
                        font.pixelSize: 12
                        font.family: "Monospace"
                    }
                }

                // Total Decoded
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Total Frames Decoded"; color: "#8894ab"; font.pixelSize: 12; Layout.fillWidth: true }
                    Text {
                        text: controller.totalFrames.toString()
                        color: "#f0f4fc"
                        font.pixelSize: 12
                        font.family: "Monospace"
                    }
                }

                // Operational Status
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Pipeline State"; color: "#8894ab"; font.pixelSize: 12; Layout.fillWidth: true }
                    Rectangle {
                        height: 20
                        width: stateLabel.implicitWidth + 12
                        radius: 3
                        color: {
                            switch (controller.playbackState) {
                            case VideoPlayerController.Playing: return "#1b382b";
                            case VideoPlayerController.Opening: return "#3d2d14";
                            case VideoPlayerController.Paused: return "#142d3d";
                            case VideoPlayerController.Error: return "#3d141d";
                            default: return "#212530";
                            }
                        }
                        border.color: {
                            switch (controller.playbackState) {
                            case VideoPlayerController.Playing: return "#00e676";
                            case VideoPlayerController.Opening: return "#ff9100";
                            case VideoPlayerController.Paused: return "#00e5ff";
                            case VideoPlayerController.Error: return "#ff1744";
                            default: return "#525c70";
                            }
                        }
                        Text {
                            id: stateLabel
                            anchors.centerIn: parent
                            text: {
                                switch (controller.playbackState) {
                                case VideoPlayerController.Playing: return "ACTIVE";
                                case VideoPlayerController.Opening: return "OPENING";
                                case VideoPlayerController.Paused: return "PAUSED";
                                case VideoPlayerController.Error: return "FAULT";
                                default: return "STANDBY";
                                }
                            }
                            font.pixelSize: 10
                            font.bold: true
                            color: {
                                switch (controller.playbackState) {
                                case VideoPlayerController.Playing: return "#00e676";
                                case VideoPlayerController.Opening: return "#ff9100";
                                case VideoPlayerController.Paused: return "#00e5ff";
                                case VideoPlayerController.Error: return "#ff1744";
                                default: return "#8894ab";
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
