import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."

Rectangle {
    id: root

    property int cameraIndex: 0
    signal kernelApplied(var weights, bool normalize)

    implicitWidth: 420
    implicitHeight: mainCol.implicitHeight + 24
    color: SightlineTheme.surfaceLight
    radius: SightlineTheme.radiusMedium
    border.color: SightlineTheme.cardBorder
    border.width: 1

    property int matrixDimension: 3
    property var kernelWeights: [0, -1, 0, -1, 5, -1, 0, -1, 0]
    property bool normalizeKernel: false

    function computeSum() {
        var s = 0;
        for (var i = 0; i < kernelWeights.length; ++i) {
            s += kernelWeights[i];
        }
        return s;
    }

    function setDimension(dim) {
        matrixDimension = dim;
        var total = dim * dim;
        var arr = [];
        var mid = Math.floor(total / 2);
        for (var i = 0; i < total; ++i) {
            arr.push(i === mid ? 1 : 0);
        }
        kernelWeights = arr;
    }

    function applyPreset(name) {
        var total = matrixDimension * matrixDimension;
        var mid = Math.floor(total / 2);
        var arr = [];
        if (name === "identity") {
            for (var i = 0; i < total; ++i) arr.push(i === mid ? 1 : 0);
        } else if (name === "sharpen") {
            if (matrixDimension === 3) {
                arr = [0, -1, 0, -1, 5, -1, 0, -1, 0];
            } else {
                for (var i = 0; i < total; ++i) arr.push(i === mid ? total : -1);
            }
        } else if (name === "boxBlur") {
            for (var i = 0; i < total; ++i) arr.push(1);
        } else if (name === "edge") {
            if (matrixDimension === 3) {
                arr = [-1, -1, -1, -1, 8, -1, -1, -1, -1];
            } else {
                for (var i = 0; i < total; ++i) arr.push(i === mid ? (total - 1) : -1);
            }
        } else if (name === "gaussian") {
            if (matrixDimension === 3) {
                arr = [1, 2, 1, 2, 4, 2, 1, 2, 1];
            } else {
                for (var i = 0; i < total; ++i) {
                    var r = Math.floor(i / matrixDimension) - Math.floor(matrixDimension / 2);
                    var c = (i % matrixDimension) - Math.floor(matrixDimension / 2);
                    var dist = r * r + c * c;
                    var w = Math.max(1, Math.round(16 * Math.exp(-dist / 2.0)));
                    arr.push(w);
                }
            }
        } else if (name === "clear") {
            for (var i = 0; i < total; ++i) arr.push(0);
        }
        kernelWeights = arr;
    }

    ColumnLayout {
        id: mainCol
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "CUSTOM SPATIAL CONVOLUTION (NxN)"
                color: SightlineTheme.primary
                font.pixelSize: 11
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "Kernel Size:"
                color: SightlineTheme.textSecondary
                font.pixelSize: 11
                Layout.alignment: Qt.AlignVCenter
            }

            ComboBox {
                id: sizeCombo
                model: ["3 x 3 (9)", "5 x 5 (25)", "7 x 7 (49)", "9 x 9 (81)"]
                currentIndex: 0
                Layout.preferredWidth: 110
                Layout.preferredHeight: 28
                onActivated: {
                    var dims = [3, 5, 7, 9];
                    root.setDimension(dims[index]);
                }
            }
        }

        // Preset quick selectors
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "Presets:"
                color: SightlineTheme.textMuted
                font.pixelSize: 11
            }

            Button {
                text: "Identity"
                implicitHeight: 24
                implicitWidth: 60
                onClicked: root.applyPreset("identity")
            }
            Button {
                text: "Sharpen"
                implicitHeight: 24
                implicitWidth: 60
                onClicked: root.applyPreset("sharpen")
            }
            Button {
                text: "Box Blur"
                implicitHeight: 24
                implicitWidth: 64
                onClicked: root.applyPreset("boxBlur")
            }
            Button {
                text: "Gaussian"
                implicitHeight: 24
                implicitWidth: 64
                onClicked: root.applyPreset("gaussian")
            }
            Button {
                text: "Edge"
                implicitHeight: 24
                implicitWidth: 50
                onClicked: root.applyPreset("edge")
            }
            Button {
                text: "Clear"
                implicitHeight: 24
                implicitWidth: 50
                onClicked: root.applyPreset("clear")
            }

            Item { Layout.fillWidth: true }
        }

        // Matrix Grid
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: matrixDimension * 38 + 8
            height: matrixDimension * 32 + 8
            color: SightlineTheme.surfaceCard
            radius: 4
            border.color: SightlineTheme.cardBorder

            GridLayout {
                anchors.centerIn: parent
                columns: root.matrixDimension
                rows: root.matrixDimension
                columnSpacing: 4
                rowSpacing: 4

                Repeater {
                    model: root.matrixDimension * root.matrixDimension
                    delegate: TextField {
                        id: cellInput
                        width: 34
                        height: 28
                        font.pixelSize: 11
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        text: (index < root.kernelWeights.length) ? root.kernelWeights[index].toString() : "0"
                        color: SightlineTheme.textPrimary
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            border.color: cellInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder
                            border.width: 1
                            radius: 3
                        }
                        validator: IntValidator { bottom: -128; top: 127 }
                        onTextEdited: {
                            var v = parseInt(text) || 0;
                            var copy = root.kernelWeights.slice();
                            copy[index] = v;
                            root.kernelWeights = copy;
                        }
                    }
                }
            }
        }

        // Options and Apply
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            CheckBox {
                id: normCheck
                text: "Normalize Matrix Sum"
                checked: root.normalizeKernel
                onCheckedChanged: root.normalizeKernel = checked
            }

            Text {
                text: "Sum: " + root.computeSum()
                color: root.computeSum() === 0 ? SightlineTheme.warning : SightlineTheme.textSecondary
                font.pixelSize: 11
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "Apply Kernel"
                implicitHeight: 28
                implicitWidth: 120
                contentItem: Text {
                    text: parent.text
                    color: "#0e1014"
                    font.bold: true
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: SightlineTheme.primary
                    radius: 4
                }
                onClicked: {
                    root.kernelApplied(root.kernelWeights, root.normalizeKernel);
                    if (bridge) {
                        bridge.setCustomConvolution(root.cameraIndex, root.kernelWeights, root.normalizeKernel);
                    }
                }
            }
        }
    }
}
