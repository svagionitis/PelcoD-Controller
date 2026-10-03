/**
 * @file TestTacticalHudFilter.cpp
 * @brief Unit tests for Tactical HUD and MISB ST 1909.1 overlay conformance.
 */

#include <gtest/gtest.h>

#include "TacticalHudFilter.h"
#include "DecoderTypes.h"
#include "KlvTypes.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace Video::Filters;

TEST(TestTacticalHud, AngleFmt) {
    // Standard ST 1909.1 angle test: 5 digits before, 4 digits after
    const std::string az1 = TacticalHudFilter::formatSt1909Angle(160.7191, 5, 4);
    EXPECT_EQ(az1, "  160.7191*");

    const std::string el1 = TacticalHudFilter::formatSt1909Angle(-36.4976, 5, 4);
    EXPECT_EQ(el1, "  -36.4976*");

    // Latitude test: 3 digits before, 4 digits after
    const std::string lat1 = TacticalHudFilter::formatSt1909Angle(33.4568, 3, 4);
    EXPECT_EQ(lat1, " 33.4568*");

    const std::string lat2 = TacticalHudFilter::formatSt1909Angle(-12.3456, 3, 4);
    EXPECT_EQ(lat2, "-12.3456*");

    // Longitude test: 4 digits before, 4 digits after
    const std::string lon1 = TacticalHudFilter::formatSt1909Angle(-104.6546, 4, 4);
    EXPECT_EQ(lon1, "-104.6546*");

    const std::string lon2 = TacticalHudFilter::formatSt1909Angle(45.1234, 4, 4);
    EXPECT_EQ(lon2, "  45.1234*");

    // Nullopt / unavailable returns "N/A"
    const std::string na = TacticalHudFilter::formatSt1909Angle(std::nullopt, 5, 4);
    EXPECT_EQ(na, "N/A");
}

TEST(TestTacticalHud, MeterFmt) {
    // 7 digits for Slant Range / Target Width
    const std::string sr1 = TacticalHudFilter::formatSt1909Meters(14265.0, 7);
    EXPECT_EQ(sr1, "  14265m");

    const std::string tw1 = TacticalHudFilter::formatSt1909Meters(50.0, 7);
    EXPECT_EQ(tw1, "     50m");

    // 5 digits for Platform Altitude / Target Elevation
    const std::string alt1 = TacticalHudFilter::formatSt1909Meters(1428.0, 5);
    EXPECT_EQ(alt1, " 1428m");

    const std::string alt2 = TacticalHudFilter::formatSt1909Meters(-15.0, 5);
    EXPECT_EQ(alt2, "  -15m");

    // Nullopt / unavailable returns "N/A"
    const std::string na = TacticalHudFilter::formatSt1909Meters(std::nullopt, 5);
    EXPECT_EQ(na, "N/A");
}

TEST(TestTacticalHud, TimeFmt) {
    // 2019-06-30T13:45:29.8Z = 1561902329800000 microseconds
    const std::uint64_t epochUs = 1561902329800000ULL;
    const std::string timeStr = TacticalHudFilter::formatSt1909IsoTime(epochUs);
    EXPECT_EQ(timeStr, "2019-06-30T13:45:29.8Z");

    // 0 / nullopt returns "N/A"
    EXPECT_EQ(TacticalHudFilter::formatSt1909IsoTime(0ULL), "N/A");
    EXPECT_EQ(TacticalHudFilter::formatSt1909IsoTime(std::nullopt), "N/A");
}

TEST(TestTacticalHud, MisbRender) {
    TacticalHudFilter hud(TacticalHudFilter::HudMode::Misb1909);
    EXPECT_EQ(hud.getMode(), TacticalHudFilter::HudMode::Misb1909);

    // Populate platform and target data
    TacticalHudFilter::PlatformData plat {};
    plat.designation = "MQ-9";
    plat.tailNumber = "AF02-014";
    plat.latitudeDeg = 34.2500;
    plat.longitudeDeg = -116.1200;
    plat.altitudeM = 4500.0;
    plat.altitudeIsHae = true;
    plat.headingDeg = 270.0;
    plat.sensorAzimuthDeg = 45.0;
    plat.sensorElevationDeg = -15.0;
    plat.sensorPayload = "EO NOSE";
    plat.timestampUs = 1561902329800000ULL;
    hud.setPlatformData(plat);

    TacticalHudFilter::TargetData tgt {};
    tgt.latitudeDeg = 34.2600;
    tgt.longitudeDeg = -116.1100;
    tgt.elevationM = 500.0;
    tgt.slantRangeM = 5200.0;
    tgt.widthM = 150.0;
    tgt.isTrueLocation = false;
    tgt.elevationIsHae = false;
    hud.setTargetData(tgt);

    // Create 1920x1080 RGB24 test buffer filled with background color
    constexpr int width { 1920 };
    constexpr int height { 1080 };
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(width * height * 3), 0x20);

    // Count non-background pixels before
    const auto countModified = [&buffer]() -> std::size_t {
        std::size_t count { 0 };
        for (const auto b : buffer) {
            if (b != 0x20) {
                ++count;
            }
        }
        return count;
    };

    EXPECT_EQ(countModified(), 0U);

    // Apply HUD overlay
    hud.process(buffer.data(), width, height, Video::PixelFormat::RGB24);

    // Verify overlay pixels were rendered
    const std::size_t modifiedAfter = countModified();
    EXPECT_GT(modifiedAfter, 1000U);
}

TEST(TestTacticalHud, TargetTrueLoc) {
    TacticalHudFilter hud(TacticalHudFilter::HudMode::Misb1909);

    TacticalHudFilter::TargetData tgt {};
    tgt.latitudeDeg = 36.1234;
    tgt.longitudeDeg = -115.5678;
    tgt.elevationM = 620.0;
    tgt.isTrueLocation = true;
    tgt.elevationIsHae = true;
    hud.setTargetData(tgt);

    const auto retrieved = hud.getTargetData();
    EXPECT_TRUE(retrieved.isTrueLocation);
    EXPECT_TRUE(retrieved.elevationIsHae);
    ASSERT_TRUE(retrieved.latitudeDeg.has_value());
    EXPECT_DOUBLE_EQ(*retrieved.latitudeDeg, 36.1234);
}

TEST(TestTacticalHud, LaserPrf) {
    TacticalHudFilter hud(TacticalHudFilter::HudMode::Misb1909);

    TacticalHudFilter::LaserData laser {};
    laser.name = "LTD-500";
    laser.active = true;
    laser.prfCode = 1688U;
    hud.setLaserData(laser);

    const auto retrieved = hud.getLaserData();
    EXPECT_EQ(retrieved.name, "LTD-500");
    EXPECT_TRUE(retrieved.active);
    ASSERT_TRUE(retrieved.prfCode.has_value());
    EXPECT_EQ(*retrieved.prfCode, 1688U);
}

TEST(TestTacticalHud, IngestKlv) {
    TacticalHudFilter hud(TacticalHudFilter::HudMode::Misb1909);

    Klv::UasDatalinkMessage msg {};
    msg.platformDesignation = "ScanEagle";
    msg.platformTailNumber = "N123SE";
    msg.sensorLatitudeDeg = 32.5555;
    msg.sensorLongitudeDeg = -117.1111;
    msg.sensorAltitudeHaeM = 1500.0;
    msg.sensorRelAzimuthDeg = 180.5;
    msg.sensorRelElevationDeg = -30.2;
    msg.frameCenterLatDeg = 32.5600;
    msg.frameCenterLonDeg = -117.1150;
    msg.frameCenterElevM = 120.0;
    msg.slantRangeM = 2500.0;
    msg.targetWidthM = 80.0;
    msg.precisionTimeStampUs = 1561902329800000ULL;

    hud.updateTelemetry(msg);

    const auto plat = hud.getPlatformData();
    EXPECT_EQ(plat.designation, "ScanEagle");
    EXPECT_EQ(plat.tailNumber, "N123SE");
    ASSERT_TRUE(plat.latitudeDeg.has_value());
    EXPECT_DOUBLE_EQ(*plat.latitudeDeg, 32.5555);
    ASSERT_TRUE(plat.altitudeM.has_value());
    EXPECT_DOUBLE_EQ(*plat.altitudeM, 1500.0);
    EXPECT_TRUE(plat.altitudeIsHae);
    EXPECT_EQ(plat.timestampUs, 1561902329800000ULL);

    const auto tgt = hud.getTargetData();
    ASSERT_TRUE(tgt.latitudeDeg.has_value());
    EXPECT_DOUBLE_EQ(*tgt.latitudeDeg, 32.5600);
    ASSERT_TRUE(tgt.elevationM.has_value());
    EXPECT_DOUBLE_EQ(*tgt.elevationM, 120.0);
    EXPECT_FALSE(tgt.isTrueLocation);
    EXPECT_FALSE(tgt.elevationIsHae);
}
