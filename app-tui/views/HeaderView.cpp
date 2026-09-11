#include "HeaderView.h"

#include "UtfSymbols.h"

#include <iomanip>
#include <sstream>

namespace PelcoDTui {

static const char* const kTabs[]
    = { "[1] PTZ Motion", "[2] Presets & Tours", "[3] Settings", "[4] Aux & OSD", "[5] Diagnostics", "[6] Traffic" };

void HeaderView::render(
    Canvas& canvas, int width, int activeTab, bool connected, const std::string& transportName, std::uint8_t address)
{
    m_tabHitboxes.clear();

    const Style bgStyle { Colors::White, Colors::HeaderBg, false, false, false, false, false };
    const Style titleStyle { Colors::Cyan, Colors::HeaderBg, true, false, false, false, false };
    const Style borderStyle { Colors::DarkGray, Colors::HeaderBg, false, false, false, false, false };

    // Row 0: Background bar
    for (int x = 0; x < width; ++x) {
        canvas.setCell(x, 0, " ", bgStyle);
    }

    // Title
    canvas.drawString(2, 0, "PELCO-D CONTROLLER", titleStyle);

    // Address
    std::ostringstream addrOss;
    addrOss << "ID: " << static_cast<int>(address);
    const Style addrStyle { Colors::Yellow, Colors::HeaderBg, true, false, false, false, false };
    canvas.drawString(24, 0, addrOss.str(), addrStyle);

    // Connection badge
    std::string connBadge;
    Style connStyle {};
    if (connected) {
        connBadge = std::string(Symbols::CircleFilled) + " CONNECTED [" + transportName + "]";
        connStyle = Style { Colors::Green, Colors::HeaderBg, true, false, false, false, false };
    } else {
        connBadge = std::string(Symbols::CircleFilled) + " DISCONNECTED";
        connStyle = Style { Colors::Red, Colors::HeaderBg, true, false, false, false, false };
    }

    const int badgeX = width - static_cast<int>(connBadge.size()) - 2;
    if (badgeX > 32) {
        canvas.drawString(badgeX, 0, connBadge, connStyle);
    }

    // Row 1: Divider line
    canvas.drawHLine(0, 1, width, Symbols::BoxHoriz, borderStyle);

    // Row 2: Tabs bar
    for (int x = 0; x < width; ++x) {
        canvas.setCell(x, 2, " ", bgStyle);
    }

    int curX = 2;
    for (int i = 0; i < 6; ++i) {
        const std::string tabText = std::string(" ") + kTabs[i] + " ";
        const int tabLen = static_cast<int>(tabText.size());

        Style tabStyle {};
        if (i == activeTab) {
            tabStyle = Style { Colors::Black, Colors::Cyan, true, false, false, false, false };
        } else {
            tabStyle = Style { Colors::White, Colors::PanelBg, false, false, false, false, false };
        }

        canvas.drawString(curX, 2, tabText, tabStyle);
        m_tabHitboxes.push_back({ curX, curX + tabLen, i });
        curX += tabLen + 2;
    }

    // Row 3: Bottom border
    canvas.drawHLine(0, 3, width, Symbols::HeavyHoriz, borderStyle);
}

int HeaderView::handleMouseClick(int x, int y) const noexcept
{
    if (y != 2) {
        return -1;
    }
    for (const auto& tab : m_tabHitboxes) {
        if (x >= tab.startX && x < tab.endX) {
            return tab.tabIndex;
        }
    }
    return -1;
}

} // namespace PelcoDTui
