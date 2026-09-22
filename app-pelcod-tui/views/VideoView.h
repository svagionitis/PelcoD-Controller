#pragma once

/// @file VideoView.h
/// @brief Terminal video viewport featuring Braille rasterization, dithering, and tactical HUD overlay.

#include "BrailleRenderer.h"
#include "Canvas.h"
#include "DecoderTypes.h"
#include "PelcoDDevice.h"
#include "Terminal.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#if defined(PELCOD_HAS_FILTERS)
#include "VideoFilters.h"
#endif

namespace PelcoDTui {

using namespace Video;

/// @class VideoView
/// @brief Interactive TUI view rendering live video frames in Braille with tactical PTZ telemetry.
class VideoView {
public:
    VideoView();
    ~VideoView() = default;

    /// @brief Render video frame and tactical telemetry overlay onto terminal canvas.
    void render(Canvas& canvas, int startY, int width, int height, const PelcoD::DeviceStatus& status);

    /// @brief Process keyboard input events when the Video tab is active.
    /// @return true if event was consumed.
    bool handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device);

    /// @brief Feed a freshly decoded video frame. Thread-safe.
    void updateFrame(const std::uint8_t* data, int width, int height, double timestamp, double decodeMs);

    /// @brief Update stream status metadata. Thread-safe.
    void setStreamInfo(
        videodecoder::StreamState state, const std::string& source, const std::string& backend, double fps = 0.0);

    [[nodiscard]] videodecoder::BrailleRenderOptions& getOptions() noexcept
    {
        return m_options;
    }
    [[nodiscard]] const videodecoder::BrailleRenderOptions& getOptions() const noexcept
    {
        return m_options;
    }

    void cycleRenderMode() noexcept;
    void cycleDither() noexcept;
    void cyclePalette() noexcept;
#if defined(PELCOD_HAS_FILTERS)
    void cycleTacticalFilter() noexcept;
    [[nodiscard]] std::string tacticalFilterName() const;
#endif

    [[nodiscard]] bool isPaused() const noexcept
    {
        return m_paused;
    }
    void setPaused(bool paused) noexcept
    {
        m_paused = paused;
    }

    [[nodiscard]] bool isLoop() const noexcept
    {
        return m_loop;
    }
    void setLoop(bool loop) noexcept
    {
        m_loop = loop;
    }

private:
    void renderHudBar(Canvas& canvas, int y, int width, const PelcoD::DeviceStatus& status);
    void renderControlsBar(Canvas& canvas, int y, int width);

    videodecoder::BrailleRenderOptions m_options {};
    bool m_paused { false };
    bool m_loop { true };
    std::uint8_t m_panSpeed { 32U };
    std::uint8_t m_tiltSpeed { 32U };

    // Thread-safe frame caching
    mutable std::mutex m_frameMutex {};
    std::vector<std::uint8_t> m_frameBuffer {};
    int m_frameWidth { 0 };
    int m_frameHeight { 0 };
    double m_framePts { 0.0 };
    double m_decodeMs { 0.0 };
    double m_fps { 0.0 };

    videodecoder::StreamState m_streamState { videodecoder::StreamState::Disconnected };
    std::string m_sourceName { "mock:smpte" };
    std::string m_backendName { "Mock" };

    // Raster cache
    std::vector<videodecoder::TerminalPixelCell> m_renderedCells {};

#if defined(PELCOD_HAS_FILTERS)
    int m_tacticalPaletteIndex { 0 };
    std::unique_ptr<Video::FalseColorFilter> m_falseColorFilter {};
#endif
};

} // namespace PelcoDTui
