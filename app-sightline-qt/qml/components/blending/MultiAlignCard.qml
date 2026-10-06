import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../.."

// MultiAlignCard.qml
// 0x74 SLASetMultipleAlignment_t / 0x75 SLACurrentMultipleAlignment_t editor:
// up to five camera alignment slots (vertical, horizontal, rotate, zoom, hzoom; 0..255).

BlendCard {
    id: root

    signal feedback(bool ok, string message)

    title: "MULTI-CAMERA ALIGNMENT · MESSAGE 0x74 / 0x75"
    subtitle: "SLASetMultipleAlignment_t · 5 slots"
    accent: SightlineTheme.accent

    readonly property bool online: bridge.isConnected
    readonly property var fields: ["vertical", "horizontal", "rotate", "zoom", "hzoom"]
    readonly property var headers: ["VERTICAL", "HORIZONTAL", "ROTATE", "ZOOM", "H-ZOOM"]

    property int nAlignments: 0
    property bool loaded: false
    property var entries: emptyEntries()

    function emptyEntries() {
        var out = [];
        for (var i = 0; i < 5; ++i) {
            out.push({ "vertical": 0, "horizontal": 0, "rotate": 0, "zoom": 0, "hzoom": 0 });
        }
        return out;
    }

    function setField(i, key, v) {
        var copy = root.entries.map(function (e) { return Object.assign({}, e); });
        copy[i][key] = v;
        root.entries = copy;
    }

    function report(ok, okMsg) {
        root.feedback(ok, ok ? okMsg : (root.online ? "0x74 rejected" : "Not connected to device"));
    }

    Connections {
        target: bridge
        function onMultipleAlignmentReceived(n, list) {
            root.nAlignments = n;
            if (list && list.length >= 5) {
                root.entries = list;
            }
            root.loaded = true;
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text { text: "Valid Alignments"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150 }

        SpinBox {
            from: 0; to: 5; editable: true
            value: root.nAlignments
            Layout.preferredWidth: 100
            onValueModified: root.nAlignments = value
        }

        Text {
            text: root.loaded ? "● loaded from device (0x75)" : "○ local values"
            color: root.loaded ? SightlineTheme.success : SightlineTheme.textMuted
            font.pixelSize: 10
            font.bold: true
        }

        Item { Layout.fillWidth: true }

        Text {
            text: "Raw byte encoding per SLASetBlendParameters_t"
            color: SightlineTheme.textMuted
            font.pixelSize: 10
        }
    }

    GridLayout {
        columns: 6
        columnSpacing: 8
        rowSpacing: 6
        Layout.fillWidth: true

        Text { text: "SLOT"; color: SightlineTheme.textMuted; font.pixelSize: 10; font.bold: true }
        Repeater {
            model: root.headers
            delegate: Text {
                required property string modelData
                text: modelData
                color: SightlineTheme.accent
                font.pixelSize: 10
                font.bold: true
            }
        }

        Repeater {
            model: 5
            delegate: Item {
                required property int index
                Layout.row: index + 1
                Layout.column: 0
                implicitWidth: 60
                implicitHeight: 30
                opacity: index < root.nAlignments ? 1.0 : 0.35
                Behavior on opacity { NumberAnimation { duration: 140 } }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "#" + index
                    color: SightlineTheme.textPrimary
                    font.bold: true
                    font.pixelSize: 12
                }
            }
        }

        Repeater {
            model: 25
            delegate: SpinBox {
                required property int index
                readonly property int row: Math.floor(index / 5)
                readonly property string key: root.fields[index % 5]
                Layout.row: row + 1
                Layout.column: (index % 5) + 1
                Layout.preferredWidth: 110
                from: 0; to: 255; editable: true
                opacity: row < root.nAlignments ? 1.0 : 0.35
                Behavior on opacity { NumberAnimation { duration: 140 } }
                value: root.entries[row] ? root.entries[row][key] : 0
                onValueModified: root.setField(row, key, value)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 10

        BlendButton {
            text: "Apply Alignments (0x74)"
            primary: true
            accent: SightlineTheme.accent
            enabled: root.online
            Layout.preferredWidth: 200
            onClicked: root.report(bridge.setMultipleAlignment(root.nAlignments, root.entries),
                                   root.nAlignments + " multi-camera alignment(s) written (0x74)")
        }

        BlendButton {
            text: "Query (0x75)"
            enabled: root.online
            onClicked: root.report(bridge.getMultipleAlignment(), "Multiple alignment requested (0x28 → 0x75)")
        }

        BlendButton {
            text: "Clear Table"
            onClicked: { root.entries = root.emptyEntries(); root.nAlignments = 0; }
        }

        Item { Layout.fillWidth: true }
    }
}
