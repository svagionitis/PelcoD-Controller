#pragma once

/// @file SettingsView.h
/// @brief Device optical settings, white balance, gain, shutter, and baud rate view.

#include "Canvas.h"
#include "PelcoDDevice.h"
#include "Terminal.h"

#include <cstdint>
#include <string>

namespace PelcoDTui {

/// @class SettingsView
/// @brief View allowing interactive configuration of camera hardware options.
class SettingsView {
public:
    SettingsView() = default;
    ~SettingsView() = default;

    void render(Canvas& canvas, int startY, int width, int height);
    bool handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device);

private:
    int m_selectedIndex { 0 };

    bool m_autoFocus { false };
    bool m_autoIris { false };
    bool m_agc { false };
    bool m_blc { false };
    bool m_awb { false };
    std::uint16_t m_lineLockDelay { 0U };
    std::uint16_t m_wbRB { 128U };
    std::uint16_t m_wbMG { 128U };
    int m_baudIndex { 2 }; // default 9600

    std::string m_lastAction { "Ready" };
};

} // namespace PelcoDTui
