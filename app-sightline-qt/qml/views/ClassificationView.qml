import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

ScrollView {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true
    contentWidth: Math.max(availableWidth, minContentWidth + 32)
    contentHeight: mainCol.implicitHeight + 32
    clip: true
    ScrollBar.vertical.policy: ScrollBar.AsNeeded
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    readonly property int minContentWidth: 640

    Connections {
        target: bridge
        function onKlvMetricFiltersReceived(cam, minW, maxW, minH, maxH, aboveH, belowH, minLat, maxLat, minLon, maxLon) {
            if (cam === aiCam.currentIndex) {
                minWidthInput.text = minW.toFixed(1);
                maxWidthInput.text = maxW.toFixed(1);
                minHeightInput.text = minH.toFixed(1);
                maxHeightInput.text = maxH.toFixed(1);
                aboveHorizonCheck.checked = aboveH;
                belowHorizonCheck.checked = belowH;
                minLatInput.text = minLat.toFixed(4);
                maxLatInput.text = maxLat.toFixed(4);
                minLonInput.text = minLon.toFixed(4);
                maxLonInput.text = maxLon.toFixed(4);
            }
        }
    }

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        // Section Title Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { width: 4; height: 18; color: SightlineTheme.primary; radius: 2 }
            Text {
                text: "DEEP LEARNING AI CLASSIFIER & COMPUTE"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }
            Text {
                text: "// Modules 0xA9, 0xC1, 0x92 (EAN-Detection-Modes Sec 4)"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }
        }

        // Live Telemetry Cards
        Flow {
            Layout.fillWidth: true
            spacing: 10
            MetricCard {
                title: "Compute Engine"
                value: npuSwitch.checked ? "NPU Dedicated" : "ARM CPU"
                accentColor: npuSwitch.checked ? SightlineTheme.success : SightlineTheme.warning
                iconText: "⚡"
            }
            MetricCard {
                title: "Execution Mode"
                value: asyncSwitch.checked ? "Async (0-Drop)" : "Synchronous"
                accentColor: asyncSwitch.checked ? SightlineTheme.info : SightlineTheme.accent
                iconText: "🧵"
            }
            MetricCard {
                title: "Pretrained Model"
                value: modelCombo.currentText.split(" ")[1]
                accentColor: SightlineTheme.primary
                iconText: "🧬"
            }
            MetricCard {
                title: "KLV Metric Filters"
                value: aboveHorizonCheck.checked || belowHorizonCheck.checked ? "Horizon Active" : "Unfiltered"
                accentColor: SightlineTheme.accent
                iconText: "🌐"
            }
        }

        // 1. Classifier Model Selection & Reporting Configuration
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: aiCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: aiCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "OFFICIAL SIGHTLINE PRE-TRAINED MODELS (EAN SEC 4.2)"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Camera Channel:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: aiCam
                        model: ["Camera 0 (EO Visible)", "Camera 1 (IR Thermal)", "Camera 2", "Camera 3"]
                        Layout.preferredWidth: 260
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Pretrained Model:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: modelCombo
                        model: [
                            "0: None (Classifier Inactive)",
                            "1: sla_drone.cls (75k Drone / 75k Non-drone)",
                            "2: sla_drone_large.cls (96k Drone / 140k Non-drone)",
                            "3: sla_vehicle_person.cls (Vehicle, Person, Background)",
                            "4: Custom Model (.cls / .slaod file)"
                        ]
                        currentIndex: 1
                        Layout.preferredWidth: 380
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    visible: modelCombo.currentIndex === 4
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Custom Model File:"; color: SightlineTheme.accent; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: customModelInput
                        text: "custom_detector.slaod"
                        placeholderText: "Path or filename on device"
                        Layout.preferredWidth: 280
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Drone Reporting Mode:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: droneModeCombo
                        model: [
                            "0: Standard (Reports Drone or Background)",
                            "1: Detailed (Fixed Wing, Multi-Rotor, Background, Vehicle)"
                        ]
                        Layout.preferredWidth: 380
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Max Targets Per Frame:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    Slider {
                        id: maxTargetsSlider
                        from: 1
                        to: 10
                        value: 3
                        stepSize: 1
                        Layout.preferredWidth: 180
                    }
                    Text { text: Math.round(maxTargetsSlider.value).toString(); color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Text { text: "Min Dimensions:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    TextField {
                        id: minDimsInput
                        text: "10"
                        Layout.preferredWidth: 50
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Text { text: "px"; color: SightlineTheme.textMuted; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Detection Padding:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    Slider {
                        id: paddingSlider
                        from: 0
                        to: 16
                        value: 4
                        stepSize: 1
                        Layout.preferredWidth: 180
                    }
                    Text { text: Math.round(paddingSlider.value) + " px"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }
                    Text { text: "Update Rate:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    TextField {
                        id: updateRateInput
                        text: "3"
                        Layout.preferredWidth: 50
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        // 2. Hardware Compute Resource Assignment (EAN Sec 4.5)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: computeCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: computeCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "HARDWARE COMPUTE RESOURCE ASSIGNMENT (EAN SEC 4.5)"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    Switch {
                        id: npuSwitch
                        text: "Force Inference to NPU Accelerator (NPU_CONTROL = 0x3C)"
                        checked: true
                    }
                    Item { Layout.fillWidth: true }
                }

                Text {
                    text: "Offloads all deep neural network matrix multiply-accumulate computations onto the onboard hardware NPU."
                    color: SightlineTheme.textMuted
                    font.pixelSize: 11
                    Layout.leftMargin: 6
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    Switch {
                        id: asyncSwitch
                        text: "Asynchronous Execution Worker (CLASSIFY_ASYNC = 0x3D)"
                        checked: true
                    }
                    Item { Layout.fillWidth: true }
                }

                Text {
                    text: "Decouples classification inference from video frame pipeline to eliminate display latency and guarantee zero frame drops."
                    color: SightlineTheme.textMuted
                    font.pixelSize: 11
                    Layout.leftMargin: 6
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Model & Compute Profile"
                        Layout.preferredWidth: 220
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            if (!bridge) return;
                            bridge.setComputeAssignment(npuSwitch.checked, asyncSwitch.checked);
                            bridge.setClassifierSettings(
                                aiCam.currentIndex,
                                modelCombo.currentIndex,
                                customModelInput.text,
                                Math.round(maxTargetsSlider.value),
                                parseInt(minDimsInput.text),
                                droneModeCombo.currentIndex,
                                Math.round(paddingSlider.value),
                                parseInt(updateRateInput.text)
                            );
                        }
                    }

                    Button {
                        text: "Query Settings"
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                        onClicked: {
                            if (bridge) {
                                bridge.queryClassifierConfig(aiCam.currentIndex);
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // 3. Target Metric Dimension & Spatial Filtering (KLV 0xC1, EAN Sec 4.4.4)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: metricCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: metricCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "TARGET METRIC DIMENSION & GEOGRAPHIC FILTERING (KLV 0xC1)"
                    color: SightlineTheme.primary
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Target Width (Meters):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: minWidthInput
                        text: "0.5"
                        placeholderText: "Min"
                        Layout.preferredWidth: 70
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Text { text: "to"; color: SightlineTheme.textMuted; font.pixelSize: 12 }
                    TextField {
                        id: maxWidthInput
                        text: "25.0"
                        placeholderText: "Max"
                        Layout.preferredWidth: 70
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Text { text: "meters"; color: SightlineTheme.textMuted; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text { text: "Target Height (Meters):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: minHeightInput
                        text: "0.5"
                        placeholderText: "Min"
                        Layout.preferredWidth: 70
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Text { text: "to"; color: SightlineTheme.textMuted; font.pixelSize: 12 }
                    TextField {
                        id: maxHeightInput
                        text: "15.0"
                        placeholderText: "Max"
                        Layout.preferredWidth: 70
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Text { text: "meters"; color: SightlineTheme.textMuted; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    CheckBox {
                        id: aboveHorizonCheck
                        text: "Exclude Targets Above Scene Horizon"
                        checked: false
                    }
                    CheckBox {
                        id: belowHorizonCheck
                        text: "Exclude Targets Below Scene Horizon"
                        checked: false
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text { text: "Latitude Bounds (deg):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: minLatInput
                        text: "-90.0000"
                        placeholderText: "Min Lat"
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Text { text: "to"; color: SightlineTheme.textMuted; font.pixelSize: 12 }
                    TextField {
                        id: maxLatInput
                        text: "90.0000"
                        placeholderText: "Max Lat"
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text { text: "Longitude Bounds (deg):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter }
                    TextField {
                        id: minLonInput
                        text: "-180.0000"
                        placeholderText: "Min Lon"
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Text { text: "to"; color: SightlineTheme.textMuted; font.pixelSize: 12 }
                    TextField {
                        id: maxLonInput
                        text: "180.0000"
                        placeholderText: "Max Lon"
                        Layout.preferredWidth: 80
                        color: SightlineTheme.textPrimary
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Metric Dimension Filters"
                        Layout.preferredWidth: 230
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.primary; radius: 4 }
                        onClicked: {
                            if (!bridge) return;
                            bridge.setKlvMetricBounds(
                                aiCam.currentIndex,
                                parseFloat(minWidthInput.text),
                                parseFloat(maxWidthInput.text),
                                parseFloat(minHeightInput.text),
                                parseFloat(maxHeightInput.text),
                                aboveHorizonCheck.checked,
                                belowHorizonCheck.checked,
                                parseFloat(minLatInput.text),
                                parseFloat(maxLatInput.text),
                                parseFloat(minLonInput.text),
                                parseFloat(maxLonInput.text)
                            );
                        }
                    }

                    Button {
                        text: "Query Metric Filters"
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 32
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                        onClicked: {
                            if (bridge) {
                                bridge.queryKlvMetricFilters(aiCam.currentIndex);
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
