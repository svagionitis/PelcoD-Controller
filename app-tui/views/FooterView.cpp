#include "FooterView.h"

#include "UtfSymbols.h"

namespace PelcoDTui {

void FooterView::render(Canvas& canvas, int y, int width, [[maybe_unused]] int activeTab)
{
    const Style bgStyle { Colors::White, Colors::HeaderBg, false, false, false, false, false };
    const Style borderStyle { Colors::DarkGray, Colors::HeaderBg, false, false, false, false, false };
    const Style keyStyle { Colors::Black, Colors::Cyan, true, false, false, false, false };
    const Style labelStyle { Colors::Gray, Colors::HeaderBg, false, false, false, false, false };
    const Style msgStyle { Colors::Yellow, Colors::HeaderBg, true, false, false, false, false };

    // Divider line
    canvas.drawHLine(0, y - 1, width, Symbols::BoxHoriz, borderStyle);

    // Background
    for (int x = 0; x < width; ++x) {
        canvas.setCell(x, y, " ", bgStyle);
    }

    // Hotkey items
    int curX = 2;
    auto drawHotkey = [&](const std::string& key, const std::string& label) {
        canvas.drawString(curX, y, " " + key + " ", keyStyle);
        curX += static_cast<int>(key.size()) + 2;
        canvas.drawString(curX, y, " " + label + " ", labelStyle);
        curX += static_cast<int>(label.size()) + 3;
    };

    drawHotkey("1-6", "Tabs");
    drawHotkey("C", "Connect");
    drawHotkey("Space", "Stop");
    drawHotkey("Q", "Quit");

    // Message banner on the right
    if (!m_statusMessage.empty()) {
        const int msgX = width - static_cast<int>(m_statusMessage.size()) - 4;
        if (msgX > curX + 2) {
            canvas.drawString(msgX, y, m_statusMessage, msgStyle);
        }
    }
}

} // namespace PelcoDTui
