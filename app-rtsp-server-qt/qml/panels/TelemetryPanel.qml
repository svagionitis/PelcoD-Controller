import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ScrollView {
    id: telemRoot
    required property var controller
    signal toastRequested(string message)
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: telemRoot.availableWidth
        spacing: 14

        // Live Health & Queue Card
        TacticalCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            title: "Transmission Health & Queue Metrics"
            subtitle: "Live Buffer Depth & Dropped Frames"

            GridLayout {
                anchors.fill: parent
                anchors.margins: 14
                columns: 4
                rowSpacing: 10
                columnSpacing: 14

                // Gauge 1: Queue Depth
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0a0c10"
                    border.color: "#242938"
                    border.width: 1
                    radius: 4

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: telemRoot.controller.queueDepth + " / " + telemRoot.controller.queueCapacity
                            color: {
                                var ratio = telemRoot.controller.queueCapacity > 0 ? telemRoot.controller.queueDepth / telemRoot.controller.queueCapacity : 0
                                if (ratio > 0.85) return "#ff1744"
                                if (ratio > 0.6) return "#ff9100"
                                return "#00e676"
                            }
                            font.pixelSize: 18
                            font.bold: true
                            font.family: "Monospace"
                        }
                        Text {
                            text: "QUEUE DEPTH"
                            color: "#64748b"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }

                // Gauge 2: Egress Bitrate
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0a0c10"
                    border.color: "#242938"
                    border.width: 1
                    radius: 4

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: telemRoot.controller.egressBitrateKbps.toFixed(1) + " kbps"
                            color: "#00e5ff"
                            font.pixelSize: 18
                            font.bold: true
                            font.family: "Monospace"
                        }
                        Text {
                            text: "EGRESS BITRATE"
                            color: "#64748b"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }

                // Gauge 3: Feeder FPS
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0a0c10"
                    border.color: "#242938"
                    border.width: 1
                    radius: 4

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: telemRoot.controller.feederFpsActual.toFixed(1) + " FPS"
                            color: "#f8fafc"
                            font.pixelSize: 18
                            font.bold: true
                            font.family: "Monospace"
                        }
                        Text {
                            text: "FEEDER RATE"
                            color: "#64748b"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }

                // Gauge 4: Dropped Frames
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0a0c10"
                    border.color: "#242938"
                    border.width: 1
                    radius: 4

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: telemRoot.controller.totalDropped.toString()
                            color: telemRoot.controller.totalDropped > 0 ? "#ff1744" : "#00e676"
                            font.pixelSize: 18
                            font.bold: true
                            font.family: "Monospace"
                        }
                        Text {
                            text: "FRAMES DROPPED"
                            color: "#64748b"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }
            }
        }

        // Configuration Presets Manager Card
        TacticalCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 145
            title: "Configuration Presets"
            subtitle: "Save & Load Server Profiles"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                TacticalComboBox {
                    id: presetCombo
                    Layout.fillWidth: true
                    label: "Saved Server Profile Presets"
                    model: telemRoot.controller.presetList
                }

                TacticalButton {
                    Layout.alignment: Qt.AlignBottom
                    Layout.preferredWidth: 90
                    Layout.preferredHeight: 34
                    text: "LOAD"
                    onClicked: {
                        if (presetCombo.currentText.length > 0) {
                            telemRoot.controller.loadPreset(presetCombo.currentText)
                            telemRoot.toastRequested("Loaded preset: " + presetCombo.currentText)
                        }
                    }
                }

                TacticalButton {
                    Layout.alignment: Qt.AlignBottom
                    Layout.preferredWidth: 90
                    Layout.preferredHeight: 34
                    text: "DELETE"
                    isDestructive: true
                    onClicked: {
                        if (presetCombo.currentText.length > 0) {
                            telemRoot.controller.deletePreset(presetCombo.currentText)
                            telemRoot.toastRequested("Deleted preset: " + presetCombo.currentText)
                        }
                    }
                }

                TacticalButton {
                    Layout.alignment: Qt.AlignBottom
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 34
                    text: "SAVE CURRENT"
                    accentColor: "#00e676"
                    onClicked: savePresetDialog.open()
                }
            }
        }
    }

    Dialog {
        id: savePresetDialog
        title: "Save Profile Preset"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: "#141720"
            border.color: "#2e3748"
            border.width: 1
            radius: 6
        }

        ColumnLayout {
            spacing: 8
            Text {
                text: "Enter a name for the current server configuration profile:"
                color: "#e2e8f0"
                font.pixelSize: 12
            }
            TacticalTextField {
                id: presetNameInput
                Layout.fillWidth: true
                placeholderText: "e.g. Flight 101 Live STANAG"
            }
        }

        onAccepted: {
            if (presetNameInput.text.trim().length > 0) {
                telemRoot.controller.savePreset(presetNameInput.text.trim())
                telemRoot.toastRequested("Saved preset: " + presetNameInput.text.trim())
                presetNameInput.text = ""
            }
        }
    }
}
