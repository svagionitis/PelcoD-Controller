#include "MercatorProjection.h"
#include <gtest/gtest.h>

using namespace Mapping;

TEST(TestMercatorProjection, EquatorAndPrimeMeridian) {
    const Klv::GeoPoint2D origin { 0.0, 0.0 };
    const double zoom = 0.0;
    const int tileSize = 256;

    // Center of world map at zoom 0 is (128, 128)
    const ScreenPoint pixel = MercatorProjection::latLonToGlobalPixel(origin, zoom, tileSize);
    EXPECT_NEAR(pixel.x, 128.0, 1e-6);
    EXPECT_NEAR(pixel.y, 128.0, 1e-6);

    // Inverse projection back to lat/lon
    const Klv::GeoPoint2D roundTrip = MercatorProjection::globalPixelToLatLon(pixel, zoom, tileSize);
    EXPECT_NEAR(roundTrip.latitudeDeg, 0.0, 1e-6);
    EXPECT_NEAR(roundTrip.longitudeDeg, 0.0, 1e-6);
}

TEST(TestMercatorProjection, ZoomZeroTileCoord) {
    // Every point on Earth maps to (0, 0) at zoom 0
    const TileCoord tile1 = MercatorProjection::latLonToTile({ 37.9838, 23.7275 }, 0); // Athens
    EXPECT_EQ(tile1.x, 0);
    EXPECT_EQ(tile1.y, 0);
    EXPECT_EQ(tile1.zoom, 0);

    const TileCoord tile2 = MercatorProjection::latLonToTile({ -33.8688, 151.2093 }, 0); // Sydney
    EXPECT_EQ(tile2.x, 0);
    EXPECT_EQ(tile2.y, 0);
    EXPECT_EQ(tile2.zoom, 0);
}

TEST(TestMercatorProjection, KnownTileCoordinates) {
    // Athens, Greece: 37.9838° N, 23.7275° E at zoom 10
    // lon = 23.7275 -> (23.7275 + 180)/360 * 1024 = 579.489 -> x = 579
    const TileCoord athensTile = MercatorProjection::latLonToTile({ 37.9838, 23.7275 }, 10);
    EXPECT_EQ(athensTile.x, 579);
    EXPECT_EQ(athensTile.zoom, 10);
    // At zoom 10, Athens y is in the northern hemisphere (< 512)
    EXPECT_LT(athensTile.y, 512);
    EXPECT_GT(athensTile.y, 300);

    // Bounding box of Athens tile must contain Athens
    const BoundingBox bbox = MercatorProjection::tileToBoundingBox(athensTile);
    EXPECT_TRUE(bbox.contains({ 37.9838, 23.7275 }));
}

TEST(TestMercatorProjection, RoundTripHighPrecision) {
    const std::vector<Klv::GeoPoint2D> testPoints {
        { 0.0, 0.0 },
        { 40.7128, -74.0060 },  // New York
        { 51.5074, -0.1278 },   // London
        { 35.6762, 139.6503 },  // Tokyo
        { -34.6037, -58.3816 }, // Buenos Aires
        { 82.0, -45.0 },        // Near Arctic limit
        { -82.0, 120.0 }        // Near Antarctic limit
    };

    const std::vector<double> zooms { 1.0, 4.5, 10.0, 16.0, 19.0 };

    for (double zoom : zooms) {
        for (const auto& pt : testPoints) {
            const ScreenPoint pixel = MercatorProjection::latLonToGlobalPixel(pt, zoom);
            const Klv::GeoPoint2D recovered = MercatorProjection::globalPixelToLatLon(pixel, zoom);

            EXPECT_NEAR(recovered.latitudeDeg, pt.latitudeDeg, 1e-5)
                << "Failed latitude roundtrip at zoom " << zoom;
            EXPECT_NEAR(recovered.longitudeDeg, pt.longitudeDeg, 1e-5)
                << "Failed longitude roundtrip at zoom " << zoom;
        }
    }
}

TEST(TestMercatorProjection, TmsCoordinateFlipping) {
    const int zoom = 12;
    const int totalTiles = 1 << zoom; // 4096

    // Slippy Y = 0 (top/north) -> TMS Y = totalTiles - 1 (top/north in TMS)
    EXPECT_EQ(MercatorProjection::slippyToTmsY(0, zoom), totalTiles - 1);
    EXPECT_EQ(MercatorProjection::tmsToSlippyY(totalTiles - 1, zoom), 0);

    // Slippy Y = totalTiles - 1 (bottom/south) -> TMS Y = 0
    EXPECT_EQ(MercatorProjection::slippyToTmsY(totalTiles - 1, zoom), 0);
    EXPECT_EQ(MercatorProjection::tmsToSlippyY(0, zoom), totalTiles - 1);

    // Arbitrary row round-trip
    const int row = 1357;
    const int tmsRow = MercatorProjection::slippyToTmsY(row, zoom);
    EXPECT_EQ(MercatorProjection::tmsToSlippyY(tmsRow, zoom), row);
}

TEST(TestMercatorProjection, MetersPerPixel) {
    // At equator zoom 0, resolution is ~156,543 meters / pixel
    const double mppEquatorZ0 = MercatorProjection::metersPerPixel(0.0, 0.0);
    EXPECT_NEAR(mppEquatorZ0, 156543.03, 1.0);

    // Each zoom level halves the ground resolution
    const double mppEquatorZ1 = MercatorProjection::metersPerPixel(0.0, 1.0);
    EXPECT_NEAR(mppEquatorZ1, mppEquatorZ0 / 2.0, 1e-3);

    // Latitude scaling: cos(60 deg) = 0.5, so resolution at 60 deg lat should be half that of equator
    const double mppLat60Z0 = MercatorProjection::metersPerPixel(60.0, 0.0);
    EXPECT_NEAR(mppLat60Z0, mppEquatorZ0 * 0.5, 1.0);
}

TEST(TestMercatorProjection, LatitudeClamping) {
    EXPECT_EQ(MercatorProjection::clampLatitude(95.0), MercatorProjection::kMaxLatitude);
    EXPECT_EQ(MercatorProjection::clampLatitude(-92.0), MercatorProjection::kMinLatitude);
    EXPECT_EQ(MercatorProjection::clampLatitude(45.0), 45.0);
}
