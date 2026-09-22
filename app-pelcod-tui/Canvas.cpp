#include "Canvas.h"

#include <algorithm>
#include <cmath>

namespace PelcoDTui {

Canvas::Canvas(int width, int height)
    : m_width(std::max(1, width))
    , m_height(std::max(1, height))
{
    const std::size_t total = static_cast<std::size_t>(m_width * m_height);
    m_front.assign(total, Cell {});
    m_back.assign(total, Cell {});
}

void Canvas::resize(int width, int height)
{
    m_width = std::max(1, width);
    m_height = std::max(1, height);
    const std::size_t total = static_cast<std::size_t>(m_width * m_height);
    m_front.assign(total, Cell {});
    m_back.assign(total, Cell {});
    m_forceFullRedraw = true;
}

void Canvas::invalidate() noexcept
{
    m_forceFullRedraw = true;
}

void Canvas::clear(const Style& style)
{
    const std::size_t total = static_cast<std::size_t>(m_width * m_height);
    for (std::size_t i = 0; i < total; ++i) {
        m_back[i].ch = " ";
        m_back[i].style = style;
    }
}

void Canvas::setCell(int x, int y, std::string_view ch, const Style& style)
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return;
    }
    const std::size_t idx = static_cast<std::size_t>(y * m_width + x);
    m_back[idx].ch = std::string(ch);
    m_back[idx].style = style;
}

void Canvas::drawString(int x, int y, std::string_view str, const Style& style, int maxLen)
{
    if (y < 0 || y >= m_height || x >= m_width) {
        return;
    }

    int curX = x;
    int printed = 0;
    std::size_t i = 0;

    while (i < str.size()) {
        if (curX >= m_width) {
            break;
        }
        if (maxLen >= 0 && printed >= maxLen) {
            break;
        }

        // Determine UTF-8 byte length
        const auto c = static_cast<unsigned char>(str[i]);
        std::size_t len = 1;
        if ((c & 0xE0U) == 0xC0U)
            len = 2;
        else if ((c & 0xF0U) == 0xE0U)
            len = 3;
        else if ((c & 0xF8U) == 0xF0U)
            len = 4;

        if (i + len > str.size()) {
            break;
        }

        if (curX >= 0) {
            setCell(curX, y, str.substr(i, len), style);
        }

        curX++;
        printed++;
        i += len;
    }
}

void Canvas::drawHLine(int x, int y, int width, std::string_view ch, const Style& style)
{
    if (y < 0 || y >= m_height) {
        return;
    }
    for (int i = 0; i < width; ++i) {
        setCell(x + i, y, ch, style);
    }
}

void Canvas::drawVLine(int x, int y, int height, std::string_view ch, const Style& style)
{
    if (x < 0 || x >= m_width) {
        return;
    }
    for (int i = 0; i < height; ++i) {
        setCell(x, y + i, ch, style);
    }
}

void Canvas::drawBox(int x, int y, int width, int height, const Style& style, bool rounded)
{
    if (width < 2 || height < 2) {
        return;
    }

    const std::string_view tl = rounded ? Symbols::BoxTopLeft : "┌";
    const std::string_view tr = rounded ? Symbols::BoxTopRight : "┐";
    const std::string_view bl = rounded ? Symbols::BoxBottomLeft : "└";
    const std::string_view br = rounded ? Symbols::BoxBottomRight : "┘";

    setCell(x, y, tl, style);
    setCell(x + width - 1, y, tr, style);
    setCell(x, y + height - 1, bl, style);
    setCell(x + width - 1, y + height - 1, br, style);

    drawHLine(x + 1, y, width - 2, Symbols::BoxHoriz, style);
    drawHLine(x + 1, y + height - 1, width - 2, Symbols::BoxHoriz, style);
    drawVLine(x, y + 1, height - 2, Symbols::BoxVert, style);
    drawVLine(x + width - 1, y + 1, height - 2, Symbols::BoxVert, style);
}

void Canvas::drawPanel(
    int x, int y, int width, int height, std::string_view title, const Style& style, const Style& titleStyle)
{
    drawBox(x, y, width, height, style, true);
    if (!title.empty() && width > 4) {
        const std::string framedTitle = " " + std::string(title) + " ";
        const int maxTitleLen = width - 4;
        drawString(x + 2, y, framedTitle, titleStyle, maxTitleLen);
    }
}

void Canvas::drawProgressBar(
    int x, int y, int width, double fraction, const Style& activeStyle, const Style& emptyStyle)
{
    if (width <= 0) {
        return;
    }
    const double clamped = std::clamp(fraction, 0.0, 1.0);
    const double totalFill = clamped * static_cast<double>(width);
    const int fullBlocks = static_cast<int>(std::floor(totalFill));
    const double remainder = totalFill - static_cast<double>(fullBlocks);

    for (int i = 0; i < fullBlocks; ++i) {
        setCell(x + i, y, Symbols::FullBlock, activeStyle);
    }

    if (fullBlocks < width) {
        // Fractional block
        const int subIndex = static_cast<int>(std::round(remainder * 8.0));
        if (subIndex > 0 && subIndex <= 8) {
            setCell(x + fullBlocks, y, Symbols::BlocksH[subIndex], activeStyle);
            for (int i = fullBlocks + 1; i < width; ++i) {
                setCell(x + i, y, Symbols::LightShade, emptyStyle);
            }
        } else {
            for (int i = fullBlocks; i < width; ++i) {
                setCell(x + i, y, Symbols::LightShade, emptyStyle);
            }
        }
    }
}

void Canvas::renderDelta(Terminal& term)
{
    std::string out;
    out.reserve(8192);

    Style curStyle {};
    bool styleInitialized = false;
    int lastCursorX = -1;
    int lastCursorY = -1;

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y * m_width + x);
            const Cell& backCell = m_back[idx];
            Cell& frontCell = m_front[idx];

            if (m_forceFullRedraw || backCell != frontCell) {
                // If cursor is not immediately at (x, y), jump
                if (lastCursorY != y || lastCursorX != x) {
                    out.append("\033[")
                        .append(std::to_string(y + 1))
                        .append(";")
                        .append(std::to_string(x + 1))
                        .append("H");
                }

                // Apply style transition
                if (!styleInitialized) {
                    out.append(toAnsiTransition(Style {}, backCell.style));
                    curStyle = backCell.style;
                    styleInitialized = true;
                } else if (curStyle != backCell.style) {
                    out.append(toAnsiTransition(curStyle, backCell.style));
                    curStyle = backCell.style;
                }

                out.append(backCell.ch);
                frontCell = backCell;

                lastCursorX = x + 1;
                lastCursorY = y;
            }
        }
    }

    if (!out.empty()) {
        term.writeRaw(out);
        term.flush();
    }

    m_forceFullRedraw = false;
}

} // namespace PelcoDTui
