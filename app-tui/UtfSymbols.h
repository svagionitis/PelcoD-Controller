#pragma once

/// @file UtfSymbols.h
/// @brief UTF-8 unicode constants for rich box drawing, meters, compass, and telemetry badges.

#include <string_view>

namespace PelcoDTui::Symbols {

// Rounded box drawing
inline constexpr std::string_view BoxTopLeft = "╭";
inline constexpr std::string_view BoxTopRight = "╮";
inline constexpr std::string_view BoxBottomLeft = "╰";
inline constexpr std::string_view BoxBottomRight = "╯";
inline constexpr std::string_view BoxHoriz = "─";
inline constexpr std::string_view BoxVert = "│";
inline constexpr std::string_view BoxTDown = "┬";
inline constexpr std::string_view BoxTUp = "┴";
inline constexpr std::string_view BoxTLeft = "┤";
inline constexpr std::string_view BoxTRight = "├";
inline constexpr std::string_view BoxCross = "┼";

// Heavy box drawing
inline constexpr std::string_view HeavyHoriz = "━";
inline constexpr std::string_view HeavyVert = "┃";

// Fractional vertical block elements for precision gauges
inline constexpr std::string_view BlocksV[] = { " ", " ", "▂", "▃", "▄", "▅", "▆", "▇", "█" };

// Fractional horizontal block elements
inline constexpr std::string_view BlocksH[] = { " ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█" };

// Shading blocks
inline constexpr std::string_view LightShade = "░";
inline constexpr std::string_view MediumShade = "▒";
inline constexpr std::string_view DarkShade = "▓";
inline constexpr std::string_view FullBlock = "█";

// Compass and directional arrows
inline constexpr std::string_view ArrowUp = "▲";
inline constexpr std::string_view ArrowDown = "▼";
inline constexpr std::string_view ArrowLeft = "◄";
inline constexpr std::string_view ArrowRight = "►";
inline constexpr std::string_view ArrowUpLeft = "↖";
inline constexpr std::string_view ArrowUpRight = "↗";
inline constexpr std::string_view ArrowDownLeft = "↙";
inline constexpr std::string_view ArrowDownRight = "↘";

// Indicators and badges
inline constexpr std::string_view CircleFilled = "●";
inline constexpr std::string_view CircleOutline = "○";
inline constexpr std::string_view CheckMark = "✔";
inline constexpr std::string_view CrossMark = "✖";
inline constexpr std::string_view WarningIcon = "▲";
inline constexpr std::string_view GearIcon = "⚙";
inline constexpr std::string_view Thermometer = "🌡";
inline constexpr std::string_view RadioActive = "◉";
inline constexpr std::string_view RadioInactive = "○";

} // namespace PelcoDTui::Symbols
