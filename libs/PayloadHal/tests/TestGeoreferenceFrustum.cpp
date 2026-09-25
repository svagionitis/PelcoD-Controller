/// @file TestGeoreferenceFrustum.cpp
/// @brief Unit tests for frustum footprint projection, auto VFOV derivation, and DEM ray-casting.

#include "GeoreferenceUtils.h"
#include "Klv/KlvGeodesy.h"
#include "sim/SimulatedPayload.h"
#include <cmath>
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

    TEST(TestGeoreferenceFrustum, FrustumInvalidInputs)
    {
        const Klv::GeoPoint3D platform { 37.7749, -122.4194, 500.0 };

        // Looking level (0.0 tilt) cannot intersect ground
        auto resLevel = GeoreferenceUtils::computeFrustumCorners(platform, 0.0, 0.0, 0.0, 30.0, 20.0);
        EXPECT_FALSE(resLevel.has_value());

        // Looking above horizon (+15.0 tilt)
        auto resUp = GeoreferenceUtils::computeFrustumCorners(platform, 0.0, 0.0, 15.0, 30.0, 20.0);
        EXPECT_FALSE(resUp.has_value());

        // Invalid HFOV <= 0
        auto resZeroFov = GeoreferenceUtils::computeFrustumCorners(platform, 0.0, 0.0, -30.0, 0.0, 20.0);
        EXPECT_FALSE(resZeroFov.has_value());

        auto resNegFov = GeoreferenceUtils::computeFrustumCorners(platform, 0.0, 0.0, -30.0, -10.0, 20.0);
        EXPECT_FALSE(resNegFov.has_value());

        // Platform below or at ground elevation
        auto resBelow = GeoreferenceUtils::computeFrustumCorners(platform, 0.0, 0.0, -30.0, 30.0, 20.0, 600.0);
        EXPECT_FALSE(resBelow.has_value());
    }

    TEST(TestGeoreferenceFrustum, FrustumValidLookingNorth)
    {
        const Klv::GeoPoint3D platform { 37.0, -122.0, 1000.0 };
        const double heading = 0.0; // Due North
        const double pan = 0.0;
        const double tilt = -40.0; // 40 degrees downward
        const double hfov = 30.0;
        const double vfov = 18.0;

        auto frustum = GeoreferenceUtils::computeFrustumCorners(platform, heading, pan, tilt, hfov, vfov, 0.0);
        ASSERT_TRUE(frustum.has_value());

        // Looking North: Upper corners (topLeft, topRight) are further North (higher latitude) than lower corners
        // (bottomLeft, bottomRight)
        EXPECT_GT(frustum->topLeft.latitudeDeg, frustum->bottomLeft.latitudeDeg);
        EXPECT_GT(frustum->topRight.latitudeDeg, frustum->bottomRight.latitudeDeg);

        // Left corners (topLeft, bottomLeft) should have more negative (westward) longitude than right corners
        // (topRight, bottomRight)
        EXPECT_LT(frustum->topLeft.longitudeDeg, frustum->topRight.longitudeDeg);
        EXPECT_LT(frustum->bottomLeft.longitudeDeg, frustum->bottomRight.longitudeDeg);
    }

    TEST(TestGeoreferenceFrustum, AutoVfovDerivation16x9)
    {
        const Klv::GeoPoint3D platform { 37.0, -122.0, 1000.0 };
        const double heading = 90.0; // East
        const double pan = 0.0;
        const double tilt = -45.0;
        const double hfov = 40.0;

        // Expected 16:9 vertical FOV
        const double expectedVfov = 2.0 * kRadToDeg * std::atan(std::tan(hfov * 0.5 * kDegToRad) * (9.0 / 16.0));

        // Call with vfov = 0.0 (auto-derive)
        auto autoFrustum = GeoreferenceUtils::computeFrustumCorners(platform, heading, pan, tilt, hfov, 0.0, 0.0);
        ASSERT_TRUE(autoFrustum.has_value());

        // Call with explicit expected vfov
        auto explicitFrustum
            = GeoreferenceUtils::computeFrustumCorners(platform, heading, pan, tilt, hfov, expectedVfov, 0.0);
        ASSERT_TRUE(explicitFrustum.has_value());

        // Compare corner coordinates
        EXPECT_NEAR(autoFrustum->topLeft.latitudeDeg, explicitFrustum->topLeft.latitudeDeg, 1e-6);
        EXPECT_NEAR(autoFrustum->topLeft.longitudeDeg, explicitFrustum->topLeft.longitudeDeg, 1e-6);
        EXPECT_NEAR(autoFrustum->bottomRight.latitudeDeg, explicitFrustum->bottomRight.latitudeDeg, 1e-6);
        EXPECT_NEAR(autoFrustum->bottomRight.longitudeDeg, explicitFrustum->bottomRight.longitudeDeg, 1e-6);
    }

    TEST(TestGeoreferenceFrustum, DemRayMarchingConvergence)
    {
        const Klv::GeoPoint3D platform { 37.0, -122.0, 1000.0 };
        const double heading = 0.0;
        const double pan = 0.0;
        const double tilt = -30.0;

        // Terrain with constant plateau elevation of 250m
        const auto elevationLookupPlateau = [](double /*lat*/, double /*lon*/) -> double { return 250.0; };

        auto target = GeoreferenceUtils::computeTargetFromGroundIntersection(
            platform, heading, pan, tilt, elevationLookupPlateau, 0.0);

        ASSERT_TRUE(target.has_value());
        EXPECT_NEAR(target->altitudeM, 250.0, 0.2);

        // Sloping terrain: elevation = 200 + (lat - 37.0) * 5000.0
        const auto elevationLookupSlope
            = [](double lat, double /*lon*/) -> double { return 200.0 + (lat - 37.0) * 5000.0; };

        auto slopeTarget = GeoreferenceUtils::computeTargetFromGroundIntersection(
            platform, heading, pan, tilt, elevationLookupSlope, 0.0);

        ASSERT_TRUE(slopeTarget.has_value());
        const double sampledAtResult = elevationLookupSlope(slopeTarget->latitudeDeg, slopeTarget->longitudeDeg);
        EXPECT_NEAR(slopeTarget->altitudeM, sampledAtResult, 0.5);
    }

    TEST(TestGeoreferenceFrustum, IPayloadFrustumDelegation)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        // Slew gimbal downwards
        ASSERT_TRUE(payload->panTilt()->setAbsoluteAngles(0.0, -35.0));

        const Klv::GeoPoint3D platform { 37.0, -122.0, 800.0 };
        auto frustum = payload->computeFrustumCorners(platform, 0.0, 0.0);
        ASSERT_TRUE(frustum.has_value());

        EXPECT_GT(frustum->topLeft.latitudeDeg, frustum->bottomLeft.latitudeDeg);
    }

} // namespace
} // namespace PayloadHal
