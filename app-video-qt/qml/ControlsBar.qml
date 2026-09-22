import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import VideoApp 1.0

Rectangle {
    id: root
    height: 54
    radius: 8
    color: "#e6141720" // Translucent dark tactical
    border.color: "#2a2f40"
    border.width: 1

    required property VideoPlayerController controller
    required property VideoItem videoItem

    signal snapshotSaved(string path)
    signal toggleFullscreen()

    function formatTime(seconds) {
        if (isNaN(seconds) || seconds < 0) return "00:00";
        var totalSec = Math.floor(seconds);
        var hrs = Math.floor(totalSec / 3600);
        var mins = Math.floor((totalSec % 3600) / 60);
        var secs = totalSec % 60;
        var sMins = (mins < 10 ? "0" : "") + mins;
        var sSecs = (secs < 10 ? "0" : "") + secs;
        if (hrs > 0) {
            var sHrs = (hrs < 10 ? "0" : "") + hrs;
            return sHrs + ":" + sMins + ":" + sSecs;
        }
        return sMins + ":" + sSecs;
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        // Play / Pause Button
        Button {
            id: playPauseBtn
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36

            readonly property bool isPlaying: controller.playbackState === VideoPlayerController.Playing

            contentItem: Text {
                text: playPauseBtn.isPlaying ? "❚❚" : "▶"
                color: "#00e5ff"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.hovered ? "#243248" : "#1b202e"
                radius: 4
                border.color: "#00e5ff"
                border.width: 1
            }
            onClicked: {
                if (playPauseBtn.isPlaying) {
                    controller.pausePlayback();
                } else if (controller.playbackState === VideoPlayerController.Paused) {
                    controller.resumePlayback();
                } else {
                    controller.startPlayback();
                }
            }
        }

        // Stop Button
        Button {
            id: stopBtn
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            contentItem: Text {
                text: "■"
                color: "#ff1744"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.hovered ? "#3d1b24" : "#1b202e"
                radius: 4
                border.color: "#2a2f40"
            }
            onClicked: controller.stopPlayback()
        }

        // Current Timestamp
        Text {
            text: root.formatTime(controller.positionSeconds)
            color: "#f0f4fc"
            font.pixelSize: 12
            font.family: "Monospace"
        }

        // Seek Slider
        Slider {
            id: seekSlider
            Layout.fillWidth: true
            enabled: controller.isSeekable && controller.durationSeconds > 0
            from: 0
            to: controller.durationSeconds > 0 ? controller.durationSeconds : 1.0
            value: controller.positionSeconds

            onMoved: {
                controller.seek(value);
            }

            background: Rectangle {
                x: seekSlider.leftPadding
                y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 4
                width: seekSlider.availableWidth
                height: implicitHeight
                radius: 2
                color: "#2a2f40"

                Rectangle {
                    width: seekSlider.visualPosition * parent.width
                    height: parent.height
                    color: "#00e5ff"
                    radius: 2
                }
            }

            handle: Rectangle {
                x: seekSlider.leftPadding + seekSlider.visualPosition * (seekSlider.availableWidth - width)
                y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                implicitWidth: 12
                implicitHeight: 12
                radius: 6
                color: seekSlider.pressed ? "#00e5ff" : "#f0f4fc"
                border.color: "#00e5ff"
                border.width: 1
            }
        }

        // Total Duration Timestamp
        Text {
            text: controller.durationSeconds > 0 ? root.formatTime(controller.durationSeconds) : "--:--"
            color: "#8894ab"
            font.pixelSize: 12
            font.family: "Monospace"
        }

        // Divider
        Rectangle {
            width: 1
            height: 24
            color: "#2a2f40"
        }

        // Loop Toggle
        Button {
            id: loopBtn
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            checkable: true
            checked: controller.isLoopPlayback
            onClicked: controller.isLoopPlayback = checked

            contentItem: Text {
                text: "⟲"
                color: loopBtn.checked ? "#00e676" : "#8894ab"
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: loopBtn.checked ? "#1a3328" : (parent.hovered ? "#283344" : "transparent")
                radius: 4
                border.color: loopBtn.checked ? "#00e676" : "transparent"
            }
        }

        // Snapshot Button
        Button {
            id: snapBtn
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            contentItem: Text {
                text: "📷"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.hovered ? "#283344" : "transparent"
                radius: 4
            }
            onClicked: {
                var saved = controller.takeSnapshot("");
                if (saved.length > 0) {
                    root.snapshotSaved(saved);
                }
            }
        }

        // Aspect Ratio Toggle
        Button {
            id: aspectBtn
            Layout.preferredWidth: 44
            Layout.preferredHeight: 34
            contentItem: Text {
                text: {
                    switch (videoItem.fillMode) {
                    case VideoItem.Stretch: return "FILL";
                    case VideoItem.PreserveAspectCrop: return "CROP";
                    default: return "FIT";
                    }
                }
                color: "#00e5ff"
                font.pixelSize: 10
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.hovered ? "#243248" : "#1b202e"
                radius: 4
                border.color: "#2a2f40"
            }
            onClicked: {
                if (videoItem.fillMode === VideoItem.PreserveAspectFit) {
                    videoItem.fillMode = VideoItem.PreserveAspectCrop;
                } else if (videoItem.fillMode === VideoItem.PreserveAspectCrop) {
                    videoItem.fillMode = VideoItem.Stretch;
                } else {
                    videoItem.fillMode = VideoItem.PreserveAspectFit;
                }
            }
        }

        // Crosshair HUD Toggle
        Button {
            id: crosshairBtn
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            checkable: true
            checked: videoItem.showOsdCrosshair
            onClicked: videoItem.showOsdCrosshair = checked

            contentItem: Text {
                text: "✛"
                color: crosshairBtn.checked ? "#00e5ff" : "#8894ab"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: crosshairBtn.checked ? "#172b38" : (parent.hovered ? "#283344" : "transparent")
                radius: 4
                border.color: crosshairBtn.checked ? "#00e5ff" : "transparent"
            }
        }

        // Fullscreen Toggle
        Button {
            id: fsBtn
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            contentItem: Text {
                text: "⛶"
                color: "#f0f4fc"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.hovered ? "#283344" : "transparent"
                radius: 4
            }
            onClicked: root.toggleFullscreen()
        }
    }
}
