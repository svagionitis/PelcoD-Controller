#pragma once

/// @file HeaderView.h
/// @brief Top header rendering application branding, connection badges, and tab selector.

#include "Canvas.h"
#include "Terminal.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PelcoDTui {

/// @class HeaderView
/// @brief Header component rendering tabs, connection status, and device metadata.
class HeaderView {
public:
    HeaderView() = default;
    ~HeaderView() = default;

    void render(Canvas& canvas, int width, int activeTab, bool connected, const std::string& transportName,
        std::uint8_t address);

    /// @brief Handle mouse click on header to switch tabs.
    /// @return Clicked tab index (0-7) or -1 if no tab was clicked.
    [[nodiscard]] int handleMouseClick(int x, int y) const noexcept;

private:
    struct TabArea {
        int startX { 0 };
        int endX { 0 };
        int tabIndex { 0 };
    };
    mutable std::vector<TabArea> m_tabHitboxes;
};

} // namespace PelcoDTui
