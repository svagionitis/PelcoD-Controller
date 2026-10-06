import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../.."

// FourPointAlignCard.qml
// 0x95 SLAFourAlignPoints_t editor: four corresponding feature points (A..D)
// in the left and right cameras define a projective homography per slot.
// Negative values are clamped to 0 by the device; all zeros resets the slot.

BlendCard {
    id: root

    required property BlendState blend
    signal feedback(bool ok, string message)

    title: "4-POINT HOMOGRAPHY · MESSAGE 0x95"
    subtitle: "SLAFourAlignPoints_t · slots 0..4 → preset 10..14"
    accent: SightlineTheme.info

    readonly property bool online: bridge.isConnected
    readonly property var cornerNames: ["A", "B", "C", "D"]

    property int slot: 0
    property int frameW: 640
    property int frameH: 480
    property bool loaded: false
    property var points: defaultPoints()

    function defaultPoints() {
        var w = root.frameW, h = root.frameH;
        var mx = Math.round(w * 0.1), my = Math.round(h * 0.1);
        return [
            { "leftCol": mx,     "leftRow": my,     "rightCol": mx,     "rightRow": my },
            { "leftCol": w - mx, "leftRow": my,     "rightCol": w - mx, "rightRow": my },
            { "leftCol": w - mx, "leftRow": h - my, "rightCol": w - mx, "rightRow": h - my },
            { "leftCol": mx,     "leftRow": h - my, "rightCol": mx,     "rightRow": h - my }
        ];
    }

    function zeroPoints() {
        var out = [];
        for (var i = 0; i < 4; ++i) {
            out.push({ "leftCol": 0, "leftRow": 0, "rightCol": 0, "rightRow": 0 });
        }
        return out;
    }

    function setField(i, key, v) {
        var copy = root.points.map(function (p) { return Object.assign({}, p); });
        copy[i][key] = v;
        root.points = copy;
    }

    function isZero() {
        return root.points.every(function (p) {
            return p.leftCol === 0 && p.leftRow === 0 && p.rightCol === 0 && p.rightRow === 0;
        });
    }

    function report(ok, okMsg) {
        root.feedback(ok, ok ? okMsg : (root.online ? "0x95 rejected" : "Not connected to device"));
    }

    onPointsChanged: preview.requestPaint()
    onFrameWChanged: preview.requestPaint()
    onFrameHChanged: preview.requestPaint()

    Connections {
        target: bridge
        function onFourAlignPointsReceived(index, pts) {
            if (index !== root.slot || !pts || pts.length < 4) {
                return;
            }
            root.points = pts;
            root.loaded = true;
        }
    }

    // ---- Slot & frame ---------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text { text: "Homography Slot"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150 }

        ComboBox {
            model: ["Slot 0 (preset 10)", "Slot 1 (preset 11)", "Slot 2 (preset 12)", "Slot 3 (preset 13)", "Slot 4 (preset 14)"]
            currentIndex: root.slot
            Layout.preferredWidth: 170
            onActivated: {
                root.slot = currentIndex;
                root.loaded = false;
                if (root.online) {
                    root.report(bridge.getFourAlignPoints(currentIndex), "4-point slot " + currentIndex + " requested (0x95)");
                }
            }
        }

        BlendButton {
            text: "Load Slot"
            enabled: root.online
            implicitHeight: 26
            onClicked: root.report(bridge.getFourAlignPoints(root.slot), "4-point slot " + root.slot + " requested (0x95)")
        }

        Text {
            text: root.loaded ? "● loaded from device" : "○ local values"
            color: root.loaded ? SightlineTheme.success : SightlineTheme.textMuted
            font.pixelSize: 10
            font.bold: true
        }

        Text {
            visible: root.blend.usePresetAlign && root.blend.presetAlignIndex === (10 + root.slot)
            text: "★ ACTIVE PRESET"
            color: SightlineTheme.info
            font.pixelSize: 10
            font.bold: true
        }

        Item { Layout.fillWidth: true }

        Text { text: "Preview frame"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
        SpinBox { from: 16; to: 8192; editable: true; value: root.frameW; Layout.preferredWidth: 100; onValueModified: root.frameW = value }
        Text { text: "×"; color: SightlineTheme.textMuted }
        SpinBox { from: 16; to: 8192; editable: true; value: root.frameH; Layout.preferredWidth: 100; onValueModified: root.frameH = value }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 18

        // ---- Point table --------------------------------------------------------------
        GridLayout {
            columns: 5
            columnSpacing: 8
            rowSpacing: 6
            Layout.alignment: Qt.AlignTop

            Text { text: "" }
            Text { text: "LEFT COL"; color: SightlineTheme.primary; font.pixelSize: 10; font.bold: true }
            Text { text: "LEFT ROW"; color: SightlineTheme.primary; font.pixelSize: 10; font.bold: true }
            Text { text: "RIGHT COL"; color: SightlineTheme.accent; font.pixelSize: 10; font.bold: true }
            Text { text: "RIGHT ROW"; color: SightlineTheme.accent; font.pixelSize: 10; font.bold: true }

            Repeater {
                model: 4
                delegate: Item {
                    required property int index
                    Layout.row: index + 1
                    Layout.column: 0
                    implicitWidth: 28
                    implicitHeight: 30
                    Rectangle {
                        anchors.centerIn: parent
                        width: 22; height: 22; radius: 11
                        color: Qt.rgba(0, 0.9, 1, 0.12)
                        border.color: SightlineTheme.info
                        Text { anchors.centerIn: parent; text: root.cornerNames[index]; color: SightlineTheme.info; font.bold: true; font.pixelSize: 11 }
                    }
                }
            }

            Repeater {
                model: 16
                delegate: SpinBox {
                    required property int index
                    readonly property int row: Math.floor(index / 4)
                    readonly property string key: ["leftCol", "leftRow", "rightCol", "rightRow"][index % 4]
                    Layout.row: row + 1
                    Layout.column: (index % 4) + 1
                    Layout.preferredWidth: 104
                    from: 0; to: 8192; editable: true
                    value: root.points[row] ? root.points[row][key] : 0
                    onValueModified: root.setField(row, key, value)
                }
            }
        }

        // ---- Live preview -------------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 196
            Layout.minimumWidth: 240
            color: "#08090c"
            radius: SightlineTheme.radiusSmall
            border.color: SightlineTheme.cardBorder

            Canvas {
                id: preview
                anchors.fill: parent
                anchors.margins: 10

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    var sx = width / Math.max(1, root.frameW);
                    var sy = height / Math.max(1, root.frameH);
                    var s = Math.min(sx, sy);
                    var ox = (width - root.frameW * s) / 2;
                    var oy = (height - root.frameH * s) / 2;

                    ctx.strokeStyle = "#242938";
                    ctx.lineWidth = 1;
                    ctx.strokeRect(ox, oy, root.frameW * s, root.frameH * s);

                    function px(c) { return ox + c * s; }
                    function py(r) { return oy + r * s; }

                    function quad(colKey, rowKey, color) {
                        ctx.strokeStyle = color;
                        ctx.fillStyle = color;
                        ctx.lineWidth = 1.5;
                        ctx.beginPath();
                        for (var i = 0; i < 4; ++i) {
                            var p = root.points[i];
                            if (i === 0) ctx.moveTo(px(p[colKey]), py(p[rowKey]));
                            else ctx.lineTo(px(p[colKey]), py(p[rowKey]));
                        }
                        ctx.closePath();
                        ctx.stroke();
                        ctx.globalAlpha = 0.08;
                        ctx.fill();
                        ctx.globalAlpha = 1.0;
                        ctx.font = "bold 10px sans-serif";
                        for (var j = 0; j < 4; ++j) {
                            var q = root.points[j];
                            ctx.beginPath();
                            ctx.arc(px(q[colKey]), py(q[rowKey]), 3, 0, 2 * Math.PI);
                            ctx.fill();
                            ctx.fillText(root.cornerNames[j], px(q[colKey]) + 5, py(q[rowKey]) - 4);
                        }
                    }

                    if (!root.points || root.points.length < 4) return;

                    // Correspondence vectors (left -> right)
                    ctx.strokeStyle = "#525c70";
                    ctx.setLineDash([3, 3]);
                    for (var k = 0; k < 4; ++k) {
                        var c = root.points[k];
                        ctx.beginPath();
                        ctx.moveTo(px(c.leftCol), py(c.leftRow));
                        ctx.lineTo(px(c.rightCol), py(c.rightRow));
                        ctx.stroke();
                    }
                    ctx.setLineDash([]);

                    quad("leftCol", "leftRow", "#00e5ff");
                    quad("rightCol", "rightRow", "#ff9100");
                }
            }

            Row {
                anchors { left: parent.left; bottom: parent.bottom; margins: 8 }
                spacing: 10
                Text { text: "■ Left camera"; color: SightlineTheme.primary; font.pixelSize: 9 }
                Text { text: "■ Right camera"; color: SightlineTheme.accent; font.pixelSize: 9 }
            }

            Text {
                visible: root.isZero()
                anchors.centerIn: parent
                text: "All zeros → slot reset"
                color: SightlineTheme.warning
                font.pixelSize: 11
                font.bold: true
            }
        }
    }

    // ---- Actions -------------------------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 10

        BlendButton {
            text: "Apply Points (0x95)"
            primary: true
            accent: SightlineTheme.info
            enabled: root.online
            Layout.preferredWidth: 170
            onClicked: root.report(bridge.setFourAlignPoints(root.slot, root.points),
                                   root.isZero() ? ("4-point slot " + root.slot + " reset (0x95 all zeros)")
                                                 : ("4-point slot " + root.slot + " written (0x95)"))
        }

        BlendButton {
            text: "Default Rectangle"
            onClicked: root.points = root.defaultPoints()
        }

        BlendButton {
            text: "Mirror Left → Right"
            onClicked: root.points = root.points.map(function (p) {
                return { "leftCol": p.leftCol, "leftRow": p.leftRow, "rightCol": p.leftCol, "rightRow": p.leftRow };
            })
        }

        BlendButton {
            text: "Clear (reset slot)"
            accent: SightlineTheme.error
            onClicked: root.points = root.zeroPoints()
        }

        Item { Layout.fillWidth: true }

        BlendButton {
            text: "Activate as Preset (0x2F)"
            accent: SightlineTheme.info
            enabled: root.online
            onClicked: {
                root.blend.usePresetAlign = true;
                root.blend.presetAlignIndex = 10 + root.slot;
                root.report(bridge.applyBlendConfig(root.blend.fusionConfig()),
                            "4-point preset " + (10 + root.slot) + " activated (0x2F)");
            }
        }
    }
}
