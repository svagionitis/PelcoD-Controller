#include "KlvGeodesy.h"
#include <gtest/gtest.h>

using namespace Klv;

TEST(KlvGeodesyTest, DirectGeodeticAndDistance) {
    const GeoPoint2D equatorOrigin { 0.0, 0.0 };

    // Move 10,000 meters Due North (bearing 0)
    const GeoPoint2D northPoint = KlvGeodesy::directGeodetic(equatorOrigin, 0.0, 10000.0);
    EXPECT_GT(northPoint.latitudeDeg, 0.0);
    EXPECT_NEAR(northPoint.longitudeDeg, 0.0, 1e-6);

    // Measure distance back
    const double measuredDist = KlvGeodesy::distanceMeters(equatorOrigin, northPoint);
    EXPECT_NEAR(measuredDist, 10000.0, 0.1);

    // Bearing from origin to northPoint should be 0 deg
    EXPECT_NEAR(KlvGeodesy::bearingDeg(equatorOrigin, northPoint), 0.0, 1e-3);

    // Bearing from northPoint back to origin should be 180 deg
    EXPECT_NEAR(KlvGeodesy::bearingDeg(northPoint, equatorOrigin), 180.0, 1e-3);
}

TEST(KlvGeodesyTest, SlantRangeCalculations) {
    // 1000m MSL, nadir (-90 deg), ground at 0m -> 1000m slant range
    const auto nadirRange = KlvGeodesy::computeSlantRange(1000.0, -90.0, 0.0);
    ASSERT_TRUE(nadirRange.has_value());
    EXPECT_NEAR(*nadirRange, 1000.0, 0.1);

    // 1000m MSL, -30 deg depression, ground at 0m -> 1000 / sin(30) = 2000m
    const auto angle30Range = KlvGeodesy::computeSlantRange(1000.0, -30.0, 0.0);
    ASSERT_TRUE(angle30Range.has_value());
    EXPECT_NEAR(*angle30Range, 2000.0, 0.5);

    // Looking horizontal (0 deg) or up (+10 deg) should return nullopt
    EXPECT_FALSE(KlvGeodesy::computeSlantRange(1000.0, 0.0, 0.0).has_value());
    EXPECT_FALSE(KlvGeodesy::computeSlantRange(1000.0, 10.0, 0.0).has_value());
}

TEST(KlvGeodesyTest, FrameCenterAndFrustum) {
    const GeoPoint3D platform { 37.7749, -122.4194, 500.0 }; // 500m altitude

    // Looking North (heading 0, rel az 0) at 45 deg depression
    const auto center = KlvGeodesy::computeFrameCenter(platform, 0.0, 0.0, -45.0, 0.0);
    ASSERT_TRUE(center.has_value());

    // Center should be approximately 500m North of platform
    EXPECT_GT(center->latitudeDeg, platform.latitudeDeg);
    EXPECT_NEAR(center->longitudeDeg, platform.longitudeDeg, 1e-4);

    const double distToCenter = KlvGeodesy::distanceMeters(GeoPoint2D { platform.latitudeDeg, platform.longitudeDeg }, *center);
    EXPECT_NEAR(distToCenter, 500.0, 1.0);

    // Frustum with 30 deg HFOV and 20 deg VFOV
    const auto frustum = KlvGeodesy::computeFrustum(platform, 0.0, 0.0, -45.0, 30.0, 20.0, 0.0);
    ASSERT_TRUE(frustum.has_value());

    // Top corners should be further North (higher latitude) than bottom corners
    EXPECT_GT(frustum->topLeft.latitudeDeg, frustum->bottomLeft.latitudeDeg);
    EXPECT_GT(frustum->topRight.latitudeDeg, frustum->bottomRight.latitudeDeg);

    // Right corners should be further East (higher longitude) than left corners
    EXPECT_GT(frustum->topRight.longitudeDeg, frustum->topLeft.longitudeDeg);
    EXPECT_GT(frustum->bottomRight.longitudeDeg, frustum->bottomLeft.longitudeDeg);
}
