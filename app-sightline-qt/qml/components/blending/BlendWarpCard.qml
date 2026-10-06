import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../.."

// BlendWarpCard.qml
// 0x2F live warp registration: incremental D-pad nudging or absolute shifts,
// rotation (+/-5 deg), zoom / h-zoom, zoom-range multiplier, and warp reset.

BlendCard {
    id: root

    required property BlendState blend
    signal feedback(bool ok, string message)

    title: "LIVE WARP REGISTRATION · MESSAGE 0x2F"
    subtitle: "absOffZoom · vertical · horizontal · rotation · zoom · hzoom · reset"
    accent: SightlineTheme.success

    readonly property int labelWidth: 150
    readonly property bool online: bridge.isConnected

    property int nudgeStep: 1
    property int absVertical: 0
    property int absHorizontal: 0
    property bool sendRotation: false
    property int rotationRaw: 128
    property bool sendZoom: false
    property int zoomRaw: 128
    property bool sendHzoom: false
    property int hzoomRaw: 128

    function report(ok, okMsg) {
        root.feedback(ok, ok ? okMsg
                             : (root.online ? "0x2F rejected: invalid warp parameters" : "Not connected to device"));
    }

    function geometry() {
        return {
            "rotation": root.sendRotation ? Math.max(1, root.rotationRaw) : 0,
            "zoom": root.sendZoom ? Math.max(1, root.zoomRaw) : 0,
            "hzoom": root.sendHzoom ? Math.max(1, root.hzoomRaw) : 0
        };
    }

    function nudge(dv, dh) {
        var cfg = root.blend.warpConfig({ "absolute": false, "vertical": dv, "horizontal": dh,
                                          "rotation": 0, "zoom": 0, "hzoom": 0 });
        root.report(bridge.applyBlendConfig(cfg),
                    "Warp nudged V " + (dv >= 0 ? "+" : "") + dv + " / H " + (dh >= 0 ? "+" : "") + dh + " px");
    }

    function applyWarp() {
        var g = geometry();
        var extra = { "rotation": g.rotation, "zoom": g.zoom, "hzoom": g.hzoom };
        if (root.blend.absolute) {
            extra["vertical"] = root.absVertical;
            extra["horizontal"] = root.absHorizontal;
        } else {
            extra["vertical"] = 0;
            extra["horizontal"] = 0;
        }
        root.report(bridge.applyBlendConfig(root.blend.warpConfig(extra)),
                    root.blend.absolute ? "Absolute warp applied (0x2F)" : "Warp geometry applied (0x2F)");
    }

    // ---- Offset mode -----------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text { text: "Offset Mode"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: root.labelWidth }

        BlendRadio {
            text: "Incremental (nudge)"
            checked: !root.blend.absolute
            onToggled: if (checked) root.blend.absolute = false
        }
        BlendRadio {
            text: "Absolute (set position)"
            checked: root.blend.absolute
            onToggled: if (checked) root.blend.absolute = true
        }

        Text { text: "Zoom Range"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 18 }
        ComboBox {
            model: ["0.90 – 1.10× (fine)", "×1 · 0.004 – 0.996", "×2 · 0.008 – 1.992", "×3 · 0.012 – 2.988",
                    "×4 · 0.016 – 3.984", "×5 · 0.020 – 4.980", "×6 · 0.024 – 5.976", "×7 · 0.028 – 6.972"]
            currentIndex: root.blend.zoomMultiplier
            Layout.preferredWidth: 190
            onActivated: root.blend.zoomMultiplier = currentIndex
        }

        Item { Layout.fillWidth: true }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 24

        // ---- Incremental D-pad --------------------------------------------------------
        ColumnLayout {
            visible: !root.blend.absolute
            spacing: 6
            Layout.alignment: Qt.AlignTop

            Text { text: "NUDGE WARP IMAGE"; color: SightlineTheme.textMuted; font.pixelSize: 10; font.bold: true }

            GridLayout {
                columns: 3
                rowSpacing: 4
                columnSpacing: 4
                Layout.alignment: Qt.AlignHCenter

                Item { implicitWidth: 36; implicitHeight: 30 }
                BlendButton { text: "▲"; implicitWidth: 36; enabled: root.online; onClicked: root.nudge(root.nudgeStep, 0) }
                Item { implicitWidth: 36; implicitHeight: 30 }

                BlendButton { text: "◄"; implicitWidth: 36; enabled: root.online; onClicked: root.nudge(0, -root.nudgeStep) }
                Rectangle {
                    implicitWidth: 36; implicitHeight: 30; radius: 4
                    color: SightlineTheme.background
                    border.color: SightlineTheme.cardBorder
                    Text { anchors.centerIn: parent; text: root.nudgeStep + "px"; color: SightlineTheme.success; font.pixelSize: 10; font.bold: true }
                }
                BlendButton { text: "►"; implicitWidth: 36; enabled: root.online; onClicked: root.nudge(0, root.nudgeStep) }

                Item { implicitWidth: 36; implicitHeight: 30 }
                BlendButton { text: "▼"; implicitWidth: 36; enabled: root.online; onClicked: root.nudge(-root.nudgeStep, 0) }
                Item { implicitWidth: 36; implicitHeight: 30 }
            }

            RowLayout {
                spacing: 4
                Layout.alignment: Qt.AlignHCenter
                Repeater {
                    model: [1, 2, 5, 10]
                    delegate: BlendButton {
                        required property int modelData
                        text: modelData
                        implicitWidth: 34
                        implicitHeight: 22
                        leftPadding: 4
                        rightPadding: 4
                        primary: root.nudgeStep === modelData
                        accent: SightlineTheme.success
                        onClicked: root.nudgeStep = modelData
                    }
                }
            }
        }

        // ---- Absolute shift -----------------------------------------------------------
        ColumnLayout {
            visible: root.blend.absolute
            spacing: 8
            Layout.alignment: Qt.AlignTop

            Text { text: "ABSOLUTE WARP SHIFT (px)"; color: SightlineTheme.textMuted; font.pixelSize: 10; font.bold: true }

            RowLayout {
                spacing: 8
                Text { text: "Vertical (+up)"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.preferredWidth: 100 }
                SpinBox {
                    from: -128; to: 127; editable: true
                    value: root.absVertical
                    Layout.preferredWidth: 120
                    onValueModified: root.absVertical = value
                }
            }
            RowLayout {
                spacing: 8
                Text { text: "Horizontal (+right)"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.preferredWidth: 100 }
                SpinBox {
                    from: -128; to: 127; editable: true
                    value: root.absHorizontal
                    Layout.preferredWidth: 120
                    onValueModified: root.absHorizontal = value
                }
            }
        }

        // ---- Geometry -----------------------------------------------------------------
        ColumnLayout {
            spacing: 6
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop

            Text { text: "WARP GEOMETRY (unchecked = leave unchanged)"; color: SightlineTheme.textMuted; font.pixelSize: 10; font.bold: true }

            RowLayout {
                spacing: 8
                CheckBox { id: rotCheck; checked: root.sendRotation; onToggled: root.sendRotation = checked }
                Text { text: "Rotation"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 60 }
                Slider {
                    enabled: rotCheck.checked
                    from: 1; to: 255; stepSize: 1
                    value: root.rotationRaw
                    Layout.preferredWidth: 200
                    onMoved: root.rotationRaw = Math.round(value)
                }
                Text {
                    text: blendCatalog.rotationDeg(root.rotationRaw).toFixed(2) + "°  (raw " + root.rotationRaw + ")"
                    color: rotCheck.checked ? SightlineTheme.textPrimary : SightlineTheme.textMuted
                    font.pixelSize: 11; font.bold: true
                }
                BlendButton { text: "0°"; implicitHeight: 22; enabled: rotCheck.checked; onClicked: root.rotationRaw = 128 }
            }

            RowLayout {
                spacing: 8
                CheckBox { id: zoomCheck; checked: root.sendZoom; onToggled: root.sendZoom = checked }
                Text { text: "Zoom"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 60 }
                Slider {
                    enabled: zoomCheck.checked
                    from: 1; to: 255; stepSize: 1
                    value: root.zoomRaw
                    Layout.preferredWidth: 200
                    onMoved: root.zoomRaw = Math.round(value)
                }
                Text {
                    text: "raw " + root.zoomRaw
                    color: zoomCheck.checked ? SightlineTheme.textPrimary : SightlineTheme.textMuted
                    font.pixelSize: 11; font.bold: true
                }
            }

            RowLayout {
                spacing: 8
                CheckBox { id: hzoomCheck; checked: root.sendHzoom; onToggled: root.sendHzoom = checked }
                Text { text: "H-Zoom"; color: SightlineTheme.textSecondary; font.pixelSize: 11; Layout.preferredWidth: 60 }
                Slider {
                    enabled: hzoomCheck.checked
                    from: 1; to: 255; stepSize: 1
                    value: root.hzoomRaw
                    Layout.preferredWidth: 200
                    onMoved: root.hzoomRaw = Math.round(value)
                }
                Text {
                    text: "raw " + root.hzoomRaw
                    color: hzoomCheck.checked ? SightlineTheme.textPrimary : SightlineTheme.textMuted
                    font.pixelSize: 11; font.bold: true
                }
            }
        }
    }

    // ---- Actions -------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 10

        BlendButton {
            text: root.blend.absolute ? "Apply Absolute Warp (0x2F)" : "Apply Geometry (0x2F)"
            primary: true
            accent: SightlineTheme.success
            enabled: root.online
            Layout.preferredWidth: 210
            onClicked: root.applyWarp()
        }

        BlendButton {
            text: "Reset Warp Calibration…"
            accent: SightlineTheme.error
            enabled: root.online
            onClicked: resetDialog.open()
        }

        Item { Layout.fillWidth: true }

        Text {
            text: root.blend.usePresetAlign ? "⚠ Preset alignment active: device may ignore warp fields" : ""
            color: SightlineTheme.warning
            font.pixelSize: 10
            font.bold: true
        }
    }

    Dialog {
        id: resetDialog
        modal: true
        title: "Reset warp calibration"
        standardButtons: Dialog.Ok | Dialog.Cancel
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 380

        background: Rectangle {
            color: SightlineTheme.surface
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.error
        }

        contentItem: Text {
            text: "This sends 0x2F with reset = 1 and restores the device's image warp calibration to factory defaults.\n\nContinue?"
            wrapMode: Text.WordWrap
            color: SightlineTheme.textPrimary
            font.pixelSize: 12
        }

        onAccepted: root.report(bridge.applyBlendConfig(root.blend.warpConfig({ "reset": true, "vertical": 0, "horizontal": 0 })),
                                "Warp calibration reset to defaults (0x2F reset=1)")
    }
}
