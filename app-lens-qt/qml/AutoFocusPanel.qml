import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Components"

ScrollView {
    id: root

    property var controller: null

    implicitWidth: 320
    implicitHeight: 720
    clip: true

    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    Column {
        width: root.width - 16
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12
        topPadding: 8
        bottomPadding: 16

        // =====================================================================
        // SECTION 1: AUTOFOCUS EXECUTION & STATE
        // =====================================================================
        Rectangle {
            width: parent.width
            height: execCol.implicitHeight + 20
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: execCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 10

                // Header & State Annunciator
                RowLayout {
                    width: parent.width

                    Text {
                        text: "AUTOFOCUS CONTROLS"
                        color: "#00e5ff"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                    }

                    Item { Layout.fillWidth: true }

                    StatusBadge {
                        stateText: root.controller ? root.controller.afStateString : "IDLE"
                        active: root.controller ? root.controller.isAfRunning : false
                        activeColor: "#00e5ff"
                        inactiveColor: (root.controller && root.controller.afState === 7) ? "#00e676" : "#242938"
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#1f2432"
                }

                // Algorithm Selection Dropdown
                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "SEARCH ALGORITHM"
                        color: "#808a9d"
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 0.5
                    }

                    TacticalComboBox {
                        width: parent.width
                        model: [
                            "Sightline Seek (Adaptive)",
                            "Sightline 3-Phase Scan",
                            "Golden Section Search",
                            "Adaptive Momentum",
                            "DCT AC Frequency",
                            "Native Block Camera"
                        ]
                        currentIndex: root.controller ? root.controller.afAlgorithm : 1
                        onActivated: function(index) {
                            if (root.controller) root.controller.setAfAlgorithm(index)
                        }
                    }
                }

                // Focus Metric Kernel Dropdown
                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "SHARPNESS METRIC KERNEL"
                        color: "#808a9d"
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 0.5
                    }

                    TacticalComboBox {
                        width: parent.width
                        model: [
                            "Sobel Gradient Squared",
                            "Discrete Laplacian",
                            "Tenengrad Threshold",
                            "Vollath Correlation F4",
                            "Vollath Correlation F5",
                            "DCT High-Frequency AC"
                        ]
                        currentIndex: root.controller ? root.controller.focusMetric : 0
                        onActivated: function(index) {
                            if (root.controller) root.controller.setFocusMetric(index)
                        }
                    }
                }

                // Action Buttons: [TRIGGER AF] and [ABORT]
                RowLayout {
                    width: parent.width
                    spacing: 8

                    Button {
                        id: triggerBtn
                        Layout.fillWidth: true
                        height: 34
                        enabled: root.controller ? !root.controller.isAfRunning : true
                        text: (root.controller && root.controller.isAfRunning) ? "SCANNING..." : "TRIGGER AUTOFOCUS"
                        onClicked: {
                            if (root.controller) root.controller.triggerAutoFocus()
                        }

                        contentItem: Text {
                            text: triggerBtn.text
                            color: "#ffffff"
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.0
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: triggerBtn.enabled ? (triggerBtn.down ? "#00b248" : "#00e676") : "#1c2a22"
                            radius: 3
                            border.color: triggerBtn.enabled ? "#69f0ae" : "#242938"
                            border.width: 1
                        }
                    }

                    Button {
                        id: abortBtn
                        Layout.preferredWidth: 80
                        height: 34
                        enabled: root.controller ? root.controller.isAfRunning : false
                        text: "ABORT"
                        onClicked: {
                            if (root.controller) root.controller.abortAutoFocus()
                        }

                        contentItem: Text {
                            text: abortBtn.text
                            color: "#ffffff"
                            font.pixelSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: abortBtn.enabled ? (abortBtn.down ? "#b71c1c" : "#ff1744") : "#241b20"
                            radius: 3
                            border.color: abortBtn.enabled ? "#ff5252" : "#242938"
                            border.width: 1
                        }
                    }
                }
            }
        }

        // =====================================================================
        // SECTION 2: CONTINUOUS AUTOFOCUS (CAF)
        // =====================================================================
        Rectangle {
            width: parent.width
            height: cafCol.implicitHeight + 20
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: cafCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 8

                RowLayout {
                    width: parent.width

                    Text {
                        text: "CONTINUOUS AF (CAF)"
                        color: "#00e676"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                    }

                    Item { Layout.fillWidth: true }

                    TacticalSwitch {
                        checked: root.controller ? root.controller.cafEnabled : false
                        onToggled: {
                            if (root.controller) root.controller.setCafEnabled(checked)
                        }
                    }
                }

                Text {
                    text: "Monitors scene sharpness continuously; automatically triggers refocus when metric drops below threshold."
                    color: "#808a9d"
                    font.pixelSize: 9
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                // Defocus Sensitivity Slider
                TacticalSlider {
                    width: parent.width
                    label: "DEFOCUS TRIGGER FRACTION"
                    accentColor: "#00e676"
                    unit: "%"
                    from: 5
                    to: 50
                    stepSize: 1
                    value: root.controller ? Math.round(root.controller.cafThreshold * 100.0) : 25
                    onValueChanged: {
                        if (root.controller) root.controller.setCafThreshold(value / 100.0)
                    }
                }
            }
        }

        // =====================================================================
        // SECTION 3: REGION OF INTEREST (ROI) & TRACKER LOCK
        // =====================================================================
        Rectangle {
            width: parent.width
            height: roiCol.implicitHeight + 20
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: roiCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 8

                RowLayout {
                    width: parent.width

                    Text {
                        text: "EVALUATION REGION (ROI)"
                        color: "#ff9100"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: root.controller ?
                            (Math.round(root.controller.roiRect.width * 100) + "% x " +
                             Math.round(root.controller.roiRect.height * 100) + "%") : "50% x 50%"
                        color: "#ff9100"
                        font.family: "Monospace"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Text {
                    text: "QUICK TACTICAL PRESETS"
                    color: "#808a9d"
                    font.pixelSize: 9
                    font.bold: true
                }

                // Preset Buttons Row
                RowLayout {
                    width: parent.width
                    spacing: 4

                    Repeater {
                        model: [
                            { name: "FULL", idx: 0 },
                            { name: "50%", idx: 1 },
                            { name: "SPOT", idx: 2 },
                            { name: "LOWER", idx: 3 },
                            { name: "UPPER", idx: 4 }
                        ]

                        Button {
                            id: presetBtn
                            Layout.fillWidth: true
                            height: 24
                            text: modelData.name
                            onClicked: {
                                if (root.controller) root.controller.applyRoiPreset(modelData.idx)
                            }

                            contentItem: Text {
                                text: presetBtn.text
                                color: "#f0f4fc"
                                font.pixelSize: 9
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                color: presetBtn.down ? "#2a3449" : "#1a2130"
                                border.color: "#242938"
                                border.width: 1
                                radius: 2
                            }
                        }
                    }
                }

                // Tracker Lock Toggle
                Button {
                    id: trackLockBtn
                    width: parent.width
                    height: 28
                    checkable: true
                    checked: root.controller ? root.controller.isRoiLockedToTrack : false
                    onToggled: {
                        if (root.controller) root.controller.setIsRoiLockedToTrack(checked)
                    }

                    contentItem: RowLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: trackLockBtn.checked ? "#00e5ff" : "#525c70"
                        }
                        Text {
                            text: trackLockBtn.checked ? "LOCKED TO TARGET TRACKER" : "LOCK ROI TO TARGET TRACKER"
                            color: trackLockBtn.checked ? "#00e5ff" : "#808a9d"
                            font.pixelSize: 9
                            font.bold: true
                            font.letterSpacing: 0.5
                        }
                    }

                    background: Rectangle {
                        color: trackLockBtn.checked ? "#142533" : "#1a2130"
                        border.color: trackLockBtn.checked ? "#00e5ff" : "#242938"
                        border.width: 1
                        radius: 3
                    }
                }
            }
        }

        // =====================================================================
        // SECTION 4: FOCUS PEAKING SHIMMER
        // =====================================================================
        Rectangle {
            width: parent.width
            height: peakCol.implicitHeight + 20
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: peakCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 8

                RowLayout {
                    width: parent.width

                    Text {
                        text: "FOCUS PEAKING OVERLAY"
                        color: "#00e676"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                    }

                    Item { Layout.fillWidth: true }

                    TacticalSwitch {
                        checked: root.controller ? root.controller.showPeaking : false
                        onToggled: {
                            if (root.controller) root.controller.setShowPeaking(checked)
                        }
                    }
                }

                // Peaking Palette Dropdown
                RowLayout {
                    width: parent.width
                    spacing: 8

                    Text {
                        text: "COLOR PALETTE:"
                        color: "#808a9d"
                        font.pixelSize: 9
                        font.bold: true
                    }

                    TacticalComboBox {
                        Layout.fillWidth: true
                        model: ["Tactical Emerald", "Tactical Cyan", "Tactical Coral"]
                        currentIndex: root.controller ? root.controller.peakingColorIndex : 0
                        onActivated: function(index) {
                            if (root.controller) root.controller.setPeakingColorIndex(index)
                        }
                    }
                }

                // Peaking Threshold Slider
                TacticalSlider {
                    width: parent.width
                    label: "EDGE SENSITIVITY THRESHOLD"
                    accentColor: root.controller ?
                        (root.controller.peakingColorIndex === 1 ? "#00e5ff" :
                         (root.controller.peakingColorIndex === 2 ? "#ff1744" : "#00e676")) : "#00e676"
                    from: 20
                    to: 200
                    stepSize: 5
                    value: root.controller ? root.controller.peakingThreshold : 80
                    onValueChanged: {
                        if (root.controller) root.controller.setPeakingThreshold(value)
                    }
                }
            }
        }

        // =====================================================================
        // SECTION 5: MATHEMATICAL PEAK FIT TELEMETRY
        // =====================================================================
        Rectangle {
            width: parent.width
            height: telCol.implicitHeight + 20
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: telCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: "PEAK FIT TELEMETRY"
                    color: "#f0f4fc"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.0
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#1f2432"
                }

                RowLayout {
                    width: parent.width
                    Text { text: "SUMMIT VERTEX:"; color: "#808a9d"; font.pixelSize: 10 }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.controller ? Math.round(root.controller.peakSummit).toString() : "---"
                        color: "#ff9100"
                        font.family: "Monospace"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                RowLayout {
                    width: parent.width
                    Text { text: "PEAK SCORE:"; color: "#808a9d"; font.pixelSize: 10 }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.controller ? root.controller.peakScore.toFixed(1) : "---"
                        color: "#00e676"
                        font.family: "Monospace"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                RowLayout {
                    width: parent.width
                    Text { text: "FWHM (STEPS):"; color: "#808a9d"; font.pixelSize: 10 }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.controller ? Math.round(root.controller.peakFwhm).toString() : "---"
                        color: "#00e5ff"
                        font.family: "Monospace"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                RowLayout {
                    width: parent.width
                    Text { text: "R² GOODNESS-OF-FIT:"; color: "#808a9d"; font.pixelSize: 10 }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.controller ? root.controller.peakR2.toFixed(3) : "---"
                        color: (root.controller && root.controller.peakR2 >= 0.85) ? "#00e676" : "#ff9100"
                        font.family: "Monospace"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                RowLayout {
                    width: parent.width
                    Text { text: "QUADRATIC CURVATURE:"; color: "#808a9d"; font.pixelSize: 10 }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.controller ? root.controller.peakCurvature.toExponential(2) : "---"
                        color: "#808a9d"
                        font.family: "Monospace"
                        font.pixelSize: 10
                    }
                }
            }
        }
    }
}
