#pragma once

/// @file Color.h
/// @brief 24-bit TrueColor and ANSI terminal style abstractions.

#include <cstdint>
#include <string>

namespace PelcoDTui {

/// @struct Rgb
/// @brief 24-bit RGB color representation.
struct Rgb {
    std::uint8_t r { 0U };
    std::uint8_t g { 0U };
    std::uint8_t b { 0U };

    [[nodiscard]] constexpr bool operator==(const Rgb& other) const noexcept
    {
        return r == other.r && g == other.g && b == other.b;
    }

    [[nodiscard]] constexpr bool operator!=(const Rgb& other) const noexcept
    {
        return !(*this == other);
    }
};

/// @enum ColorType
/// @brief Tag denoting standard ANSI 16-color, 24-bit TrueColor, or default terminal color.
enum class ColorType : std::uint8_t { Default, Ansi16, TrueColor };

/// @struct Color
/// @brief Color representation supporting terminal defaults, 16 ANSI colors, or 24-bit RGB.
struct Color {
    ColorType type { ColorType::Default };
    std::uint8_t ansiIndex { 0U };
    Rgb rgb {};

    [[nodiscard]] static constexpr Color defaultColor() noexcept
    {
        return Color { ColorType::Default, 0U, {} };
    }

    [[nodiscard]] static constexpr Color ansi(std::uint8_t idx) noexcept
    {
        return Color { ColorType::Ansi16, idx, {} };
    }

    [[nodiscard]] static constexpr Color fromRgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
    {
        return Color { ColorType::TrueColor, 0U, { r, g, b } };
    }

    [[nodiscard]] constexpr bool operator==(const Color& other) const noexcept
    {
        if (type != other.type) {
            return false;
        }
        if (type == ColorType::Ansi16) {
            return ansiIndex == other.ansiIndex;
        }
        if (type == ColorType::TrueColor) {
            return rgb == other.rgb;
        }
        return true;
    }

    [[nodiscard]] constexpr bool operator!=(const Color& other) const noexcept
    {
        return !(*this == other);
    }
};

/// @struct Style
/// @brief Terminal cell styling attributes including colors and text decorations.
struct Style {
    Color fg { Color::defaultColor() };
    Color bg { Color::defaultColor() };
    bool bold { false };
    bool dim { false };
    bool italic { false };
    bool underline { false };
    bool reverse { false };

    [[nodiscard]] constexpr bool operator==(const Style& other) const noexcept
    {
        return fg == other.fg && bg == other.bg && bold == other.bold && dim == other.dim && italic == other.italic
            && underline == other.underline && reverse == other.reverse;
    }

    [[nodiscard]] constexpr bool operator!=(const Style& other) const noexcept
    {
        return !(*this == other);
    }
};

namespace Colors {
    inline constexpr Color Reset = Color::defaultColor();
    inline constexpr Color Black = Color::fromRgb(20, 20, 20);
    inline constexpr Color White = Color::fromRgb(240, 240, 240);
    inline constexpr Color Gray = Color::fromRgb(128, 128, 128);
    inline constexpr Color DarkGray = Color::fromRgb(50, 54, 60);
    inline constexpr Color PanelBg = Color::fromRgb(24, 28, 36);
    inline constexpr Color HeaderBg = Color::fromRgb(15, 18, 24);
    inline constexpr Color Red = Color::fromRgb(235, 75, 75);
    inline constexpr Color Green = Color::fromRgb(70, 210, 110);
    inline constexpr Color Yellow = Color::fromRgb(240, 195, 60);
    inline constexpr Color Blue = Color::fromRgb(75, 140, 245);
    inline constexpr Color Cyan = Color::fromRgb(60, 205, 230);
    inline constexpr Color Magenta = Color::fromRgb(200, 90, 225);
    inline constexpr Color Orange = Color::fromRgb(245, 140, 50);
} // namespace Colors

namespace Styles {
    inline constexpr Style Default { Colors::White, Colors::PanelBg, false, false, false, false, false };
    inline constexpr Style Border { Colors::DarkGray, Colors::PanelBg, false, false, false, false, false };
    inline constexpr Style Title { Colors::Cyan, Colors::PanelBg, true, false, false, false, false };
    inline constexpr Style Label { Colors::Gray, Colors::PanelBg, false, false, false, false, false };
    inline constexpr Style Text { Colors::White, Colors::PanelBg, false, false, false, false, false };
    inline constexpr Style Highlight { Colors::Yellow, Colors::PanelBg, true, false, false, false, false };
    inline constexpr Style ActiveBar { Colors::Cyan, Colors::PanelBg, false, false, false, false, false };
    inline constexpr Style EmptyBar { Colors::DarkGray, Colors::PanelBg, false, false, false, false, false };
    inline constexpr Style Ok { Colors::Green, Colors::PanelBg, true, false, false, false, false };
    inline constexpr Style Warn { Colors::Red, Colors::PanelBg, true, false, false, false, false };
    inline constexpr Style HeaderBorder { Colors::DarkGray, Colors::HeaderBg, false, false, false, false, false };
} // namespace Styles

/// @brief Appends ANSI escape sequence for a Color (foreground or background) to a string buffer.
inline void appendColorEscape(std::string& s, const Color& color, bool isBackground)
{
    const char* prefix = isBackground ? "\033[48;" : "\033[38;";
    if (color.type == ColorType::TrueColor) {
        s.append(prefix)
            .append("2;")
            .append(std::to_string(color.rgb.r))
            .append(";")
            .append(std::to_string(color.rgb.g))
            .append(";")
            .append(std::to_string(color.rgb.b))
            .append("m");
    } else if (color.type == ColorType::Ansi16) {
        s.append(prefix).append("5;").append(std::to_string(color.ansiIndex)).append("m");
    } else if (color.type == ColorType::Default) {
        s.append(isBackground ? "\033[49m" : "\033[39m");
    }
}

/// @brief Generate ANSI escape sequence to transition from one style to another.
/// @param[in] prev Previous style active on the terminal.
/// @param[in] next Next desired style to apply.
/// @return ANSI escape sequence string.
[[nodiscard]] inline std::string toAnsiTransition(const Style& prev, const Style& next)
{
    if (prev == next) {
        return "";
    }

    std::string s;
    s.reserve(48);

    // If flags differ or changing to default, reset first
    if ((prev.bold && !next.bold) || (prev.dim && !next.dim) || (prev.italic && !next.italic)
        || (prev.underline && !next.underline) || (prev.reverse && !next.reverse)
        || (prev.fg.type != ColorType::Default && next.fg.type == ColorType::Default)
        || (prev.bg.type != ColorType::Default && next.bg.type == ColorType::Default)) {
        s.append("\033[0m");
        if (next.bold) {
            s.append("\033[1m");
        }
        if (next.dim) {
            s.append("\033[2m");
        }
        if (next.italic) {
            s.append("\033[3m");
        }
        if (next.underline) {
            s.append("\033[4m");
        }
        if (next.reverse) {
            s.append("\033[7m");
        }
        if (next.fg.type != ColorType::Default) {
            appendColorEscape(s, next.fg, false);
        }
        if (next.bg.type != ColorType::Default) {
            appendColorEscape(s, next.bg, true);
        }
        return s;
    }

    if (!prev.bold && next.bold) {
        s.append("\033[1m");
    }
    if (!prev.dim && next.dim) {
        s.append("\033[2m");
    }
    if (!prev.italic && next.italic) {
        s.append("\033[3m");
    }
    if (!prev.underline && next.underline) {
        s.append("\033[4m");
    }
    if (!prev.reverse && next.reverse) {
        s.append("\033[7m");
    }

    if (prev.fg != next.fg) {
        appendColorEscape(s, next.fg, false);
    }

    if (prev.bg != next.bg) {
        appendColorEscape(s, next.bg, true);
    }

    return s;
}

} // namespace PelcoDTui
