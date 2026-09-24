#include "MapViewport.h"
#include <gtest/gtest.h>

using namespace Mapping;

TEST(TestMapViewport, CenterAlignsWithScreenCenter) {
    const Klv::GeoPoint2D athens { 37.9838, 23.7275 };
    const double width = 800.0;
    const double height = 600.0;
    const double zoom = 12.0;

    MapViewport viewport(athens, zoom, width, height);

    // Map center must map to screen center (width/2, height/2)
    const ScreenPoint screenCenter = viewport.geoToScreen(athens);
    EXPECT_NEAR(screenCenter.x, 400.0, 1e-4);
    EXPECT_NEAR(screenCenter.y, 300.0, 1e-4);

    // Screen center must inverse-project to map center
    const Klv::GeoPoint2D recoveredGeo = viewport.screenToGeo(ScreenPoint { 400.0, 300.0 });
    EXPECT_NEAR(recoveredGeo.latitudeDeg, athens.latitudeDeg, 1e-5);
    EXPECT_NEAR(recoveredGeo.longitudeDeg, athens.longitudeDeg, 1e-5);
}

TEST(TestMapViewport, ScreenToGeoRoundTrip) {
    const Klv::GeoPoint2D nyc { 40.7128, -74.0060 };
    MapViewport viewport(nyc, 14.25, 1024.0, 768.0);

    const std::vector<ScreenPoint> testPixels {
        { 0.0, 0.0 },
        { 1024.0, 768.0 },
        { 512.0, 384.0 },
        { 120.5, 450.2 },
        { 890.1, 710.9 }
    };

    for (const auto& sp : testPixels) {
        const Klv::GeoPoint2D geo = viewport.screenToGeo(sp);
        const ScreenPoint recoveredSp = viewport.geoToScreen(geo);

        EXPECT_NEAR(recoveredSp.x, sp.x, 1e-3);
        EXPECT_NEAR(recoveredSp.y, sp.y, 1e-3);
    }
}

TEST(TestMapViewport, PanOperation) {
    const Klv::GeoPoint2D origin { 0.0, 0.0 };
    MapViewport viewport(origin, 8.0, 800.0, 600.0);

    // Drag viewport by +100 px horizontally and -50 px vertically
    viewport.pan(100.0, -50.0);

    // The original origin should now be displayed at (400 + 100, 300 - 50)
    const ScreenPoint shiftedOrigin = viewport.geoToScreen(origin);
    EXPECT_NEAR(shiftedOrigin.x, 500.0, 1e-3);
    EXPECT_NEAR(shiftedOrigin.y, 250.0, 1e-3);
}

TEST(TestMapViewport, ZoomByAroundPivot) {
    const Klv::GeoPoint2D center { 35.0, 25.0 };
    MapViewport viewport(center, 5.0, 800.0, 600.0);

    // An arbitrary cursor position
    const ScreenPoint pivot { 250.0, 180.0 };

    // Record the geographic coordinate underneath the cursor before zooming
    const Klv::GeoPoint2D targetGeo = viewport.screenToGeo(pivot);

    // Zoom in by 2.5 levels
    viewport.zoomBy(2.5, pivot);

    // Target geo should remain pinned precisely underneath the cursor
    const ScreenPoint afterZoom = viewport.geoToScreen(targetGeo);
    EXPECT_NEAR(afterZoom.x, pivot.x, 1e-3);
    EXPECT_NEAR(afterZoom.y, pivot.y, 1e-3);
}

TEST(TestMapViewport, VisibleBoundingBox) {
    const Klv::GeoPoint2D center { 48.8566, 2.3522 }; // Paris
    MapViewport viewport(center, 10.0, 800.0, 600.0);

    const BoundingBox bbox = viewport.visibleBoundingBox();

    // Center must be within visible bounding box
    EXPECT_TRUE(bbox.contains(center));
    EXPECT_GT(bbox.north, bbox.south);
    EXPECT_GT(bbox.east, bbox.west);

    // Top-left screen point should match north-west
    const Klv::GeoPoint2D tl = viewport.screenToGeo({ 0.0, 0.0 });
    EXPECT_NEAR(bbox.north, tl.latitudeDeg, 1e-5);
    EXPECT_NEAR(bbox.west, tl.longitudeDeg, 1e-5);
}

TEST(TestMapViewport, VisibleTilesCalculation) {
    const Klv::GeoPoint2D center { 37.9838, 23.7275 };
    MapViewport viewport(center, 10.0, 800.0, 600.0);

    const auto visibleTiles = viewport.calculateVisibleTiles(0);

    // For 800x600 at 256px tiles, width covers ceil(800/256)+1 = 4-5 tiles, height covers 3-4 tiles
    EXPECT_GE(visibleTiles.size(), 12U);
    EXPECT_LE(visibleTiles.size(), 30U);

    // All tiles should be at base zoom 10 and have positive dimensions
    for (const auto& vt : visibleTiles) {
        EXPECT_EQ(vt.coord.zoom, 10);
        EXPECT_GE(vt.coord.x, 0);
        EXPECT_GE(vt.coord.y, 0);
        EXPECT_EQ(vt.screenRect.width, 256.0);
        EXPECT_EQ(vt.screenRect.height, 256.0);
    }
}
