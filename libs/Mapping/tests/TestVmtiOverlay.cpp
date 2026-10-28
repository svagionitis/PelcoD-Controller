/// @file TestVmtiOverlay.cpp
/// @brief Unit tests for VmtiOverlay tactical target projection and symbology.

#include "VmtiOverlay.h"

#include <cmath>
#include <cstdint>

#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "GeoTypes.h"
#include "KlvTypes.h"
#include "MapViewport.h"
#include "VmtiTypes.h"

using namespace Mapping;

TEST(TestVmtiOverlay, ProjectTargetFromLocationAndCe90)
{
    const Klv::GeoPoint2D center { 37.7749, -122.4194 };
    MapViewport viewport(center, 15.0, 800.0, 600.0);

    Klv::VTargetPack pack { };
    pack.targetId = 42U;
    pack.targetLocation = Klv::GeoPoint3D { center.latitudeDeg, center.longitudeDeg, 15.0 };
    pack.targetCe90M = 20.0;
    pack.confidence = static_cast<std::uint8_t>(92U);
    pack.detectionStatus = static_cast<std::uint8_t>(1U); // Moving

    const ScreenVmtiTarget st { VmtiOverlay::projectTarget(viewport, pack) };
    EXPECT_TRUE(st.valid);
    EXPECT_EQ(st.targetId, 42U);
    EXPECT_EQ(st.callsign, "TRK-42");
    EXPECT_EQ(st.confidence, 92.0);
    EXPECT_TRUE(st.isMoving);

    // Centered at (400, 300)
    EXPECT_NEAR(st.screenPos.x, 400.0, 1e-4);
    EXPECT_NEAR(st.screenPos.y, 300.0, 1e-4);

    // CE90 in pixels should be positive and proportional to metersPerPixel
    EXPECT_GT(st.ce90Pixels, 5.0);

    // Visible in 800x600 viewport
    const ScreenRect viewportRect { 0.0, 0.0, 800.0, 600.0 };
    EXPECT_TRUE(VmtiOverlay::isTargetVisible(st, viewportRect));
}

TEST(TestVmtiOverlay, ProjectTargetFromFrameCenterOffset)
{
    const Klv::GeoPoint2D center { 37.7749, -122.4194 };
    MapViewport viewport(center, 15.0, 800.0, 600.0);

    Klv::VTargetPack pack { };
    pack.targetId = 7U;
    pack.locationOffsetDeg = Klv::GeoPoint2D { 0.001, 0.001 };
    pack.confidence = static_cast<std::uint8_t>(75U);

    // Without frameCenter, should be invalid
    const ScreenVmtiTarget stNoCenter { VmtiOverlay::projectTarget(viewport, pack, std::nullopt) };
    EXPECT_FALSE(stNoCenter.valid);

    // With frameCenter, resolves to center + offset
    const ScreenVmtiTarget st { VmtiOverlay::projectTarget(viewport, pack, center) };
    EXPECT_TRUE(st.valid);
    EXPECT_EQ(st.targetId, 7U);
    // Target is further North (smaller Y) and further East (larger X) than center
    EXPECT_GT(st.screenPos.x, 400.0);
    EXPECT_LT(st.screenPos.y, 300.0);
}

TEST(TestVmtiOverlay, ProjectTargetSeriesAndVisibility)
{
    const Klv::GeoPoint2D center { 37.7749, -122.4194 };
    MapViewport viewport(center, 15.0, 800.0, 600.0);
    const ScreenRect viewportRect { 0.0, 0.0, 800.0, 600.0 };

    Klv::VmtiLocalSet vmti { };
    // Target 1: on-screen near center
    Klv::VTargetPack p1 { };
    p1.targetId = 1U;
    p1.targetLocation = Klv::GeoPoint3D { 37.7749, -122.4194, 0.0 };
    vmti.targets.push_back(p1);

    // Target 2: off-screen in Australia
    Klv::VTargetPack p2 { };
    p2.targetId = 2U;
    p2.targetLocation = Klv::GeoPoint3D { -33.8688, 151.2093, 0.0 };
    vmti.targets.push_back(p2);

    const auto series { VmtiOverlay::projectTargetSeries(viewport, vmti) };
    ASSERT_EQ(series.size(), 2U);

    EXPECT_TRUE(VmtiOverlay::isTargetVisible(series[0], viewportRect));
    EXPECT_FALSE(VmtiOverlay::isTargetVisible(series[1], viewportRect));
}

TEST(TestVmtiOverlay, ProjectTargetNaNCe90Safety)
{
    // CERT ERR55-CPP: projectTarget constructs std::string on heap, so it must NOT be declared noexcept
    EXPECT_FALSE(noexcept(
        VmtiOverlay::projectTarget(std::declval<const MapViewport&>(), std::declval<const Klv::VTargetPack&>())));

    const Klv::GeoPoint2D center { 37.7749, -122.4194 };
    MapViewport viewport(center, 15.0, 800.0, 600.0);

    Klv::VTargetPack pack { };
    pack.targetId = 99U;
    pack.targetLocation = Klv::GeoPoint3D { center.latitudeDeg, center.longitudeDeg, 10.0 };
    pack.targetCe90M = std::numeric_limits<double>::quiet_NaN();

    const ScreenVmtiTarget st { VmtiOverlay::projectTarget(viewport, pack) };
    EXPECT_TRUE(st.valid);
    EXPECT_TRUE(std::isfinite(st.ce90Pixels));
    EXPECT_DOUBLE_EQ(st.ce90Pixels, 0.0);
}
