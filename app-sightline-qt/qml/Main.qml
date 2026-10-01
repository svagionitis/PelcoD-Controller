import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "components"
import "views"

ApplicationWindow {
    id: window
    visible: true
    width: 1440
    height: 900
    minimumWidth: 1100
    minimumHeight: 720
    title: "Sightline SLA Protocol Control Suite v3.11.6"
    color: SightlineTheme.background

    // ApplicationWindow top header slot prevents any overlap with body views
    header: StatusHeader {
        id: statusHeader
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. Central Workspace: 3-Pane Layout
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left Navigation Sidebar (Vertical Tabs)
            ModuleNavigationSidebar {
                id: sidebar
                Layout.preferredWidth: SightlineTheme.sidebarWidth
                Layout.minimumWidth: 180
                Layout.maximumWidth: 240
                Layout.fillHeight: true
                currentIndex: stackLayout.currentIndex
                onModuleSelected: function(index) {
                    if (index === 19) {
                        // Focus and expand bottom protocol traffic inspector drawer
                        trafficDrawer.isCollapsed = false;
                    } else {
                        stackLayout.currentIndex = index;
                    }

                    // Auto-execute SLA getters to populate the active tab's structs/fields
                    if (bridge) {
                        bridge.queryModuleParameters(index);
                    }
                }
            }

            // Central Resizable Workspace: Video Viewport & Contextual Options
            SplitView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Horizontal

                handle: Rectangle {
                    implicitWidth: 6
                    color: SplitHandle.hovered || SplitHandle.pressed ? SightlineTheme.primary : SightlineTheme.cardBorder
                    Rectangle {
                        anchors.centerIn: parent
                        width: 2
                        height: 24
                        radius: 1
                        color: SightlineTheme.cardBorderHighlight
                    }
                }

                // Center: Persistent Tactical Video Viewport (visible at all times)
                TacticalVideoViewport {
                    id: videoViewport
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 380
                    SplitView.preferredWidth: 680
                }

                // Right: Contextual Module Options Panel (Resizable)
                Rectangle {
                    id: optionsContainer
                    SplitView.preferredWidth: 560
                    SplitView.minimumWidth: 420
                    SplitView.fillWidth: true
                    color: SightlineTheme.surface
                    border.color: SightlineTheme.cardBorder
                    border.width: 1

                    StackLayout {
                        id: stackLayout
                        anchors.fill: parent
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

                    // 19: Traffic Inspector Details
                    TrafficInspectorView {}
                }
            }
        }
    }

    // 2. Bottom: Persistent Protocol Traffic Inspector Drawer (docked at all times)
    TrafficInspectorDrawer {
        id: trafficDrawer
        Layout.fillWidth: true
    }
    }

    Component.onCompleted: {
        if (bridge) {
            // Automatically execute initial getters for default tab (Tracking)
            bridge.queryModuleParameters(0);
        }
    }
}
