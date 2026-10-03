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

    function formatClock(totalSec) {
        var sec = Math.max(0, Math.floor(totalSec));
        var h = Math.floor(sec / 3600);
        var m = Math.floor((sec % 3600) / 60);
        var s = sec % 60;
        var hh = (h < 10 ? "0" : "") + h;
        var mm = (m < 10 ? "0" : "") + m;
        var ss = (s < 10 ? "0" : "") + s;
        return hh + ":" + mm + ":" + ss;
    }

    Timer {
        id: snapTimer
        interval: 2500
        repeat: false
        onTriggered: snapFeedback.visible = false
    }

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        spacing: 16

        Item { Layout.preferredHeight: 4 }

        // Header Title Banner with Live Recording Status
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                width: 4
                height: 22
                color: SightlineTheme.primary
                radius: 2
            }

            Text {
                text: "ONBOARD MEDIA STORAGE & RECORDING"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium + 2
                font.bold: true
                font.letterSpacing: 1.0
            }

            Text {
                text: "// Modules 0x70, 0xC4-0xCA Hardened Media Pipeline"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
            }

            Item { Layout.fillWidth: true }

            // Live State Pill Badge
            Rectangle {
                implicitWidth: statusRow.implicitWidth + 20
                implicitHeight: 28
                radius: 14
                color: (bridge && bridge.isRecordingActive) ? Qt.rgba(1.0, 0.09, 0.27, 0.15) : Qt.rgba(0.0, 0.9, 0.46, 0.12)
                border.color: (bridge && bridge.isRecordingActive) ? SightlineTheme.error : SightlineTheme.success
                border.width: 1

                RowLayout {
                    id: statusRow
                    anchors.centerIn: parent
                    spacing: 8

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: (bridge && bridge.isRecordingActive) ? SightlineTheme.error : SightlineTheme.success

                        SequentialAnimation on opacity {
                            running: bridge ? bridge.isRecordingActive : false
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.2; duration: 500 }
                            NumberAnimation { to: 1.0; duration: 500 }
                        }
                    }

                    Text {
                        text: (bridge && bridge.isRecordingActive)
                              ? ("RECORDING [" + formatClock(bridge.elapsedRecordingSec) + "]")
                              : "RECORDER STANDBY"
                        color: (bridge && bridge.isRecordingActive) ? SightlineTheme.error : SightlineTheme.success
                        font.pixelSize: SightlineTheme.fontSizeSmall
                        font.bold: true
                        font.letterSpacing: 0.5
                    }
                }
            }
        }

        // Real-Time Storage & Throughput Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 12

            MetricCard {
                title: "Storage Free"
                value: bridge ? (bridge.freeStorageMB > 1024 ? (bridge.freeStorageMB / 1024.0).toFixed(1) : bridge.freeStorageMB) : "---"
                unit: bridge ? (bridge.freeStorageMB > 1024 ? "GB" : "MB") : ""
                accentColor: (bridge && bridge.storageUsagePercent > 85.0) ? SightlineTheme.error : SightlineTheme.info
                iconText: "💽"
            }

            MetricCard {
                title: "Active Bitrate"
                value: bridge ? (bridge.currentBitrateKbps > 0 ? (bridge.currentBitrateKbps / 1000.0).toFixed(1) : "0.0") : "0.0"
                unit: "Mbps"
                accentColor: SightlineTheme.primary
                iconText: "📡"
            }

            MetricCard {
                title: "Dropped Frames"
                value: bridge ? bridge.droppedFrames.toString() : "0"
                unit: "frames"
                accentColor: (bridge && bridge.droppedFrames > 0) ? SightlineTheme.error : SightlineTheme.success
                iconText: "⚠️"
            }

            MetricCard {
                title: "Catalog Clips"
                value: (bridge && bridge.recordingFileListModel) ? bridge.recordingFileListModel.rowCount().toString() : "0"
                unit: "files"
                accentColor: SightlineTheme.accent
                iconText: "📁"
            }
        }

        // Storage Capacity Bar
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 34
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusSmall
            border.color: SightlineTheme.cardBorder
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 10

                Text {
                    text: "Capacity:"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 10
                    color: SightlineTheme.surfaceLight
                    radius: 5
                    clip: true

                    Rectangle {
                        width: parent.width * Math.min(1.0, Math.max(0.0, (bridge ? bridge.storageUsagePercent / 100.0 : 0.05)))
                        height: parent.height
                        radius: 5
                        color: {
                            var pct = bridge ? bridge.storageUsagePercent : 5.0;
                            if (pct > 90.0) return SightlineTheme.error;
                            if (pct > 75.0) return SightlineTheme.warning;
                            return SightlineTheme.primary;
                        }
                    }
                }

                Text {
                    text: (bridge ? bridge.storageUsagePercent.toFixed(1) : "0.0") + "% used"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.bold: true
                }
            }
        }

        // Main Dual Operations Grid
        GridLayout {
            Layout.fillWidth: true
            columns: root.width > 900 ? 2 : 1
            rowSpacing: 16
            columnSpacing: 16

            // Card 1: Continuous Stream Recording
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: recInnerCol.implicitHeight + 28
                color: SightlineTheme.surfaceCard
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder
                border.width: 1

                ColumnLayout {
                    id: recInnerCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        spacing: 8
                        Rectangle { width: 3; height: 14; color: SightlineTheme.primary; radius: 1 }
                        Text {
                            text: "CONTINUOUS STREAM RECORDING (V2)"
                            color: SightlineTheme.primary
                            font.pixelSize: SightlineTheme.fontSizeNormal
                            font.bold: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Sensor Source:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 120 }
                        ComboBox {
                            id: recCam
                            model: ["Camera 0 (EO Video)", "Camera 1 (IR Video)", "Camera 2 (Secondary)", "Camera 3"]
                            Layout.fillWidth: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Storage Media:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 120 }
                        ComboBox {
                            id: recDest
                            model: ["MicroSD Card (Slot 0)", "External USB 3.0 SSD", "FTP Push Remote"]
                            Layout.fillWidth: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Container Format:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 120 }
                        ComboBox {
                            id: recFormat
                            model: ["MPEG-2 Transport Stream (.ts)", "Fragmented MP4 (.mp4)", "Raw Elementary Stream"]
                            Layout.fillWidth: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: "Filename Prefix:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 120 }
                            TextField {
                                id: prefixInput
                                text: "flight_rec"
                                Layout.fillWidth: true
                                color: SightlineTheme.textPrimary
                                selectByMouse: true
                                property var validation: bridge ? bridge.validateFilename(text) : ({ valid: true, error: "" })
                                onTextChanged: {
                                    if (bridge) validation = bridge.validateFilename(text);
                                }
                                background: Rectangle {
                                    color: SightlineTheme.surfaceLight
                                    radius: SightlineTheme.radiusSmall
                                    border.color: !prefixInput.validation.valid ? SightlineTheme.error : (prefixInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder)
                                    border.width: !prefixInput.validation.valid ? 2 : 1
                                }
                            }
                        }

                        // Inline Validation Warning
                        Text {
                            visible: !prefixInput.validation.valid
                            text: prefixInput.validation.error
                            color: SightlineTheme.error
                            font.pixelSize: 10
                            Layout.leftMargin: 132
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                    }

                    CheckBox {
                        id: autoSplitCheck
                        text: "Allow numerical rollover & auto-split (1 GB chunks)"
                        checked: true
                        contentItem: Text { text: parent.text; color: SightlineTheme.textSecondary; font.pixelSize: 11; leftPadding: 24 }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Layout.topMargin: 4

                        Button {
                            id: recBtn
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            enabled: bridge && bridge.isConnected && prefixInput.validation.valid
                            contentItem: Text {
                                text: (bridge && bridge.isRecordingActive) ? "⏹  Stop Video Recording" : "⏺  Start Video Recording"
                                color: (bridge && bridge.isRecordingActive) ? "#ffffff" : "#0e1014"
                                font.bold: true
                                font.pixelSize: SightlineTheme.fontSizeNormal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: (bridge && bridge.isRecordingActive) ? SightlineTheme.error : SightlineTheme.success
                                radius: SightlineTheme.radiusSmall
                                border.color: (bridge && bridge.isRecordingActive) ? SightlineTheme.error : SightlineTheme.success
                            }
                            onClicked: {
                                if (bridge) {
                                    if (bridge.isRecordingActive) {
                                        bridge.stopRecordingV2(recCam.currentIndex);
                                    } else {
                                        bridge.startRecordingV2(
                                            recCam.currentIndex,
                                            prefixInput.text,
                                            recFormat.currentIndex,
                                            recDest.currentIndex,
                                            0,
                                            0,
                                            autoSplitCheck.checked
                                        );
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Card 2: High-Resolution Snapshot Capture
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: snapInnerCol.implicitHeight + 28
                color: SightlineTheme.surfaceCard
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder
                border.width: 1

                ColumnLayout {
                    id: snapInnerCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        spacing: 8
                        Rectangle { width: 3; height: 14; color: SightlineTheme.accent; radius: 1 }
                        Text {
                            text: "STILL FRAME SNAPSHOT ENGINE (V2)"
                            color: SightlineTheme.accent
                            font.pixelSize: SightlineTheme.fontSizeNormal
                            font.bold: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Sensor Channel:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 120 }
                        ComboBox {
                            id: snapCam
                            model: ["Camera 0 (EO Video)", "Camera 1 (IR Video)", "All Channels (Simultaneous)"]
                            Layout.fillWidth: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Snapshot Format:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 120 }
                        ComboBox {
                            id: snapFormat
                            model: ["JPEG Baseline (EXIF/XMP)", "Lossless PNG", "Sightline RAW (.slraw)", "16-bit Grayscale TIFF"]
                            Layout.fillWidth: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Quality Level:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 120 }
                        Slider {
                            id: snapQuality
                            from: 10
                            to: 100
                            value: 85
                            stepSize: 1
                            Layout.fillWidth: true
                        }
                        Text {
                            text: Math.round(snapQuality.value) + "%"
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 11
                            font.bold: true
                            Layout.preferredWidth: 36
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Custom Suffix:"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 120 }
                        TextField {
                            id: snapPrefix
                            text: "snap"
                            Layout.fillWidth: true
                            color: SightlineTheme.textPrimary
                            selectByMouse: true
                            background: Rectangle {
                                color: SightlineTheme.surfaceLight
                                radius: SightlineTheme.radiusSmall
                                border.color: snapPrefix.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                            }
                        }
                    }

                    CheckBox {
                        id: xmpMetadataCheck
                        text: "Embed Geospatial XMP / KLV Telemetry Metadata"
                        checked: true
                        contentItem: Text { text: parent.text; color: SightlineTheme.textSecondary; font.pixelSize: 11; leftPadding: 24 }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Layout.topMargin: 4

                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            enabled: bridge && bridge.isConnected
                            contentItem: Text {
                                text: "📸  Capture Still Snapshot"
                                color: "#0e1014"
                                font.bold: true
                                font.pixelSize: SightlineTheme.fontSizeNormal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: SightlineTheme.accent
                                radius: SightlineTheme.radiusSmall
                            }
                            onClicked: {
                                if (bridge) {
                                    var camIdx = (snapCam.currentIndex === 2) ? 0xFF : snapCam.currentIndex;
                                    bridge.captureSnapshotV2(
                                        camIdx,
                                        snapPrefix.text,
                                        snapFormat.currentIndex,
                                        Math.round(snapQuality.value),
                                        xmpMetadataCheck.checked
                                    );
                                    snapFeedback.visible = true;
                                    snapTimer.restart();
                                }
                            }
                        }
                    }

                    Text {
                        id: snapFeedback
                        visible: false
                        text: "✓ Snapshot acquisition command dispatched"
                        color: SightlineTheme.success
                        font.pixelSize: 11
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }

        // Remote Storage File Catalog & Explorer
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 320
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                // Catalog Toolbar
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle { width: 3; height: 14; color: SightlineTheme.info; radius: 1 }
                    Text {
                        text: "MEDIA CATALOG & REMOTE STORAGE BROWSER (0xC8)"
                        color: SightlineTheme.textPrimary
                        font.pixelSize: SightlineTheme.fontSizeNormal
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    TextField {
                        id: searchFilter
                        placeholderText: "Filter files..."
                        Layout.preferredWidth: 150
                        color: SightlineTheme.textPrimary
                        font.pixelSize: 11
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            radius: 4
                            border.color: searchFilter.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                        }
                    }

                    Button {
                        text: "🔄 Refresh Catalog"
                        Layout.preferredHeight: 28
                        contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.pixelSize: 11; font.bold: true }
                        background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                        onClicked: {
                            if (bridge) bridge.requestDirectoryListing(recDest.currentIndex, 0, 50, searchFilter.text);
                        }
                    }
                }

                // Table Header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    color: SightlineTheme.surfaceLight
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Text { text: "PIN"; color: SightlineTheme.textMuted; font.pixelSize: 10; font.bold: true; Layout.preferredWidth: 32 }
                        Text { text: "FILE NAME"; color: SightlineTheme.textMuted; font.pixelSize: 10; font.bold: true; Layout.fillWidth: true }
                        Text { text: "FORMAT"; color: SightlineTheme.textMuted; font.pixelSize: 10; font.bold: true; Layout.preferredWidth: 90 }
                        Text { text: "SIZE"; color: SightlineTheme.textMuted; font.pixelSize: 10; font.bold: true; Layout.preferredWidth: 80 }
                        Text { text: "TIMESTAMP"; color: SightlineTheme.textMuted; font.pixelSize: 10; font.bold: true; Layout.preferredWidth: 130 }
                        Text { text: "ACTIONS"; color: SightlineTheme.textMuted; font.pixelSize: 10; font.bold: true; Layout.preferredWidth: 80 }
                    }
                }

                // File List View
                ListView {
                    id: fileListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: bridge ? bridge.recordingFileListModel : null
                    spacing: 4

                    delegate: Rectangle {
                        width: fileListView.width
                        height: 32
                        color: index % 2 === 0 ? Qt.rgba(1, 1, 1, 0.02) : "transparent"
                        radius: 3

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            Text {
                                text: model.isPinned ? "📌" : "  "
                                font.pixelSize: 11
                                Layout.preferredWidth: 32
                            }

                            Text {
                                text: model.filename
                                color: SightlineTheme.textPrimary
                                font.pixelSize: 11
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                Layout.preferredWidth: 70
                                Layout.preferredHeight: 18
                                color: Qt.rgba(0.0, 0.9, 1.0, 0.1)
                                radius: 3
                                Text {
                                    anchors.centerIn: parent
                                    text: model.formatString
                                    color: SightlineTheme.primary
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }

                            Text {
                                text: model.formattedSize
                                color: SightlineTheme.textSecondary
                                font.pixelSize: 11
                                Layout.preferredWidth: 80
                            }

                            Text {
                                text: model.formattedDate
                                color: SightlineTheme.textMuted
                                font.pixelSize: 10
                                font.family: "Monospace"
                                Layout.preferredWidth: 130
                            }

                            RowLayout {
                                Layout.preferredWidth: 80
                                spacing: 4

                                Button {
                                    text: model.isPinned ? "Unpin" : "Pin"
                                    Layout.preferredHeight: 22
                                    Layout.preferredWidth: 42
                                    contentItem: Text { text: parent.text; color: SightlineTheme.textPrimary; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: SightlineTheme.surfaceLight; radius: 3; border.color: SightlineTheme.inputBorder }
                                    onClicked: {
                                        if (bridge) bridge.pinStorageFile(recDest.currentIndex, model.filename, !model.isPinned);
                                    }
                                }

                                Button {
                                    text: "🗑"
                                    Layout.preferredHeight: 22
                                    Layout.preferredWidth: 26
                                    contentItem: Text { text: parent.text; color: SightlineTheme.error; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: Qt.rgba(1.0, 0.09, 0.27, 0.15); radius: 3; border.color: SightlineTheme.error }
                                    onClicked: {
                                        if (bridge) bridge.deleteStorageFile(recDest.currentIndex, model.filename);
                                    }
                                }
                            }
                        }
                    }

                    // Empty Placeholder
                    Text {
                        anchors.centerIn: parent
                        visible: fileListView.count === 0
                        text: "No files cataloged. Click 'Refresh Catalog' to query remote storage."
                        color: SightlineTheme.textMuted
                        font.pixelSize: 12
                    }
                }
            }
        }

        // Live Event & Diagnostic Console Drawer
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 46
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusSmall
            border.color: SightlineTheme.cardBorder
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                Text {
                    text: "LIVE TELEMETRY:"
                    color: SightlineTheme.primary
                    font.pixelSize: 10
                    font.bold: true
                }

                Text {
                    text: (bridge && bridge.lastRecordingEvent.length > 0) ? bridge.lastRecordingEvent : "Ready. No asynchronous recording events."
                    color: SightlineTheme.textPrimary
                    font.pixelSize: 11
                    font.family: "Monospace"
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: bridge ? bridge.lastAckStatus : ""
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 10
                    font.family: "Monospace"
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
