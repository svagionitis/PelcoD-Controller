import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Components"

ScrollView {
    id: root

    property var controller: null

    implicitWidth: 340
    implicitHeight: 760
    clip: true

    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    Column {
        width: root.width - 16
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12
        topPadding: 8
        bottomPadding: 16

        // =====================================================================
        // CARD 1: REAL-TIME PARFOCAL TRACKING MASTER
        // =====================================================================
        Rectangle {
            width: parent.width
            height: trackingCol.implicitHeight + 20
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: trackingCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 10

                RowLayout {
                    width: parent.width

                    Text {
                        text: "PARFOCAL TRACKING MASTER"
                        color: "#00e5ff"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                    }

                    Item { Layout.fillWidth: true }

                    StatusBadge {
                        stateText: (root.controller && root.controller.zoomTrackEnabled) ? "ENGAGED" : "BYPASS"
                        active: root.controller ? root.controller.zoomTrackEnabled : false
                        activeColor: "#00e676"
                        inactiveColor: "#242938"
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#1f2432"
                }

                // Master Zoom Tracking Toggle Switch
                RowLayout {
                    width: parent.width

                    Column {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: "SYNCHRONOUS FOCUS SLEW"
                            color: "#f0f4fc"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: "Actuates focus dynamically during zoom"
                            color: "#808a9d"
                            font.pixelSize: 9
                        }
                    }

                    TacticalSwitch {
                        checked: root.controller ? root.controller.zoomTrackEnabled : false
                        onToggled: {
                            if (root.controller) root.controller.setZoomTrackEnabled(checked)
                        }
                    }
                }

                // Mathematical Interpolation Algorithm
                Column {
                    width: parent.width
                    spacing: 4

                    Text {
                        text: "INTERPOLATION ALGORITHM"
                        color: "#808a9d"
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 0.5
                    }

                    RowLayout {
                        width: parent.width
                        spacing: 4

                        Button {
                            id: pchipBtn
                            Layout.fillWidth: true
                            text: "PCHIP SPLINE"
                            checkable: true
                            checked: root.controller ? (root.controller.interpolationMode === 0) : true
                            onClicked: {
                                if (root.controller) root.controller.setInterpolationMode(0)
                            }
                            contentItem: Text {
                                text: pchipBtn.text
                                color: pchipBtn.checked ? "#00e5ff" : "#808a9d"
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitHeight: 26
                                color: pchipBtn.checked ? "#1f2738" : "#141720"
                                border.color: pchipBtn.checked ? "#00e5ff" : "#242938"
                                border.width: pchipBtn.checked ? 1.5 : 1
                                radius: 3
                            }
                        }

                        Button {
                            id: linearBtn
                            Layout.fillWidth: true
                            text: "LINEAR"
                            checkable: true
                            checked: root.controller ? (root.controller.interpolationMode === 1) : false
                            onClicked: {
                                if (root.controller) root.controller.setInterpolationMode(1)
                            }
                            contentItem: Text {
                                text: linearBtn.text
                                color: linearBtn.checked ? "#ff9100" : "#808a9d"
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitHeight: 26
                                color: linearBtn.checked ? "#1f2738" : "#141720"
                                border.color: linearBtn.checked ? "#ff9100" : "#242938"
                                border.width: linearBtn.checked ? 1.5 : 1
                                radius: 3
                            }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // CARD 2: DISTANCE CURVE SET MANAGER
        // =====================================================================
        Rectangle {
            width: parent.width
            height: curveSetCol.implicitHeight + 20
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: curveSetCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 10

                RowLayout {
                    width: parent.width

                    Text {
                        text: "DISTANCE PROFILE SELECTOR"
                        color: "#00e5ff"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: root.controller ? ("CURVES: " + root.controller.curveCount + "/8") : "CURVES: 1/8"
                        color: "#808a9d"
                        font.family: "Monospace"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#1f2432"
                }

                // Active Curve Dropdown
                TacticalComboBox {
                    width: parent.width
                    model: {
                        var list = [];
                        var count = (root.controller) ? root.controller.curveCount : 1;
                        for (var i = 0; i < count; ++i) {
                            if (i === 0) list.push("Curve 0 (Infinity / Nominal)");
                            else list.push("Curve " + i + " (Finite Distance)");
                        }
                        return list;
                    }
                    currentIndex: root.controller ? root.controller.activeCurveIndex : 0
                    onActivated: function(index) {
                        if (root.controller) root.controller.setActiveCurveIndex(index)
                    }
                }

                // Distance in Meters Input
                RowLayout {
                    width: parent.width
                    spacing: 8

                    Text {
                        text: "TARGET DISTANCE:"
                        color: "#808a9d"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    TextField {
                        id: distInput
                        Layout.fillWidth: true
                        text: {
                            if (!root.controller) return "0.0";
                            return root.controller.activeCurveDistance.toFixed(1);
                        }
                        color: "#f0f4fc"
                        font.family: "Monospace"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                        background: Rectangle {
                            color: "#0a0c10"
                            border.color: distInput.activeFocus ? "#00e5ff" : "#242938"
                            border.width: 1
                            radius: 3
                        }
                        onEditingFinished: {
                            var val = parseFloat(text);
                            if (!isNaN(val) && root.controller) {
                                root.controller.setActiveCurveDistance(val);
                            }
                        }
                    }

                    Text {
                        text: "METERS (0=INF)"
                        color: "#808a9d"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }
        }

        // =====================================================================
        // CARD 3: KNOT TABLE EDITOR
        // =====================================================================
        Rectangle {
            width: parent.width
            height: knotEditorCol.implicitHeight + 20
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: knotEditorCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 8

                RowLayout {
                    width: parent.width

                    Text {
                        text: "CALIBRATED KNOT TABLE"
                        color: "#00e5ff"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                    }

                    Item { Layout.fillWidth: true }

                    // Monotonic Validity Indicator
                    Text {
                        text: (root.controller && root.controller.isCurveValid) ? "STRICTLY MONOTONIC" : "INVALID / NON-MONOTONIC"
                        color: (root.controller && root.controller.isCurveValid) ? "#00e676" : "#ff1744"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#1f2432"
                }

                // Table Header
                Rectangle {
                    width: parent.width
                    height: 22
                    color: "#181d28"
                    radius: 2

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6

                        Text { text: "#"; color: "#808a9d"; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 20 }
                        Text { text: "ZOOM"; color: "#00e5ff"; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 60 }
                        Text { text: "FOCUS"; color: "#00e676"; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 60 }
                        Text { text: "SLOPE"; color: "#ff9100"; font.pixelSize: 9; font.bold: true; Layout.fillWidth: true }
                        Text { text: "ACT"; color: "#808a9d"; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 28 }
                    }
                }

                // Knots List (scrollable / clamped)
                Column {
                    width: parent.width
                    spacing: 2

                    Repeater {
                        model: root.controller ? root.controller.activeCurveKnots : []

                        Rectangle {
                            width: knotEditorCol.width
                            height: 24
                            color: (index % 2 === 0) ? "#11141c" : "#141720"
                            border.color: "#1f2432"
                            border.width: 1
                            radius: 2

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 4
                                spacing: 4

                                Text {
                                    text: (index + 1).toString()
                                    color: "#808a9d"
                                    font.family: "Monospace"
                                    font.pixelSize: 10
                                    Layout.preferredWidth: 20
                                }

                                Text {
                                    text: modelData.zoom.toString()
                                    color: "#00e5ff"
                                    font.family: "Monospace"
                                    font.pixelSize: 10
                                    font.bold: true
                                    Layout.preferredWidth: 60
                                }

                                Text {
                                    text: modelData.focus.toString()
                                    color: "#00e676"
                                    font.family: "Monospace"
                                    font.pixelSize: 10
                                    font.bold: true
                                    Layout.preferredWidth: 60
                                }

                                Text {
                                    text: modelData.slope.toFixed(4)
                                    color: "#ff9100"
                                    font.family: "Monospace"
                                    font.pixelSize: 9
                                    Layout.fillWidth: true
                                }

                                // Delete Knot Button
                                Button {
                                    id: delBtn
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 18
                                    text: "X"
                                    onClicked: {
                                        if (root.controller) root.controller.removeKnot(index)
                                    }
                                    contentItem: Text {
                                        text: delBtn.text
                                        color: "#ff1744"
                                        font.pixelSize: 10
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        color: delBtn.pressed ? "#3b1219" : "#1a1215"
                                        border.color: "#ff1744"
                                        border.width: 1
                                        radius: 2
                                    }
                                }
                            }
                        }
                    }
                }

                // Add Current Operating Point as Knot
                RowLayout {
                    width: parent.width
                    spacing: 6

                    Button {
                        id: addOpKnotBtn
                        Layout.fillWidth: true
                        text: "+ CAPTURE CURRENT POS"
                        onClicked: {
                            if (root.controller) {
                                root.controller.addKnot(root.controller.currentZoom, root.controller.currentFocus);
                            }
                        }
                        contentItem: Text {
                            text: addOpKnotBtn.text
                            color: "#00e5ff"
                            font.pixelSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 26
                            color: addOpKnotBtn.pressed ? "#1f2738" : "#141720"
                            border.color: "#00e5ff"
                            border.width: 1
                            radius: 3
                        }
                    }

                    Button {
                        id: sortBtn
                        Layout.preferredWidth: 70
                        text: "SORT"
                        onClicked: {
                            if (root.controller) root.controller.sortKnots();
                        }
                        contentItem: Text {
                            text: sortBtn.text
                            color: "#808a9d"
                            font.pixelSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 26
                            color: "#141720"
                            border.color: "#242938"
                            border.width: 1
                            radius: 3
                        }
                    }

                    Button {
                        id: clearBtn
                        Layout.preferredWidth: 70
                        text: "CLEAR"
                        onClicked: {
                            if (root.controller) root.controller.clearKnots();
                        }
                        contentItem: Text {
                            text: clearBtn.text
                            color: "#ff1744"
                            font.pixelSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 26
                            color: "#141720"
                            border.color: "#ff1744"
                            border.width: 1
                            radius: 3
                        }
                    }
                }
            }
        }

        // =====================================================================
        // CARD 4: AUTOMATED STEP-AND-LEARN CALIBRATION WIZARD
        // =====================================================================
        Rectangle {
            width: parent.width
            height: wizardCol.implicitHeight + 20
            color: "#141720"
            border.color: (root.controller && root.controller.isCalibrating) ? "#ff9100" : "#242938"
            border.width: (root.controller && root.controller.isCalibrating) ? 1.5 : 1
            radius: 4

            Column {
                id: wizardCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 10

                RowLayout {
                    width: parent.width

                    Text {
                        text: "AUTOMATED CALIBRATION WIZARD"
                        color: "#ff9100"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                    }

                    Item { Layout.fillWidth: true }

                    StatusBadge {
                        stateText: (root.controller && root.controller.isCalibrating) ? "CALIBRATING" : "IDLE"
                        active: root.controller ? root.controller.isCalibrating : false
                        activeColor: "#ff9100"
                        inactiveColor: "#242938"
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#1f2432"
                }

                // Sample Knot Count Slider
                TacticalSlider {
                    width: parent.width
                    label: "SAMPLE KNOTS COUNT"
                    fromValue: 4
                    toValue: 24
                    stepSize: 1
                    value: 8
                    id: knotsSlider
                }

                // Action Pushbuttons: Start vs Cancel
                RowLayout {
                    width: parent.width
                    spacing: 8

                    Button {
                        id: startWizardBtn
                        Layout.fillWidth: true
                        text: "START CALIBRATION WIZARD"
                        enabled: root.controller ? !root.controller.isCalibrating : true
                        onClicked: {
                            if (root.controller) {
                                root.controller.startCalibWizard(
                                    Math.round(knotsSlider.value),
                                    root.controller.zoomMin + 1000,
                                    root.controller.zoomMax - 1000,
                                    root.controller.activeCurveDistance
                                );
                            }
                        }
                        contentItem: Text {
                            text: startWizardBtn.text
                            color: startWizardBtn.enabled ? "#00e5ff" : "#4a5568"
                            font.pixelSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 30
                            color: startWizardBtn.enabled ? (startWizardBtn.pressed ? "#1f2738" : "#141720") : "#0a0c10"
                            border.color: startWizardBtn.enabled ? "#00e5ff" : "#242938"
                            border.width: 1.5
                            radius: 3
                        }
                    }

                    Button {
                        id: abortWizardBtn
                        Layout.preferredWidth: 80
                        text: "ABORT"
                        enabled: root.controller ? root.controller.isCalibrating : false
                        onClicked: {
                            if (root.controller) root.controller.cancelCalibWizard();
                        }
                        contentItem: Text {
                            text: abortWizardBtn.text
                            color: abortWizardBtn.enabled ? "#ff1744" : "#4a5568"
                            font.pixelSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 30
                            color: abortWizardBtn.enabled ? "#2b1216" : "#0a0c10"
                            border.color: abortWizardBtn.enabled ? "#ff1744" : "#242938"
                            border.width: 1
                            radius: 3
                        }
                    }
                }

                // Wizard Progress Bar and Status Readout
                Column {
                    width: parent.width
                    spacing: 4
                    visible: root.controller ? root.controller.isCalibrating : false

                    RowLayout {
                        width: parent.width
                        Text {
                            text: root.controller ? ("STEP " + root.controller.calibStep + " / " + root.controller.calibTotalSteps) : "STEP 0/0"
                            color: "#ff9100"
                            font.family: "Monospace"
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.controller ? Math.round(root.controller.calibProgress * 100) + "%" : "0%"
                            color: "#f0f4fc"
                            font.family: "Monospace"
                            font.pixelSize: 10
                        }
                    }

                    // Progress Bar Fill
                    Rectangle {
                        width: parent.width
                        height: 6
                        color: "#0a0c10"
                        border.color: "#242938"
                        border.width: 1
                        radius: 3

                        Rectangle {
                            height: parent.height - 2
                            x: 1
                            y: 1
                            width: Math.max(0, (parent.width - 2) * (root.controller ? root.controller.calibProgress : 0.0))
                            color: "#ff9100"
                            radius: 2
                        }
                    }

                    Text {
                        text: root.controller ? root.controller.calibStatusText : "Idle"
                        color: "#808a9d"
                        font.pixelSize: 10
                    }
                }
            }
        }

        // =====================================================================
        // CARD 5: PROFILE FILE IMPORT & EXPORT
        // =====================================================================
        Rectangle {
            width: parent.width
            height: fileIoCol.implicitHeight + 20
            color: "#141720"
            border.color: "#242938"
            border.width: 1
            radius: 4

            Column {
                id: fileIoCol
                width: parent.width - 20
                anchors.centerIn: parent
                spacing: 8

                Text {
                    text: "CALIBRATION PROFILE I/O"
                    color: "#00e5ff"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.0
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#1f2432"
                }

                // File Path Field
                TextField {
                    id: pathField
                    width: parent.width
                    placeholderText: "lut_curves_profile.txt / .json"
                    text: "parfocal_curves.txt"
                    color: "#f0f4fc"
                    font.family: "Monospace"
                    font.pixelSize: 10
                    background: Rectangle {
                        color: "#0a0c10"
                        border.color: pathField.activeFocus ? "#00e5ff" : "#242938"
                        border.width: 1
                        radius: 3
                    }
                }

                // Sightline LUT Buttons
                RowLayout {
                    width: parent.width
                    spacing: 6

                    Button {
                        id: loadLutBtn
                        Layout.fillWidth: true
                        text: "LOAD SIGHTLINE LUT"
                        onClicked: {
                            if (root.controller) root.controller.loadTrackCurves(pathField.text);
                        }
                        contentItem: Text {
                            text: loadLutBtn.text
                            color: "#f0f4fc"
                            font.pixelSize: 9
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 26
                            color: "#1f2738"
                            border.color: "#242938"
                            border.width: 1
                            radius: 3
                        }
                    }

                    Button {
                        id: saveLutBtn
                        Layout.fillWidth: true
                        text: "SAVE SIGHTLINE LUT"
                        onClicked: {
                            if (root.controller) root.controller.saveTrackCurves(pathField.text);
                        }
                        contentItem: Text {
                            text: saveLutBtn.text
                            color: "#f0f4fc"
                            font.pixelSize: 9
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 26
                            color: "#1f2738"
                            border.color: "#242938"
                            border.width: 1
                            radius: 3
                        }
                    }
                }

                // JSON Profile Buttons
                RowLayout {
                    width: parent.width
                    spacing: 6

                    Button {
                        id: loadJsonBtn
                        Layout.fillWidth: true
                        text: "LOAD JSON PROFILE"
                        onClicked: {
                            if (root.controller) root.controller.importJsonProfile(pathField.text);
                        }
                        contentItem: Text {
                            text: loadJsonBtn.text
                            color: "#00e5ff"
                            font.pixelSize: 9
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 26
                            color: "#141720"
                            border.color: "#00e5ff"
                            border.width: 1
                            radius: 3
                        }
                    }

                    Button {
                        id: saveJsonBtn
                        Layout.fillWidth: true
                        text: "SAVE JSON PROFILE"
                        onClicked: {
                            if (root.controller) root.controller.exportJsonProfile(pathField.text);
                        }
                        contentItem: Text {
                            text: saveJsonBtn.text
                            color: "#00e5ff"
                            font.pixelSize: 9
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 26
                            color: "#141720"
                            border.color: "#00e5ff"
                            border.width: 1
                            radius: 3
                        }
                    }
                }
            }
        }
    }
}
