/// @file HeatmapColorMap.cpp
/// @brief Zero-heap Turbo colormap LUT and frame difference computer.
#include "HeatmapColorMap.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <cmath>

namespace StabilizerApp {

namespace {

    struct ControlPoint {
        float pos;
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
    };

    // Tactical Turbo Palette control anchors
    constexpr std::array<ControlPoint, 9U> Anchors { {
        { 0.000F, 14U, 16U, 20U }, // Dark canvas background
        { 0.125F, 50U, 70U, 180U }, // Deep blue
        { 0.250F, 33U, 150U, 240U }, // Cyan-blue
        { 0.375F, 0U, 229U, 255U }, // Vivid cyan
        { 0.500F, 0U, 230U, 118U }, // Tactical emerald green
        { 0.625F, 180U, 230U, 40U }, // Yellow-green
        { 0.750F, 255U, 180U, 0U }, // Amber
        { 0.875F, 255U, 80U, 0U }, // Bright orange
        { 1.000F, 255U, 23U, 68U } // High-alert crimson red
    } };

} // namespace

HeatmapColorMap::HeatmapColorMap() noexcept
{
    initializeTurboLut();
}

void HeatmapColorMap::initializeTurboLut() noexcept
{
    for (std::size_t i = 0U; i < 256U; ++i) {
        const float t = static_cast<float>(i) / 255.0F;

        std::size_t seg = 0U;
        while ((seg < Anchors.size() - 2U) && (t > Anchors[seg + 1U].pos)) {
            ++seg;
        }

        const auto& c0 = Anchors[seg];
        const auto& c1 = Anchors[seg + 1U];
        const float range = c1.pos - c0.pos;
        const float frac = (range > 1e-5F) ? std::clamp((t - c0.pos) / range, 0.0F, 1.0F) : 0.0F;

        const auto r = static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(c0.r) + (frac * (static_cast<float>(c1.r) - static_cast<float>(c0.r))), 0.0F, 255.0F));
        const auto g = static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(c0.g) + (frac * (static_cast<float>(c1.g) - static_cast<float>(c0.g))), 0.0F, 255.0F));
        const auto b = static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(c0.b) + (frac * (static_cast<float>(c1.b) - static_cast<float>(c0.b))), 0.0F, 255.0F));

        lut_[i] = RgbColor { r, g, b };
    }
}

void HeatmapColorMap::computeDifference(const std::uint8_t* raw, const std::uint8_t* stab, std::uint8_t* outDiff,
    std::size_t pixelCount, float gain) const noexcept
{
    if ((raw == nullptr) || (stab == nullptr) || (outDiff == nullptr) || (pixelCount == 0U)) {
        return;
    }

    const float effectiveGain = std::clamp(gain, 1.0F, 10.0F);

    for (std::size_t i = 0U; i < pixelCount; ++i) {
        const std::size_t offset = i * 3U;

        const auto r0 = static_cast<int>(raw[offset]);
        const auto g0 = static_cast<int>(raw[offset + 1U]);
        const auto b0 = static_cast<int>(raw[offset + 2U]);

        const auto r1 = static_cast<int>(stab[offset]);
        const auto g1 = static_cast<int>(stab[offset + 1U]);
        const auto b1 = static_cast<int>(stab[offset + 2U]);

        const int dr = std::abs(r1 - r0);
        const int dg = std::abs(g1 - g0);
        const int db = std::abs(b1 - b0);

        const float meanDiff = static_cast<float>(dr + dg + db) / 3.0F;
        const float amplified = meanDiff * effectiveGain;
        const auto intensity = static_cast<std::uint8_t>(std::clamp(amplified, 0.0F, 255.0F));

        const RgbColor color = lut_[intensity];
        outDiff[offset] = color.r;
        outDiff[offset + 1U] = color.g;
        outDiff[offset + 2U] = color.b;
    }
}

} // namespace StabilizerApp