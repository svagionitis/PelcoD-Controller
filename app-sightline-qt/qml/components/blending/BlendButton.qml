import QtQuick 2.15
import QtQuick.Controls 2.15
import "../.."

// BlendButton.qml
// Primary/secondary themed button with hover, press and disabled states.

Button {
    id: control

    property bool primary: false
    property color accent: SightlineTheme.primary

    implicitHeight: 30
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        color: control.primary ? "#0e1014" : SightlineTheme.textPrimary
        opacity: control.enabled ? 1.0 : 0.45
        font.bold: true
        font.pixelSize: 11
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: SightlineTheme.radiusSmall
        opacity: control.enabled ? 1.0 : 0.45
        color: {
            if (control.primary) {
                if (control.down) return Qt.darker(control.accent, 1.3);
                if (control.hovered) return Qt.lighter(control.accent, 1.15);
                return control.accent;
            }
            if (control.down) return SightlineTheme.cardBorder;
            if (control.hovered) return SightlineTheme.cardBorderHighlight;
            return SightlineTheme.surfaceLight;
        }
        border.width: control.primary ? 0 : 1
        border.color: control.hovered ? control.accent : SightlineTheme.inputBorder

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }
    }

    scale: control.down ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: 90 } }
}
