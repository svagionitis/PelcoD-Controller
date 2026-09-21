/// @file TestSpectrogramColorMap.cpp
/// @brief Unit tests for SpectrogramColorMap: all presets, boundary clamping, mapDb, and ANSI half-block output.

#include "SpectrogramColorMap.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

using namespace PelcoD;

namespace {

// ---------------------------------------------------------------------------

void testRgbColorEquality()
{
    std::cout << "[Test] testRgbColorEquality\n";
    const RgbColor a { 10U, 20U, 30U };
    const RgbColor b { 10U, 20U, 30U };
    const RgbColor c { 11U, 20U, 30U };
    assert(a == b);
    assert(!(a == c));
    std::cout << "  -> PASSED\n";
}

void testMapNormalizedInfernoExtremes()
{
    std::cout << "[Test] testMapNormalizedInfernoExtremes\n";
    // At value=0.0 Inferno should be very dark (nearly black)
    const auto low = SpectrogramColorMap::mapNormalized(0.0, SpectrogramColorMap::Preset::Inferno);
    assert(low.r < 10U && low.g < 10U && low.b < 20U);

    // At value=1.0 Inferno should be bright (high RGB)
    const auto high = SpectrogramColorMap::mapNormalized(1.0, SpectrogramColorMap::Preset::Inferno);
    assert(high.r > 200U && high.g > 200U);
    std::cout << "  -> PASSED\n";
}

void testMapNormalizedViridisExtremes()
{
    std::cout << "[Test] testMapNormalizedViridisExtremes\n";
    const auto low = SpectrogramColorMap::mapNormalized(0.0, SpectrogramColorMap::Preset::Viridis);
    // Viridis low: dark purple (68,1,84)
    assert(low.r < 100U && low.b > 50U);

    const auto high = SpectrogramColorMap::mapNormalized(1.0, SpectrogramColorMap::Preset::Viridis);
    // Viridis high: bright yellow (253,231,37)
    assert(high.r > 200U && high.g > 200U);
    std::cout << "  -> PASSED\n";
}

void testMapNormalizedTacticalGreenExtremes()
{
    std::cout << "[Test] testMapNormalizedTacticalGreenExtremes\n";
    const auto low = SpectrogramColorMap::mapNormalized(0.0, SpectrogramColorMap::Preset::TacticalGreen);
    // Should be black
    assert(low.r == 0U && low.g == 0U && low.b == 0U);

    const auto high = SpectrogramColorMap::mapNormalized(1.0, SpectrogramColorMap::Preset::TacticalGreen);
    // Should be neon mint-ish (bright green dominant)
    assert(high.g > 200U);
    std::cout << "  -> PASSED\n";
}

void testMapNormalizedJetExtremes()
{
    std::cout << "[Test] testMapNormalizedJetExtremes\n";
    const auto low = SpectrogramColorMap::mapNormalized(0.0, SpectrogramColorMap::Preset::Jet);
    // Jet low: blue (0,0,140)
    assert(low.r == 0U && low.b > 100U);

    const auto high = SpectrogramColorMap::mapNormalized(1.0, SpectrogramColorMap::Preset::Jet);
    // Jet high: red (255,0,0)
    assert(high.r > 200U && high.g < 50U && high.b < 50U);
    std::cout << "  -> PASSED\n";
}

void testMapNormalizedClampingBelowZero()
{
    std::cout << "[Test] testMapNormalizedClampingBelowZero\n";
    const auto at0 = SpectrogramColorMap::mapNormalized(0.0);
    const auto below = SpectrogramColorMap::mapNormalized(-5.0);
    assert(at0 == below); // Out-of-range should clamp to minimum stop
    std::cout << "  -> PASSED\n";
}

void testMapNormalizedClampingAboveOne()
{
    std::cout << "[Test] testMapNormalizedClampingAboveOne\n";
    const auto at1 = SpectrogramColorMap::mapNormalized(1.0);
    const auto above = SpectrogramColorMap::mapNormalized(2.0);
    assert(at1 == above); // Out-of-range should clamp to maximum stop
    std::cout << "  -> PASSED\n";
}

void testMapNormalizedMidpointInterpolated()
{
    std::cout << "[Test] testMapNormalizedMidpointInterpolated\n";
    const auto lo = SpectrogramColorMap::mapNormalized(0.0);
    const auto mid = SpectrogramColorMap::mapNormalized(0.5);
    const auto hi = SpectrogramColorMap::mapNormalized(1.0);
    // Mid-point must be distinct from both extremes
    assert(!(mid == lo));
    assert(!(mid == hi));
    std::cout << "  -> PASSED\n";
}

void testMapDbFullRange()
{
    std::cout << "[Test] testMapDbFullRange\n";
    // mapDb(-60, -60, 0) → 0.0 normalized → same as mapNormalized(0.0)
    const auto minColor = SpectrogramColorMap::mapDb(-60.0, -60.0, 0.0);
    const auto expected0 = SpectrogramColorMap::mapNormalized(0.0);
    assert(minColor == expected0);

    // mapDb(0, -60, 0) → 1.0 normalized
    const auto maxColor = SpectrogramColorMap::mapDb(0.0, -60.0, 0.0);
    const auto expected1 = SpectrogramColorMap::mapNormalized(1.0);
    assert(maxColor == expected1);
    std::cout << "  -> PASSED\n";
}

void testMapDbClampBelowMin()
{
    std::cout << "[Test] testMapDbClampBelowMin\n";
    const auto atMin = SpectrogramColorMap::mapDb(-60.0, -60.0, 0.0);
    const auto belowMin = SpectrogramColorMap::mapDb(-120.0, -60.0, 0.0);
    assert(atMin == belowMin);
    std::cout << "  -> PASSED\n";
}

void testMapDbClampAboveMax()
{
    std::cout << "[Test] testMapDbClampAboveMax\n";
    const auto atMax = SpectrogramColorMap::mapDb(0.0, -60.0, 0.0);
    const auto aboveMax = SpectrogramColorMap::mapDb(10.0, -60.0, 0.0);
    assert(atMax == aboveMax);
    std::cout << "  -> PASSED\n";
}

void testMapDbDegenerateSpan()
{
    std::cout << "[Test] testMapDbDegenerateSpan\n";
    // minDb == maxDb → degenerate span → should return mapNormalized(0.0) without crash
    const auto c = SpectrogramColorMap::mapDb(-30.0, -30.0, -30.0);
    const auto expected = SpectrogramColorMap::mapNormalized(0.0);
    assert(c == expected);
    std::cout << "  -> PASSED\n";
}

void testMapHalfBlockAnsiContainsEscapes()
{
    std::cout << "[Test] testMapHalfBlockAnsiContainsEscapes\n";
    const auto s = SpectrogramColorMap::mapHalfBlockAnsi(0.5, 0.2);
    // Must contain ANSI ESC character and half-block glyph bytes
    assert(!s.empty());
    assert(s.find('\x1b') != std::string::npos);
    // Must contain upper half-block UTF-8 glyph 0xE2 0x96 0x80
    const std::string halfBlock = "\xE2\x96\x80";
    assert(s.find(halfBlock) != std::string::npos);
    // Must contain reset sequence
    assert(s.find("\x1b[0m") != std::string::npos);
    std::cout << "  -> PASSED\n";
}

void testMapHalfBlockAnsiAllPresets()
{
    std::cout << "[Test] testMapHalfBlockAnsiAllPresets\n";
    using P = SpectrogramColorMap::Preset;
    for (const auto preset : { P::Inferno, P::Viridis, P::TacticalGreen, P::Jet }) {
        const auto s = SpectrogramColorMap::mapHalfBlockAnsi(0.3, 0.7, preset);
        assert(!s.empty());
        assert(s.find('\x1b') != std::string::npos);
    }
    std::cout << "  -> PASSED\n";
}

void testMapHalfBlockAnsiExtremes()
{
    std::cout << "[Test] testMapHalfBlockAnsiExtremes\n";
    // Both extremes (0.0 and 1.0) must produce non-empty valid strings
    const auto s0 = SpectrogramColorMap::mapHalfBlockAnsi(0.0, 0.0);
    const auto s1 = SpectrogramColorMap::mapHalfBlockAnsi(1.0, 1.0);
    assert(!s0.empty());
    assert(!s1.empty());
    // Two different intensities should produce different strings
    assert(s0 != s1);
    std::cout << "  -> PASSED\n";
}

} // namespace

int main()
{
    std::cout << "Running TestSpectrogramColorMap Test Suite\n";
    testRgbColorEquality();
    testMapNormalizedInfernoExtremes();
    testMapNormalizedViridisExtremes();
    testMapNormalizedTacticalGreenExtremes();
    testMapNormalizedJetExtremes();
    testMapNormalizedClampingBelowZero();
    testMapNormalizedClampingAboveOne();
    testMapNormalizedMidpointInterpolated();
    testMapDbFullRange();
    testMapDbClampBelowMin();
    testMapDbClampAboveMax();
    testMapDbDegenerateSpan();
    testMapHalfBlockAnsiContainsEscapes();
    testMapHalfBlockAnsiAllPresets();
    testMapHalfBlockAnsiExtremes();
    std::cout << "All TestSpectrogramColorMap Tests Passed!\n";
    return 0;
}
