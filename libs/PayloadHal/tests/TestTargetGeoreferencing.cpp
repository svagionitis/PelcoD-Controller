#include "GeoreferenceUtils.h"
#include "Klv/KlvGeodesy.h"
#include <gtest/gtest.h>
#include <cmath>

namespace PayloadHal {
namespace {

constexpr double kToleranceMeters { 0.1 };

TEST(TestTargetGeoreferencing, SlantRangeInvalidInputs) {
    const Klv::GeoPoint3D platform { 37.7749, -122.4194, 500.0 };
    // Zero or negative slant range must return nullopt
    auto res0 = GeoreferenceUtils::computeTargetFromSlantRange(platform, 0.0, 0.0, 0.0, 0.0);
    EXPECT_FALSE(res0.has_value());

    auto resNeg = GeoreferenceUtils::computeTargetFromSlantRange(platform, 0.0, 0.0, 0.0, -100.0);
    EXPECT_FALSE(resNeg.has_value());
}

TEST(TestTargetGeoreferencing, SlantRangeDueNorthLevel) {
    const Klv::GeoPoint3D platform { 0.0, 0.0, 100.0 };
    const double platformHeading = 0.0; // North
    const double panDeg = 0.0;          // Straight ahead
    const double tiltDeg = 0.0;         // Level
    const double slantRange = 1000.0;   // 1 km

    auto target = GeoreferenceUtils::computeTargetFromSlantRange(platform, platformHeading, panDeg, tiltDeg, slantRange);
    ASSERT_TRUE(target.has_value());

    EXPECT_NEAR(target->altitudeM, 100.0, 1e-4);
    EXPECT_NEAR(target->longitudeDeg, 0.0, 1e-6);
    EXPECT_GT(target->latitudeDeg, 0.0);

    // Verify distance from origin is exactly ~1000 m
    const double measuredDist = Klv::KlvGeodesy::distanceMeters(
        Klv::GeoPoint2D { platform.latitudeDeg, platform.longitudeDeg },
        Klv::GeoPoint2D { target->latitudeDeg, target->longitudeDeg });
    EXPECT_NEAR(measuredDist, 1000.0, kToleranceMeters);
}

TEST(TestTargetGeoreferencing, SlantRangeWithDepressionAndPan) {
    const Klv::GeoPoint3D platform { 37.7749, -122.4194, 500.0 };
    const double platformHeading = 90.0; // East
    const double panDeg = 45.0;          // 45° to the right -> Bearing 135° (SE)
    const double tiltDeg = -30.0;        // 30° downwards
    const double slantRange = 600.0;     // 600m slant range

    auto target = GeoreferenceUtils::computeTargetFromSlantRange(platform, platformHeading, panDeg, tiltDeg, slantRange);
    ASSERT_TRUE(target.has_value());

    // Delta altitude = 600 * sin(-30°) = -300 m -> Target Alt = 500 - 300 = 200 m
    EXPECT_NEAR(target->altitudeM, 200.0, 1e-4);

    // Ground distance = 600 * cos(-30°) = 519.6152 m
    const double expectedGroundDist = 600.0 * std::cos(30.0 * 3.14159265358979323846 / 180.0);
    const double measuredDist = Klv::KlvGeodesy::distanceMeters(
        Klv::GeoPoint2D { platform.latitudeDeg, platform.longitudeDeg },
        Klv::GeoPoint2D { target->latitudeDeg, target->longitudeDeg });
    EXPECT_NEAR(measuredDist, expectedGroundDist, kToleranceMeters);

    // Bearing should be 135°
    const double bearing = Klv::KlvGeodesy::bearingDeg(
        Klv::GeoPoint2D { platform.latitudeDeg, platform.longitudeDeg },
        Klv::GeoPoint2D { target->latitudeDeg, target->longitudeDeg });
    EXPECT_NEAR(bearing, 135.0, 0.01);
}

TEST(TestTargetGeoreferencing, GroundIntersectionInvalidElevation) {
    const Klv::GeoPoint3D platform { 37.7749, -122.4194, 500.0 };

    // Tilt level or up cannot intersect ground
    auto resLevel = GeoreferenceUtils::computeTargetFromGroundIntersection(platform, 0.0, 0.0, 0.0, 0.0);
    EXPECT_FALSE(resLevel.has_value());

    auto resUp = GeoreferenceUtils::computeTargetFromGroundIntersection(platform, 0.0, 0.0, 15.0, 0.0);
    EXPECT_FALSE(resUp.has_value());

    // Platform below or at ground elevation
    const Klv::GeoPoint3D undergroundPlatform { 37.7749, -122.4194, 50.0 };
    auto resUnderground = GeoreferenceUtils::computeTargetFromGroundIntersection(undergroundPlatform, 0.0, 0.0, -10.0, 100.0);
    EXPECT_FALSE(resUnderground.has_value());
}

TEST(TestTargetGeoreferencing, GroundIntersectionValid) {
    const Klv::GeoPoint3D platform { 35.0, 25.0, 1000.0 }; // 1000 m MSL
    const double groundElev = 0.0;                         // Sea level
    const double platformHeading = 180.0;                  // South
    const double panDeg = 0.0;                             // Straight ahead
    const double tiltDeg = -45.0;                          // 45° depression

    auto target = GeoreferenceUtils::computeTargetFromGroundIntersection(platform, platformHeading, panDeg, tiltDeg, groundElev);
    ASSERT_TRUE(target.has_value());

    EXPECT_NEAR(target->altitudeM, groundElev, 1e-4);

    // At 45° depression, ground distance = altitude delta = 1000 m
    const double measuredDist = Klv::KlvGeodesy::distanceMeters(
        Klv::GeoPoint2D { platform.latitudeDeg, platform.longitudeDeg },
        Klv::GeoPoint2D { target->latitudeDeg, target->longitudeDeg });
    EXPECT_NEAR(measuredDist, 1000.0, 1.0); // within 1m considering spherical Earth
}

TEST(TestTargetGeoreferencing, LookAnglesRoundTrip) {
    const Klv::GeoPoint3D platform { 38.0, 24.0, 300.0 };
    const double platformHeading = 45.0; // NE

    // Target located at bearing 90° (East), distance 2000m, altitude 100m
    const Klv::GeoPoint2D target2D = Klv::KlvGeodesy::directGeodetic(
        Klv::GeoPoint2D { platform.latitudeDeg, platform.longitudeDeg },
        90.0, 2000.0);
    const Klv::GeoPoint3D target { target2D.latitudeDeg, target2D.longitudeDeg, 100.0 };

    const GimbalLookAngles angles = GeoreferenceUtils::computeLookAnglesToTarget(platform, platformHeading, target);

    EXPECT_NEAR(angles.trueBearingDeg, 90.0, 0.01);
    // Relative pan = 90° - 45° = +45°
    EXPECT_NEAR(angles.panAngleDeg, 45.0, 0.01);

    // Slant range = sqrt(2000^2 + (100 - 300)^2) = sqrt(4000000 + 40000) = sqrt(4040000) ~ 2009.975 m
    EXPECT_NEAR(angles.slantRangeMeters, 2009.975, 1.0);

    // Target is lower -> negative tilt
    EXPECT_LT(angles.tiltAngleDeg, 0.0);

    // Now round-trip: compute target back from computed look angles and slant range
    auto reprojectedTarget = GeoreferenceUtils::computeTargetFromSlantRange(
        platform, platformHeading, angles.panAngleDeg, angles.tiltAngleDeg, angles.slantRangeMeters);
    ASSERT_TRUE(reprojectedTarget.has_value());

    EXPECT_NEAR(reprojectedTarget->altitudeM, target.altitudeM, kToleranceMeters);
    const double errorDist = Klv::KlvGeodesy::distanceMeters(
        Klv::GeoPoint2D { reprojectedTarget->latitudeDeg, reprojectedTarget->longitudeDeg },
        Klv::GeoPoint2D { target.latitudeDeg, target.longitudeDeg });
    EXPECT_NEAR(errorDist, 0.0, kToleranceMeters);
}

} // namespace
} // namespace PayloadHal
