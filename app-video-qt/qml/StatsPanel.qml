import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import VideoApp 1.0

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    required property VideoPlayerController controller

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
                        text: "DECODE LATENCY"
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
