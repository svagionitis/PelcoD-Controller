#include "TelemetryInterpolator.h"
#include <gtest/gtest.h>

namespace Klv {
namespace {

TEST(TestTelemetryInterpolator, ScalarLerp)
{
    EXPECT_DOUBLE_EQ(TelemetryInterpolator::lerp(10.0, 20.0, 0.0), 10.0);
    EXPECT_DOUBLE_EQ(TelemetryInterpolator::lerp(10.0, 20.0, 0.5), 15.0);
    EXPECT_DOUBLE_EQ(TelemetryInterpolator::lerp(10.0, 20.0, 1.0), 20.0);
}

TEST(TestTelemetryInterpolator, AngleInterpolationShortestArc)
{
    // Normal interpolation
    EXPECT_NEAR(TelemetryInterpolator::interpolateAngle(10.0, 20.0, 0.5), 15.0, 1e-6);

    // Wraparound across 0/360 degrees: 350 -> 10 deg (shortest delta is +20 deg)
    EXPECT_NEAR(TelemetryInterpolator::interpolateAngle(350.0, 10.0, 0.5), 0.0, 1e-6);
    EXPECT_NEAR(TelemetryInterpolator::interpolateAngle(350.0, 10.0, 0.25), 355.0, 1e-6);
    EXPECT_NEAR(TelemetryInterpolator::interpolateAngle(350.0, 10.0, 0.75), 5.0, 1e-6);

    // Opposite direction: 10 -> 350 deg (shortest delta is -20 deg)
    EXPECT_NEAR(TelemetryInterpolator::interpolateAngle(10.0, 350.0, 0.5), 0.0, 1e-6);
}

TEST(TestTelemetryInterpolator, LongitudeInterpolationAntiMeridian)
{
    // Normal longitude
    EXPECT_NEAR(TelemetryInterpolator::interpolateLongitude(10.0, 20.0, 0.5), 15.0, 1e-6);

    // Anti-meridian crossing: 170 deg to -170 deg (shortest delta is +20 deg across +/-180)
    EXPECT_NEAR(TelemetryInterpolator::interpolateLongitude(170.0, -170.0, 0.5), 180.0, 1e-6);
    EXPECT_NEAR(TelemetryInterpolator::interpolateLongitude(170.0, -170.0, 0.25), 175.0, 1e-6);
    EXPECT_NEAR(TelemetryInterpolator::interpolateLongitude(170.0, -170.0, 0.75), -175.0, 1e-6);
}

TEST(TestTelemetryInterpolator, FullMessageInterpolation)
{
    UasDatalinkMessage m1 {};
    m1.precisionTimeStampUs = 1000000ULL;
    m1.platformHeadingDeg = 350.0;
    m1.platformPitchDeg = -5.0;
    m1.platformRollDeg = 2.0;
    m1.sensorLatitudeDeg = 34.0;
    m1.sensorLongitudeDeg = -118.0;
    m1.sensorTrueAltitudeM = 1500.0;
    m1.sensorHfovDeg = 30.0;
    m1.missionId = "MISSION_ALPHA";

    UasDatalinkMessage m2 {};
    m2.precisionTimeStampUs = 2000000ULL;
    m2.platformHeadingDeg = 10.0;
    m2.platformPitchDeg = 5.0;
    m2.platformRollDeg = -2.0;
    m2.sensorLatitudeDeg = 36.0;
    m2.sensorLongitudeDeg = -116.0;
    m2.sensorTrueAltitudeM = 2500.0;
    m2.sensorHfovDeg = 10.0;
    m2.missionId = "MISSION_BRAVO";

    const std::uint64_t pts1 = 90000U;
    const std::uint64_t pts2 = 180000U; // 1.0 second later
    const std::uint64_t midPts = 135000U; // 0.5 second

    auto mid = TelemetryInterpolator::interpolate(m1, pts1, m2, pts2, midPts);

    ASSERT_TRUE(mid.precisionTimeStampUs.has_value());
    EXPECT_EQ(*mid.precisionTimeStampUs, 1500000ULL);

    ASSERT_TRUE(mid.platformHeadingDeg.has_value());
    EXPECT_NEAR(*mid.platformHeadingDeg, 0.0, 1e-5); // 350 -> 10 deg midpoint is 0 deg

    ASSERT_TRUE(mid.platformPitchDeg.has_value());
    EXPECT_NEAR(*mid.platformPitchDeg, 0.0, 1e-5);

    ASSERT_TRUE(mid.platformRollDeg.has_value());
    EXPECT_NEAR(*mid.platformRollDeg, 0.0, 1e-5);

    ASSERT_TRUE(mid.sensorLatitudeDeg.has_value());
    EXPECT_NEAR(*mid.sensorLatitudeDeg, 35.0, 1e-5);

    ASSERT_TRUE(mid.sensorLongitudeDeg.has_value());
    EXPECT_NEAR(*mid.sensorLongitudeDeg, -117.0, 1e-5);

    ASSERT_TRUE(mid.sensorTrueAltitudeM.has_value());
    EXPECT_NEAR(*mid.sensorTrueAltitudeM, 2000.0, 1e-5);

    ASSERT_TRUE(mid.sensorHfovDeg.has_value());
    EXPECT_NEAR(*mid.sensorHfovDeg, 20.0, 1e-5);

    // Boundary conditions
    auto atStart = TelemetryInterpolator::interpolate(m1, pts1, m2, pts2, pts1);
    EXPECT_EQ(atStart.missionId, "MISSION_ALPHA");

    auto atEnd = TelemetryInterpolator::interpolate(m1, pts1, m2, pts2, pts2);
    EXPECT_EQ(atEnd.missionId, "MISSION_BRAVO");
}

} // namespace
} // namespace Klv
