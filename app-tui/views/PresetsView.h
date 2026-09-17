#pragma once

/// @file PresetsView.h
/// @brief Preset positions and touring manager view (Presets 1–32 and automated patrol).

#include "Canvas.h"
#include "PatrolController.h"
#include "PelcoDDevice.h"
#include "Terminal.h"

#include <cstdint>
#include <memory>
#include <string>

namespace PelcoDTui {

/// @class PresetsView
/// @brief View for managing preset positions, 180-degree flip, zero pan, and automated patrol.
class PresetsView {
public:
    PresetsView();
    ~PresetsView() = default;

    void render(Canvas& canvas, int startY, int width, int height);
    bool handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device);

    [[nodiscard]] PelcoD::PatrolController* patrolController() const noexcept
    {
        return m_patrol.get();
    }

private:
    int m_selectedIndex { 0 };
    std::string m_lastAction { "Ready" };
    std::unique_ptr<PelcoD::PatrolController> m_patrol;
};

} // namespace PelcoDTui
