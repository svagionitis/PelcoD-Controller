import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../.."

// BlendTelemetryCard.qml
// Full decode of inbound 0x4D (SLACurrentBlendParameters_t) / 0x2F telemetry.

BlendCard {
    id: root

    required property BlendState blend

    title: "INBOUND TELEMETRY · MESSAGE 0x4D"
    subtitle: "SLACurrentBlendParameters_t · 19-byte payload"
    accent: SightlineTheme.info

    readonly property var t: blend.telemetry

    function val(key, fallback) {
        return (root.t && root.t[key] !== undefined) ? root.t[key] : fallback;
    }

    function fmtTime(d) {
        return d.getTime() === 0 ? "—" : Qt.formatTime(d, "hh:mm:ss.zzz");
    }

    // Pulse when a new frame lands
    Connections {
        target: root.blend
        function onUpdateCountChanged() { pulse.restart(); }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        Rectangle {
            id: dot
            implicitWidth: 8; implicitHeight: 8; radius: 4
            color: root.blend.synced ? SightlineTheme.success : SightlineTheme.textMuted
            SequentialAnimation on scale {
                id: pulse
                running: false
                NumberAnimation { to: 1.8; duration: 120; easing.type: Easing.OutQuad }
                NumberAnimation { to: 1.0; duration: 280; easing.type: Easing.InQuad }
            }
        }

        Text {
            text: root.blend.synced ? "Synchronized" : "Listening…"
            color: root.blend.synced ? SightlineTheme.success : SightlineTheme.textMuted
            font.pixelSize: 11
            font.bold: true
        }

        Item { Layout.fillWidth: true }

        Text {
            text: "frames " + root.blend.updateCount + "   ·   last " + root.fmtTime(root.blend.lastUpdate)
            color: SightlineTheme.textMuted
            font.pixelSize: 10
            font.family: "Monospace"
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 4
        columnSpacing: 28
        rowSpacing: 10

        Repeater {
            model: [
                { "label": "Algorithm",         "text": blendCatalog.modeName(root.val("mode", 0)) },
                { "label": "Blend Mix",         "text": root.val("amt", 0) + " · " + blendCatalog.mixLabel(root.val("mode", 0), root.val("amt", 0)) },
                { "label": "Routing",           "text": "Warp Cam " + root.val("warpIndex", "—") + " → Fixed Cam " + root.val("fixedIndex", "—") },
                { "label": "Hue / Flags",       "text": root.val("hue", 0) + " · " + blendCatalog.flagsLabel(root.val("flags", 0)) },
                { "label": "Thermal Window",    "text": "hot " + root.val("hotStart", 0) + " · cold " + root.val("coldEnd", 0) },
                { "label": "Wire Offsets",      "text": "U " + root.val("up", 0) + " · R " + root.val("right", 0) + " · D " + root.val("down", 0) + " · L " + root.val("left", 0) },
                { "label": "Net Shift (V / H)", "text": root.val("vertical", 0) + " / " + root.val("horizontal", 0) + " px" },
                { "label": "Rotation",          "text": root.val("rotation", 0) === 0 ? "0 (unchanged)" : (root.val("rotation", 0) + " → " + blendCatalog.rotationDeg(root.val("rotation", 0)).toFixed(2) + "°") },
                { "label": "Zoom / H-Zoom",     "text": root.val("zoom", 0) + " / " + root.val("hzoom", 0) },
                { "label": "Zoom Range",        "text": root.val("zoomMultiplier", 0) === 0 ? "0.90 – 1.10× (fine)" : ("×" + root.val("zoomMultiplier", 0)) },
                { "label": "Preset Alignment",  "text": root.val("usePresetAlign", 0) ? blendCatalog.presetLabel(root.val("presetAlignIndex", 0)) : "Off" },
                { "label": "absOffZoom (raw)",  "text": "0x" + ("0" + Number(root.val("absOffZoom", 0)).toString(16).toUpperCase()).slice(-2) }
            ]

            delegate: ColumnLayout {
                required property var modelData
                spacing: 2
                Layout.fillWidth: true
                Text { text: modelData.label; color: SightlineTheme.textMuted; font.pixelSize: 10 }
                Text {
                    text: root.blend.synced ? modelData.text : "—"
                    color: SightlineTheme.textPrimary
                    font.pixelSize: 12
                    font.family: "Monospace"
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }
    }
}
