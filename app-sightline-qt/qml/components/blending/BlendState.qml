import QtQuick 2.15

// BlendState.qml
// Shared 0x2F (SLASetBlendParameters_t) editor model for all blending cards.
// Fusion and warp cards both emit complete 0x2F packets, so they must agree on
// every field; this object is that single source of truth.

QtObject {
    id: state

    // --- Fusion fields -------------------------------------------------------
    property int warpIndex: 0
    property int fixedIndex: 1
    property int mode: 1
    property int amt: 128
    property int hue: 0
    property int flags: 0
    property int hotStart: 0
    property int coldEnd: 255

    // --- Preset alignment ----------------------------------------------------
    property bool usePresetAlign: false
    property int presetAlignIndex: 0

    // --- Warp offset mode (absOffZoom) ----------------------------------------
    property bool absolute: false
    property int zoomMultiplier: 0

    // --- Telemetry -------------------------------------------------------------
    property bool synced: false
    property var telemetry: ({})
    property date lastUpdate: new Date(0)
    property int updateCount: 0

    // Fusion-only packet: warp fields are "no change" (incremental, zero shift,
    // rotation/zoom/hzoom = 0) so applying fusion never disturbs registration.
    function fusionConfig() {
        return {
            "warpIndex": warpIndex, "fixedIndex": fixedIndex, "mode": mode,
            "amt": amt, "hue": hue, "flags": flags,
            "hotStart": hotStart, "coldEnd": coldEnd,
            "absolute": false, "zoomMultiplier": zoomMultiplier,
            "vertical": 0, "horizontal": 0, "rotation": 0, "zoom": 0, "hzoom": 0,
            "reset": false,
            "usePresetAlign": usePresetAlign, "presetAlignIndex": presetAlignIndex
        };
    }

    // Full packet with caller-supplied warp overrides layered on top.
    function warpConfig(extra) {
        var cfg = fusionConfig();
        cfg["absolute"] = absolute;
        for (var k in extra) {
            cfg[k] = extra[k];
        }
        return cfg;
    }

    // Merge 0x4D / 0x2F telemetry maps into the editor model.
    function applyTelemetry(p) {
        if (p.warpIndex !== undefined) warpIndex = p.warpIndex;
        if (p.fixedIndex !== undefined) fixedIndex = p.fixedIndex;
        if (p.mode !== undefined && p.mode !== 0) mode = p.mode;
        if (p.amt !== undefined) amt = p.amt;
        if (p.hue !== undefined) hue = p.hue;
        if (p.flags !== undefined) flags = p.flags;
        if (p.hotStart !== undefined) hotStart = p.hotStart;
        if (p.coldEnd !== undefined) coldEnd = p.coldEnd;
        if (p.usePresetAlign !== undefined) usePresetAlign = (p.usePresetAlign !== 0);
        if (p.presetAlignIndex !== undefined) presetAlignIndex = p.presetAlignIndex;
        if (p.zoomMultiplier !== undefined) zoomMultiplier = p.zoomMultiplier;
        if (p.absolute !== undefined) absolute = p.absolute;

        var merged = Object.assign({}, telemetry);
        for (var k in p) {
            merged[k] = p[k];
        }
        telemetry = merged;
        lastUpdate = new Date();
        updateCount += 1;
        synced = true;
    }
}
