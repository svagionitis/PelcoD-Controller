import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "components"
import "views"

ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 820
    minimumWidth: 1080
    minimumHeight: 700
    title: "Sightline SLA Protocol Control Suite v3.11.6"
    color: SightlineTheme.background

    // ApplicationWindow top header slot prevents any overlap with body views
    header: StatusHeader {
        id: statusHeader
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Left Navigation Sidebar
        ModuleNavigationSidebar {
            id: sidebar
            Layout.preferredWidth: SightlineTheme.sidebarWidth
            Layout.minimumWidth: SightlineTheme.sidebarWidth
            Layout.maximumWidth: SightlineTheme.sidebarWidth
            Layout.fillHeight: true
            currentIndex: stackLayout.currentIndex
            onModuleSelected: function(index) {
                stackLayout.currentIndex = index;
            }
        }

        // Central Dynamic Module Workspace
        StackLayout {
            id: stackLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: 0

            // 0: Tracking
            TrackingView {}

            // 1: Stabilization
            StabilizationView {}

            // 2: Detection
            DetectionView {}

            // 3: Classification
            ClassificationView {}

            // 4: Landing Aid
            LandingAidView {}

            // 5: Capture
            CaptureView {}

            // 6: Display
            DisplayView {}

            // 7: Enhancement
            EnhancementView {}

            // 8: Compression
            CompressionView {}

            // 9: Blending
            BlendingView {}

            // 10: Overlays
            OverlayView {}

            // 11: Focus & Lens
            FocusLensView {}

            // 12: NUC Calibration
            NucView {}

            // 13: Telemetry
            TelemetryView {}

            // 14: KLV Metadata
            KlvMetadataView {}

            // 15: Recording
            RecordingView {}

            // 16: Network
            NetworkView {}

            // 17: Serial Port
            SerialPortView {}

            // 18: General System
            GeneralSystemView {}

            // 19: Traffic Inspector
            TrafficInspectorView {}
        }
    }
}
