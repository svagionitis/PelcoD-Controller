#pragma once

/// @file Canvas.h
/// @brief Double-buffered 2D terminal canvas with ANSI delta rendering.

#include "Color.h"
#include "Terminal.h"
#include "UtfSymbols.h"

#include <string>
#include <string_view>
#include <vector>

namespace PelcoDTui {

/// @struct Cell
/// @brief Single terminal cell supporting multi-byte UTF-8 graphemes and styling.
struct Cell {
    std::string ch { " " };
    Style style {};

    [[nodiscard]] bool operator==(const Cell& other) const noexcept
    {
        return style == other.style && ch == other.ch;
    }

    [[nodiscard]] bool operator!=(const Cell& other) const noexcept
    {
        return !(*this == other);
    }
};

/// @class Canvas
/// @brief High-performance double-buffered 2D screen buffer with delta ANSI emission.
class Canvas {
public:
    Canvas(int width = 80, int height = 24);
    ~Canvas() = default;

    // Non-copyable, movable
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;
    Canvas(Canvas&&) noexcept = default;
    Canvas& operator=(Canvas&&) noexcept = default;

    void resize(int width, int height);
    [[nodiscard]] int getWidth() const noexcept
    {
        return m_width;
    }
    [[nodiscard]] int getHeight() const noexcept
    {
        return m_height;
    }

    void clear(const Style& style = {});

    void setCell(int x, int y, std::string_view ch, const Style& style);
    void drawString(int x, int y, std::string_view str, const Style& style, int maxLen = -1);
    void drawHLine(int x, int y, int width, std::string_view ch = Symbols::BoxHoriz, const Style& style = {});
    void drawVLine(int x, int y, int height, std::string_view ch = Symbols::BoxVert, const Style& style = {});
    void drawBox(int x, int y, int width, int height, const Style& style = {}, bool rounded = true);
    void drawPanel(int x, int y, int width, int height, std::string_view title, const Style& style = {},
        const Style& titleStyle = {});
    void drawProgressBar(int x, int y, int width, double fraction, const Style& activeStyle, const Style& emptyStyle);

    /// @brief Compare front and back buffers, emitting minimal ANSI updates to terminal.
    void renderDelta(Terminal& term);

    /// @brief Force complete redraw next time renderDelta is called.
    void invalidate() noexcept;

private:
    int m_width { 80 };
    int m_height { 24 };
    std::vector<Cell> m_front;
    std::vector<Cell> m_back;
    bool m_forceFullRedraw { true };
};

} // namespace PelcoDTui
