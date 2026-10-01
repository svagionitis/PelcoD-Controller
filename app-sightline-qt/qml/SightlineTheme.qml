pragma Singleton
import QtQuick 2.15

QtObject {
    id: theme

    // Tactical dark theme palette matching app-video-qt and ThemeDark.qss
    readonly property color background: "#0e1014"
    readonly property color surface: "#141720"
    readonly property color surfaceCard: "#181b24"
    readonly property color surfaceLight: "#1e2230"
    readonly property color cardBorder: "#242938"
    readonly property color cardBorderHighlight: "#2a2f40"
    readonly property color inputBorder: "#2d3446"

    readonly property color textPrimary: "#f0f4fc"
    readonly property color textSecondary: "#8894ab"
    readonly property color textMuted: "#525c70"

    readonly property color primary: "#00e5ff"
    readonly property color primaryHover: "#33ebff"
    readonly property color primaryActive: "#00b4cc"
    readonly property color accent: "#ff9100"

    readonly property color success: "#00e676"
    readonly property color warning: "#ff9100"
    readonly property color error: "#ff1744"
    readonly property color info: "#00e5ff"

    // Typography
    readonly property string fontFamily: "Segoe UI, Roboto, Helvetica Neue, sans-serif"
    readonly property int fontSizeSmall: 10
    readonly property int fontSizeNormal: 12
    readonly property int fontSizeMedium: 14
    readonly property int fontSizeLarge: 16
    readonly property int fontSizeXLarge: 18

    // Dimensions & Spacing
    readonly property int radiusSmall: 4
    readonly property int radiusMedium: 6
    readonly property int radiusLarge: 8
    readonly property int headerHeight: 48
    readonly property int sidebarWidth: 200
}
