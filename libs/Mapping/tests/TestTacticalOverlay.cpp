#include "KlvGeodesy.h"
#include "TacticalOverlay.h"
#include <gtest/gtest.h>

using namespace Mapping;

TEST(TestTacticalOverlay, FrustumProjectionToScreen) {
    const Klv::GeoPoint3D platform { 37.7749, -122.4194, 500.0 }; // 500m MSL
    // Compute real frustum using KlvGeodesy looking North at -45 deg elevation
    const auto frustumOpt = Klv::KlvGeodesy::computeFrustum(platform, 0.0, 0.0, -45.0, 30.0, 20.0, 0.0);
    ASSERT_TRUE(frustumOpt.has_value());

    // Center map around platform
    const Klv::GeoPoint2D platform2D { platform.latitudeDeg, platform.longitudeDeg };
    MapViewport viewport(platform2D, 15.0, 800.0, 600.0);

    const ScreenFrustum screenFrustum = TacticalOverlay::projectFrustum(viewport, *frustumOpt);
    EXPECT_TRUE(screenFrustum.valid);

    // Because camera is looking North (-Y in screen coords):
    // Top-Left corner (further North) must have a smaller Y pixel coordinate than Bottom-Left corner
    EXPECT_LT(screenFrustum.corners[0].y, screenFrustum.corners[3].y);
    // Top-Right corner must have a greater X pixel coordinate than Top-Left corner
    EXPECT_GT(screenFrustum.corners[1].x, screenFrustum.corners[0].x);

    // Frustum is near the center, so it must be visible in an 800x600 viewport
    const ScreenRect viewportRect { 0.0, 0.0, 800.0, 600.0 };
    EXPECT_TRUE(TacticalOverlay::isFrustumVisible(screenFrustum, viewportRect));
}

TEST(TestTacticalOverlay, HeadingVectorDirections) {
    const Klv::GeoPoint2D platform { 0.0, 0.0 };
    MapViewport viewport(platform, 10.0, 800.0, 600.0);
    const ScreenPoint origin = viewport.geoToScreen(platform); // (400, 300)
    constexpr double kLen = 50.0;

    // Heading 0 deg (North): tip should be strictly above origin (-Y)
    const ScreenVector northVec = TacticalOverlay::projectHeadingVector(viewport, platform, 0.0, kLen);
    EXPECT_NEAR(northVec.origin.x, origin.x, 1e-4);
    EXPECT_NEAR(northVec.origin.y, origin.y, 1e-4);
    EXPECT_NEAR(northVec.tip.x, origin.x, 1e-4);
    EXPECT_NEAR(northVec.tip.y, origin.y - kLen, 1e-4);

    // Heading 90 deg (East): tip should be strictly to the right (+X)
    const ScreenVector eastVec = TacticalOverlay::projectHeadingVector(viewport, platform, 90.0, kLen);
    EXPECT_NEAR(eastVec.tip.x, origin.x + kLen, 1e-4);
    EXPECT_NEAR(eastVec.tip.y, origin.y, 1e-4);

    // Heading 180 deg (South): tip should be strictly below (+Y)
    const ScreenVector southVec = TacticalOverlay::projectHeadingVector(viewport, platform, 180.0, kLen);
    EXPECT_NEAR(southVec.tip.x, origin.x, 1e-4);
    EXPECT_NEAR(southVec.tip.y, origin.y + kLen, 1e-4);

    // Heading 270 deg (West): tip should be strictly to the left (-X)
    const ScreenVector westVec = TacticalOverlay::projectHeadingVector(viewport, platform, 270.0, kLen);
    EXPECT_NEAR(westVec.tip.x, origin.x - kLen, 1e-4);
    EXPECT_NEAR(westVec.tip.y, origin.y, 1e-4);
}

TEST(TestTacticalOverlay, LineOfSightProjection) {
    const Klv::GeoPoint2D platform { 35.0, 25.0 };
    const Klv::GeoPoint2D target { 35.01, 25.01 };
    MapViewport viewport(platform, 12.0, 800.0, 600.0);

    const ScreenVector los = TacticalOverlay::projectLineOfSight(viewport, platform, target);
    EXPECT_TRUE(los.valid);

    const ScreenPoint expectedOrigin = viewport.geoToScreen(platform);
    const ScreenPoint expectedTip = viewport.geoToScreen(target);

    EXPECT_NEAR(los.origin.x, expectedOrigin.x, 1e-4);
    EXPECT_NEAR(los.origin.y, expectedOrigin.y, 1e-4);
    EXPECT_NEAR(los.tip.x, expectedTip.x, 1e-4);
    EXPECT_NEAR(los.tip.y, expectedTip.y, 1e-4);
}

TEST(TestTacticalOverlay, FrustumOffscreenClipping) {
    ScreenFrustum frustum;
    frustum.valid = true;
    frustum.corners = {
        ScreenPoint { 1200.0, 1200.0 },
        ScreenPoint { 1300.0, 1200.0 },
        ScreenPoint { 1300.0, 1300.0 },
        ScreenPoint { 1200.0, 1300.0 }
    };

    const ScreenRect viewportRect { 0.0, 0.0, 800.0, 600.0 };
    EXPECT_FALSE(TacticalOverlay::isFrustumVisible(frustum, viewportRect));
}
