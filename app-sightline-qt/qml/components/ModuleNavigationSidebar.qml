import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Rectangle {
    id: sidebar
    width: SightlineTheme.sidebarWidth
    color: SightlineTheme.surface
    border.color: SightlineTheme.cardBorder
    border.width: 1

    property int currentIndex: 0
    signal moduleSelected(int index)

    ScrollView {
        anchors.fill: parent
        anchors.margins: 8
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        Column {
            width: parent.width
            spacing: 6

            // Section 1: Target Tracking & AI
            Text {
                text: "TRACKING & INTELLIGENCE"
                color: SightlineTheme.textMuted
                font.pixelSize: 10
                font.bold: true
                leftPadding: 8
                topPadding: 6
            }

            ModuleTabButton {
                width: parent.width
                text: "Tracking"
                iconText: "🎯"
                selected: sidebar.currentIndex === 0
                badgeText: bridge.trackListModel.rowCount() > 0 ? (bridge.trackListModel.rowCount() + " tracks") : ""
                onClicked: sidebar.moduleSelected(0)
            }

            ModuleTabButton {
                width: parent.width
                text: "Stabilization"
                iconText: "⚖️"
                selected: sidebar.currentIndex === 1
                onClicked: sidebar.moduleSelected(1)
            }

            ModuleTabButton {
                width: parent.width
                text: "Detection"
                iconText: "🔍"
                selected: sidebar.currentIndex === 2
                onClicked: sidebar.moduleSelected(2)
            }

            ModuleTabButton {
                width: parent.width
                text: "Classification"
                iconText: "🧠"
                selected: sidebar.currentIndex === 3
                onClicked: sidebar.moduleSelected(3)
            }

            ModuleTabButton {
                width: parent.width
                text: "Landing Aid"
                iconText: "🛬"
                selected: sidebar.currentIndex === 4
                onClicked: sidebar.moduleSelected(4)
            }

            // Section 2: Video Pipeline
            Text {
                text: "VIDEO PIPELINE"
                color: SightlineTheme.textMuted
                font.pixelSize: 10
                font.bold: true
                leftPadding: 8
                topPadding: 10
            }

            ModuleTabButton {
                width: parent.width
                text: "Capture"
                iconText: "📹"
                selected: sidebar.currentIndex === 5
                onClicked: sidebar.moduleSelected(5)
            }

            ModuleTabButton {
                width: parent.width
                text: "Display"
                iconText: "🖥️"
                selected: sidebar.currentIndex === 6
                onClicked: sidebar.moduleSelected(6)
            }

            ModuleTabButton {
                width: parent.width
                text: "Enhancement"
                iconText: "✨"
                selected: sidebar.currentIndex === 7
                onClicked: sidebar.moduleSelected(7)
            }

            ModuleTabButton {
                width: parent.width
                text: "Compression"
                iconText: "🗜️"
                selected: sidebar.currentIndex === 8
                onClicked: sidebar.moduleSelected(8)
            }

            ModuleTabButton {
                width: parent.width
                text: "Blending"
                iconText: "🔀"
                selected: sidebar.currentIndex === 9
                onClicked: sidebar.moduleSelected(9)
            }

            ModuleTabButton {
                width: parent.width
                text: "Overlays"
                iconText: "📐"
                selected: sidebar.currentIndex === 10
                onClicked: sidebar.moduleSelected(10)
            }

            // Section 3: Optics & Sensors
            Text {
                text: "OPTICS & SENSORS"
                color: SightlineTheme.textMuted
                font.pixelSize: 10
                font.bold: true
                leftPadding: 8
                topPadding: 10
            }

            ModuleTabButton {
                width: parent.width
                text: "Focus & Lens"
                iconText: "🔭"
                selected: sidebar.currentIndex === 11
                onClicked: sidebar.moduleSelected(11)
            }

            ModuleTabButton {
                width: parent.width
                text: "NUC Calibration"
                iconText: "🌡️"
                selected: sidebar.currentIndex === 12
                onClicked: sidebar.moduleSelected(12)
            }

            // Section 4: Telemetry & Recording
            Text {
                text: "TELEMETRY & LOGGING"
                color: SightlineTheme.textMuted
                font.pixelSize: 10
                font.bold: true
                leftPadding: 8
                topPadding: 10
            }

            ModuleTabButton {
                width: parent.width
                text: "Telemetry"
                iconText: "📡"
                selected: sidebar.currentIndex === 13
                onClicked: sidebar.moduleSelected(13)
            }

            ModuleTabButton {
                width: parent.width
                text: "KLV Metadata"
                iconText: "🏷️"
                selected: sidebar.currentIndex === 14
                onClicked: sidebar.moduleSelected(14)
            }

            ModuleTabButton {
                width: parent.width
                text: "Recording"
                iconText: "💾"
                selected: sidebar.currentIndex === 15
                onClicked: sidebar.moduleSelected(15)
            }

            // Section 5: System & Transport
            Text {
                text: "SYSTEM & COMM"
                color: SightlineTheme.textMuted
                font.pixelSize: 10
                font.bold: true
                leftPadding: 8
                topPadding: 10
            }

            ModuleTabButton {
                width: parent.width
                text: "Network"
                iconText: "🌐"
                selected: sidebar.currentIndex === 16
                onClicked: sidebar.moduleSelected(16)
            }

            ModuleTabButton {
                width: parent.width
                text: "Serial Port"
                iconText: "🔌"
                selected: sidebar.currentIndex === 17
                onClicked: sidebar.moduleSelected(17)
            }

            ModuleTabButton {
                width: parent.width
                text: "General System"
                iconText: "⚙️"
                selected: sidebar.currentIndex === 18
                onClicked: sidebar.moduleSelected(18)
            }

            ModuleTabButton {
                width: parent.width
                text: "Traffic Inspector"
                iconText: "🔬"
                selected: sidebar.currentIndex === 19
                onClicked: sidebar.moduleSelected(19)
            }
        }
    }
}
