import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Components"

Rectangle {
    id: root

    property var controller: null
    property color curveColor: "#00e5ff"
    property color samplePointColor: "#00e676"
    property color summitLineColor: "#ff9100"

    implicitWidth: 640
    implicitHeight: 220
    color: "#141720"
    border.color: "#242938"
    border.width: 1
    radius: 4
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        // Top Header Row with Telemetry Readout
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "SHARPNESS PROFILE & PEAK FIT"
                color: "#f0f4fc"
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 1.0
            }

            Rectangle {
                width: 1
                height: 14
                color: "#242938"
            }

            // Summit Vertex Pill
            Rectangle {
                implicitWidth: summitText.implicitWidth + 12
                implicitHeight: 20
                color: "#1f2738"
                border.color: "#ff9100"
                border.width: 1
                radius: 3

                Text {
                    id: summitText
                    anchors.centerIn: parent
                    text: root.controller ? ("SUMMIT: " + Math.round(root.controller.peakSummit)) : "SUMMIT: ---"
                    color: "#ff9100"
                    font.family: "Monospace"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            // FWHM Pill
            Rectangle {
                implicitWidth: fwhmText.implicitWidth + 12
                implicitHeight: 20
                color: "#1f2738"
                border.color: "#00e5ff"
                border.width: 1
                radius: 3

                Text {
                    id: fwhmText
                    anchors.centerIn: parent
                    text: root.controller ? ("FWHM: " + Math.round(root.controller.peakFwhm)) : "FWHM: ---"
                    color: "#00e5ff"
                    font.family: "Monospace"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            // R^2 Goodness of Fit Pill
            Rectangle {
                implicitWidth: r2Text.implicitWidth + 12
                implicitHeight: 20
                color: "#1f2738"
                border.color: (root.controller && root.controller.peakR2 >= 0.85) ? "#00e676" : "#ff9100"
                border.width: 1
                radius: 3

                Text {
                    id: r2Text
                    anchors.centerIn: parent
                    text: root.controller ? ("R²: " + root.controller.peakR2.toFixed(3)) : "R²: ---"
                    color: (root.controller && root.controller.peakR2 >= 0.85) ? "#00e676" : "#ff9100"
                    font.family: "Monospace"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Item { Layout.fillWidth: true }

            StatusBadge {
                stateText: (root.controller && root.controller.isPeakValid) ? "PEAK VALID" : "ESTIMATING"
                active: root.controller ? root.controller.isPeakValid : false
                activeColor: "#00e676"
                inactiveColor: "#242938"
            }
        }

        // Main Chart Canvas
        Canvas {
            id: chartCanvas
            Layout.fillWidth: true
            Layout.fillHeight: true

            antialiasing: true

            Connections {
                target: root.controller
                function onSamplesUpdated() { chartCanvas.requestPaint() }
                function onPeakFitUpdated() { chartCanvas.requestPaint() }
                function onTelemetryUpdated() { chartCanvas.requestPaint() }
            }

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()

                var w = width
                var h = height

                if (w <= 0 || h <= 0) return

                var padLeft = 45
                var padRight = 20
                var padTop = 15
                var padBottom = 25

                var plotW = w - padLeft - padRight
                var plotH = h - padTop - padBottom

                // Background
                ctx.fillStyle = "#0c0e14"
                ctx.fillRect(padLeft, padTop, plotW, plotH)

                // Grid lines (3 horizontal, 4 vertical)
                ctx.strokeStyle = "#1b212f"
                ctx.lineWidth = 1.0

                for (var r = 1; r <= 3; ++r) {
                    var gy = padTop + (plotH * (r / 4.0))
                    ctx.beginPath()
                    ctx.moveTo(padLeft, gy)
                    ctx.lineTo(padLeft + plotW, gy)
                    ctx.stroke()
                }
                for (var c = 1; c <= 4; ++c) {
                    var gx = padLeft + (plotW * (c / 5.0))
                    ctx.beginPath()
                    ctx.moveTo(gx, padTop)
                    ctx.lineTo(gx, padTop + plotH)
                    ctx.stroke()
                }

                // Plot border
                ctx.strokeStyle = "#242938"
                ctx.strokeRect(padLeft, padTop, plotW, plotH)

                var samples = (root.controller && root.controller.samplePoints) ? root.controller.samplePoints : []

                var minX = 0
                var maxX = 65535
                var minY = 0
                var maxY = 100

                if (samples.length > 0) {
                    minX = samples[0].x
                    maxX = samples[0].x
                    minY = samples[0].y
                    maxY = samples[0].y

                    for (var i = 1; i < samples.length; ++i) {
                        if (samples[i].x < minX) minX = samples[i].x
                        if (samples[i].x > maxX) maxX = samples[i].x
                        if (samples[i].y < minY) minY = samples[i].y
                        if (samples[i].y > maxY) maxY = samples[i].y
                    }

                    // Margins around range
                    var spanX = Math.max(maxX - minX, 500)
                    minX = Math.max(0, minX - (spanX * 0.1))
                    maxX = Math.min(65535, maxX + (spanX * 0.1))

                    maxY = Math.max(maxY * 1.15, 10.0)
                    minY = Math.max(0, minY * 0.85)
                } else if (root.controller) {
                    var curF = root.controller.currentFocus
                    minX = Math.max(0, curF - 2000)
                    maxX = Math.min(65535, curF + 2000)
                    maxY = Math.max(root.controller.latestMetricScore * 1.25, 100.0)
                }

                if (maxX <= minX) maxX = minX + 1000
                if (maxY <= minY) maxY = minY + 100

                function mapX(val) {
                    return padLeft + ((val - minX) / (maxX - minX)) * plotW
                }
                function mapY(val) {
                    return padTop + plotH - (((val - minY) / (maxY - minY)) * plotH)
                }

                // Y-axis tick labels
                ctx.fillStyle = "#808a9d"
                ctx.font = "9px monospace"
                ctx.textAlign = "right"
                ctx.fillText(Math.round(maxY).toString(), padLeft - 6, padTop + 10)
                ctx.fillText(Math.round((minY + maxY) * 0.5).toString(), padLeft - 6, padTop + (plotH * 0.5) + 3)
                ctx.fillText(Math.round(minY).toString(), padLeft - 6, padTop + plotH)

                // X-axis tick labels
                ctx.textAlign = "center"
                ctx.fillText(Math.round(minX).toString(), padLeft, padTop + plotH + 16)
                ctx.fillText(Math.round((minX + maxX) * 0.5).toString(), padLeft + (plotW * 0.5), padTop + plotH + 16)
                ctx.fillText(Math.round(maxX).toString(), padLeft + plotW, padTop + plotH + 16)

                // Draw fitted Gaussian curve if peak is valid
                if (root.controller && root.controller.isPeakValid && root.controller.peakFwhm > 0) {
                    var mu = root.controller.peakSummit
                    var amp = root.controller.peakScore
                    var fwhm = root.controller.peakFwhm
                    var sigma = fwhm / 2.35482

                    if (sigma > 1.0) {
                        ctx.strokeStyle = root.curveColor
                        ctx.lineWidth = 2.0
                        ctx.beginPath()

                        var started = false
                        var stepPx = 3
                        for (var px = padLeft; px <= padLeft + plotW; px += stepPx) {
                            var vx = minX + ((px - padLeft) / plotW) * (maxX - minX)
                            var diff = (vx - mu) / sigma
                            var vy = amp * Math.exp(-0.5 * diff * diff)
                            var py = mapY(vy)

                            if (!started) {
                                ctx.moveTo(px, py)
                                started = true
                            } else {
                                ctx.lineTo(px, py)
                            }
                        }
                        ctx.stroke()
                    }

                    // Draw vertical summit reticle
                    var summitPx = mapX(mu)
                    if (summitPx >= padLeft && summitPx <= padLeft + plotW) {
                        ctx.strokeStyle = root.summitLineColor
                        ctx.lineWidth = 1.5
                        ctx.setLineDash([4, 3])
                        ctx.beginPath()
                        ctx.moveTo(summitPx, padTop)
                        ctx.lineTo(summitPx, padTop + plotH)
                        ctx.stroke()
                        ctx.setLineDash([])

                        // Summit marker tag
                        ctx.fillStyle = root.summitLineColor
                        ctx.beginPath()
                        ctx.arc(summitPx, mapY(amp), 4, 0, 2 * Math.PI)
                        ctx.fill()
                    }
                }

                // Draw recorded sample points
                if (samples.length > 0) {
                    // Thin connecting line
                    ctx.strokeStyle = Qt.rgba(0.0, 0.9, 0.46, 0.45)
                    ctx.lineWidth = 1.0
                    ctx.beginPath()
                    for (var s = 0; s < samples.length; ++s) {
                        var sx = mapX(samples[s].x)
                        var sy = mapY(samples[s].y)
                        if (s === 0) ctx.moveTo(sx, sy)
                        else ctx.lineTo(sx, sy)
                    }
                    ctx.stroke()

                    // Sample dots
                    ctx.fillStyle = root.samplePointColor
                    for (var d = 0; d < samples.length; ++d) {
                        var dx = mapX(samples[d].x)
                        var dy = mapY(samples[d].y)
                        ctx.beginPath()
                        ctx.arc(dx, dy, 2.5, 0, 2 * Math.PI)
                        ctx.fill()
                    }
                }
            }
        }
    }
}
