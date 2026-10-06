import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"
import "../components/blending"

// BlendingView.qml
// Dual-sensor fusion & multi-camera registration module.
// Composes the blending cards around a shared BlendState (0x2F editor model).
//   0x2F Set Blend Parameters    -> BlendFusionCard, BlendWarpCard
//   0x30 Get Blend Parameters    -> header refresh / fusion query
//   0x4D Current Blend Params    -> BlendState.applyTelemetry, BlendTelemetryCard
//   0xB9 Blend Align             -> BlendAlignCard
//   0x95 Four Align Points       -> FourPointAlignCard
//   0x74 / 0x75 Multi Alignment  -> MultiAlignCard

Item {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true

    readonly property int minContentWidth: 1020

    BlendState { id: blendState }

    function showFeedback(ok, message) {
        toast.ok = ok;
        toast.message = message;
        toast.shown = true;
        toastTimer.restart();
    }

    function refreshAll() {
        if (!bridge.isConnected) {
            showFeedback(false, "Not connected to device");
            return;
        }
        var ok = bridge.getBlendParameters();
        ok = bridge.getBlendAlign(alignCard.slot) && ok;
        ok = bridge.getFourAlignPoints(fourPointCard.slot) && ok;
        ok = bridge.getMultipleAlignment() && ok;
        showFeedback(ok, ok ? "Refresh requested: 0x30 · 0xB9 · 0x95 · 0x74" : "One or more refresh queries failed");
    }

    Connections {
        target: bridge
        function onCurrentBlendParamsReceived(params) { blendState.applyTelemetry(params); }
        function onBlendParametersReceived(params) { blendState.applyTelemetry(params); }
        function onConnectionChanged() {
            if (!bridge.isConnected) {
                blendState.synced = false;
            }
        }
    }

    UserPaletteDialog { id: paletteDialog }

    ScrollView {
        id: scroller
        anchors.fill: parent
        contentWidth: Math.max(availableWidth, root.minContentWidth + 32)
        contentHeight: mainCol.implicitHeight + 40
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AsNeeded

        ColumnLayout {
            id: mainCol
            x: 16
            width: Math.max(scroller.availableWidth - 32, root.minContentWidth)
            spacing: 16

            Item { Layout.preferredHeight: 4 }

            // ---- Header ---------------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Rectangle {
                    implicitWidth: 4; implicitHeight: 22; radius: 2
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: SightlineTheme.primary }
                        GradientStop { position: 1.0; color: SightlineTheme.success }
                    }
                }

                Text {
                    text: "DUAL-SENSOR FUSION & MULTI-CAMERA REGISTRATION"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: SightlineTheme.fontSizeMedium
                    font.bold: true
                    font.letterSpacing: 1.0
                }

                Text {
                    text: "// IDD 3.11: 0x2F · 0x30 · 0x4D · 0x95 · 0xB9 · 0x74 · 0x75"
                    color: SightlineTheme.textMuted
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.family: "Monospace"
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    id: statusPill
                    readonly property color tone: !bridge.isConnected ? SightlineTheme.error
                                                                      : (blendState.synced ? SightlineTheme.success : SightlineTheme.warning)
                    implicitHeight: 24
                    radius: 12
                    color: !bridge.isConnected ? "#2a1218" : (blendState.synced ? "#122a1f" : "#2a1f12")
                    border.width: 1
                    border.color: tone
                    implicitWidth: statusRow.implicitWidth + 20
                    Behavior on color { ColorAnimation { duration: 200 } }

                    RowLayout {
                        id: statusRow
                        anchors.centerIn: parent
                        spacing: 6
                        Rectangle {
                            implicitWidth: 6; implicitHeight: 6; radius: 3
                            color: statusPill.tone
                            SequentialAnimation on opacity {
                                running: bridge.isConnected && !blendState.synced
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.2; duration: 600 }
                                NumberAnimation { to: 1.0; duration: 600 }
                            }
                        }
                        Text {
                            text: !bridge.isConnected ? "OFFLINE"
                                                      : (blendState.synced ? "TELEMETRY SYNCED (0x4D)" : "AWAITING SYNC")
                            color: statusPill.tone
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                BlendButton {
                    id: refreshAllBtn
                    text: "⟳ Refresh All"
                    enabled: bridge.isConnected
                    implicitHeight: 26
                    onClicked: root.refreshAll()
                }
            }

            // ---- Metrics ---------------------------------------------------------------
            Flow {
                Layout.fillWidth: true
                spacing: 12

                MetricCard {
                    title: "Sensor Routing"
                    value: "Cam " + blendState.warpIndex + " → Cam " + blendState.fixedIndex
                    accentColor: SightlineTheme.primary
                    iconText: "🎥"
                }
                MetricCard {
                    implicitWidth: 330
                    title: "Active Algorithm"
                    value: blendCatalog.modeName(blendState.mode)
                    accentColor: SightlineTheme.info
                    iconText: "✨"
                }
                MetricCard {
                    implicitWidth: 190
                    title: "Blend Mix"
                    value: blendCatalog.mixLabel(blendState.mode, blendState.amt)
                    accentColor: SightlineTheme.success
                    iconText: "⚖️"
                }
                MetricCard {
                    implicitWidth: 200
                    title: "Active Preset"
                    value: blendState.usePresetAlign ? blendCatalog.presetLabel(blendState.presetAlignIndex) : "Off (message warp)"
                    accentColor: SightlineTheme.accent
                    iconText: "🎯"
                }
                MetricCard {
                    title: "Offset Mode"
                    value: blendState.absolute ? "Absolute" : "Incremental"
                    accentColor: SightlineTheme.warning
                    iconText: "📐"
                }
            }

            // ---- Cards -----------------------------------------------------------------
            BlendFusionCard {
                id: fusionCard
                blend: blendState
                onFeedback: function (ok, message) { root.showFeedback(ok, message); }
                onPaletteRequested: paletteDialog.open()
            }

            BlendWarpCard {
                id: warpCard
                blend: blendState
                onFeedback: function (ok, message) { root.showFeedback(ok, message); }
            }

            BlendAlignCard {
                id: alignCard
                blend: blendState
                onFeedback: function (ok, message) { root.showFeedback(ok, message); }
            }

            FourPointAlignCard {
                id: fourPointCard
                blend: blendState
                onFeedback: function (ok, message) { root.showFeedback(ok, message); }
            }

            MultiAlignCard {
                id: multiAlignCard
                onFeedback: function (ok, message) { root.showFeedback(ok, message); }
            }

            BlendTelemetryCard {
                id: telemetryCard
                blend: blendState
            }

            Item { Layout.preferredHeight: 16 }
        }
    }

    // ---- Feedback toast --------------------------------------------------------------
    Rectangle {
        id: toast

        property bool ok: true
        property bool shown: false
        property string message: ""

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: shown ? 20 : -height
        width: Math.min(parent.width - 40, toastText.implicitWidth + 56)
        height: 38
        radius: 19
        color: ok ? "#10261c" : "#2a1218"
        border.width: 1
        border.color: ok ? SightlineTheme.success : SightlineTheme.error
        opacity: shown ? 1.0 : 0.0
        z: 100

        Behavior on anchors.bottomMargin { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 220 } }

        RowLayout {
            anchors.centerIn: parent
            spacing: 8
            Text {
                text: toast.ok ? "✓" : "✕"
                color: toast.border.color
                font.bold: true
                font.pixelSize: 13
            }
            Text {
                id: toastText
                text: toast.message
                color: SightlineTheme.textPrimary
                font.pixelSize: 12
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: toast.shown = false
        }

        Timer {
            id: toastTimer
            interval: 3200
            onTriggered: toast.shown = false
        }
    }
}
