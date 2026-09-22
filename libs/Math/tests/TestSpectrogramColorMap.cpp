/// @file TestSpectrogramColorMap.cpp
/// @brief Unit tests for SpectrogramColorMap: all presets, boundary clamping, mapDb, and ANSI half-block output.

#include "SpectrogramColorMap.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

using namespace Math;

namespace {

// ---------------------------------------------------------------------------

TEST(SpectrogramColorMapTest, RgbColorEquality)
{
    std::cout << "[Test] testRgbColorEquality\n";
    const RgbColor a { 10U, 20U, 30U };
    const RgbColor b { 10U, 20U, 30U };
    const RgbColor c { 11U, 20U, 30U };
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(!(a == c));
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapNormalizedInfernoExtremes)
{
    std::cout << "[Test] testMapNormalizedInfernoExtremes\n";
    // At value=0.0 Inferno should be very dark (nearly black)
    const auto low = SpectrogramColorMap::mapNormalized(0.0, SpectrogramColorMap::Preset::Inferno);
    EXPECT_TRUE(low.r < 10U && low.g < 10U && low.b < 20U);

    // At value=1.0 Inferno should be bright (high RGB)
    const auto high = SpectrogramColorMap::mapNormalized(1.0, SpectrogramColorMap::Preset::Inferno);
    EXPECT_TRUE(high.r > 200U && high.g > 200U);
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapNormalizedViridisExtremes)
{
    std::cout << "[Test] testMapNormalizedViridisExtremes\n";
    const auto low = SpectrogramColorMap::mapNormalized(0.0, SpectrogramColorMap::Preset::Viridis);
    // Viridis low: dark purple (68,1,84)
    EXPECT_TRUE(low.r < 100U && low.b > 50U);

    const auto high = SpectrogramColorMap::mapNormalized(1.0, SpectrogramColorMap::Preset::Viridis);
    // Viridis high: bright yellow (253,231,37)
    EXPECT_TRUE(high.r > 200U && high.g > 200U);
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapNormalizedTacticalGreenExtremes)
{
    std::cout << "[Test] testMapNormalizedTacticalGreenExtremes\n";
    const auto low = SpectrogramColorMap::mapNormalized(0.0, SpectrogramColorMap::Preset::TacticalGreen);
    // Should be black
    EXPECT_TRUE(low.r == 0U && low.g == 0U && low.b == 0U);

    const auto high = SpectrogramColorMap::mapNormalized(1.0, SpectrogramColorMap::Preset::TacticalGreen);
    // Should be neon mint-ish (bright green dominant)
    EXPECT_TRUE(high.g > 200U);
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapNormalizedJetExtremes)
{
    std::cout << "[Test] testMapNormalizedJetExtremes\n";
    const auto low = SpectrogramColorMap::mapNormalized(0.0, SpectrogramColorMap::Preset::Jet);
    // Jet low: blue (0,0,140)
    EXPECT_TRUE(low.r == 0U && low.b > 100U);

    const auto high = SpectrogramColorMap::mapNormalized(1.0, SpectrogramColorMap::Preset::Jet);
    // Jet high: red (255,0,0)
    EXPECT_TRUE(high.r > 200U && high.g < 50U && high.b < 50U);
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapNormalizedClampingBelowZero)
{
    std::cout << "[Test] testMapNormalizedClampingBelowZero\n";
    const auto at0 = SpectrogramColorMap::mapNormalized(0.0);
    const auto below = SpectrogramColorMap::mapNormalized(-5.0);
    EXPECT_TRUE(at0 == below); // Out-of-range should clamp to minimum stop
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapNormalizedClampingAboveOne)
{
    std::cout << "[Test] testMapNormalizedClampingAboveOne\n";
    const auto at1 = SpectrogramColorMap::mapNormalized(1.0);
    const auto above = SpectrogramColorMap::mapNormalized(2.0);
    EXPECT_TRUE(at1 == above); // Out-of-range should clamp to maximum stop
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapNormalizedMidpointInterpolated)
{
    std::cout << "[Test] testMapNormalizedMidpointInterpolated\n";
    const auto lo = SpectrogramColorMap::mapNormalized(0.0);
    const auto mid = SpectrogramColorMap::mapNormalized(0.5);
    const auto hi = SpectrogramColorMap::mapNormalized(1.0);
    // Mid-point must be distinct from both extremes
    EXPECT_TRUE(!(mid == lo));
    EXPECT_TRUE(!(mid == hi));
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapDbFullRange)
{
    std::cout << "[Test] testMapDbFullRange\n";
    // mapDb(-60, -60, 0) → 0.0 normalized → same as mapNormalized(0.0)
    const auto minColor = SpectrogramColorMap::mapDb(-60.0, -60.0, 0.0);
    const auto expected0 = SpectrogramColorMap::mapNormalized(0.0);
    EXPECT_TRUE(minColor == expected0);

    // mapDb(0, -60, 0) → 1.0 normalized
    const auto maxColor = SpectrogramColorMap::mapDb(0.0, -60.0, 0.0);
    const auto expected1 = SpectrogramColorMap::mapNormalized(1.0);
    EXPECT_TRUE(maxColor == expected1);
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapDbClampBelowMin)
{
    std::cout << "[Test] testMapDbClampBelowMin\n";
    const auto atMin = SpectrogramColorMap::mapDb(-60.0, -60.0, 0.0);
    const auto belowMin = SpectrogramColorMap::mapDb(-120.0, -60.0, 0.0);
    EXPECT_TRUE(atMin == belowMin);
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapDbClampAboveMax)
{
    std::cout << "[Test] testMapDbClampAboveMax\n";
    const auto atMax = SpectrogramColorMap::mapDb(0.0, -60.0, 0.0);
    const auto aboveMax = SpectrogramColorMap::mapDb(10.0, -60.0, 0.0);
    EXPECT_TRUE(atMax == aboveMax);
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapDbDegenerateSpan)
{
    std::cout << "[Test] testMapDbDegenerateSpan\n";
    // minDb == maxDb → degenerate span → should return mapNormalized(0.0) without crash
    const auto c = SpectrogramColorMap::mapDb(-30.0, -30.0, -30.0);
    const auto expected = SpectrogramColorMap::mapNormalized(0.0);
    EXPECT_TRUE(c == expected);
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapHalfBlockAnsiContainsEscapes)
{
    std::cout << "[Test] testMapHalfBlockAnsiContainsEscapes\n";
    const auto s = SpectrogramColorMap::mapHalfBlockAnsi(0.5, 0.2);
    // Must contain ANSI ESC character and half-block glyph bytes
    EXPECT_TRUE(!s.empty());
    EXPECT_TRUE(s.find('\x1b') != std::string::npos);
    // Must contain upper half-block UTF-8 glyph 0xE2 0x96 0x80
    const std::string halfBlock = "\xE2\x96\x80";
    EXPECT_TRUE(s.find(halfBlock) != std::string::npos);
    // Must contain reset sequence
    EXPECT_TRUE(s.find("\x1b[0m") != std::string::npos);
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapHalfBlockAnsiAllPresets)
{
    std::cout << "[Test] testMapHalfBlockAnsiAllPresets\n";
    using P = SpectrogramColorMap::Preset;
    for (const auto preset : { P::Inferno, P::Viridis, P::TacticalGreen, P::Jet }) {
        const auto s = SpectrogramColorMap::mapHalfBlockAnsi(0.3, 0.7, preset);
        EXPECT_TRUE(!s.empty());
        EXPECT_TRUE(s.find('\x1b') != std::string::npos);
    }
    std::cout << "  -> PASSED\n";
}

TEST(SpectrogramColorMapTest, MapHalfBlockAnsiExtremes)
{
    std::cout << "[Test] testMapHalfBlockAnsiExtremes\n";
    // Both extremes (0.0 and 1.0) must produce non-empty valid strings
    const auto s0 = SpectrogramColorMap::mapHalfBlockAnsi(0.0, 0.0);
    const auto s1 = SpectrogramColorMap::mapHalfBlockAnsi(1.0, 1.0);
    EXPECT_TRUE(!s0.empty());
    EXPECT_TRUE(!s1.empty());
    // Two different intensities should produce different strings
    EXPECT_TRUE(s0 != s1);
    std::cout << "  -> PASSED\n";
}

} // namespace
