#pragma once

/// @file PtzView.h
/// @brief Interactive PTZ motion dashboard with compass rose and precision optic meters.

#include "Canvas.h"
#include "PelcoDDevice.h"
#include "Terminal.h"

#include <cstdint>
#include <string>

namespace PelcoDTui {

/// @class PtzView
/// @brief View rendering directional PTZ crosshair, motion bars, and optics telemetry.
class PtzView {
public:
    PtzView() = default;
    ~PtzView() = default;

    void render(Canvas& canvas, int startY, int width, int height, const PelcoD::DeviceStatus& status);
    bool handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device);

    [[nodiscard]] std::uint8_t getPanSpeed() const noexcept
    {
        return m_panSpeed;
    }
    [[nodiscard]] std::uint8_t getTiltSpeed() const noexcept
    {
        return m_tiltSpeed;
    }

private:
    std::uint8_t m_panSpeed { 32U };
    std::uint8_t m_tiltSpeed { 32U };
    std::string m_lastAction { "Idle" };
};

} // namespace PelcoDTui
