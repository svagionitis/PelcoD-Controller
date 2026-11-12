/// @file TestHeatmapColorMap.cpp
/// @brief Unit tests for HeatmapColorMap Turbo colormap LUT and frame difference computer.

#include <cstddef>
#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "HeatmapColorMap.h"

namespace StabilizerApp {
namespace {

    TEST(TestHeatmapColorMap, VerifyTurboLutZeroIntensity)
    {
        const HeatmapColorMap heatmap;
        const RgbColor color0 = heatmap.mapIntensity(0U);

        // Intensity 0 corresponds to dark canvas background
        EXPECT_EQ(color0.r, 14U);
        EXPECT_EQ(color0.g, 16U);
        EXPECT_EQ(color0.b, 20U);
    }

    TEST(TestHeatmapColorMap, VerifyTurboLutPeakIntensity)
    {
        const HeatmapColorMap heatmap;
        const RgbColor color255 = heatmap.mapIntensity(255U);

        // Peak intensity 255 corresponds to high-alert crimson red
        EXPECT_EQ(color255.r, 255U);
        EXPECT_EQ(color255.g, 23U);
        EXPECT_EQ(color255.b, 68U);
    }

    TEST(TestHeatmapColorMap, VerifyTurboLutMidIntensity)
    {
        const HeatmapColorMap heatmap;
        const RgbColor midColor = heatmap.mapIntensity(128U);

        // Mid intensity corresponds to green/emerald range
        EXPECT_GT(midColor.g, 150U);
    }

    TEST(TestHeatmapColorMap, VerifyDifferenceIdenticalImages)
    {
        const HeatmapColorMap heatmap;
        constexpr std::size_t Width = 64U;
        constexpr std::size_t Height = 64U;
        constexpr std::size_t Pixels = Width * Height;
        constexpr std::size_t ByteCount = Pixels * 3U;

        std::vector<std::uint8_t> raw(ByteCount, 128U);
        std::vector<std::uint8_t> stab(ByteCount, 128U);
        std::vector<std::uint8_t> outDiff(ByteCount, 0U);

        heatmap.computeDifference(raw.data(), stab.data(), outDiff.data(), Pixels, 1.0F);

        const RgbColor bg = heatmap.mapIntensity(0U);
        for (std::size_t i = 0U; i < Pixels; ++i) {
            EXPECT_EQ(outDiff[3U * i], bg.r);
            EXPECT_EQ(outDiff[(3U * i) + 1U], bg.g);
            EXPECT_EQ(outDiff[(3U * i) + 2U], bg.b);
        }
    }

    TEST(TestHeatmapColorMap, VerifyDifferenceMaxInversion)
    {
        const HeatmapColorMap heatmap;
        constexpr std::size_t Pixels = 16U;
        constexpr std::size_t ByteCount = Pixels * 3U;

        std::vector<std::uint8_t> raw(ByteCount, 0U);
        std::vector<std::uint8_t> stab(ByteCount, 255U);
        std::vector<std::uint8_t> outDiff(ByteCount, 0U);

        heatmap.computeDifference(raw.data(), stab.data(), outDiff.data(), Pixels, 1.0F);

        const RgbColor peak = heatmap.mapIntensity(255U);
        for (std::size_t i = 0U; i < Pixels; ++i) {
            EXPECT_EQ(outDiff[3U * i], peak.r);
            EXPECT_EQ(outDiff[(3U * i) + 1U], peak.g);
            EXPECT_EQ(outDiff[(3U * i) + 2U], peak.b);
        }
    }

    TEST(TestHeatmapColorMap, VerifyGainScaling)
    {
        const HeatmapColorMap heatmap;
        constexpr std::size_t Pixels = 1U;
        constexpr std::size_t ByteCount = Pixels * 3U;

        // A subtle 10-level difference
        std::vector<std::uint8_t> raw { 100U, 100U, 100U };
        std::vector<std::uint8_t> stab { 110U, 110U, 110U };
        std::vector<std::uint8_t> outDiffGain1(ByteCount, 0U);
        std::vector<std::uint8_t> outDiffGain5(ByteCount, 0U);

        heatmap.computeDifference(raw.data(), stab.data(), outDiffGain1.data(), Pixels, 1.0F);
        heatmap.computeDifference(raw.data(), stab.data(), outDiffGain5.data(), Pixels, 5.0F);

        // Difference under gain 1 is intensity 10; under gain 5 is intensity 50
        const RgbColor color1 = heatmap.mapIntensity(10U);
        const RgbColor color5 = heatmap.mapIntensity(50U);

        EXPECT_EQ(outDiffGain1[0U], color1.r);
        EXPECT_EQ(outDiffGain1[1U], color1.g);
        EXPECT_EQ(outDiffGain1[2U], color1.b);

        EXPECT_EQ(outDiffGain5[0U], color5.r);
        EXPECT_EQ(outDiffGain5[1U], color5.g);
        EXPECT_EQ(outDiffGain5[2U], color5.b);
    }

    TEST(TestHeatmapColorMap, VerifyNullSafety)
    {
        const HeatmapColorMap heatmap;
        std::vector<std::uint8_t> buf(12U, 0U);

        // Must not crash on null inputs or zero count
        heatmap.computeDifference(nullptr, buf.data(), buf.data(), 4U);
        heatmap.computeDifference(buf.data(), nullptr, buf.data(), 4U);
        heatmap.computeDifference(buf.data(), buf.data(), nullptr, 4U);
        heatmap.computeDifference(buf.data(), buf.data(), buf.data(), 0U);
    }

} // namespace
} // namespace StabilizerApp
