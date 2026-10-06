import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../.."

// BlendAlignCard.qml
// 0xB9 SLABlendAlign_t editor for preset slots 0..4: int16 offsets,
// rotation (deg * 128, wrapped 0..360), zoom / h-zoom (x * 4096, 0.01..15.99).

BlendCard {
    id: root

    required property BlendState blend
    signal feedback(bool ok, string message)

    title: "PRESET FINE ALIGNMENT · MESSAGE 0xB9"
    subtitle: "SLABlendAlign_t · slots 0..4 · 11-byte payload"
    accent: SightlineTheme.warning

    readonly property int labelWidth: 150
    readonly property bool online: bridge.isConnected

    property int slot: 0
    property int vertical: 0
    property int horizontal: 0
    property real rotateDeg: 0.0
    property real zoom: 1.0
    property real hzoom: 1.0
    property bool loaded: false

    function report(ok, okMsg) {
        root.feedback(ok, ok ? okMsg : (root.online ? "0xB9 rejected" : "Not connected to device"));
    }

    function applyAlign(v, h, deg, z, hz) {
        return bridge.setBlendAlign(root.slot, v, h, Math.round(deg * 128.0),
                                    Math.round(z * 4096.0), Math.round(hz * 4096.0));
    }

    Connections {
        target: bridge
        function onBlendAlignReceived(align) {
            if (align.index !== undefined && align.index !== root.slot) {
                return;
            }
            if (align.vertical !== undefined) root.vertical = align.vertical;
            if (align.horizontal !== undefined) root.horizontal = align.horizontal;
            if (align.rotate !== undefined) {
                var deg = align.rotate / 128.0;
                root.rotateDeg = (deg > 180.0) ? (deg - 360.0) : deg;
            }
            if (align.zoom !== undefined) root.zoom = align.zoom / 4096.0;
            if (align.hzoom !== undefined) root.hzoom = align.hzoom / 4096.0;
            root.loaded = true;
        }
    }

    // ---- Slot ----------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text { text: "Alignment Slot"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        ComboBox {
            model: ["Slot 0", "Slot 1", "Slot 2", "Slot 3", "Slot 4"]
            currentIndex: root.slot
            Layout.preferredWidth: 110
            onActivated: {
                root.slot = currentIndex;
                root.loaded = false;
                if (root.online) {
                    root.report(bridge.getBlendAlign(currentIndex), "Slot " + currentIndex + " requested (0xB9)");
                }
            }
        }

        BlendButton {
            text: "Load Slot"
            enabled: root.online
            implicitHeight: 26
            onClicked: root.report(bridge.getBlendAlign(root.slot), "Slot " + root.slot + " requested (0xB9)")
        }

        Text {
            text: root.loaded ? "● loaded from device" : "○ local values"
            color: root.loaded ? SightlineTheme.success : SightlineTheme.textMuted
            font.pixelSize: 10
            font.bold: true
        }

        Text {
            visible: root.blend.usePresetAlign && root.blend.presetAlignIndex === root.slot
            text: "★ ACTIVE PRESET"
            color: SightlineTheme.warning
            font.pixelSize: 10
            font.bold: true
        }

        Item { Layout.fillWidth: true }
    }

    // ---- Offsets -------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text { text: "Pixel Offsets"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        Text { text: "Vertical"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
        SpinBox {
            from: -4096; to: 4096; editable: true
            value: root.vertical
            Layout.preferredWidth: 120
            onValueModified: root.vertical = value
        }

        Text { text: "Horizontal"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
        SpinBox {
            from: -4096; to: 4096; editable: true
            value: root.horizontal
            Layout.preferredWidth: 120
            onValueModified: root.horizontal = value
        }

        Item { Layout.fillWidth: true }
    }

    // ---- Rotation ------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text { text: "Rotation (°)"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        DecimalSpinBox {
            decimals: 2
            realFrom: -180.0; realTo: 180.0; realStep: 0.1
            realValue: root.rotateDeg
            Layout.preferredWidth: 130
            onRealValueModified: function (v) { root.rotateDeg = v; }
        }

        Slider {
            from: -180; to: 180; stepSize: 0.5
            value: root.rotateDeg
            Layout.preferredWidth: 220
            onMoved: root.rotateDeg = value
        }

        Text {
            text: "wire: " + ((((Math.round(root.rotateDeg * 128) % 46080) + 46080) % 46080))
            color: SightlineTheme.textMuted
            font.pixelSize: 10
            font.family: "Monospace"
        }

        Item { Layout.fillWidth: true }
    }

    // ---- Zoom ----------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text { text: "Zoom (×)"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        Text { text: "V+H"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
        DecimalSpinBox {
            decimals: 3
            realFrom: 0.01; realTo: 15.99; realStep: 0.005
            realValue: root.zoom
            Layout.preferredWidth: 130
            onRealValueModified: function (v) { root.zoom = v; }
        }

        Text { text: "H only"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
        DecimalSpinBox {
            decimals: 3
            realFrom: 0.01; realTo: 15.99; realStep: 0.005
            realValue: root.hzoom
            Layout.preferredWidth: 130
            onRealValueModified: function (v) { root.hzoom = v; }
        }

        BlendButton {
            text: "Lock H = V"
            implicitHeight: 24
            onClicked: root.hzoom = root.zoom
        }

        Item { Layout.fillWidth: true }
    }

    // ---- Actions -------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 10

        BlendButton {
            text: "Apply Slot (0xB9)"
            primary: true
            accent: SightlineTheme.warning
            enabled: root.online
            Layout.preferredWidth: 170
            onClicked: root.report(root.applyAlign(root.vertical, root.horizontal, root.rotateDeg, root.zoom, root.hzoom),
                                   "Alignment slot " + root.slot + " written (0xB9)")
        }

        BlendButton {
            text: "Apply Identity"
            enabled: root.online
            onClicked: {
                root.vertical = 0; root.horizontal = 0; root.rotateDeg = 0.0; root.zoom = 1.0; root.hzoom = 1.0;
                root.report(root.applyAlign(0, 0, 0.0, 1.0, 1.0), "Slot " + root.slot + " set to identity (0xB9)");
            }
        }

        BlendButton {
            text: "Reset Fields"
            onClicked: { root.vertical = 0; root.horizontal = 0; root.rotateDeg = 0.0; root.zoom = 1.0; root.hzoom = 1.0; }
        }

        Item { Layout.fillWidth: true }

        BlendButton {
            text: "Activate as Preset (0x2F)"
            accent: SightlineTheme.warning
            enabled: root.online
            onClicked: {
                root.blend.usePresetAlign = true;
                root.blend.presetAlignIndex = root.slot;
                root.report(bridge.applyBlendConfig(root.blend.fusionConfig()),
                            "Preset alignment slot " + root.slot + " activated (0x2F)");
            }
        }
    }
}
