import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "Components"

Rectangle {
    id: root
    width: 320
    height: parent ? parent.height : 600
    color: "#0e1014"
    border.color: "#242938"
    border.width: 1

    property var controller: null
    property int selectedPresetId: 1

    FileDialog {
        id: csvExportDialog
        title: "Export Telemetry CSV"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV files (*.csv)", "All files (*)"]
        defaultSuffix: "csv"
        onAccepted: {
            if (controller && selectedFile) {
                var path = selectedFile.toString().replace(/^file:\/{2,3}/, "")
                controller.exportTelemetryCsv(path)
            }
        }
    }

    FileDialog {
        id: presetExportDialog
        title: "Export Optical Presets (JSON)"
        fileMode: FileDialog.SaveFile
        nameFilters: ["JSON files (*.json)", "All files (*)"]
        defaultSuffix: "json"
        onAccepted: {
            if (controller && selectedFile) {
                var path = selectedFile.toString().replace(/^file:\/{2,3}/, "")
                controller.exportPresetsJson(path)
            }
        }
    }

    FileDialog {
        id: presetImportDialog
        title: "Import Optical Presets (JSON)"
        fileMode: FileDialog.OpenFile
        nameFilters: ["JSON files (*.json)", "All files (*)"]
        onAccepted: {
            if (controller && selectedFile) {
                var path = selectedFile.toString().replace(/^file:\/{2,3}/, "")
                controller.importPresetsJson(path)
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 8
        clip: true

        ColumnLayout {
            width: root.width - 16
            spacing: 8

            // Header Title Bar
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "SYSTEM DIAGNOSTICS & TELEMETRY"
                    color: "#f0f4fc"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.0
                }

                Item { Layout.fillWidth: true }

                StatusBadge {
                    active: controller ? controller.isPortConnected : true
                    activeColor: "#00e676"
                    inactiveColor: "#ff1744"
                    stateText: {
                        if (!controller) return "OFFLINE"
                        if (controller.limitFlags !== 0) return "LIMIT"
                        if (controller.isAfRunning) return "AF BUSY"
                        return "NOMINAL"
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#22293a"
            }

            // CARD 1: ACTUATOR TELEMETRY & TRAVEL LIMITS
            Rectangle {
                Layout.fillWidth: true
                height: 140
                color: "#141720"
                border.color: "#242938"
                border.width: 1
                radius: 3

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Text {
                        text: "ACTUATOR POSITION & LIMITS"
                        color: "#808a9d"
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 0.5
                    }

                    // Focus Readout & Progress
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "FOCUS"; color: "#00e676"; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 45 }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 8
                            radius: 2
                            color: "#0e1014"
                            Rectangle {
                                width: parent.width * (controller ? (controller.currentFocus / 65535.0) : 0.0)
                                height: parent.height
                                radius: 2
                                color: "#00e676"
                            }
                        }
                        Text {
                            text: controller ? controller.currentFocus.toString() : "0"
                            color: "#f0f4fc"
                            font.pixelSize: 10
                            font.family: "Monospace"
                            font.bold: true
                            Layout.preferredWidth: 48
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    // Zoom Readout & Progress
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "ZOOM"; color: "#00e5ff"; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 45 }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 8
                            radius: 2
                            color: "#0e1014"
                            Rectangle {
                                width: parent.width * (controller ? (controller.currentZoom / 65535.0) : 0.0)
                                height: parent.height
                                radius: 2
                                color: "#00e5ff"
                            }
                        }
                        Text {
                            text: controller ? controller.currentZoom.toString() : "0"
                            color: "#f0f4fc"
                            font.pixelSize: 10
                            font.family: "Monospace"
                            font.bold: true
                            Layout.preferredWidth: 48
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    // Actuation Speeds
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "SPEEDS:"; color: "#808a9d"; font.pixelSize: 9 }
                        Text {
                            text: "Focus: " + (controller ? controller.focusSpeed : 127) + " | Zoom: " + (controller ? controller.zoomSpeed : 127)
                            color: "#f0f4fc"
                            font.pixelSize: 9
                            font.family: "Monospace"
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Limit Switch Indicators
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text { text: "LIMITS:"; color: "#808a9d"; font.pixelSize: 9; font.bold: true }

                        StatusBadge {
                            stateText: "NEAR"
                            active: controller ? ((controller.limitFlags & 0x01) !== 0) : false
                            activeColor: "#ff1744"
                            inactiveColor: "#1a2130"
                        }
                        StatusBadge {
                            stateText: "FAR"
                            active: controller ? ((controller.limitFlags & 0x02) !== 0) : false
                            activeColor: "#ff1744"
                            inactiveColor: "#1a2130"
                        }
                        StatusBadge {
                            stateText: "WIDE"
                            active: controller ? ((controller.limitFlags & 0x04) !== 0) : false
                            activeColor: "#ff1744"
                            inactiveColor: "#1a2130"
                        }
                        StatusBadge {
                            stateText: "TELE"
                            active: controller ? ((controller.limitFlags & 0x08) !== 0) : false
                            activeColor: "#ff1744"
                            inactiveColor: "#1a2130"
                        }
                    }
                }
            }

            // CARD 2: HARDWARE DRIVER & SERIAL COMM TELEMETRY
            Rectangle {
                Layout.fillWidth: true
                height: 90
                color: "#141720"
                border.color: "#242938"
                border.width: 1
                radius: 3

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 3

                    Text {
                        text: "HARDWARE DRIVER & COMMUNICATION"
                        color: "#808a9d"
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 0.5
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "DRIVER:"; color: "#808a9d"; font.pixelSize: 9 }
                        Text {
                            text: controller ? controller.driverName : "MOCK"
                            color: "#00e5ff"
                            font.bold: true
                            font.pixelSize: 9
                        }
                        Item { Layout.fillWidth: true }
                        StatusBadge {
                            active: controller ? controller.isPortConnected : true
                            activeColor: "#00e676"
                            inactiveColor: "#ff1744"
                            stateText: (controller && controller.isPortConnected) ? "ONLINE" : "OFFLINE"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "PORT:"; color: "#808a9d"; font.pixelSize: 9 }
                        Text {
                            text: controller ? (controller.boundPortName + " @ " + controller.boundBaudRate + " bps") : "IN-MEMORY"
                            color: "#f0f4fc"
                            font.pixelSize: 9
                            font.family: "Monospace"
                        }
                        Item { Layout.fillWidth: true }
                        Text { text: "ADDR:"; color: "#808a9d"; font.pixelSize: 9 }
                        Text {
                            text: controller ? controller.cameraAddress.toString() : "1"
                            color: "#00e676"
                            font.pixelSize: 9
                            font.family: "Monospace"
                            font.bold: true
                        }
                    }
                }
            }

            // CARD 3: AUTOFOCUS ENGINE & OPTICAL SUMMIT
            Rectangle {
                Layout.fillWidth: true
                height: 110
                color: "#141720"
                border.color: "#242938"
                border.width: 1
                radius: 3

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 3

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "AUTOFOCUS ENGINE & PEAK SUMMIT"
                            color: "#808a9d"
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: controller ? controller.afStateString : "Idle"
                            color: (controller && controller.isAfRunning) ? "#ff9100" : "#00e676"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "SHARPNESS SCORE:"; color: "#808a9d"; font.pixelSize: 9 }
                        Text {
                            text: controller ? controller.latestMetricScore.toFixed(2) : "0.00"
                            color: "#00e5ff"
                            font.pixelSize: 12
                            font.bold: true
                            font.family: "Monospace"
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "R²: " + (controller ? (controller.isPeakValid ? controller.peakR2.toFixed(3) : "N/A") : "N/A")
                            color: (controller && controller.isPeakValid && controller.peakR2 > 0.90) ? "#00e676" : "#808a9d"
                            font.pixelSize: 9
                            font.bold: true
                            font.family: "Monospace"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "SUMMIT (x0):"; color: "#808a9d"; font.pixelSize: 9 }
                        Text {
                            text: controller ? (controller.isPeakValid ? controller.peakSummit.toFixed(1) : "---") : "---"
                            color: "#f0f4fc"
                            font.pixelSize: 9
                            font.family: "Monospace"
                        }
                        Item { Layout.fillWidth: true }
                        Text { text: "FWHM:"; color: "#808a9d"; font.pixelSize: 9 }
                        Text {
                            text: controller ? (controller.isPeakValid ? controller.peakFwhm.toFixed(1) : "---") : "---"
                            color: "#f0f4fc"
                            font.pixelSize: 9
                            font.family: "Monospace"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "CURVATURE:"; color: "#808a9d"; font.pixelSize: 9 }
                        Text {
                            text: controller ? (controller.isPeakValid ? controller.peakCurvature.toFixed(4) : "---") : "---"
                            color: "#f0f4fc"
                            font.pixelSize: 9
                            font.family: "Monospace"
                        }
                    }
                }
            }

            // CARD 4: REAL-TIME EXECUTION LATENCIES & PROFILING
            Rectangle {
                Layout.fillWidth: true
                height: 120
                color: "#141720"
                border.color: "#242938"
                border.width: 1
                radius: 3

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "EXECUTION LATENCIES & PROFILING"
                            color: "#808a9d"
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: (controller ? controller.currentFps.toFixed(1) : "0.0") + " FPS"
                            color: "#00e676"
                            font.pixelSize: 10
                            font.bold: true
                            font.family: "Monospace"
                        }
                    }

                    // Metric Latency Bar
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: "Metric Kernel"; color: "#c0c8d8"; font.pixelSize: 9; Layout.preferredWidth: 80 }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 8
                            radius: 2
                            color: "#0e1014"
                            Rectangle {
                                width: Math.min(parent.width, parent.width * (controller && controller.telemetryBuffer ? (controller.telemetryBuffer.latestMetricLatencyUs / 3000.0) : 0.0))
                                height: parent.height
                                radius: 2
                                color: (controller && controller.telemetryBuffer && controller.telemetryBuffer.latestMetricLatencyUs > 2000) ? "#ff9100" : "#00e5ff"
                            }
                        }
                        Text {
                            text: (controller && controller.telemetryBuffer ? controller.telemetryBuffer.latestMetricLatencyUs : 0) + " us"
                            color: "#808a9d"
                            font.pixelSize: 9
                            font.family: "Monospace"
                            Layout.preferredWidth: 50
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    // Loop Latency Bar
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: "Control Loop"; color: "#c0c8d8"; font.pixelSize: 9; Layout.preferredWidth: 80 }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 8
                            radius: 2
                            color: "#0e1014"
                            Rectangle {
                                width: Math.min(parent.width, parent.width * (controller && controller.telemetryBuffer ? (controller.telemetryBuffer.latestLoopLatencyUs / 10000.0) : 0.0))
                                height: parent.height
                                radius: 2
                                color: (controller && controller.telemetryBuffer && controller.telemetryBuffer.latestLoopLatencyUs > 8000) ? "#ff1744" : "#00e676"
                            }
                        }
                        Text {
                            text: (controller && controller.telemetryBuffer ? controller.telemetryBuffer.latestLoopLatencyUs : 0) + " us"
                            color: "#808a9d"
                            font.pixelSize: 9
                            font.family: "Monospace"
                            Layout.preferredWidth: 50
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    // Latency Stats Summary
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Metric Stats (us): Min " + (controller && controller.telemetryBuffer ? controller.telemetryBuffer.minMetricScore.toFixed(0) : "0") +
                                  " | Avg " + (controller && controller.telemetryBuffer ? controller.telemetryBuffer.avgMetricLatencyUs : 0) +
                                  " | Max " + (controller && controller.telemetryBuffer ? controller.telemetryBuffer.maxMetricLatencyUs : 0)
                            color: "#808a9d"
                            font.pixelSize: 9
                            font.family: "Monospace"
                        }
                    }
                }
            }

            // CARD 5: TACTICAL OPTICAL PRESETS
            Rectangle {
                Layout.fillWidth: true
                height: 200
                color: "#141720"
                border.color: "#242938"
                border.width: 1
                radius: 3

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "OPTICAL PRESETS (CHANNEL " + (controller ? controller.activeChannel : 0) + ")"
                            color: "#808a9d"
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "DEFAULTS"
                            onClicked: if (controller) controller.resetPresetsToDefault()
                            contentItem: Text { text: "DEFAULTS"; color: "#808a9d"; font.pixelSize: 8; font.bold: true }
                            background: Rectangle { implicitWidth: 50; implicitHeight: 18; color: "#1a2130"; radius: 2 }
                        }
                    }

                    // 8-Slot Quick Grid (2 rows of 4)
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        rowSpacing: 4
                        columnSpacing: 4

                        Repeater {
                            model: controller && controller.presetManager ? controller.presetManager.currentChannelPresets : 8

                            Button {
                                id: pBtn
                                Layout.fillWidth: true
                                implicitHeight: 28
                                checkable: true
                                checked: root.selectedPresetId === (index + 1)
                                onClicked: root.selectedPresetId = index + 1

                                property var pData: modelData

                                contentItem: Column {
                                    anchors.centerIn: parent
                                    Text {
                                        text: "P" + (index + 1)
                                        color: pBtn.checked ? "#00e5ff" : ((pData && pData.isValid) ? "#00e676" : "#606a7d")
                                        font.pixelSize: 10
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }

                                background: Rectangle {
                                    color: pBtn.checked ? "#1f2738" : ((pData && pData.isValid) ? "#161c28" : "#10131a")
                                    border.color: pBtn.checked ? "#00e5ff" : ((pData && pData.isValid) ? "#2a3d35" : "#1e2433")
                                    border.width: pBtn.checked ? 1.5 : 1
                                    radius: 2
                                }
                            }
                        }
                    }

                    // Selected Preset Info
                    Rectangle {
                        Layout.fillWidth: true
                        height: 48
                        color: "#0e1014"
                        border.color: "#242938"
                        border.width: 1
                        radius: 2

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 4
                            spacing: 1

                            Text {
                                property var pMap: controller && controller.presetManager ? controller.presetManager.getPresetMap(root.selectedPresetId) : null
                                text: {
                                    if (!pMap || !pMap.isValid) return "P" + root.selectedPresetId + ": [EMPTY / UNASSIGNED SLOT]"
                                    return "P" + root.selectedPresetId + ": " + pMap.name + " (Z: " + pMap.zoomPosition + ", F: " + pMap.focusPosition + ")"
                                }
                                color: (pMap && pMap.isValid) ? "#00e676" : "#808a9d"
                                font.pixelSize: 9
                                font.bold: true
                            }

                            Text {
                                property var pMap: controller && controller.presetManager ? controller.presetManager.getPresetMap(root.selectedPresetId) : null
                                text: {
                                    if (!pMap || !pMap.isValid) return "Click 'SAVE CURRENT' to record active lens position"
                                    return "Speeds: Z " + pMap.zoomSpeed + " / F " + pMap.focusSpeed + " | AF: " + pMap.afAlgorithmName
                                }
                                color: "#808a9d"
                                font.pixelSize: 8
                            }
                        }
                    }

                    // Preset Action Buttons
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Button {
                            Layout.fillWidth: true
                            text: "RECALL"
                            onClicked: if (controller) controller.recallPreset(root.selectedPresetId)
                            contentItem: Text { text: "RECALL"; color: "#00e676"; font.pixelSize: 9; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { implicitHeight: 22; color: "#16281e"; border.color: "#00e676"; radius: 2 }
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "SAVE CURRENT"
                            onClicked: {
                                if (controller) {
                                    var defaultName = "Preset " + root.selectedPresetId
                                    controller.saveCurrentAsPreset(root.selectedPresetId, defaultName)
                                }
                            }
                            contentItem: Text { text: "SAVE"; color: "#00e5ff"; font.pixelSize: 9; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { implicitHeight: 22; color: "#16222e"; border.color: "#00e5ff"; radius: 2 }
                        }

                        Button {
                            Layout.preferredWidth: 32
                            text: "X"
                            onClicked: if (controller) controller.deletePreset(root.selectedPresetId)
                            contentItem: Text { text: "DEL"; color: "#ff1744"; font.pixelSize: 8; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { implicitHeight: 22; color: "#28161a"; border.color: "#ff1744"; radius: 2 }
                        }
                    }

                    // JSON Profile I/O Buttons
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Button {
                            Layout.fillWidth: true
                            text: "IMPORT JSON"
                            onClicked: presetImportDialog.open()
                            contentItem: Text { text: "IMPORT JSON"; color: "#808a9d"; font.pixelSize: 8; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { implicitHeight: 20; color: "#141720"; border.color: "#242938"; radius: 2 }
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "EXPORT JSON"
                            onClicked: presetExportDialog.open()
                            contentItem: Text { text: "EXPORT JSON"; color: "#808a9d"; font.pixelSize: 8; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { implicitHeight: 20; color: "#141720"; border.color: "#242938"; radius: 2 }
                        }
                    }
                }
            }

            // CARD 6: SHARPNESS STRIP-CHART & CSV LOGGING
            Rectangle {
                Layout.fillWidth: true
                height: 150
                color: "#141720"
                border.color: "#242938"
                border.width: 1
                radius: 3

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "SHARPNESS HISTORY STRIP-CHART"
                            color: "#808a9d"
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: (controller && controller.telemetryBuffer ? controller.telemetryBuffer.count : 0) + " / " +
                                  (controller && controller.telemetryBuffer ? controller.telemetryBuffer.capacity : 300)
                            color: "#00e5ff"
                            font.pixelSize: 9
                            font.family: "Monospace"
                        }
                    }

                    // Canvas Mini Strip-Chart
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#0e1014"
                        border.color: "#242938"
                        border.width: 1
                        radius: 2

                        Canvas {
                            id: stripChartCanvas
                            anchors.fill: parent
                            anchors.margins: 2

                            Connections {
                                target: controller && controller.telemetryBuffer ? controller.telemetryBuffer : null
                                function onHistoryUpdated() {
                                    stripChartCanvas.requestPaint()
                                }
                            }

                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)

                                if (!controller || !controller.telemetryBuffer) return

                                var points = controller.telemetryBuffer.getMetricHistory()
                                if (!points || points.length < 2) return

                                ctx.beginPath()
                                ctx.lineWidth = 1.5
                                ctx.strokeStyle = "#00e5ff"

                                var stepX = width / Math.max(1, points.length - 1)
                                for (var i = 0; i < points.length; ++i) {
                                    var px = i * stepX
                                    var py = height - (points[i] * (height - 4)) - 2
                                    if (i === 0) {
                                        ctx.moveTo(px, py)
                                    } else {
                                        ctx.lineTo(px, py)
                                    }
                                }
                                ctx.stroke()
                            }
                        }
                    }

                    // CSV Actions
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Button {
                            Layout.fillWidth: true
                            text: "EXPORT CSV LOG"
                            onClicked: csvExportDialog.open()
                            contentItem: Text { text: "EXPORT CSV"; color: "#00e676"; font.pixelSize: 9; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { implicitHeight: 22; color: "#16281e"; border.color: "#00e676"; radius: 2 }
                        }

                        Button {
                            Layout.preferredWidth: 60
                            text: "CLEAR"
                            onClicked: if (controller) controller.clearTelemetryBuffer()
                            contentItem: Text { text: "CLEAR"; color: "#808a9d"; font.pixelSize: 9; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { implicitHeight: 22; color: "#141720"; border.color: "#242938"; radius: 2 }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }
        }
    }
}
