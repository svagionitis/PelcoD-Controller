pragma Singleton
import QtQuick 2.15

QtObject {
    id: theme

    // Colors
    readonly property color background: "#0F1115"
    readonly property color surface: "#181A20"
    readonly property color surfaceLight: "#22252D"
    readonly property color cardBorder: "#2E333D"
    readonly property color cardBorderHover: "#434B59"

    readonly property color textPrimary: "#FFFFFF"
    readonly property color textSecondary: "#8E95A5"
    readonly property color textMuted: "#5B6171"

    readonly property color primary: "#00C0FF"
    readonly property color primaryHover: "#33CDFF"
    readonly property color primaryActive: "#0099CC"

    readonly property color success: "#00E676"
    readonly property color warning: "#FFB300"
    readonly property color error: "#FF5252"
    readonly property color info: "#448AFF"

    // Typography
    readonly property string fontFamily: "Inter, Roboto, sans-serif"
    readonly property int fontSizeSmall: 11
    readonly property int fontSizeNormal: 13
    readonly property int fontSizeMedium: 15
    readonly property int fontSizeLarge: 18
    readonly property int fontSizeXLarge: 22

    // Dimensions & Spacing
    readonly property int radiusSmall: 4
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12
    readonly property int headerHeight: 64
    readonly property int sidebarWidth: 260
}
