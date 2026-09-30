#pragma once

/// @file DiagnosticsView.h
/// @brief Device telemetry queries, temperature sensor, and ACK/NAK diagnostic inspection.

#include "Canvas.h"
#include "PelcoDDevice.h"
#include "PelcoDStats.h"
#include "PlantIdentifier.h"
#include "RttProfiler.h"
#include "SpectrogramColorMap.h"
#include "Stft.h"
#include "Terminal.h"
#include "TransportStats.h"

#include <cstdint>
#include <string>

namespace PelcoDTui {

/// @class DiagnosticsView
/// @brief View for triggering queries, viewing real-time telemetry, and inspecting ACK responses.
class DiagnosticsView {
public:
    DiagnosticsView();
    ~DiagnosticsView() = default;

    /// @brief Renders the diagnostics view, including device status and kernel transport telemetry.
    /// @param[in,out] canvas Target character grid for drawing.
    /// @param[in] startY Row offset where view rendering begins.
    /// @param[in] width Total width of available rendering viewport.
    /// @param[in] height Total height of available rendering viewport.
    /// @param[in] status Device telemetry and query states.
    /// @param[in] info Device hardware and protocol information.
    /// @param[in] stats Real-time transport and kernel-level metrics.
    /// @param[in] protoStats Real-time Pelco-D protocol telemetry snapshot.
    void render(Canvas& canvas, int startY, int width, int height, const PelcoD::DeviceStatus& status,
        const PelcoD::DeviceInfo& info, const Transport::TransportStatsSnapshot& stats = {},
        const PelcoD::PelcoDProtocolStats& protoStats = {});

    /// @brief Handles keyboard input events for diagnostics operations.
    /// @param[in] event Input key event.
    /// @param[in,out] device Pelco-D device instance.
    /// @return True if event was handled; false otherwise.
    bool handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device);

private:
    void ensureProfilerConnected(PelcoD::PelcoDDevice& device);
    void renderWaterfall(Canvas& canvas, int startX, int startY, int width, int height);

    std::string m_lastAction { "Ready" };
    PelcoD::RttProfiler m_profiler;
    PelcoD::ScopedConnection m_latencyConn;
    bool m_profilerConnected { false };

    Math::Stft m_stft {};
    bool m_showWaterfall { false };

    Tracking::PlantIdentifier m_plantIdentifier {};
    Tracking::PlantIdentificationResult m_plantResult {};
};

} // namespace PelcoDTui
