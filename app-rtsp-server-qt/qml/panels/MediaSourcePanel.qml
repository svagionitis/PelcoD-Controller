import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"

ScrollView {
    id: sourceRoot
    required property var controller
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: sourceRoot.availableWidth
        spacing: 14

        // Ingest Mode Selector Card
        TacticalCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            title: "Ingestion Media Source"
            subtitle: "Feeder Mode Selection"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                TacticalComboBox {
                    Layout.fillWidth: true
                    label: "Selected Media Generator / File Feeder"
                    model: [
                        "Synthetic Test Pattern Generator (Zero-Dependency H.264)",
                        "MPEG-2 Transport Stream File (.ts / STANAG 4609 KLV)",
                        "H.264 Elementary Stream File (.h264 Annex-B)"
                    ]
                    currentIndex: sourceRoot.controller.feederMode
                    enabledState: sourceRoot.controller.serverState !== 2
                    onActivated: function(idx) { sourceRoot.controller.feederMode = idx }
                }
            }
        }

        // File Selection & Quick Shortcuts Card
        TacticalCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 220
            visible: sourceRoot.controller.feederMode !== 0
            title: "Source File Configuration"
            subtitle: "Local Media Path & Quick Presets"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TacticalTextField {
                        Layout.fillWidth: true
                        label: "Source Media File Path"
                        placeholderText: "e.g. sample-videos/mpegts-klv-day-flight.ts"
                        text: sourceRoot.controller.feederFilePath
                        enabledState: sourceRoot.controller.serverState !== 2
                        onTextChanged: sourceRoot.controller.feederFilePath = text
                    }

                    TacticalButton {
                        Layout.alignment: Qt.AlignBottom
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 34
                        text: "BROWSE"
                        enabledState: sourceRoot.controller.serverState !== 2
                        onClicked: fileDialog.open()
                    }
                }

                Text {
                    text: "QUICK SAMPLE FEEDS (STANAG 4609 FLIGHT RECORDINGS):"
                    color: "#64748b"
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 0.8
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TacticalButton {
                        Layout.fillWidth: true
                        text: "DAY FLIGHT TS"
                        enabledState: sourceRoot.controller.serverState !== 2
                        onClicked: {
                            sourceRoot.controller.feederMode = 1
                            sourceRoot.controller.feederFilePath = "sample-videos/mpegts-klv-day-flight.ts"
                        }
                    }

                    TacticalButton {
                        Layout.fillWidth: true
                        text: "NIGHT IR FLIGHT TS"
                        enabledState: sourceRoot.controller.serverState !== 2
                        onClicked: {
                            sourceRoot.controller.feederMode = 1
                            sourceRoot.controller.feederFilePath = "sample-videos/mpegts-klv-night-flight-IR.ts"
                        }
                    }

                    TacticalButton {
                        Layout.fillWidth: true
                        text: "MOON ROTATION TS"
                        enabledState: sourceRoot.controller.serverState !== 2
                        onClicked: {
                            sourceRoot.controller.feederMode = 1
                            sourceRoot.controller.feederFilePath = "sample-videos/rotating-moon-from-LRO.ts"
                        }
                    }
                }
            }
        }

        // Pacing & Looping Options Card
        TacticalCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            title: "Playback & Pacing Settings"
            subtitle: "Frame Rate & Rewind Behavior"

            GridLayout {
                anchors.fill: parent
                anchors.margins: 14
                columns: 2
                rowSpacing: 10
                columnSpacing: 14

                TacticalTextField {
                    Layout.fillWidth: true
                    label: "Target Frame Rate (FPS)"
                    text: sourceRoot.controller.feederFps.toString()
                    enabledState: sourceRoot.controller.serverState !== 2
                    onTextChanged: {
                        var f = parseInt(text)
                        if (!isNaN(f) && f > 0) sourceRoot.controller.feederFps = f
                    }
                }

                CheckBox {
                    id: loopCheckBox
                    Layout.alignment: Qt.AlignVCenter
                    text: "Continuous Loop Playback (Rewind on EOF)"
                    checked: sourceRoot.controller.feederLoop
                    enabled: sourceRoot.controller.serverState !== 2
                    onToggled: sourceRoot.controller.feederLoop = checked

                    contentItem: Text {
                        text: loopCheckBox.text
                        font.pixelSize: 11
                        font.bold: true
                        color: loopCheckBox.checked ? "#00e5ff" : "#94a3b8"
                        leftPadding: loopCheckBox.indicator ? (loopCheckBox.indicator.width + 8) : 24
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "Select Video File to Stream"
        nameFilters: ["All Supported (*.ts *.h264 *.mp4)", "MPEG-2 Transport Stream (*.ts)", "H.264 Raw (*.h264)", "All Files (*)"]
        onAccepted: {
            var path = String(selectedFile)
            if (path.indexOf("file://") === 0) {
                path = path.substring(7)
            }
            sourceRoot.controller.feederFilePath = path
        }
    }
}
