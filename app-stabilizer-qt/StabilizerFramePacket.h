#pragma once

/// @file StabilizerFramePacket.h
/// @brief Pre-allocated frame payload for lock-free triple buffer exchange.

#include <cstddef>
#include <cstdint>

#include <array>
#include <vector>

#include "StabilizerTypes.h"
#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

/// @struct VisualFeaturePoint
/// @brief Sub-pixel feature point coordinate with displacement and classification for UI rendering.
struct VisualFeaturePoint {
    float x { 0.0F }; ///< Feature X coordinate (current frame)
    float y { 0.0F }; ///< Feature Y coordinate (current frame)
    float vx { 0.0F }; ///< Inter-frame motion vector dx
    float vy { 0.0F }; ///< Inter-frame motion vector dy
    bool isInlier { false }; ///< Consensus inlier classification
};

/// @struct StabilizerFramePacket
/// @brief Memory slot exchanged through AtomicTripleBuffer between worker and UI.
struct StabilizerFramePacket {
    static constexpr std::size_t MaxVisualFeatures { 500U };

    std::vector<std::uint8_t> rawRgb; ///< Raw RGB24 frame buffer (width * height * 3)
    std::vector<std::uint8_t> stabilizedRgb; ///< Stabilized RGB24 frame buffer (width * height * 3)
    std::uint32_t width { 0U }; ///< Frame width in pixels
    std::uint32_t height { 0U }; ///< Frame height in pixels
    std::uint32_t stride { 0U }; ///< Row stride in bytes (width * 3)
    double timestampSec { 0.0 }; ///< Presentation timestamp (PTS) in seconds
    std::uint64_t frameNumber { 0U }; ///< Monotonically increasing frame index

    Stabilizer::StabilizerDiagnostics diagnostics { }; ///< Real-time pipeline diagnostics snapshot

    std::array<VisualFeaturePoint, MaxVisualFeatures> features { }; ///< Feature landmark coordinates
    std::size_t featureCount { 0U }; ///< Valid features in current frame

    /// @brief Pre-allocates buffer capacities to prevent heap allocation during streaming.
    /// @param w Target frame width in pixels.
    /// @param h Target frame height in pixels.
    void preallocate(std::uint32_t w, std::uint32_t h)
    {
        width = w;
        height = h;
        stride = w * 3U;
        const std::size_t byteCount = static_cast<std::size_t>(stride) * static_cast<std::size_t>(h);
        if (rawRgb.size() != byteCount) {
            rawRgb.assign(byteCount, 0U);
        }
        if (stabilizedRgb.size() != byteCount) {
            stabilizedRgb.assign(byteCount, 0U);
        }
        featureCount = 0U;
    }
};

} // namespace StabilizerApp
