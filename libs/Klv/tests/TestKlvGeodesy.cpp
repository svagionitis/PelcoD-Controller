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

TEST(KlvGeodesyTest, Attitude3DRotation) {
    const GeoPoint3D platform { 0.0, 0.0, 1000.0 }; // 1000m MSL at Equator
    const GeoPoint2D platform2D { platform.latitudeDeg, platform.longitudeDeg };

    const PlatformAttitude levelAtt { 0.0, 0.0, 0.0 };       // Heading 0, Pitch 0, Roll 0
    const PlatformAttitude pitchUpAtt { 0.0, 5.0, 0.0 };      // Pitch up +5 deg
    const CameraOrientation camNominal { 0.0, -30.0, 0.0 };   // Az 0, El -30 deg, Roll 0
    const CameraOrientation camRolled { 0.0, -30.0, 45.0 };   // Optical roll 45 deg

    // Frame center under level flight
    const auto centerLevel = KlvGeodesy::computeFrameCenter(platform, levelAtt, camNominal, 0.0);
    ASSERT_TRUE(centerLevel.has_value());
    const double distLevel = KlvGeodesy::distanceMeters(platform2D, *centerLevel);

    // Frame center with pitch-up (+5 deg nose-up with el -30 results in -25 deg effective depression)
    const auto centerPitched = KlvGeodesy::computeFrameCenter(platform, pitchUpAtt, camNominal, 0.0);
    ASSERT_TRUE(centerPitched.has_value());
    const double distPitched = KlvGeodesy::distanceMeters(platform2D, *centerPitched);

    // Pitched up camera looks further out
    EXPECT_GT(distPitched, distLevel);

    // Optical roll should not change boresight center
    const auto centerRolled = KlvGeodesy::computeFrameCenter(platform, levelAtt, camRolled, 0.0);
    ASSERT_TRUE(centerRolled.has_value());
    EXPECT_NEAR(centerRolled->latitudeDeg, centerLevel->latitudeDeg, 1e-5);
    EXPECT_NEAR(centerRolled->longitudeDeg, centerLevel->longitudeDeg, 1e-5);

    // But optical roll alters the frustum corners (rotates footprint)
    const auto frustum0 = KlvGeodesy::computeFrustum(platform, levelAtt, camNominal, 20.0, 15.0, 0.0);
    const auto frustum45 = KlvGeodesy::computeFrustum(platform, levelAtt, camRolled, 20.0, 15.0, 0.0);
    ASSERT_TRUE(frustum0.has_value());
    ASSERT_TRUE(frustum45.has_value());

    // With 0 deg roll, top-left is west of top-right
    EXPECT_LT(frustum0->topLeft.longitudeDeg, frustum0->topRight.longitudeDeg);
    // With 45 deg roll, the corners are rotated and no longer match the 0 deg roll footprint
    EXPECT_NE(frustum45->topLeft.latitudeDeg, frustum0->topLeft.latitudeDeg);
    EXPECT_NE(frustum45->topLeft.longitudeDeg, frustum0->topLeft.longitudeDeg);
}

TEST(KlvGeodesyTest, HorizonClippingAndDistance) {
    const GeoPoint3D platform { 0.0, 0.0, 1000.0 }; // 1000m MSL
    const GeoPoint2D platform2D { platform.latitudeDeg, platform.longitudeDeg };

    // Horizon distance: sqrt(2 * R * h + h^2)
    const double hDist = KlvGeodesy::horizonDistance(1000.0, 0.0);
    EXPECT_NEAR(hDist, 112885.0, 1.0); // ~112.88 km

    // Pointing upwards (+10 deg) should have no physical ground intersection
    const PlatformAttitude levelAtt { 0.0, 0.0, 0.0 };
    const CameraOrientation skywardCam { 0.0, 10.0, 0.0 };
    const auto centerSky = KlvGeodesy::computeFrameCenter(platform, levelAtt, skywardCam, 0.0);
    EXPECT_FALSE(centerSky.has_value());

    // Intersect ray with clipToHorizon = false
    const Vector3D upRay = KlvGeodesy::computeRayNed(levelAtt, skywardCam);
    const auto unclipped = KlvGeodesy::intersectRayEarth(platform, upRay, 0.0, false);
    EXPECT_FALSE(unclipped.has_value());

    // Intersect ray with clipToHorizon = true: clips exactly to geometric horizon distance
    const auto clipped = KlvGeodesy::intersectRayEarth(platform, upRay, 0.0, true);
    ASSERT_TRUE(clipped.has_value());
    const double clippedDist = KlvGeodesy::distanceMeters(platform2D, *clipped);
    EXPECT_NEAR(clippedDist, hDist, 5.0);

    // Frustum with shallow depression pointing above horizon (e.g., el = -2 deg, vfov = 10 deg)
    // Top rays point at +3 deg (above horizon), but computeFrustum clips them to horizon
    const CameraOrientation shallowCam { 0.0, -2.0, 0.0 };
    const auto frustum = KlvGeodesy::computeFrustum(platform, levelAtt, shallowCam, 10.0, 10.0, 0.0);
    ASSERT_TRUE(frustum.has_value());

    // Top corners should be bounded near horizon distance
    const double distTl = KlvGeodesy::distanceMeters(platform2D, frustum->topLeft);
    EXPECT_NEAR(distTl, hDist, 5.0);
}
