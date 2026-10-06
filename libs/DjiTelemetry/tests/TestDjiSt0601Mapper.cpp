/// @file TestDjiSt0601Mapper.cpp
/// @brief Unit tests for DJI telemetry to MISB ST 0601 mapping.

#include "DjiSt0601Mapper.h"

#include <gtest/gtest.h>

#include <cstdint>

using Dji::deriveUtcOffset;
using Dji::DjiMapConfig;
using Dji::DjiTelemetrySample;
using Dji::mapToSt0601;
using Dji::wrapDegrees360;

namespace {

/// @brief Builds the Matrice 4T sample 0 as parsed values.
/// @return Populated telemetry sample.
DjiTelemetrySample m4tSample()
{
    DjiTelemetrySample s {};
    s.frameCount = 0U;
    s.localTimeUs = 1790263255713000;
    s.focalLenMm = 52.70;
    s.digitalZoom = 1.0;
    s.latitudeDeg = 38.375988;
    s.longitudeDeg = 23.257121;
    s.relAltM = 20.160;
    s.absAltM = 169.523;
    s.gimbalYawDeg = -77.4;
    s.gimbalPitchDeg = 7.4;
    s.gimbalRollDeg = 0.0;
    return s;
}

} // namespace

TEST(DjiSt0601MapperTest, PositionAndAltitudePassThrough)
{
    const auto msg { mapToSt0601(m4tSample(), DjiMapConfig {}) };
    ASSERT_TRUE(msg.sensorLatitudeDeg.has_value());
    EXPECT_DOUBLE_EQ(*msg.sensorLatitudeDeg, 38.375988);
    ASSERT_TRUE(msg.sensorLongitudeDeg.has_value());
    EXPECT_DOUBLE_EQ(*msg.sensorLongitudeDeg, 23.257121);
    ASSERT_TRUE(msg.sensorTrueAltitudeM.has_value());
    EXPECT_DOUBLE_EQ(*msg.sensorTrueAltitudeM, 169.523);
    EXPECT_FALSE(msg.sensorAltitudeHaeM.has_value());
}

TEST(DjiSt0601MapperTest, AltitudeToHaeWhenConfigured)
{
    DjiMapConfig cfg {};
    cfg.absAltIsHae = true;
    const auto msg { mapToSt0601(m4tSample(), cfg) };
    EXPECT_FALSE(msg.sensorTrueAltitudeM.has_value());
    ASSERT_TRUE(msg.sensorAltitudeHaeM.has_value());
    EXPECT_DOUBLE_EQ(*msg.sensorAltitudeHaeM, 169.523);
}

TEST(DjiSt0601MapperTest, GimbalAnglesNormalisedAndPlatformAttitudeUnset)
{
    const auto msg { mapToSt0601(m4tSample(), DjiMapConfig {}) };
    ASSERT_TRUE(msg.sensorRelAzimuthDeg.has_value());
    EXPECT_NEAR(*msg.sensorRelAzimuthDeg, 282.6, 1e-9);
    ASSERT_TRUE(msg.sensorRelElevationDeg.has_value());
    EXPECT_DOUBLE_EQ(*msg.sensorRelElevationDeg, 7.4);
    ASSERT_TRUE(msg.sensorRelRollDeg.has_value());
    EXPECT_DOUBLE_EQ(*msg.sensorRelRollDeg, 0.0);

    EXPECT_FALSE(msg.platformHeadingDeg.has_value());
    EXPECT_FALSE(msg.platformPitchDeg.has_value());
    EXPECT_FALSE(msg.platformRollDeg.has_value());
}

TEST(DjiSt0601MapperTest, TimestampConvertedToUtc)
{
    DjiMapConfig cfg {};
    cfg.utcOffsetMin = 180;
    const auto msg { mapToSt0601(m4tSample(), cfg) };
    ASSERT_TRUE(msg.precisionTimeStampUs.has_value());
    // 2026-09-24T12:20:55.713Z
    EXPECT_EQ(*msg.precisionTimeStampUs, 1790252455713000ULL);
}

TEST(DjiSt0601MapperTest, OutOfRangeAndNoFixPositionsDropped)
{
    DjiTelemetrySample s { m4tSample() };
    s.latitudeDeg = 91.0;
    auto msg { mapToSt0601(s, DjiMapConfig {}) };
    EXPECT_FALSE(msg.sensorLatitudeDeg.has_value());
    EXPECT_FALSE(msg.sensorLongitudeDeg.has_value());

    s.latitudeDeg = 0.0;
    s.longitudeDeg = 0.0; // DJI writes 0/0 before GPS lock
    msg = mapToSt0601(s, DjiMapConfig {});
    EXPECT_FALSE(msg.sensorLatitudeDeg.has_value());
    EXPECT_FALSE(msg.sensorLongitudeDeg.has_value());

    s = m4tSample();
    s.absAltM = 25000.0;
    msg = mapToSt0601(s, DjiMapConfig {});
    EXPECT_FALSE(msg.sensorTrueAltitudeM.has_value());
}

TEST(DjiSt0601MapperTest, FieldOfViewDerivedFromEquivalentFocalLength)
{
    DjiMapConfig cfg {};
    cfg.deriveFov = true;
    cfg.sensorAspect = 1.25; // 1280x1024 thermal
    auto msg { mapToSt0601(m4tSample(), cfg) };
    ASSERT_TRUE(msg.sensorHfovDeg.has_value());
    EXPECT_NEAR(*msg.sensorHfovDeg, 35.546, 1e-2);
    ASSERT_TRUE(msg.sensorVfovDeg.has_value());
    EXPECT_NEAR(*msg.sensorVfovDeg, 28.766, 1e-2);

    DjiTelemetrySample zoomed { m4tSample() };
    zoomed.digitalZoom = 2.0;
    msg = mapToSt0601(zoomed, cfg);
    ASSERT_TRUE(msg.sensorHfovDeg.has_value());
    EXPECT_NEAR(*msg.sensorHfovDeg, 18.211, 1e-2);

    cfg.deriveFov = false;
    msg = mapToSt0601(m4tSample(), cfg);
    EXPECT_FALSE(msg.sensorHfovDeg.has_value());
}

TEST(DjiSt0601MapperTest, PlatformDesignationFromConfig)
{
    DjiMapConfig cfg {};
    cfg.platform = "DJI Matrice 4T";
    const auto msg { mapToSt0601(m4tSample(), cfg) };
    ASSERT_TRUE(msg.platformDesignation.has_value());
    EXPECT_EQ(*msg.platformDesignation, "DJI Matrice 4T");

    EXPECT_FALSE(mapToSt0601(m4tSample(), DjiMapConfig {}).platformDesignation.has_value());
}

TEST(DjiSt0601MapperTest, DeriveUtcOffsetRoundsToQuarterHour)
{
    // Local 15:20:55.713 vs mvhd creation 12:20:55Z -> +180 min.
    const auto off { deriveUtcOffset(1790263255713000, 1790252455000000ULL) };
    ASSERT_TRUE(off.has_value());
    EXPECT_EQ(*off, 180);

    // India +05:30, with a few seconds of skew.
    const auto ist { deriveUtcOffset(1790252455000000 + 19800000000 + 4000000, 1790252455000000ULL) };
    ASSERT_TRUE(ist.has_value());
    EXPECT_EQ(*ist, 330);

    // West of UTC.
    const auto pdt { deriveUtcOffset(1790252455000000 - 25200000000, 1790252455000000ULL) };
    ASSERT_TRUE(pdt.has_value());
    EXPECT_EQ(*pdt, -420);

    // Beyond +/-14 h is rejected.
    EXPECT_FALSE(deriveUtcOffset(1790252455000000 + 20 * 3600000000LL, 1790252455000000ULL).has_value());
}

TEST(DjiSt0601MapperTest, WrapDegrees)
{
    EXPECT_DOUBLE_EQ(wrapDegrees360(0.0), 0.0);
    EXPECT_DOUBLE_EQ(wrapDegrees360(360.0), 0.0);
    EXPECT_DOUBLE_EQ(wrapDegrees360(-90.0), 270.0);
    EXPECT_DOUBLE_EQ(wrapDegrees360(725.0), 5.0);
}
