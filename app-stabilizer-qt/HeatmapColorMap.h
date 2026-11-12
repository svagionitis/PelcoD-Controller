#pragma once

/// @file HeatmapColorMap.h
/// @brief Zero-heap Turbo colormap LUT and frame difference computer.

#include <cstddef>
#include <cstdint>

#include <array>

namespace StabilizerApp {

/// @struct RgbColor
/// @brief 24-bit RGB color representation.
struct RgbColor {
    std::uint8_t r { 0U };
    std::uint8_t g { 0U };
    std::uint8_t b { 0U };
};

/// @class HeatmapColorMap
/// @brief Precomputed 256-entry Turbo colormap lookup table.
class HeatmapColorMap {
public:
    /// @brief Initialize static colormap LUT.
    HeatmapColorMap() noexcept;

    /// @brief Retrieve RGB color for an 8-bit difference intensity [0 .. 255].
    /// @param intensity Normalized difference magnitude.
    /// @return 24-bit RGB color from Turbo LUT.
    [[nodiscard]] constexpr RgbColor mapIntensity(std::uint8_t intensity) const noexcept
    {
        return lut_[intensity];
    }

    /// @brief Compute pixel-wise difference between raw and stabilized RGB24 frames.
    /// @param raw Input raw RGB24 buffer.
    /// @param stab Input stabilized RGB24 buffer.
    /// @param outDiff Destination RGB24 buffer for colorized heatmap pixels.
    /// @param pixelCount Number of pixels to process (width * height).
    /// @param gain Contrast magnification multiplier (1.0 to 10.0).
    void computeDifference(const std::uint8_t* raw, const std::uint8_t* stab, std::uint8_t* outDiff,
        std::size_t pixelCount, float gain = 3.0F) const noexcept;

private:
    void initializeTurboLut() noexcept;

    std::array<RgbColor, 256U> lut_ { };
};

} // namespace StabilizerApp
