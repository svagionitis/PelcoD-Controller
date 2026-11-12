import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    width: parent ? parent.width : 600
    height: 140
    color: "#0f1218"
    border.color: "#1e2433"
    border.width: 1

    property var controller: null
    readonly property var ringBuffer: controller ? controller.telemetryRingBuffer : null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        // Top Status Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "TRAJECTORY STRIP-CHART (150 FRAMES)"
                color: "#8894ab"
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.0
            }

            Rectangle { width: 8; height: 2; color: "#00e5ff" }
            Text { text: "RAW Tx"; color: "#00e5ff"; font.pixelSize: 9; font.bold: true }

            Rectangle { width: 8; height: 2; color: "#00e676" }
            Text { text: "SMOOTH Tx"; color: "#00e676"; font.pixelSize: 9; font.bold: true }

            Rectangle { width: 8; height: 2; color: "#ff9100" }
            Text { text: "JITTER VAR"; color: "#ff9100"; font.pixelSize: 9; font.bold: true }

            Item { Layout.fillWidth: true }

            // Soft Damping Engagement Indicator
            Rectangle {
                width: 110
                height: 18
                radius: 2
                color: (ringBuffer && ringBuffer.isSoftMarginEngaged) ? "#3d240d" : "#141720"
                border.color: (ringBuffer && ringBuffer.isSoftMarginEngaged) ? "#ff9100" : "#242938"

                Text {
                    anchors.centerIn: parent
                    text: (ringBuffer && ringBuffer.isSoftMarginEngaged) ? "! SOFT DAMPING !" : "MARGIN: " + (ringBuffer ? (ringBuffer.latestMarginUtilization * 100).toFixed(0) : "0") + "%"
                    color: (ringBuffer && ringBuffer.isSoftMarginEngaged) ? "#ff9100" : "#8894ab"
                    font.pixelSize: 9
                    font.bold: true
                }
            }
        }

        // QML 2D Canvas Strip-Chart
        Canvas {
            id: chartCanvas
            Layout.fillWidth: true
            Layout.fillHeight: true

            Connections {
                target: root.ringBuffer
                function onHistoryUpdated() {
                    chartCanvas.requestPaint();
                }
            }

            onPaint: {
                var ctx = getContext("2d");
                var w = width;
                var h = height;

                ctx.clearRect(0, 0, w, h);

                // Grid background
                ctx.strokeStyle = "#161c28";
                ctx.lineWidth = 1;

                // Center horizon line
                ctx.beginPath();
                ctx.moveTo(0, h * 0.5);
                ctx.lineTo(w, h * 0.5);
                ctx.stroke();

                // Upper & lower soft margin thresholds (80% and 20%)
                ctx.strokeStyle = "#222018";
                ctx.setLineDash([4, 4]);
                ctx.beginPath();
                ctx.moveTo(0, h * 0.2);
                ctx.lineTo(w, h * 0.2);
                ctx.moveTo(0, h * 0.8);
                ctx.lineTo(w, h * 0.8);
                ctx.stroke();
                ctx.setLineDash([]);

                if (!root.ringBuffer || root.ringBuffer.count < 2) {
                    return;
                }

                var rawData = root.ringBuffer.getRawTrajectory(0);
                var smoothData = root.ringBuffer.getSmoothTrajectory(0);
                var jitterData = root.ringBuffer.getJitterHistory();

                var n = rawData.length;
                if (n < 2) return;

                var dx = w / (root.ringBuffer.capacity - 1);

                // 1. Shaded Jitter Variance Area
                if (jitterData.length === n) {
                    ctx.fillStyle = "rgba(255, 145, 0, 0.12)";
                    ctx.beginPath();
                    ctx.moveTo(0, h);
                    for (var j = 0; j < n; ++j) {
                        var jy = h - (jitterData[j] * (h * 0.4));
                        ctx.lineTo(j * dx, jy);
                    }
                    ctx.lineTo((n - 1) * dx, h);
                    ctx.closePath();
                    ctx.fill();
                }

                // 2. Raw Trajectory Line (Cyan)
                ctx.strokeStyle = "#00e5ff";
                ctx.lineWidth = 1.2;
                ctx.beginPath();
                for (var i = 0; i < n; ++i) {
                    var x = i * dx;
                    var y = h - (rawData[i] * h);
                    if (i === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                }
                ctx.stroke();

                // 3. Smooth Trajectory Line (Neon Green)
                ctx.strokeStyle = "#00e676";
                ctx.lineWidth = 1.8;
                ctx.beginPath();
                for (var k = 0; k < n; ++k) {
                    var sx = k * dx;
                    var sy = h - (smoothData[k] * h);
                    if (k === 0) ctx.moveTo(sx, sy);
                    else ctx.lineTo(sx, sy);
                }
                ctx.stroke();
            }
        }
    }
}

