import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."

Rectangle {
    id: sidebar
    implicitWidth: SightlineTheme.sidebarWidth
    color: SightlineTheme.surface
    border.color: SightlineTheme.cardBorder
    border.width: 1

    property int currentIndex: 0
    signal moduleSelected(int index)

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: parent.width - 16
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 3

            // Section 1: Target Tracking & AI
            Text {
                text: "TRACKING & INTELLIGENCE"
                color: SightlineTheme.textMuted
                font.pixelSize: 10
                font.bold: true
                Layout.topMargin: 6
                Layout.bottomMargin: 2
                Layout.leftMargin: 4
            }

            ModuleTabButton {
                text: "Tracking"
                iconText: "🎯"
                selected: sidebar.currentIndex === 0
                badgeText: (bridge && bridge.trackListModel && bridge.trackListModel.rowCount() > 0) ? (bridge.trackListModel.rowCount() + " tracks") : ""
                onClicked: sidebar.moduleSelected(0)
            }

            ModuleTabButton {
                text: "Stabilization"
                iconText: "⚖️"
                selected: sidebar.currentIndex === 1
                onClicked: sidebar.moduleSelected(1)
            }

            ModuleTabButton {
                text: "Detection"
                iconText: "🔍"
                selected: sidebar.currentIndex === 2
                onClicked: sidebar.moduleSelected(2)
            }

            ModuleTabButton {
                text: "Classification"
                iconText: "🧠"
                selected: sidebar.currentIndex === 3
                onClicked: sidebar.moduleSelected(3)
            }

            ModuleTabButton {
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
                Layout.topMargin: 10
                Layout.bottomMargin: 2
                Layout.leftMargin: 4
            }

            ModuleTabButton {
                text: "Capture"
                iconText: "📹"
                selected: sidebar.currentIndex === 5
                onClicked: sidebar.moduleSelected(5)
            }

            ModuleTabButton {
                text: "Display"
                iconText: "🖥️"
                selected: sidebar.currentIndex === 6
                onClicked: sidebar.moduleSelected(6)
            }

            ModuleTabButton {
                text: "Enhancement"
                iconText: "✨"
                selected: sidebar.currentIndex === 7
                onClicked: sidebar.moduleSelected(7)
            }

            ModuleTabButton {
                text: "Compression"
                iconText: "🗜️"
                selected: sidebar.currentIndex === 8
                onClicked: sidebar.moduleSelected(8)
            }

            ModuleTabButton {
                text: "Blending"
                iconText: "🔀"
                selected: sidebar.currentIndex === 9
                onClicked: sidebar.moduleSelected(9)
            }

            ModuleTabButton {
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
                Layout.topMargin: 10
                Layout.bottomMargin: 2
                Layout.leftMargin: 4
            }

            ModuleTabButton {
                text: "Focus & Lens"
                iconText: "🔭"
                selected: sidebar.currentIndex === 11
                onClicked: sidebar.moduleSelected(11)
            }

            ModuleTabButton {
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
                Layout.topMargin: 10
                Layout.bottomMargin: 2
                Layout.leftMargin: 4
            }

            ModuleTabButton {
                text: "Telemetry"
                iconText: "📡"
                selected: sidebar.currentIndex === 13
                onClicked: sidebar.moduleSelected(13)
            }

            ModuleTabButton {
                text: "KLV Metadata"
                iconText: "🏷️"
                selected: sidebar.currentIndex === 14
                onClicked: sidebar.moduleSelected(14)
            }

            ModuleTabButton {
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
                Layout.topMargin: 10
                Layout.bottomMargin: 2
                Layout.leftMargin: 4
            }

            ModuleTabButton {
                text: "Network"
                iconText: "🌐"
                selected: sidebar.currentIndex === 16
                onClicked: sidebar.moduleSelected(16)
            }

            ModuleTabButton {
                text: "Serial Port"
                iconText: "🔌"
                selected: sidebar.currentIndex === 17
                onClicked: sidebar.moduleSelected(17)
            }

            ModuleTabButton {
                text: "General System"
                iconText: "⚙️"
                selected: sidebar.currentIndex === 18
                onClicked: sidebar.moduleSelected(18)
            }

            ModuleTabButton {
                text: "Traffic Inspector"
                iconText: "🔬"
                selected: sidebar.currentIndex === 19
                onClicked: sidebar.moduleSelected(19)
            }

            Item { Layout.preferredHeight: 12 }
        }
    }
}
