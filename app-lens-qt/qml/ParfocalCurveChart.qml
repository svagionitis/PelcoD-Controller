import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Components"

Rectangle {
    id: root

    property var controller: null
    property color activeSplineColor: "#00e5ff"
    property color linearLineColor: "#ff9100"
    property color knotPointColor: "#00e676"
    property color reticleColor: "#00e676"

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

        // Top Header Row with Parfocal Telemetry Readout
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "PARFOCAL ZOOM-TO-FOCUS PROFILE"
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

            // Distance Curve Tag Pill
            Rectangle {
                implicitWidth: distText.implicitWidth + 12
                implicitHeight: 20
                color: "#1f2738"
                border.color: "#00e5ff"
                border.width: 1
                radius: 3

                Text {
                    id: distText
                    anchors.centerIn: parent
                    text: {
                        if (!root.controller) return "DIST: ---";
                        if (root.controller.activeCurveDistance <= 0.0) return "DIST: INFINITY";
                        return "DIST: " + root.controller.activeCurveDistance.toFixed(1) + "m";
                    }
                    color: "#00e5ff"
                    font.family: "Monospace"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            // Mode Pill (PCHIP vs Linear)
            Rectangle {
                implicitWidth: modeText.implicitWidth + 12
                implicitHeight: 20
                color: "#1f2738"
                border.color: (root.controller && root.controller.interpolationMode === 0) ? "#00e5ff" : "#ff9100"
                border.width: 1
                radius: 3

                Text {
                    id: modeText
                    anchors.centerIn: parent
                    text: (root.controller && root.controller.interpolationMode === 0) ? "PCHIP SPLINE" : "PIECEWISE LINEAR"
                    color: (root.controller && root.controller.interpolationMode === 0) ? "#00e5ff" : "#ff9100"
                    font.family: "Monospace"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            // Tracking Engaged / Bypass Status Badge
            Rectangle {
                implicitWidth: trackText.implicitWidth + 12
                implicitHeight: 20
                color: "#1f2738"
                border.color: (root.controller && root.controller.zoomTrackEnabled) ? "#00e676" : "#808a9d"
                border.width: 1
                radius: 3

                Text {
                    id: trackText
                    anchors.centerIn: parent
                    text: (root.controller && root.controller.zoomTrackEnabled) ? "TRACKING: ENGAGED" : "TRACKING: BYPASS"
                    color: (root.controller && root.controller.zoomTrackEnabled) ? "#00e676" : "#808a9d"
                    font.family: "Monospace"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Item { Layout.fillWidth: true }

            // Numerical Real-Time Operating Coordinate Readout
            Text {
                text: {
                    if (!root.controller) return "Z: -----  F: -----";
                    var zStr = ("00000" + root.controller.currentZoom).slice(-5);
                    var fStr = ("00000" + root.controller.currentFocus).slice(-5);
                    return "OPERATING: Z=" + zStr + "  F=" + fStr;
                }
                color: "#00e676"
                font.family: "Monospace"
                font.pixelSize: 11
                font.bold: true
            }
        }

        // Main Curve Plot Area
        Rectangle {
            id: plotArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0a0c10"
            border.color: "#1f2432"
            border.width: 1
            radius: 3
            clip: true

            Canvas {
                id: curveCanvas
                anchors.fill: parent
                antialiasing: true

                // Re-render when knots or controller telemetry change
                Connections {
                    target: root.controller
                    function onKnotsUpdated() { curveCanvas.requestPaint() }
                    function onTelemetryUpdated() { curveCanvas.requestPaint() }
                    function onInterpolationModeChanged() { curveCanvas.requestPaint() }
                    function onActiveCurveChanged() { curveCanvas.requestPaint() }
                    function onZoomTrackChanged() { curveCanvas.requestPaint() }
                }

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();

                    var w = width;
                    var h = height;
                    if (w <= 0 || h <= 0) return;

                    var padLeft = 46;
                    var padRight = 16;
                    var padTop = 14;
                    var padBottom = 24;

                    var plotW = w - padLeft - padRight;
                    var plotH = h - padTop - padBottom;
                    if (plotW <= 0 || plotH <= 0) return;

                    // Background fill
                    ctx.fillStyle = "#0a0c10";
                    ctx.fillRect(0, 0, w, h);

                    var zMin = (root.controller) ? root.controller.zoomMin : 0;
                    var zMax = (root.controller) ? root.controller.zoomMax : 65535;
                    var fMin = (root.controller) ? root.controller.focusMin : 0;
                    var fMax = (root.controller) ? root.controller.focusMax : 65535;
                    if (zMax <= zMin) zMax = 65535;
                    if (fMax <= fMin) fMax = 65535;

                    function toScreenX(z) {
                        return padLeft + ((z - zMin) / (zMax - zMin)) * plotW;
                    }

                    function toScreenY(f) {
                        return padTop + (1.0 - ((f - fMin) / (fMax - fMin))) * plotH;
                    }

                    // --- Grid lines and Axis Ticks ---
                    ctx.lineWidth = 1;
                    ctx.strokeStyle = "#161b26";
                    ctx.fillStyle = "#636d80";
                    ctx.font = "9px monospace";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "top";

                    // Vertical grid (Zoom)
                    var zSteps = 5;
                    for (var zi = 0; zi <= zSteps; ++zi) {
                        var zVal = Math.round(zMin + (zi / zSteps) * (zMax - zMin));
                        var gx = toScreenX(zVal);
                        ctx.beginPath();
                        ctx.moveTo(gx, padTop);
                        ctx.lineTo(gx, padTop + plotH);
                        ctx.stroke();
                        ctx.fillText(zVal.toString(), gx, padTop + plotH + 5);
                    }

                    // Horizontal grid (Focus)
                    ctx.textAlign = "right";
                    ctx.textBaseline = "middle";
                    var fSteps = 4;
                    for (var fi = 0; fi <= fSteps; ++fi) {
                        var fVal = Math.round(fMin + (fi / fSteps) * (fMax - fMin));
                        var gy = toScreenY(fVal);
                        ctx.beginPath();
                        ctx.moveTo(padLeft, gy);
                        ctx.lineTo(padLeft + plotW, gy);
                        ctx.stroke();
                        ctx.fillText(fVal.toString(), padLeft - 6, gy);
                    }

                    // Axis Labels
                    ctx.fillStyle = "#808a9d";
                    ctx.font = "8px sans-serif";
                    ctx.textAlign = "right";
                    ctx.fillText("ZOOM (Z) ->", w - padRight, padTop + plotH + 14);

                    // --- Draw Spline or Linear Curve ---
                    var knots = (root.controller) ? root.controller.activeCurveKnots : [];
                    var isLinear = (root.controller && root.controller.interpolationMode === 1);

                    if (knots && knots.length >= 2) {
                        ctx.lineWidth = 2.5;
                        ctx.strokeStyle = isLinear ? root.linearLineColor : root.activeSplineColor;

                        ctx.beginPath();
                        var startX = toScreenX(knots[0].zoom);
                        var startY = toScreenY(knots[0].focus);
                        ctx.moveTo(startX, startY);

                        for (var k = 0; k + 1 < knots.length; ++k) {
                            var k0 = knots[k];
                            var k1 = knots[k + 1];

                            if (isLinear) {
                                ctx.lineTo(toScreenX(k1.zoom), toScreenY(k1.focus));
                            } else {
                                // Exact PCHIP Hermite to Cubic Bezier conversion:
                                var hk = k1.zoom - k0.zoom;
                                var p1_z = k0.zoom + hk / 3.0;
                                var p1_f = k0.focus + (hk * k0.slope) / 3.0;
                                var p2_z = k1.zoom - hk / 3.0;
                                var p2_f = k1.focus - (hk * k1.slope) / 3.0;

                                ctx.bezierCurveTo(
                                    toScreenX(p1_z), toScreenY(p1_f),
                                    toScreenX(p2_z), toScreenY(p2_f),
                                    toScreenX(k1.zoom), toScreenY(k1.focus)
                                );
                            }
                        }
                        ctx.stroke();

                        // Draw Knots as nodes
                        for (var i = 0; i < knots.length; ++i) {
                            var kx = toScreenX(knots[i].zoom);
                            var ky = toScreenY(knots[i].focus);

                            ctx.beginPath();
                            ctx.arc(kx, ky, 4.5, 0, 2 * Math.PI);
                            ctx.fillStyle = "#141720";
                            ctx.fill();
                            ctx.lineWidth = 1.8;
                            ctx.strokeStyle = root.activeSplineColor;
                            ctx.stroke();

                            ctx.beginPath();
                            ctx.arc(kx, ky, 2.0, 0, 2 * Math.PI);
                            ctx.fillStyle = root.activeSplineColor;
                            ctx.fill();
                        }
                    }

                    // --- Draw Live Operating Position Reticle ---
                    if (root.controller) {
                        var curZ = root.controller.currentZoom;
                        var curF = root.controller.currentFocus;
                        var opX = toScreenX(curZ);
                        var opY = toScreenY(curF);

                        // Operating coordinate guidelines
                        ctx.lineWidth = 1;
                        ctx.strokeStyle = Qt.rgba(0.0, 0.9, 0.46, 0.35);
                        ctx.setLineDash([3, 3]);

                        // Vertical guideline
                        ctx.beginPath();
                        ctx.moveTo(opX, padTop);
                        ctx.lineTo(opX, padTop + plotH);
                        ctx.stroke();

                        // Horizontal guideline
                        ctx.beginPath();
                        ctx.moveTo(padLeft, opY);
                        ctx.lineTo(padLeft + plotW, opY);
                        ctx.stroke();
                        ctx.setLineDash([]);

                        // Crosshair reticle
                        ctx.lineWidth = 1.5;
                        ctx.strokeStyle = root.reticleColor;

                        ctx.beginPath();
                        ctx.moveTo(opX - 8, opY);
                        ctx.lineTo(opX + 8, opY);
                        ctx.moveTo(opX, opY - 8);
                        ctx.lineTo(opX, opY + 8);
                        ctx.stroke();

                        // Outer pulsing target ring
                        ctx.beginPath();
                        ctx.arc(opX, opY, 6, 0, 2 * Math.PI);
                        ctx.lineWidth = 1.5;
                        ctx.strokeStyle = root.reticleColor;
                        ctx.stroke();
                    }
                }
            }
        }
    }
}
