import QtQuick 2.15
import QtQuick.Controls 2.15
import "../.."

// BlendRadio.qml
// RadioButton with theme-coloured label that dims when disabled.

RadioButton {
    id: control

    contentItem: Text {
        text: control.text
        color: SightlineTheme.textPrimary
        opacity: control.enabled ? 1.0 : 0.4
        font.pixelSize: 11
        leftPadding: control.indicator.width + 6
        verticalAlignment: Text.AlignVCenter
    }
}
