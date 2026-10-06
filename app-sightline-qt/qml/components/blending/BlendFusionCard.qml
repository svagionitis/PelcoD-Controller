import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../.."

// BlendFusionCard.qml
// 0x2F fusion subset: sensor routing, algorithm, mix, hue, flags, thermal window,
// and preset alignment selection. Warp fields are always sent as "no change".

BlendCard {
    id: root

    required property BlendState blend
    signal feedback(bool ok, string message)
    signal paletteRequested()

    title: "SENSOR FUSION · MESSAGE 0x2F"
    subtitle: "EAN-Blending · IDD 3.11"
    accent: SightlineTheme.primary

    readonly property int labelWidth: 150
    readonly property bool online: bridge.isConnected
    readonly property int m: root.blend.mode

    function report(ok, okMsg) {
        root.feedback(ok, ok ? okMsg
                             : (root.online ? "0x2F rejected: invalid blend parameters" : "Not connected to device"));
    }

    // ---- Sensor routing -----------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text { text: "Sensor Routing"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        Text { text: "Warp"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
        ComboBox {
            id: warpCombo
            model: ["Cam 0", "Cam 1", "Cam 2", "Cam 3"]
            currentIndex: root.blend.warpIndex
            Layout.preferredWidth: 100
            onActivated: root.blend.warpIndex = currentIndex
        }

        Text { text: "⇄"; color: SightlineTheme.textMuted; font.pixelSize: 14 }

        Text { text: "Fixed"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
        ComboBox {
            id: fixedCombo
            model: ["Cam 0", "Cam 1", "Cam 2", "Cam 3"]
            currentIndex: root.blend.fixedIndex
            Layout.preferredWidth: 100
            onActivated: root.blend.fixedIndex = currentIndex
        }

        BlendButton {
            text: "Swap"
            implicitHeight: 26
            onClicked: {
                var w = root.blend.warpIndex;
                root.blend.warpIndex = root.blend.fixedIndex;
                root.blend.fixedIndex = w;
            }
        }

        Text {
            visible: root.blend.warpIndex === root.blend.fixedIndex
            text: "⚠ Warp and fixed channels are identical"
            color: SightlineTheme.warning
            font.pixelSize: 11
            font.bold: true
        }

        Item { Layout.fillWidth: true }
    }

    // ---- Algorithm ---------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text { text: "Fusion Algorithm"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        ComboBox {
            id: modeCombo
            model: blendCatalog.modeNames
            currentIndex: blendCatalog.indexOfMode(root.blend.mode)
            Layout.preferredWidth: 360
            onActivated: root.blend.mode = blendCatalog.modeAt(currentIndex)
        }

        BlendButton {
            visible: blendCatalog.usesPalette(root.m)
            text: "🎨 Edit User Palette"
            implicitHeight: 26
            accent: SightlineTheme.accent
            onClicked: root.paletteRequested()
        }

        Item { Layout.fillWidth: true }
    }

    // ---- Mix ---------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text { text: "Blend Mix (amt)"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        ColumnLayout {
            spacing: 4
            Layout.fillWidth: false
            Layout.preferredWidth: 300

            Slider {
                id: amtSlider
                from: 0; to: 255; stepSize: 1
                value: root.blend.amt
                Layout.fillWidth: true
                onMoved: root.blend.amt = Math.round(value)
            }

            // Visual EO/IR ratio bar
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 6
                radius: 3
                color: "#2a1d10"
                Rectangle {
                    width: parent.width * (amtSlider.value / 255.0)
                    height: parent.height
                    radius: 3
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: SightlineTheme.primaryActive }
                        GradientStop { position: 1.0; color: SightlineTheme.primary }
                    }
                }
            }
        }

        Text {
            text: Math.round(amtSlider.value) + "  ·  " + blendCatalog.mixLabel(root.m, Math.round(amtSlider.value))
            color: SightlineTheme.textPrimary
            font.bold: true
            font.pixelSize: 12
        }

        Item { Layout.fillWidth: true }
    }

    // ---- Hue ---------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        enabled: blendCatalog.usesHue(root.m)
        opacity: enabled ? 1.0 : 0.4

        Text { text: "Color Hue"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        Slider {
            id: hueSlider
            from: 0; to: 255; stepSize: 1
            value: root.blend.hue
            Layout.preferredWidth: 300
            onMoved: root.blend.hue = Math.round(value)
        }

        Rectangle {
            implicitWidth: 18; implicitHeight: 18; radius: 9
            color: Qt.hsva(hueSlider.value / 255.0, 0.85, 0.95, 1.0)
            border.color: SightlineTheme.cardBorder
        }

        Text { text: Math.round(hueSlider.value) + " / 255"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 12 }

        Text {
            visible: !parent.enabled
            text: "(Night / Color modes only)"
            color: SightlineTheme.textMuted
            font.pixelSize: 10
        }

        Item { Layout.fillWidth: true }
    }

    // ---- Flags -------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 18

        Text { text: "Feature Flags"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        BlendCheckBox {
            text: "Hue controls color (bit 1)"
            enabled: blendCatalog.usesHueFlag(root.m)
            checked: (root.blend.flags & 0x02) !== 0
            onToggled: root.blend.flags = checked ? (root.blend.flags | 0x02) : (root.blend.flags & ~0x02)
        }

        BlendCheckBox {
            text: "IR histogram eq. (bit 0 · legacy < v3.7)"
            enabled: blendCatalog.usesHistEq(root.m)
            checked: (root.blend.flags & 0x01) !== 0
            onToggled: root.blend.flags = checked ? (root.blend.flags | 0x01) : (root.blend.flags & ~0x01)
        }

        Item { Layout.fillWidth: true }
    }

    // ---- Thermal window ------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        enabled: blendCatalog.usesThermal(root.m)
        opacity: enabled ? 1.0 : 0.4

        Text { text: "Thermal Window"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        Text { text: "Hot start"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
        Slider {
            id: hotSlider
            from: 0; to: 255; stepSize: 1
            value: root.blend.hotStart
            Layout.preferredWidth: 140
            onMoved: root.blend.hotStart = Math.round(value)
        }
        Text { text: Math.round(hotSlider.value); color: SightlineTheme.warning; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 26 }

        Text { text: "Cold end"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
        Slider {
            id: coldSlider
            from: 0; to: 255; stepSize: 1
            value: root.blend.coldEnd
            Layout.preferredWidth: 140
            onMoved: root.blend.coldEnd = Math.round(value)
        }
        Text { text: Math.round(coldSlider.value); color: SightlineTheme.info; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 26 }

        BlendButton {
            text: "Full Range"
            implicitHeight: 24
            onClicked: { root.blend.hotStart = 0; root.blend.coldEnd = 255; }
        }

        Text {
            visible: !parent.enabled
            text: "(Thermal / Color-IR modes only)"
            color: SightlineTheme.textMuted
            font.pixelSize: 10
        }

        Item { Layout.fillWidth: true }
    }

    // ---- Preset alignment ------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text { text: "Preset Alignment"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        Switch {
            id: presetSwitch
            checked: root.blend.usePresetAlign
            onToggled: root.blend.usePresetAlign = checked
        }

        ComboBox {
            enabled: presetSwitch.checked
            model: blendCatalog.presetNames
            currentIndex: Math.max(0, blendCatalog.presetValues.indexOf(root.blend.presetAlignIndex))
            Layout.preferredWidth: 220
            onActivated: root.blend.presetAlignIndex = blendCatalog.presetValues[currentIndex]
        }

        Text {
            text: presetSwitch.checked ? "Device uses stored alignment instead of message warp fields"
                                       : "Warp fields from this message are used"
            color: SightlineTheme.textMuted
            font.pixelSize: 10
        }

        Item { Layout.fillWidth: true }
    }

    // ---- Actions -------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 10

        BlendButton {
            id: applyFusionBtn
            text: "Apply Fusion (0x2F)"
            primary: true
            enabled: root.online
            Layout.preferredWidth: 190
            onClicked: root.report(bridge.applyBlendConfig(root.blend.fusionConfig()),
                                   "Fusion parameters sent (0x2F) · " + blendCatalog.modeName(root.blend.mode))
        }

        BlendButton {
            id: queryFusionBtn
            text: "Query Active (0x30)"
            enabled: root.online
            onClicked: root.report(bridge.getBlendParameters(), "Blend parameters requested (0x30)")
        }

        Item { Layout.fillWidth: true }

        Text {
            text: root.online ? "" : "Connect to a device to apply"
            color: SightlineTheme.textMuted
            font.pixelSize: 10
        }
    }
}
