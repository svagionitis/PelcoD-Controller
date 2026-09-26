/// @file TestDemRayCaster.cpp
/// @brief Comprehensive unit tests for DemRayCaster, GridDemProvider, and DEM integration in PayloadHal.

#include "DemRayCaster.h"
#include "GeoreferenceUtils.h"
#include "GridDemProvider.h"
#include "PayloadKlvGenerator.h"
#include "sim/SimulatedPayload.h"

#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace PayloadHal {
namespace {

    TEST(TestDemRayCaster, FlatPlaneEquivalence)
    {
        // Constant elevation terrain at 200m MSL
        ProceduralDemProvider flatTerrain([](double /*lat*/, double /*lon*/) {
            return 200.0;
        }, 200.0, 200.0);

        // Platform at 1000m MSL (800m above terrain), looking North (0 deg) with -30 deg depression
        const Klv::GeoPoint3D platform { 37.0, -122.0, 1000.0 };
        const double heading = 0.0;
        const double pan = 0.0;
        const double tilt = -30.0;

        auto result = DemRayCaster::intersect(flatTerrain, platform, heading, pan, tilt);
        ASSERT_TRUE(result.has_value());

        // Target elevation must match 200.0m
        EXPECT_NEAR(result->targetPosition.altitudeM, 200.0, 0.2);

        // Slant range in flat trigonometry: (1000 - 200) / sin(30 deg) = 1600m
        // Curved Earth slightly shifts this, should be within 2m
        EXPECT_NEAR(result->slantRangeMeters, 1600.0, 2.0);

        // Target should be directly North (positive latitude delta, identical longitude)
        EXPECT_GT(result->targetPosition.latitudeDeg, 37.0);
        EXPECT_NEAR(result->targetPosition.longitudeDeg, -122.0, 1e-4);
    }

    TEST(TestDemRayCaster, SlopedTerrainIntersection)
    {
        // Sloped hillside rising towards North: 100m at lat 37.0, rising 10000m per degree (~90m per km)
        ProceduralDemProvider slopedTerrain([](double lat, double /*lon*/) {
            return 100.0 + (lat - 37.0) * 10000.0;
        }, 0.0, 2000.0);

        const Klv::GeoPoint3D platform { 37.0, -122.0, 1500.0 };
        const double heading = 0.0;
        const double pan = 0.0;
        const double tilt = -20.0;

        auto result = DemRayCaster::intersect(slopedTerrain, platform, heading, pan, tilt);
        ASSERT_TRUE(result.has_value());

        // Sample ground elevation directly at intersected coordinate
        const double expectedElevation = 100.0 + (result->targetPosition.latitudeDeg - 37.0) * 10000.0;
        EXPECT_NEAR(result->targetPosition.altitudeM, expectedElevation, 0.2);
    }

    TEST(TestDemRayCaster, MountainRidgeForegroundOcclusion)
    {
        // Mountain ridge located ~1.5 km north of platform (lat ~ 37.0135) with 600m peak.
        // Behind the mountain, valley drops back down to 100m.
        ProceduralDemProvider ridgeTerrain([](double lat, double /*lon*/) {
            const double dLat = (lat - 37.0135);
            // Gaussian peak with sigma = 0.003 (~330m)
            const double peak = 500.0 * std::exp(-(dLat * dLat) / (2.0 * 0.003 * 0.003));
            return 100.0 + peak;
        }, 100.0, 600.0);

        const Klv::GeoPoint3D platform { 37.0, -122.0, 1000.0 };
        const double heading = 0.0;
        const double pan = 0.0;
        const double tilt = -20.0; // Aimed through the mountain crest

        auto result = DemRayCaster::intersect(ridgeTerrain, platform, heading, pan, tilt);
        ASSERT_TRUE(result.has_value());

        // The intersection MUST occur on the mountain (elevation > 400m), NOT in the valley behind it
        EXPECT_GT(result->targetPosition.altitudeM, 350.0);
        EXPECT_TRUE(result->isOccludedByForeground);

        // Distance should be around ~1.5-2.0 km, not the 3+ km valley distance
        EXPECT_LT(result->slantRangeMeters, 2500.0);
    }

    TEST(TestDemRayCaster, HorizonAndAboveHorizonClipping)
    {
        ProceduralDemProvider terrain([](double /*lat*/, double /*lon*/) {
            return 50.0;
        }, 0.0, 100.0);

        const Klv::GeoPoint3D platform { 37.0, -122.0, 1000.0 };

        // Looking horizontal (tilt = 0.0)
        auto resultHorizon = DemRayCaster::intersect(terrain, platform, 0.0, 0.0, 0.0);
        EXPECT_FALSE(resultHorizon.has_value());

        // Looking up towards the sky (tilt = +15.0)
        auto resultSky = DemRayCaster::intersect(terrain, platform, 0.0, 0.0, 15.0);
        EXPECT_FALSE(resultSky.has_value());
    }

    TEST(TestDemRayCaster, GridDemProviderBilinearInterpolation)
    {
        // 3x3 regular grid covering [37.0 .. 37.02] latitude and [-122.02 .. -122.0] longitude
        // Row 0 (lat 37.00): 100, 110, 120
        // Row 1 (lat 37.01): 200, 210, 220
        // Row 2 (lat 37.02): 300, 310, 320
        const std::vector<float> data = {
            100.0f, 110.0f, 120.0f,
            200.0f, 210.0f, 220.0f,
            300.0f, 310.0f, 320.0f
        };

        GridDemProvider grid(37.0, 37.02, -122.02, -122.0, 3, 3, data);

        EXPECT_TRUE(grid.hasCoverage(37.01, -122.01));
        EXPECT_FALSE(grid.hasCoverage(37.05, -122.01)); // Outside north boundary

        EXPECT_NEAR(grid.minElevationM(), 100.0, 0.1);
        EXPECT_NEAR(grid.maxElevationM(), 320.0, 0.1);

        // Exact grid post sample at center
        auto elevCenter = grid.getElevationM(37.01, -122.01);
        ASSERT_TRUE(elevCenter.has_value());
        EXPECT_NEAR(*elevCenter, 210.0, 0.1);

        // Interpolated mid-point between row 0 and row 1, col 0 and col 1 (lat 37.005, lon -122.015)
        // Expected = average(100, 110, 200, 210) = 155.0
        auto elevMid = grid.getElevationM(37.005, -122.015);
        ASSERT_TRUE(elevMid.has_value());
        EXPECT_NEAR(*elevMid, 155.0, 0.1);
    }

    TEST(TestDemRayCaster, GeoreferenceUtilsIntegration)
    {
        ProceduralDemProvider terrain([](double /*lat*/, double /*lon*/) {
            return 300.0;
        }, 0.0, 500.0);

        const Klv::GeoPoint3D platform { 37.0, -122.0, 1200.0 };

        auto target = GeoreferenceUtils::computeTargetFromDem(terrain, platform, 0.0, 0.0, -30.0);
        ASSERT_TRUE(target.has_value());
        EXPECT_NEAR(target->altitudeM, 300.0, 0.2);

        // Footprint frustum calculation across terrain
        auto frustum = GeoreferenceUtils::computeFrustumCorners(terrain, platform, 0.0, 0.0, -30.0, 40.0, 24.0);
        ASSERT_TRUE(frustum.has_value());
        EXPECT_NE(frustum->topLeft.latitudeDeg, 0.0);
        EXPECT_NE(frustum->bottomRight.latitudeDeg, 0.0);
        // Top-left is farther north than bottom-right when looking North
        EXPECT_GT(frustum->topLeft.latitudeDeg, frustum->bottomRight.latitudeDeg);
    }

    TEST(TestDemRayCaster, KlvGeneratorWithDemIntegration)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());
        ASSERT_TRUE(payload->panTilt()->setAbsoluteAngles(0.0, -30.0));

        auto dem = std::make_shared<ProceduralDemProvider>([](double /*lat*/, double /*lon*/) {
            return 450.0;
        }, 0.0, 1000.0);

        // Inject DEM into composite payload
        payload->setDemProvider(dem);

        PayloadKlvConfig config;
        config.enableFrustumCorners = true;
        PayloadKlvGenerator generator(payload, config);

        PlatformNavData nav;
        nav.position = { 37.0, -122.0, 1500.0 };
        nav.headingDeg = 0.0;

        auto msg = generator.buildMessage(nav);

        // Slant range to 450m elevation from 1500m: delta = 1050m / sin(30) = 2100m
        ASSERT_TRUE(msg.slantRangeM.has_value());
        EXPECT_NEAR(*msg.slantRangeM, 2100.0, 10.0);

        // Frame center elevation (Tag 25) must match DEM height (450m)
        ASSERT_TRUE(msg.frameCenterElevM.has_value());
        EXPECT_NEAR(*msg.frameCenterElevM, 450.0, 0.5);

        // Frustum corners should be populated
        ASSERT_TRUE(msg.cornerCoordinates.has_value());
        EXPECT_NE(msg.cornerCoordinates->topLeft.latitudeDeg, 0.0);
    }

} // namespace
} // namespace PayloadHal
