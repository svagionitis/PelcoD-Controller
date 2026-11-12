import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Components"

Rectangle {
    id: root
    width: 360
    height: parent ? parent.height : 600
    color: "#12151d"
    border.color: "#1e2433"
    border.width: 1

    property var controller: null

    ScrollView {
        anchors.fill: parent
        anchors.margins: 12
        clip: true

        ColumnLayout {
            width: root.width - 24
            spacing: 12

            // Title Bar
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "DIAGNOSTICS & TELEMETRY"
                    color: "#f0f4fc"
                    font.pixelSize: 12
                    font.bold: true
                    font.letterSpacing: 1.5
                }

                Item { Layout.fillWidth: true }

                StatusBadge {
                    stateText: controller ? controller.engineStateText : "IDLE"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#22293a"
            }

            // Tracking Health Card
            Rectangle {
                Layout.fillWidth: true
                height: 104
                color: "#161a24"
                border.color: "#242c3d"
                border.width: 1
                radius: 3

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Text {
                        text: "FEATURE TRACKING HEALTH"
                        color: "#8894ab"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Column {
                            Layout.fillWidth: true
                            Text { text: "DETECTED"; color: "#8894ab"; font.pixelSize: 9 }
                            Text { text: controller ? controller.detectedFeatures.toString() : "0"; color: "#f0f4fc"; font.bold: true; font.pixelSize: 14 }
                        }

                        Column {
                            Layout.fillWidth: true
                            Text { text: "TRACKED"; color: "#8894ab"; font.pixelSize: 9 }
                            Text { text: controller ? controller.trackedFeatures.toString() : "0"; color: "#00e5ff"; font.bold: true; font.pixelSize: 14 }
                        }

                        Column {
                            Layout.fillWidth: true
                            Text { text: "INLIERS"; color: "#8894ab"; font.pixelSize: 9 }
                            Text { text: controller ? controller.inlierCount.toString() : "0"; color: "#00e676"; font.bold: true; font.pixelSize: 14 }
                        }

                        Column {
                            Layout.fillWidth: true
                            Text { text: "CONSENSUS"; color: "#8894ab"; font.pixelSize: 9 }
                            Text { text: controller ? (controller.inlierRatio * 100).toFixed(1) + "%" : "0.0%"; color: "#00e676"; font.bold: true; font.pixelSize: 14 }
                        }
                    }

                    // Inlier Ratio Progress Bar
                    Rectangle {
                        Layout.fillWidth: true
                        height: 6
                        radius: 3
                        color: "#10131a"

                        Rectangle {
                            width: parent.width * (controller ? controller.inlierRatio : 0.0)
                            height: parent.height
                            radius: 3
                            color: (controller && controller.inlierRatio < 0.25) ? "#ff1744" : "#00e676"
                        }
                    }
                }
            }

            // Pipeline Stage Execution Latencies Card
            Rectangle {
                Layout.fillWidth: true
                height: 180
                color: "#161a24"
                border.color: "#242c3d"
                border.width: 1
                radius: 3

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "STAGE LATENCIES (MICROSECONDS)"; color: "#8894ab"; font.pixelSize: 10; font.bold: true }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "TOTAL: " + (controller ? controller.totalLatencyMs.toFixed(2) : "0.00") + " ms"
                            color: (controller && controller.totalLatencyMs > 33.3) ? "#ff1744" : "#00e5ff"
                            font.bold: true
                            font.pixelSize: 10
                        }
                    }

                    LatencyBar { label: "Pyramid Gen"; valueUs: controller ? controller.pyramidLatencyUs : 0; maxUs: 4000 }
                    LatencyBar { label: "Shi-Tomasi Corners"; valueUs: controller ? controller.featureDetectLatencyUs : 0; maxUs: 4000 }
                    LatencyBar { label: "Pyramidal LK Flow"; valueUs: controller ? controller.opticalFlowLatencyUs : 0; maxUs: 4000 }
                    LatencyBar { label: "MSAC Model Fit"; valueUs: controller ? controller.motionModelLatencyUs : 0; maxUs: 4000 }
                    LatencyBar { label: "Kalman Filter"; valueUs: controller ? controller.trajectoryFilterLatencyUs : 0; maxUs: 1000 }
                    LatencyBar { label: "SIMD Warp"; valueUs: controller ? controller.warpLatencyUs : 0; maxUs: 4000 }
                }
            }

            // 2x3 Affine Transformation Matrices Card
            Rectangle {
                Layout.fillWidth: true
                height: 160
                color: "#161a24"
                border.color: "#242c3d"
                border.width: 1
                radius: 3

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Text { text: "2x3 AFFINE MATRICES [ a11  a12  tx / a21  a22  ty ]"; color: "#8894ab"; font.pixelSize: 10; font.bold: true }

                    MatrixView {
                        title: "Raw Inter-Frame Motion"
                        accentColor: "#00e5ff"
                        elements: controller ? controller.getMatrixElements(0) : [1,0,0,0,1,0]
                    }

                    MatrixView {
                        title: "Smoothed Trajectory"
                        accentColor: "#00e676"
                        elements: controller ? controller.getMatrixElements(1) : [1,0,0,0,1,0]
                    }

                    MatrixView {
                        title: "Active Warp Correction"
                        accentColor: "#ff9100"
                        elements: controller ? controller.getMatrixElements(2) : [1,0,0,0,1,0]
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }

    // Helper Component for Latency Bar
    component LatencyBar: RowLayout {
        Layout.fillWidth: true
        spacing: 6

        property string label: ""
        property int valueUs: 0
        property int maxUs: 4000

        Text {
            text: label
            color: "#c0c8d8"
            font.pixelSize: 9
            Layout.preferredWidth: 90
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.fillWidth: true
            height: 10
            radius: 2
            color: "#10131a"

            Rectangle {
                width: Math.min(parent.width, parent.width * (valueUs / maxUs))
                height: parent.height
                radius: 2
                color: valueUs > (maxUs * 0.75) ? "#ff9100" : "#00b4d8"
            }
        }

        Text {
            text: valueUs + " us"
            color: "#8894ab"
            font.pixelSize: 9
            font.family: "Monospace"
            Layout.preferredWidth: 48
            horizontalAlignment: Text.AlignRight
        }
    }

    // Helper Component for Matrix View
    component MatrixView: Column {
        width: parent ? parent.width : 300
        spacing: 2

        property string title: ""
        property color accentColor: "#00e5ff"
        property var elements: [1,0,0,0,1,0]

        Text {
            text: title
            color: accentColor
            font.pixelSize: 9
            font.bold: true
        }

        Rectangle {
            width: parent.width
            height: 22
            color: "#10131a"
            radius: 2

            Text {
                anchors.centerIn: parent
                text: {
                    if (!elements || elements.length < 6) return "[ 1.000 0.000 +0.00 | 0.000 1.000 +0.00 ]";
                    return "[" + elements[0].toFixed(3) + " " + elements[1].toFixed(3) + " " + (elements[2] >= 0 ? "+" : "") + elements[2].toFixed(1) +
                           " | " + elements[3].toFixed(3) + " " + elements[4].toFixed(3) + " " + (elements[5] >= 0 ? "+" : "") + elements[5].toFixed(1) + "]";
                }
                color: "#e0e6f0"
                font.pixelSize: 9
                font.family: "Monospace"
            }
        }
    }
}

