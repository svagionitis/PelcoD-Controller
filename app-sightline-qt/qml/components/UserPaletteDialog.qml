import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import ".."

Dialog {
    id: root

    title: "User False Color Palette Designer (Message 0x72)"
    modal: true
    width: 680
    height: 580
    anchors.centerIn: parent

    background: Rectangle {
        color: SightlineTheme.surface
        border.color: SightlineTheme.cardBorder
        border.width: 1
        radius: SightlineTheme.radiusMedium
    }

    header: Rectangle {
        height: 44
        color: SightlineTheme.surfaceCard
        border.color: SightlineTheme.cardBorder
        border.width: 1
        radius: SightlineTheme.radiusMedium

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            Rectangle { width: 4; height: 16; color: SightlineTheme.primary; radius: 2 }
            Text {
                text: "USER FALSE COLOR PALETTE DESIGNER"
                color: SightlineTheme.textPrimary
                font.bold: true
                font.pixelSize: 12
                font.letterSpacing: 0.8
            }
            Text {
                text: "// 256x3 YUV Look-Up Table (LUT)"
                color: SightlineTheme.textMuted
                font.pixelSize: 10
                font.family: "Monospace"
            }
            Item { Layout.fillWidth: true }
        }
    }

    // 256 RGB colors array stored internally
    property var rgbTable: [] // Array of {r: 0..255, g: 0..255, b: 0..255}
    property int selectedIndex: 0
    property int rangeFrom: 0
    property int rangeTo: 255
    property int targetPaletteSlot: 0

    Component.onCompleted: {
        resetToGrayscale();
    }

    function resetToGrayscale() {
        var arr = [];
        for (var i = 0; i < 256; ++i) {
            arr.push({ r: i, g: i, b: i });
        }
        rgbTable = arr;
        paletteCanvas.requestPaint();
    }

    function smoothRange() {
        if (rangeFrom >= rangeTo) return;
        var startColor = rgbTable[rangeFrom];
        var endColor = rgbTable[rangeTo];
        var steps = rangeTo - rangeFrom;

        var copy = rgbTable.slice();
        for (var i = 0; i <= steps; ++i) {
            var t = i / steps;
            var r = Math.round(startColor.r + t * (endColor.r - startColor.r));
            var g = Math.round(startColor.g + t * (endColor.g - startColor.g));
            var b = Math.round(startColor.b + t * (endColor.b - startColor.b));
            copy[rangeFrom + i] = { r: r, g: g, b: b };
        }
        rgbTable = copy;
        paletteCanvas.requestPaint();
    }

    function applyRainbow() {
        var copy = [];
        for (var i = 0; i < 256; ++i) {
            var h = i / 256.0;
            var rgb = hsvToRgb(h, 1.0, 1.0);
            copy.push(rgb);
        }
        rgbTable = copy;
        paletteCanvas.requestPaint();
    }

    function applyIronbow() {
        var copy = [];
        for (var i = 0; i < 256; ++i) {
            var t = i / 255.0;
            var r = Math.min(255, Math.max(0, Math.round(255 * Math.sin(t * Math.PI * 0.7))));
            var g = Math.min(255, Math.max(0, Math.round(255 * Math.pow(t, 2))));
            var b = Math.min(255, Math.max(0, Math.round(255 * (1.0 - Math.cos(t * Math.PI * 0.8)) * 0.5)));
            copy.push({ r: r, g: g, b: b });
        }
        rgbTable = copy;
        paletteCanvas.requestPaint();
    }

    function rgbToYuv(r, g, b) {
        var y = Math.round(0.299 * r + 0.587 * g + 0.114 * b);
        var u = Math.round(-0.168736 * r - 0.331264 * g + 0.5 * b + 128);
        var v = Math.round(0.5 * r - 0.418688 * g - 0.081312 * b + 128);
        return {
            y: Math.min(255, Math.max(0, y)),
            u: Math.min(255, Math.max(0, u)),
            v: Math.min(255, Math.max(0, v))
        };
    }

    function yuvToRgb(y, u, v) {
        var c = y;
        var d = u - 128;
        var e = v - 128;
        var r = Math.round(c + 1.402 * e);
        var g = Math.round(c - 0.344136 * d - 0.714136 * e);
        var b = Math.round(c + 1.772 * d);
        return {
            r: Math.min(255, Math.max(0, r)),
            g: Math.min(255, Math.max(0, g)),
            b: Math.min(255, Math.max(0, b))
        };
    }

    function hsvToRgb(h, s, v) {
        var r, g, b;
        var i = Math.floor(h * 6);
        var f = h * 6 - i;
        var p = v * (1 - s);
        var q = v * (1 - f * s);
        var t = v * (1 - (1 - f) * s);
        switch (i % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            case 5: r = v; g = p; b = q; break;
        }
        return { r: Math.round(r * 255), g: Math.round(g * 255), b: Math.round(b * 255) };
    }

    function getYuvList() {
        var list = [];
        for (var i = 0; i < 256; ++i) {
            var color = (i < rgbTable.length) ? rgbTable[i] : { r: i, g: i, b: i };
            var yuv = rgbToYuv(color.r, color.g, color.b);
            list.push(yuv.y);
            list.push(yuv.u);
            list.push(yuv.v);
        }
        return list;
    }

    function loadFromYuvList(list) {
        if (!list || list.length < 768) return;
        var arr = [];
        for (var i = 0; i < 256; ++i) {
            var y = list[i * 3];
            var u = list[i * 3 + 1];
            var v = list[i * 3 + 2];
            arr.push(yuvToRgb(y, u, v));
        }
        rgbTable = arr;
        paletteCanvas.requestPaint();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // Canvas Palette Preview (256 distinct columns)
        Rectangle {
            Layout.fillWidth: true
            height: 64
            color: SightlineTheme.surfaceCard
            border.color: SightlineTheme.cardBorder
            border.width: 1
            radius: 4

            Canvas {
                id: paletteCanvas
                anchors.fill: parent
                anchors.margins: 2
                onPaint: {
                    var ctx = getContext("2d");
                    var w = width / 256.0;
                    var h = height;
                    for (var i = 0; i < 256; ++i) {
                        var c = (i < root.rgbTable.length) ? root.rgbTable[i] : { r: i, g: i, b: i };
                        ctx.fillStyle = "rgb(" + c.r + "," + c.g + "," + c.b + ")";
                        ctx.fillRect(i * w, 0, w + 1, h);
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                onPositionChanged: {
                    var idx = Math.min(255, Math.max(0, Math.floor((mouseX / width) * 256)));
                    root.selectedIndex = idx;
                }
                onClicked: {
                    var idx = Math.min(255, Math.max(0, Math.floor((mouseX / width) * 256)));
                    root.selectedIndex = idx;
                }
            }
        }

        // Palette presets quick bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text { text: "Quick Presets:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
            Button { text: "Grayscale"; implicitHeight: 26; onClicked: root.resetToGrayscale() }
            Button { text: "Ironbow"; implicitHeight: 26; onClicked: root.applyIronbow() }
            Button { text: "Rainbow"; implicitHeight: 26; onClicked: root.applyRainbow() }
            Item { Layout.fillWidth: true }

            Text { text: "Target Palette Slot:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
            ComboBox {
                model: ["User Slot 0", "User Slot 1", "User Slot 2", "User Slot 3"]
                currentIndex: root.targetPaletteSlot
                implicitHeight: 26
                onActivated: root.targetPaletteSlot = index
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: SightlineTheme.cardBorder }

        // Range Editing & Gradient Tools
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text { text: "Gradient Range:"; color: SightlineTheme.textPrimary; font.bold: true; font.pixelSize: 11 }

            Text { text: "From Index:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
            SpinBox {
                from: 0; to: 255; value: root.rangeFrom
                implicitHeight: 28; implicitWidth: 80
                onValueModified: root.rangeFrom = value
            }

            Text { text: "To Index:"; color: SightlineTheme.textSecondary; font.pixelSize: 11 }
            SpinBox {
                from: 0; to: 255; value: root.rangeTo
                implicitHeight: 28; implicitWidth: 80
                onValueModified: root.rangeTo = value
            }

            Button {
                text: "Smooth Gradient"
                implicitHeight: 28
                contentItem: Text { text: parent.text; color: SightlineTheme.primary; font.bold: true; font.pixelSize: 11 }
                background: Rectangle { color: SightlineTheme.surfaceLight; border.color: SightlineTheme.primary; radius: 4 }
                onClicked: root.smoothRange()
            }

            Item { Layout.fillWidth: true }
        }

        // Active Index Color Editor
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: colorCol.implicitHeight + 16
            color: SightlineTheme.surfaceCard
            radius: 4
            border.color: SightlineTheme.cardBorder

            ColumnLayout {
                id: colorCol
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: "Selected Index: " + root.selectedIndex
                        color: SightlineTheme.primary
                        font.bold: true
                        font.pixelSize: 12
                    }

                    Rectangle {
                        width: 24
                        height: 18
                        radius: 2
                        border.color: "#ffffff"
                        color: {
                            var c = (root.selectedIndex < root.rgbTable.length) ? root.rgbTable[root.selectedIndex] : { r: 0, g: 0, b: 0 };
                            return "rgb(" + c.r + "," + c.g + "," + c.b + ")";
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Set As 'From'"
                        implicitHeight: 24
                        onClicked: root.rangeFrom = root.selectedIndex
                    }
                    Button {
                        text: "Set As 'To'"
                        implicitHeight: 24
                        onClicked: root.rangeTo = root.selectedIndex
                    }
                }

                // RGB sliders for selected index
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text { text: "Red:"; color: "#ff5252"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                    Slider {
                        id: rSlider
                        from: 0; to: 255
                        value: (root.selectedIndex < root.rgbTable.length) ? root.rgbTable[root.selectedIndex].r : 0
                        Layout.fillWidth: true
                        onMoved: {
                            var copy = root.rgbTable.slice();
                            var cur = copy[root.selectedIndex] || { r: 0, g: 0, b: 0 };
                            copy[root.selectedIndex] = { r: Math.round(value), g: cur.g, b: cur.b };
                            root.rgbTable = copy;
                            paletteCanvas.requestPaint();
                        }
                    }
                    Text { text: Math.round(rSlider.value).toString(); color: SightlineTheme.textPrimary; font.pixelSize: 11; Layout.preferredWidth: 30 }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text { text: "Green:"; color: "#69f0ae"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                    Slider {
                        id: gSlider
                        from: 0; to: 255
                        value: (root.selectedIndex < root.rgbTable.length) ? root.rgbTable[root.selectedIndex].g : 0
                        Layout.fillWidth: true
                        onMoved: {
                            var copy = root.rgbTable.slice();
                            var cur = copy[root.selectedIndex] || { r: 0, g: 0, b: 0 };
                            copy[root.selectedIndex] = { r: cur.r, g: Math.round(value), b: cur.b };
                            root.rgbTable = copy;
                            paletteCanvas.requestPaint();
                        }
                    }
                    Text { text: Math.round(gSlider.value).toString(); color: SightlineTheme.textPrimary; font.pixelSize: 11; Layout.preferredWidth: 30 }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text { text: "Blue:"; color: "#448aff"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                    Slider {
                        id: bSlider
                        from: 0; to: 255
                        value: (root.selectedIndex < root.rgbTable.length) ? root.rgbTable[root.selectedIndex].b : 0
                        Layout.fillWidth: true
                        onMoved: {
                            var copy = root.rgbTable.slice();
                            var cur = copy[root.selectedIndex] || { r: 0, g: 0, b: 0 };
                            copy[root.selectedIndex] = { r: cur.r, g: cur.g, b: Math.round(value) };
                            root.rgbTable = copy;
                            paletteCanvas.requestPaint();
                        }
                    }
                    Text { text: Math.round(bSlider.value).toString(); color: SightlineTheme.textPrimary; font.pixelSize: 11; Layout.preferredWidth: 30 }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Bottom Action Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: "Load LUT (.bin/.lut)..."
                implicitHeight: 32
                onClicked: loadDialog.open()
            }

            Button {
                text: "Save LUT..."
                implicitHeight: 32
                onClicked: saveDialog.open()
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "Send to Board (Msg 0x72)"
                implicitHeight: 32
                contentItem: Text {
                    text: parent.text
                    color: "#0e1014"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: SightlineTheme.primary
                    radius: 4
                }
                onClicked: {
                    var yuvList = root.getYuvList();
                    if (bridge) {
                        bridge.setUserPaletteLut(root.targetPaletteSlot, yuvList);
                    }
                    root.close();
                }
            }

            Button {
                text: "Close"
                implicitHeight: 32
                onClicked: root.close()
            }
        }
    }

    FileDialog {
        id: loadDialog
        title: "Load Sightline Palette File"
        nameFilters: ["LUT and Binary files (*.lut *.bin)", "All files (*)"]
        onAccepted: {
            var path = selectedFile.toString().replace(/^(file:\/{3}|file:\/\/)/, "");
            if (Qt.platform.os === "windows" && path.length > 2 && path[0] === '/' && path[2] === ':') {
                path = path.substring(1);
            }
            if (bridge) {
                var list = bridge.loadPaletteFile(path);
                if (list && list.length >= 768) {
                    root.loadFromYuvList(list);
                }
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: "Save Sightline Palette File"
        fileMode: FileDialog.SaveFile
        nameFilters: ["LUT files (*.lut)", "Binary files (*.bin)", "All files (*)"]
        onAccepted: {
            var path = selectedFile.toString().replace(/^(file:\/{3}|file:\/\/)/, "");
            if (Qt.platform.os === "windows" && path.length > 2 && path[0] === '/' && path[2] === ':') {
                path = path.substring(1);
            }
            var yuvList = root.getYuvList();
            if (bridge) {
                bridge.savePaletteFile(path, yuvList);
            }
        }
    }
}
