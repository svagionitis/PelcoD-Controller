import QtQuick 2.15
import QtQuick.Controls 2.15

// DecimalSpinBox.qml
// SpinBox editing a real value with fixed decimals (stored internally as scaled int).

SpinBox {
    id: control

    property int decimals: 1
    property real realFrom: 0.0
    property real realTo: 100.0
    property real realStep: 0.1
    property real realValue: 0.0
    readonly property real factor: Math.pow(10, decimals)

    signal realValueModified(real value)

    from: Math.round(realFrom * factor)
    to: Math.round(realTo * factor)
    stepSize: Math.max(1, Math.round(realStep * factor))
    value: Math.round(realValue * factor)
    editable: true

    validator: DoubleValidator {
        bottom: Math.min(control.realFrom, control.realTo)
        top: Math.max(control.realFrom, control.realTo)
        decimals: control.decimals
        notation: DoubleValidator.StandardNotation
    }

    textFromValue: function (v, locale) {
        return Number(v / control.factor).toLocaleString(locale, "f", control.decimals);
    }

    valueFromText: function (text, locale) {
        return Math.round(Number.fromLocaleString(locale, text) * control.factor);
    }

    onValueModified: {
        realValue = value / factor;
        realValueModified(realValue);
    }
}
