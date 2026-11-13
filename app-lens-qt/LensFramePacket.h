#pragma once

/// @file LensFramePacket.h
/// @brief Pre-allocated frame payload for lock-free triple buffer exchange.

#include <cstddef>
#include <cstdint>

#include <vector>

#include "LensTypes.h"

namespace LensApp {

/// @struct LensFramePacket
/// @brief Memory slot exchanged through AtomicTripleBuffer between worker and UI.
struct LensFramePacket {
    std::vector<std::uint8_t> rgbBuffer { }; ///< Packed RGB24 buffer (width * height * 3)
    std::vector<std::uint8_t> lumaBuffer { }; ///< Grayscale Y plane (width * height)
    std::uint32_t width { 0U };
    std::uint32_t height { 0U };
    std::uint32_t stride { 0U }; ///< Row byte stride (width * 3)
    double timestampSec { 0.0 }; ///< Presentation timestamp (PTS)
    std::uint64_t frameId { 0U }; ///< Monotonic frame sequence counter

    // Optical Telemetry Snapshot accompanying this specific video frame
    LensControl::LensStatus status { };
    LensControl::FocusMetricScore metricScore { };
    LensControl::NormalizedRoi activeRoi { };
    bool isSynthetic { false };
    float currentBlurSigma { 0.0F };

    /// @brief Pre-allocates buffer capacities to prevent heap allocations during streaming.
    /// @param w Target frame width in pixels.
    /// @param h Target frame height in pixels.
    void preallocate(std::uint32_t w, std::uint32_t h)
    {
        width = w;
        height = h;
        stride = w * 3U;
        const std::size_t rgbBytes = static_cast<std::size_t>(stride) * static_cast<std::size_t>(h);
        const std::size_t lumaBytes = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);

        if (rgbBuffer.size() != rgbBytes) {
            rgbBuffer.assign(rgbBytes, 0U);
        }
        if (lumaBuffer.size() != lumaBytes) {
            lumaBuffer.assign(lumaBytes, 0U);
        }
    }
};

} // namespace LensApp
