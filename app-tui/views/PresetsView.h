#pragma once

/// @file PresetsView.h
/// @brief Preset positions and touring manager view (Presets 1–32).

#include "Canvas.h"
#include "PelcoDDevice.h"
#include "Terminal.h"

#include <cstdint>
#include <string>

namespace PelcoDTui {

/// @class PresetsView
/// @brief View for managing preset positions, 180-degree flip, and zero pan.
class PresetsView {
public:
    PresetsView() = default;
    ~PresetsView() = default;

    void render(Canvas& canvas, int startY, int width, int height);
    bool handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device);

private:
    int m_selectedIndex { 0 };
    std::string m_lastAction { "Ready" };
};

} // namespace PelcoDTui
