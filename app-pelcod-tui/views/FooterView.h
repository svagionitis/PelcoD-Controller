#pragma once

/// @file FooterView.h
/// @brief Bottom status bar displaying context hotkeys, messages, and warnings.

#include "Canvas.h"

#include <string>

namespace PelcoDTui {

/// @class FooterView
/// @brief Footer component rendering bottom hotkeys and dynamic notifications.
class FooterView {
public:
    FooterView() = default;
    ~FooterView() = default;

    void setStatusMessage(const std::string& message)
    {
        m_statusMessage = message;
    }
    void render(Canvas& canvas, int y, int width, int activeTab);

private:
    std::string m_statusMessage { "Ready" };
};

} // namespace PelcoDTui
