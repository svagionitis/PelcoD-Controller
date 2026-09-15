#pragma once

/// @file DiagnosticsView.h
/// @brief Device telemetry queries, temperature sensor, and ACK/NAK diagnostic inspection.

#include "Canvas.h"
#include "PelcoDDevice.h"
#include "RttProfiler.h"
#include "Terminal.h"

#include <cstdint>
#include <string>

namespace PelcoDTui {

/// @class DiagnosticsView
/// @brief View for triggering queries, viewing real-time telemetry, and inspecting ACK responses.
class DiagnosticsView {
public:
    DiagnosticsView();
    ~DiagnosticsView() = default;

    void render(Canvas& canvas, int startY, int width, int height, const PelcoD::DeviceStatus& status,
        const PelcoD::DeviceInfo& info);
    bool handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device);

private:
    void ensureProfilerConnected(PelcoD::PelcoDDevice& device);

    std::string m_lastAction { "Ready" };
    PelcoD::RttProfiler m_profiler;
    PelcoD::ScopedConnection m_latencyConn;
    bool m_profilerConnected { false };
};

} // namespace PelcoDTui
