import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Sightline 1.0
import ".."

Rectangle {
    id: root

    property int activeCamera: 0
    property int frameWidth: videoController && videoController.frameWidth > 0 ? videoController.frameWidth : 1920
    property int frameHeight: videoController && videoController.frameHeight > 0 ? videoController.frameHeight : 1080
    property int selectedCol: 960
    property int selectedRow: 540
    property int gateWidth: 80
    property int gateHeight: 80

    property bool showCrosshair: true
    property bool showGrid: true
    property bool showTelemetry: true
    property real zoomLevel: 1.0
    property int viewportFillMode: VideoItem.PreserveAspectFit
    readonly property bool devConnected: Boolean(bridge && bridge.isConnected)

    color: "#080a0f"
    border.color: SightlineTheme.cardBorder
    border.width: 1
    clip: true

    onActiveCameraChanged: {
        if (typeof videoController !== "undefined" && videoController) {
            videoController.selectCamera(root.activeCamera);
        }
    }

    Connections {
        target: typeof videoController !== "undefined" ? videoController : null
        function onActiveCameraChanged() {
            if (videoController && root.activeCamera !== videoController.activeCamera) {
                root.activeCamera = videoController.activeCamera;
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. Top HUD Status Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#0f121a"
            border.color: SightlineTheme.cardBorder
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                // Camera Badge & Mode
                Rectangle {
                    implicitWidth: 120
                    implicitHeight: 24
                    color: root.activeCamera === 1 ? "#3d1414" : "#14253d"
                    radius: 3
                    border.color: root.activeCamera === 1 ? SightlineTheme.accent : SightlineTheme.primary
                    border.width: 1

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        Rectangle {
                            width: 6; height: 6; radius: 3
                            color: root.activeCamera === 1 ? SightlineTheme.accent : SightlineTheme.primary
                        }
                        Text {
                            text: root.activeCamera === 1 ? "CAM 1: IR THERMAL" : "CAM 0: EO DAYLIGHT"
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                // Telemetry Tape Indicators
                Text {
                    text: "RES: " + root.frameWidth + "x" + root.frameHeight + " @" +
                          (videoController ? videoController.fps.toFixed(0) : "30") + " FPS"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                    font.family: "Monospace"
                }

                Text {
                    text: "LAT: " + (videoController ? videoController.avgDecodeTimeMs.toFixed(1) : "0.0") + " ms"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 10
                    font.family: "Monospace"
                }

                Text {
                    text: "FOV: 2.1° TELE"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 10
                    font.family: "Monospace"
                }

                Text {
                    text: "ACQ: L-Click [PRI] | R-Click [SEC]"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                    font.family: "Monospace"
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "RETICLE: (" + root.selectedCol + ", " + root.selectedRow + ")"
                    color: SightlineTheme.primary
                    font.pixelSize: 10
                    font.bold: true
                    font.family: "Monospace"
                }

                // Enhancement Mode & Palette Status Badge
                Rectangle {
                    implicitWidth: 125
                    implicitHeight: 22
                    color: (bridge && bridge.activeContrastMode > 0) ? "#1a233a" : "#12141c"
                    radius: 3
                    border.color: (bridge && bridge.activeContrastMode > 0) ? SightlineTheme.primary : SightlineTheme.cardBorder
                    border.width: 1

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: {
                                var modeNames = ["ENH: OFF", "ENH: HIST", "ENH: CLAHE", "ENH: SCINT"];
                                var m = (bridge ? bridge.activeContrastMode : 0);
                                return (m >= 0 && m < modeNames.length) ? modeNames[m] : "ENH";
                            }
                            color: (bridge && bridge.activeContrastMode > 0) ? SightlineTheme.primary : SightlineTheme.textMuted
                            font.pixelSize: 9
                            font.bold: true
                            font.family: "Monospace"
                        }
                        Text {
                            text: "|"
                            color: SightlineTheme.textMuted
                            font.pixelSize: 9
                        }
                        Text {
                            text: {
                                var palNames = ["WHITE", "BLACK", "RAIN", "IRON", "USER"];
                                var p = (bridge ? bridge.activePaletteIndex : 0);
                                return (p >= 0 && p < palNames.length) ? palNames[p] : "PAL";
                            }
                            color: SightlineTheme.textSecondary
                            font.pixelSize: 9
                            font.family: "Monospace"
                        }
                    }
                }

                // Stream Status Badge
                Rectangle {
                    implicitWidth: 110
                    implicitHeight: 22
                    color: {
                        if (!videoController) return "#2e1214";
                        if (videoController.playbackState === 2) {
                            return videoController.isSynthetic ? "#0f252e" : "#0f2e1a";
                        } else if (videoController.playbackState === 1) {
                            return "#3d2b0f";
                        } else {
                            return "#2e1214";
                        }
                    }
                    radius: 3
                    border.color: {
                        if (!videoController) return SightlineTheme.error;
                        if (videoController.playbackState === 2) {
                            return videoController.isSynthetic ? SightlineTheme.primary : SightlineTheme.success;
                        } else if (videoController.playbackState === 1) {
                            return SightlineTheme.warning;
                        } else {
                            return SightlineTheme.error;
                        }
                    }
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: {
                            if (!videoController) return "OFFLINE";
                            if (videoController.playbackState === 2) {
                                return videoController.isSynthetic ? "SYNTHETIC FEED" : "LIVE FEED";
                            } else if (videoController.playbackState === 1) {
                                return "CONNECTING...";
                            } else {
                                return "STANDBY";
                            }
                        }
                        color: {
                            if (!videoController) return SightlineTheme.error;
                            if (videoController.playbackState === 2) {
                                return videoController.isSynthetic ? SightlineTheme.primary : SightlineTheme.success;
                            } else if (videoController.playbackState === 1) {
                                return SightlineTheme.warning;
                            } else {
                                return SightlineTheme.error;
                            }
                        }
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }
        }

        // 2. Central Video Stream & Tactical Reticle Canvas
        Item {
            id: canvasArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // Real Hardware / Synthetic Video Presentation Item
            VideoItem {
                id: videoSurface
                anchors.fill: parent
                fillMode: root.viewportFillMode

                Component.onCompleted: {
                    if (typeof videoController !== "undefined" && videoController) {
                        videoController.attachVideoItem(videoSurface);
                    }
                }
            }

            // Grid Lines Overlay (Optional)
            Item {
                anchors.fill: parent
                visible: root.showGrid
                opacity: 0.25

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width; height: 1
                    color: SightlineTheme.primary
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 1; height: parent.height
                    color: SightlineTheme.primary
                }

                // Intermediate grid lines
                Repeater {
                    model: 4
                    Rectangle {
                        x: (index + 1) * (canvasArea.width / 5)
                        y: 0
                        width: 1; height: canvasArea.height
                        color: SightlineTheme.textMuted
                        opacity: 0.2
                    }
                }
                Repeater {
                    model: 4
                    Rectangle {
                        x: 0
                        y: (index + 1) * (canvasArea.height / 5)
                        width: canvasArea.width; height: 1
                        color: SightlineTheme.textMuted
                        opacity: 0.2
                    }
                }
            }

            // Central Tactical Crosshair & Mil-Dots (Optional)
            Item {
                anchors.centerIn: parent
                visible: root.showCrosshair

                // Horizontal crosshair lines
                Rectangle {
                    x: -120; y: 0; width: 100; height: 1
                    color: SightlineTheme.primary
                }
                Rectangle {
                    x: 20; y: 0; width: 100; height: 1
                    color: SightlineTheme.primary
                }

                // Vertical crosshair lines
                Rectangle {
                    x: 0; y: -120; width: 1; height: 100
                    color: SightlineTheme.primary
                }
                Rectangle {
                    x: 0; y: 20; width: 1; height: 100
                    color: SightlineTheme.primary
                }

                // Center Circle
                Rectangle {
                    anchors.centerIn: parent
                    width: 20; height: 20; radius: 10
                    color: "transparent"
                    border.color: SightlineTheme.primary
                    border.width: 1
                }
            }

            // Multi-Target Tracking Reticles Overlay
            Repeater {
                model: bridge ? bridge.trackListModel : null
                Item {
                    id: trackItem
                    x: (model.centerCol / root.frameWidth) * canvasArea.width - width / 2
                    y: (model.centerRow / root.frameHeight) * canvasArea.height - height / 2
                    width: Math.max(20, (model.trackWidth / root.frameWidth) * canvasArea.width)
                    height: Math.max(20, (model.trackHeight / root.frameHeight) * canvasArea.height)

                    readonly property color trackColor: model.isPrimary ? SightlineTheme.primary : "#ffb300"

                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border.color: trackItem.trackColor
                        border.width: 2

                        // Target Gate Corner Brackets
                        Rectangle { x: -1; y: -1; width: 5; height: 5; color: trackItem.trackColor }
                        Rectangle { x: parent.width - 4; y: -1; width: 5; height: 5; color: trackItem.trackColor }
                        Rectangle { x: -1; y: parent.height - 4; width: 5; height: 5; color: trackItem.trackColor }
                        Rectangle { x: parent.width - 4; y: parent.height - 4; width: 5; height: 5; color: trackItem.trackColor }

                        // Center cross dot
                        Rectangle {
                            anchors.centerIn: parent
                            width: 4; height: 4; radius: 2
                            color: trackItem.trackColor
                        }

                        // Target Label Banner
                        Rectangle {
                            y: -18
                            anchors.horizontalCenter: parent.horizontalCenter
                            implicitWidth: trkItemLabel.implicitWidth + 8
                            implicitHeight: 14
                            color: "#cc080a0f"
                            border.color: trackItem.trackColor
                            border.width: 1

                            Text {
                                id: trkItemLabel
                                anchors.centerIn: parent
                                text: (model.isPrimary ? "PRI #" : "SEC #") + model.trackId + " [" + model.confidence + "%]"
                                color: trackItem.trackColor
                                font.pixelSize: 8
                                font.bold: true
                                font.family: "Monospace"
                            }
                        }
                    }
                }
            }

            // Default Single Acquisition Gate Guide (visible when no active tracks)
            Rectangle {
                id: targetGate
                visible: !bridge || !bridge.trackListModel || bridge.trackListModel.rowCount() === 0
                x: (root.selectedCol / root.frameWidth) * canvasArea.width - width / 2
                y: (root.selectedRow / root.frameHeight) * canvasArea.height - height / 2
                width: Math.max(20, (root.gateWidth / root.frameWidth) * canvasArea.width)
                height: Math.max(20, (root.gateHeight / root.frameHeight) * canvasArea.height)
                color: "transparent"
                border.color: SightlineTheme.primary
                border.width: 2

                // Target Gate Corner Brackets
                Rectangle { x: -1; y: -1; width: 5; height: 5; color: SightlineTheme.primary }
                Rectangle { x: parent.width - 4; y: -1; width: 5; height: 5; color: SightlineTheme.primary }
                Rectangle { x: -1; y: parent.height - 4; width: 5; height: 5; color: SightlineTheme.primary }
                Rectangle { x: parent.width - 4; y: parent.height - 4; width: 5; height: 5; color: SightlineTheme.primary }

                // Center cross dot
                Rectangle {
                    anchors.centerIn: parent
                    width: 4; height: 4; radius: 2
                    color: SightlineTheme.primary
                }

                // Target Label Banner
                Rectangle {
                    y: -18
                    anchors.horizontalCenter: parent.horizontalCenter
                    implicitWidth: trkLabel.implicitWidth + 8
                    implicitHeight: 14
                    color: "#cc080a0f"
                    border.color: SightlineTheme.primary
                    border.width: 1

                    Text {
                        id: trkLabel
                        anchors.centerIn: parent
                        text: "ACQ GATE"
                        color: SightlineTheme.primary
                        font.pixelSize: 8
                        font.bold: true
                        font.family: "Monospace"
                    }
                }
            }

            // Enhancement ROI Overlay Rectangle
            Rectangle {
                id: enhRoiOverlay
                readonly property rect roi: bridge ? bridge.enhancementRoi : Qt.rect(0, 0, 0, 0)
                readonly property bool hasRoi: roi.width > 0 && roi.height > 0 &&
                                              (roi.width < root.frameWidth || roi.height < root.frameHeight)
                visible: hasRoi
                x: (roi.x / root.frameWidth) * canvasArea.width
                y: (roi.y / root.frameHeight) * canvasArea.height
                width: (roi.width / root.frameWidth) * canvasArea.width
                height: (roi.height / root.frameHeight) * canvasArea.height
                color: "#1a00e5ff"
                border.color: SightlineTheme.primary
                border.width: 1

                // Corner indicators
                Rectangle { x: -2; y: -2; width: 6; height: 6; color: SightlineTheme.primary }
                Rectangle { x: parent.width - 4; y: -2; width: 6; height: 6; color: SightlineTheme.primary }
                Rectangle { x: -2; y: parent.height - 4; width: 6; height: 6; color: SightlineTheme.primary }
                Rectangle { x: parent.width - 4; y: parent.height - 4; width: 6; height: 6; color: SightlineTheme.primary }

                // ROI Banner Label
                Rectangle {
                    y: -16
                    anchors.left: parent.left
                    implicitWidth: enhRoiLabel.implicitWidth + 8
                    implicitHeight: 14
                    color: "#cc080a0f"
                    border.color: SightlineTheme.primary
                    border.width: 1

                    Text {
                        id: enhRoiLabel
                        anchors.centerIn: parent
                        text: "ENH ROI [" + enhRoiOverlay.roi.width + "x" + enhRoiOverlay.roi.height + "]"
                        color: SightlineTheme.primary
                        font.pixelSize: 8
                        font.bold: true
                        font.family: "Monospace"
                    }
                }
            }

            // Point-and-Click Multi-Target Video Acquisition Interaction
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: root.devConnected ? Qt.CrossCursor : Qt.ArrowCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                enabled: root.devConnected

                function designateTarget(mouse, flags) {
                    var col = Math.max(0, Math.min(root.frameWidth, Math.round((mouse.x / canvasArea.width) * root.frameWidth)));
                    var row = Math.max(0, Math.min(root.frameHeight, Math.round((mouse.y / canvasArea.height) * root.frameHeight)));
                    root.selectedCol = col;
                    root.selectedRow = row;

                    if (bridge) {
                        bridge.startTracking(root.activeCamera, col, row, root.gateWidth, root.gateHeight, flags);
                    }
                }

                onClicked: function(mouse) {
                    var flags = (mouse.button === Qt.RightButton) ? 0x02 : 0x01;
                    designateTarget(mouse, flags);
                }
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var flags = (mouse.buttons & Qt.RightButton) ? 0x02 : 0x01;
                        designateTarget(mouse, flags);
                    }
                }
            }

            // Picture-in-Picture (PIP) Alternate Camera Inset
            Rectangle {
                id: pipContainer
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 12
                anchors.rightMargin: 12
                width: 256
                height: 144
                color: "#080a0f"
                radius: 4
                clip: true
                border.color: pipHoverArea.containsMouse ? SightlineTheme.primary : SightlineTheme.cardBorder
                border.width: 2
                visible: typeof videoController !== "undefined" && videoController && videoController.pipEnabled
                z: 10

                // Secondary Stream Video Presentation Item
                VideoItem {
                    id: pipVideoSurface
                    anchors.fill: parent
                    fillMode: VideoItem.PreserveAspectCrop

                    Component.onCompleted: {
                        if (typeof videoController !== "undefined" && videoController) {
                            videoController.attachPipVideoItem(pipVideoSurface);
                        }
                    }
                }

                // Inset Header Overlay
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 22
                    color: "#d90b0e14"
                    border.color: SightlineTheme.cardBorder
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 4

                        Rectangle {
                            width: 6; height: 6; radius: 3
                            color: (videoController && videoController.pipCamera === 1) ? SightlineTheme.accent : SightlineTheme.primary
                        }

                        Text {
                            text: (videoController && videoController.pipCamera === 1) ? "PIP: CAM 1 (IR)" : "PIP: CAM 0 (EO)"
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 9
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "⇄ SWAP"
                            color: pipHoverArea.containsMouse ? SightlineTheme.primary : SightlineTheme.textMuted
                            font.pixelSize: 8
                            font.bold: true
                        }

                        // Close PIP button
                        Rectangle {
                            width: 14; height: 14; radius: 2
                            color: "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "×"
                                color: SightlineTheme.textMuted
                                font.pixelSize: 12
                                font.bold: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (videoController) {
                                        videoController.setPipEnabled(false);
                                    }
                                }
                            }
                        }
                    }
                }

                // Hover overlay indication & Click to Swap
                MouseArea {
                    id: pipHoverArea
                    anchors.fill: parent
                    anchors.topMargin: 22
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    Rectangle {
                        anchors.fill: parent
                        color: "#66000000"
                        visible: pipHoverArea.containsMouse

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6
                            Text {
                                text: "⇄ Click to Swap Feeds"
                                color: SightlineTheme.primary
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    onClicked: {
                        if (videoController) {
                            videoController.swapPipFeeds();
                        }
                    }
                }
            }

            // Toast / Snapshot notification banner
            Rectangle {
                id: toastBanner
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 12
                implicitHeight: 28
                implicitWidth: toastText.implicitWidth + 24
                color: "#e600e5ff"
                radius: 4
                opacity: 0.0
                visible: opacity > 0.0

                Behavior on opacity {
                    NumberAnimation { duration: 250 }
                }

                Text {
                    id: toastText
                    anchors.centerIn: parent
                    text: "Snapshot Saved"
                    color: "#080a0f"
                    font.bold: true
                    font.pixelSize: 11
                }

                Timer {
                    id: toastTimer
                    interval: 2200
                    onTriggered: toastBanner.opacity = 0.0
                }
            }

            Connections {
                target: typeof videoController !== "undefined" ? videoController : null
                function onSnapshotTaken(path) {
                    toastText.text = "Snapshot: " + path.split("/").pop();
                    toastBanner.opacity = 1.0;
                    toastTimer.restart();
                }
            }
        }

        // 3. Bottom Viewport Control Toolbar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#0f121a"
            border.color: SightlineTheme.cardBorder
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                // Camera Switcher Buttons
                Text {
                    text: "SENSOR:"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }

                Button {
                    text: "EO (Cam 0)"
                    implicitHeight: 28
                    implicitWidth: 84
                    checked: root.activeCamera === 0
                    checkable: true
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: root.activeCamera = 0
                }

                Button {
                    text: "IR (Cam 1)"
                    implicitHeight: 28
                    implicitWidth: 84
                    checked: root.activeCamera === 1
                    checkable: true
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: root.activeCamera = 1
                }

                Rectangle { width: 1; height: 20; color: SightlineTheme.cardBorder }

                // Picture-in-Picture Toggle & Swap
                Button {
                    text: videoController && videoController.pipEnabled ? "PIP [ON]" : "PIP [OFF]"
                    implicitHeight: 28
                    implicitWidth: 80
                    checkable: true
                    checked: videoController ? videoController.pipEnabled : false
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: {
                        if (videoController) {
                            videoController.setPipEnabled(!videoController.pipEnabled);
                        }
                    }
                }

                Button {
                    text: "⇄ Swap"
                    implicitHeight: 28
                    implicitWidth: 70
                    visible: videoController && videoController.pipEnabled
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: {
                        if (videoController) {
                            videoController.swapPipFeeds();
                        }
                    }
                }

                Rectangle { width: 1; height: 20; color: SightlineTheme.cardBorder }

                // Video Source Selector (Synthetic vs Live RTSP)
                Button {
                    text: videoController && videoController.isSynthetic ? "Synthetic [ON]" : "Synthetic [OFF]"
                    implicitHeight: 28
                    implicitWidth: 98
                    checkable: true
                    checked: videoController ? videoController.isSynthetic : true
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: {
                        if (videoController) {
                            videoController.setSyntheticMode(!videoController.isSynthetic);
                        }
                    }
                }

                Button {
                    text: "Stream URL..."
                    implicitHeight: 28
                    implicitWidth: 90
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: streamDialog.open()
                }

                Button {
                    text: "📸 Snap"
                    implicitHeight: 28
                    implicitWidth: 64
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: {
                        if (videoController) {
                            videoController.takeSnapshot();
                        }
                    }
                }

                Rectangle { width: 1; height: 20; color: SightlineTheme.cardBorder }

                // HUD Display Toggles
                CheckBox {
                    text: "Crosshair"
                    checked: root.showCrosshair
                    onCheckedChanged: root.showCrosshair = checked
                }

                CheckBox {
                    text: "Grid"
                    checked: root.showGrid
                    onCheckedChanged: root.showGrid = checked
                }

                Item { Layout.fillWidth: true }

                // Quick Nudge Buttons
                Text {
                    text: "NUDGE:"
                    color: SightlineTheme.textMuted
                    opacity: root.devConnected ? 1.0 : 0.4
                    font.pixelSize: 10
                    font.bold: true
                }

                Button {
                    text: "◄"
                    implicitWidth: 28; implicitHeight: 28
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: if (bridge) bridge.nudgeTracking(root.activeCamera, -5, 0)
                }
                Button {
                    text: "▲"
                    implicitWidth: 28; implicitHeight: 28
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: if (bridge) bridge.nudgeTracking(root.activeCamera, 0, -5)
                }
                Button {
                    text: "▼"
                    implicitWidth: 28; implicitHeight: 28
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: if (bridge) bridge.nudgeTracking(root.activeCamera, 0, 5)
                }
                Button {
                    text: "►"
                    implicitWidth: 28; implicitHeight: 28
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: if (bridge) bridge.nudgeTracking(root.activeCamera, 5, 0)
                }

                Button {
                    text: "Center"
                    implicitHeight: 28
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: {
                        root.selectedCol = Math.round(root.frameWidth / 2);
                        root.selectedRow = Math.round(root.frameHeight / 2);
                        if (bridge) {
                            bridge.startTracking(root.activeCamera, root.selectedCol, root.selectedRow, root.gateWidth, root.gateHeight, 0x01);
                        }
                    }
                }

                Button {
                    text: "Clear Tracks"
                    implicitHeight: 28
                    implicitWidth: 90
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: {
                        if (bridge) {
                            bridge.stopTracking(root.activeCamera, 0xFF);
                        }
                    }
                }
            }
        }
    }

    // Stream Configuration Dialog
    Dialog {
        id: streamDialog
        title: "Video Stream Configuration"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 440

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Configure RTSP / UDP Sensor Network Stream:"
                color: SightlineTheme.textPrimary
                font.bold: true
                font.pixelSize: 12
            }

            TextField {
                id: streamUriField
                Layout.fillWidth: true
                placeholderText: "rtsp://" + (bridge && bridge.host ? bridge.host : "127.0.0.1") + ":554/net" + root.activeCamera
                text: {
                    if (videoController && videoController.sourceUri) {
                        return videoController.sourceUri;
                    }
                    var h = (bridge && bridge.host) ? bridge.host : "127.0.0.1";
                    return "rtsp://" + h + ":554/net" + root.activeCamera;
                }
            }

            RowLayout {
                spacing: 8
                Text {
                    text: "Quick Presets:"
                    color: SightlineTheme.textMuted
                    font.pixelSize: 11
                }
                Button {
                    text: "Auto (Net " + root.activeCamera + ")"
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: {
                        var h = (bridge && bridge.host) ? bridge.host : "127.0.0.1";
                        streamUriField.text = "rtsp://" + h + ":554/net" + root.activeCamera;
                    }
                }
                Button {
                    text: "RTSP Net 0"
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: streamUriField.text = "rtsp://" + (bridge && bridge.host ? bridge.host : "127.0.0.1") + ":554/net0"
                }
                Button {
                    text: "RTSP Net 1"
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: streamUriField.text = "rtsp://" + (bridge && bridge.host ? bridge.host : "127.0.0.1") + ":554/net1"
                }
                Button {
                    text: "UDP 15004"
                    enabled: root.devConnected
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: streamUriField.text = "udp://@:15004"
                }
            }

            Text {
                text: "Status: " + (videoController ? videoController.statusMessage : "Idle")
                color: SightlineTheme.textSecondary
                font.pixelSize: 11
            }
        }

        onAccepted: {
            if (videoController) {
                videoController.sourceUri = streamUriField.text;
                videoController.setSyntheticMode(false);
            }
        }
    }
}
