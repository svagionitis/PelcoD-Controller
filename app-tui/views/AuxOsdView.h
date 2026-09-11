#pragma once

/// @file AuxOsdView.h
/// @brief Auxiliaries 1–8 switches, Zone scan, and OSD screen keypad view.

#include "Canvas.h"
#include "PelcoDDevice.h"
#include "Terminal.h"

#include <array>
#include <cstdint>
#include <string>

namespace PelcoDTui {

/// @class AuxOsdView
/// @brief View for managing relay aux toggles, zone scans, and OSD menu operations.
class AuxOsdView {
public:
    AuxOsdView() = default;
    ~AuxOsdView() = default;

    void render(Canvas& canvas, int startY, int width, int height);
    bool handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device);

private:
    int m_selectedAux { 0 };
    std::array<bool, 8> m_auxStates { false, false, false, false, false, false, false, false };
    bool m_zoneScanActive { false };
    std::string m_lastAction { "Ready" };
};

} // namespace PelcoDTui
