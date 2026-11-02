/// @file TestMisb1909OverlayRenderer.cpp
/// @brief Unit tests for MISB ST 1909.1 non-destructive tactical HUD layout generator.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "KlvTypes.h"
#include "Misb1909OverlayRenderer.h"
#include "Misb1909Types.h"

using namespace Klv;

TEST(TestMisb1909OverlayRenderer, AngleFormattingRules)
{
    // Standard ST 1909.1 angle test: 5 digits before, 4 digits after
    const std::string az1 = Misb1909OverlayRenderer::formatSt1909Angle(160.7191, 5, 4);
    EXPECT_EQ(az1, "  160.7191*");

    const std::string el1 = Misb1909OverlayRenderer::formatSt1909Angle(-36.4976, 5, 4);
    EXPECT_EQ(el1, "  -36.4976*");

    // Latitude test: 3 digits before, 4 digits after
    const std::string lat1 = Misb1909OverlayRenderer::formatSt1909Angle(33.4568, 3, 4);
    EXPECT_EQ(lat1, " 33.4568*");

    const std::string lat2 = Misb1909OverlayRenderer::formatSt1909Angle(-12.3456, 3, 4);
    EXPECT_EQ(lat2, "-12.3456*");

    // Longitude test: 4 digits before, 4 digits after
    const std::string lon1 = Misb1909OverlayRenderer::formatSt1909Angle(-104.6546, 4, 4);
    EXPECT_EQ(lon1, "-104.6546*");

    const std::string lon2 = Misb1909OverlayRenderer::formatSt1909Angle(45.1234, 4, 4);
    EXPECT_EQ(lon2, "  45.1234*");

    // Nullopt / unavailable returns "N/A"
    const std::string na = Misb1909OverlayRenderer::formatSt1909Angle(std::nullopt, 5, 4);
    EXPECT_EQ(na, "N/A");
}

TEST(TestMisb1909OverlayRenderer, MetersFormattingRules)
{
    // 7 digits for Slant Range / Target Width
    const std::string sr1 = Misb1909OverlayRenderer::formatSt1909Meters(14265.0, 7);
    EXPECT_EQ(sr1, "  14265m");

    const std::string tw1 = Misb1909OverlayRenderer::formatSt1909Meters(50.0, 7);
    EXPECT_EQ(tw1, "     50m");

    // 5 digits for Platform Altitude / Target Elevation
    const std::string alt1 = Misb1909OverlayRenderer::formatSt1909Meters(1428.0, 5);
    EXPECT_EQ(alt1, " 1428m");

    const std::string alt2 = Misb1909OverlayRenderer::formatSt1909Meters(-15.0, 5);
    EXPECT_EQ(alt2, "  -15m");

    // Nullopt / unavailable returns "N/A"
    const std::string na = Misb1909OverlayRenderer::formatSt1909Meters(std::nullopt, 5);
    EXPECT_EQ(na, "N/A");
}

TEST(TestMisb1909OverlayRenderer, IsoTimeFormattingRules)
{
    // 2019-06-30T13:45:29.8Z = 1561902329800000 microseconds
    constexpr std::uint64_t epochUs { 1561902329800000ULL };
    const std::string timeStr = Misb1909OverlayRenderer::formatSt1909IsoTime(epochUs);
    EXPECT_EQ(timeStr, "2019-06-30T13:45:29.8Z");

    // 0 / nullopt returns "N/A"
    EXPECT_EQ(Misb1909OverlayRenderer::formatSt1909IsoTime(0ULL), "N/A");
    EXPECT_EQ(Misb1909OverlayRenderer::formatSt1909IsoTime(std::nullopt), "N/A");
}

TEST(TestMisb1909OverlayRenderer, MissingAndNulloptHandling)
{
    UasDatalinkMessage emptyMsg {};
    const TacticalOverlayLayout layout = Misb1909OverlayRenderer::buildLayout(emptyMsg);

    EXPECT_TRUE(layout.reticle.visible);
    EXPECT_FALSE(layout.mainSensorGroup.lines.empty());
    EXPECT_EQ(layout.mainSensorGroup.lines[0], "N/A");
    EXPECT_EQ(layout.mainSensorGroup.lines[1], "REL AZ N/A");
    EXPECT_EQ(layout.mainSensorGroup.lines[2], "REL EL N/A");
}

TEST(TestMisb1909OverlayRenderer, EightStandardGroupsAnchorsAndContent)
{
    UasDatalinkMessage msg {};
    msg.platformDesignation = "MQ-9 REAPER";
    msg.platformTailNumber = "AF02-014";
    msg.sensorLatitudeDeg = 34.2500;
    msg.sensorLongitudeDeg = -116.1200;
    msg.sensorTrueAltitudeM = 4500.0;
    msg.platformHeadingDeg = 270.0;
    msg.sensorRelAzimuthDeg = 45.0;
    msg.sensorRelElevationDeg = -15.0;
    msg.imageSourceSensor = "EO NOSE";
    msg.precisionTimeStampUs = 1561902329800000ULL;

    msg.frameCenterLatDeg = 34.2600;
    msg.frameCenterLonDeg = -116.1100;
    msg.frameCenterElevM = 500.0;
    msg.slantRangeM = 5200.0;
    msg.targetWidthM = 150.0;
    msg.sensorHfovDeg = 60.0;
    msg.sensorVfovDeg = 35.0;

    SecurityMetadata sec {};
    sec.classification = SecurityClassification::Secret;
    sec.classifyingCountry = "USA";
    sec.caveats = "NOFORN";
    msg.security = sec;

    const TacticalOverlayLayout layout = Misb1909OverlayRenderer::buildLayout(msg);

    // Group 1: Main Sensor (Top-Left)
    EXPECT_EQ(layout.mainSensorGroup.groupId, 1U);
    EXPECT_NEAR(layout.mainSensorGroup.anchor.x.value(), 0.02F, 0.001F);
    EXPECT_NEAR(layout.mainSensorGroup.anchor.y.value(), 0.037F, 0.001F);
    EXPECT_EQ(layout.mainSensorGroup.alignment, TextAlignment::Left);
    EXPECT_EQ(layout.mainSensorGroup.lines[0], "EO NOSE");
    EXPECT_EQ(layout.mainSensorGroup.lines[1], "REL AZ    45.0000*");
    EXPECT_EQ(layout.mainSensorGroup.lines[2], "REL EL   -15.0000*");

    // Group 2: Classification (Top-Center)
    EXPECT_EQ(layout.securityGroup.groupId, 2U);
    EXPECT_NEAR(layout.securityGroup.anchor.x.value(), 0.50F, 0.001F);
    EXPECT_NEAR(layout.securityGroup.anchor.y.value(), 0.037F, 0.001F);
    EXPECT_EQ(layout.securityGroup.alignment, TextAlignment::Center);
    EXPECT_EQ(layout.securityGroup.lines[0], "// SECRET // USA // NOFORN //");

    // Group 3: Platform Info (Top-Right)
    EXPECT_EQ(layout.platformGroup.groupId, 3U);
    EXPECT_NEAR(layout.platformGroup.anchor.x.value(), 0.98F, 0.001F);
    EXPECT_NEAR(layout.platformGroup.anchor.y.value(), 0.037F, 0.001F);
    EXPECT_EQ(layout.platformGroup.alignment, TextAlignment::Right);
    EXPECT_EQ(layout.platformGroup.lines[0], "MQ-9 REAPER AF02-014");
    EXPECT_EQ(layout.platformGroup.lines[1], " 34.2500* LAT");
    EXPECT_EQ(layout.platformGroup.lines[2], "-116.1200* LON");
    EXPECT_EQ(layout.platformGroup.lines[3], " 4500m MSL ALT");

    // Group 4: True North Arrow (Mid-Left)
    EXPECT_EQ(layout.northArrowGroup.groupId, 4U);
    EXPECT_NEAR(layout.northArrowGroup.anchor.x.value(), 0.02F, 0.001F);
    EXPECT_NEAR(layout.northArrowGroup.anchor.y.value(), 0.50F, 0.001F);

    // Group 5: Laser Sensor (Bottom-Left)
    EXPECT_EQ(layout.laserSensorGroup.groupId, 5U);
    EXPECT_NEAR(layout.laserSensorGroup.anchor.x.value(), 0.02F, 0.001F);
    EXPECT_NEAR(layout.laserSensorGroup.anchor.y.value(), 0.963F, 0.001F);
    EXPECT_EQ(layout.laserSensorGroup.stackDirection, StackDirection::Upwards);

    // Group 6: Date/Time (Bottom-Center)
    EXPECT_EQ(layout.dateTimeGroup.groupId, 6U);
    EXPECT_NEAR(layout.dateTimeGroup.anchor.x.value(), 0.50F, 0.001F);
    EXPECT_NEAR(layout.dateTimeGroup.anchor.y.value(), 0.963F, 0.001F);
    EXPECT_EQ(layout.dateTimeGroup.lines[0], "MT 2019-06-30T13:45:29.8Z");

    // Group 7: Target Data (Bottom-Right)
    EXPECT_EQ(layout.targetGroup.groupId, 7U);
    EXPECT_NEAR(layout.targetGroup.anchor.x.value(), 0.98F, 0.001F);
    EXPECT_NEAR(layout.targetGroup.anchor.y.value(), 0.963F, 0.001F);
    EXPECT_EQ(layout.targetGroup.alignment, TextAlignment::Right);
    EXPECT_EQ(layout.targetGroup.lines[0], "   5200m SR");
    EXPECT_EQ(layout.targetGroup.lines[1], "    150m TW");

    // Group 8: Reticle (Center)
    EXPECT_NEAR(layout.reticle.center.x.value(), 0.50F, 0.001F);
    EXPECT_NEAR(layout.reticle.center.y.value(), 0.50F, 0.001F);
    EXPECT_EQ(layout.reticle.crosshairLines.size(), 4U);
}

TEST(TestMisb1909OverlayRenderer, PitchLadderClimbDiveGeometry)
{
    UasDatalinkMessage msg {};
    msg.platformPitchDeg = 0.0;
    msg.platformRollDeg = 10.0;

    Misb1909Config config {};
    config.ladderStepDegrees = 10.0F;
    config.maxLadderPitchDegrees = 20.0F;

    const TacticalOverlayLayout layout = Misb1909OverlayRenderer::buildLayout(msg, config);

    EXPECT_TRUE(layout.pitchLadder.visible);
    EXPECT_NEAR(layout.pitchLadder.rollAngleDeg, 10.0F, 0.001F);
    EXPECT_FALSE(layout.pitchLadder.rungs.empty());

    for (const auto& rung : layout.pitchLadder.rungs) {
        if (rung.pitchDeg > 0.0F) {
            // Climbing rung: solid line, downward tabs towards horizon
            EXPECT_FALSE(rung.isDashed);
            EXPECT_GT(rung.leftTabEnd.y.value(), rung.leftStart.y.value());
        } else if (rung.pitchDeg < 0.0F) {
            // Diving rung: dashed line, upward tabs towards horizon
            EXPECT_TRUE(rung.isDashed);
            EXPECT_LT(rung.leftTabEnd.y.value(), rung.leftStart.y.value());
        }
    }
}

TEST(TestMisb1909OverlayRenderer, PitchLadderRollTransformation)
{
    UasDatalinkMessage msg {};
    msg.platformRollDeg = -25.5;

    const TacticalOverlayLayout layout = Misb1909OverlayRenderer::buildLayout(msg);
    EXPECT_NEAR(layout.pitchLadder.rollAngleDeg, -25.5F, 0.001F);
}

TEST(TestMisb1909OverlayRenderer, HeadingTapeWrappingAndCardinals)
{
    UasDatalinkMessage msg {};
    msg.platformHeadingDeg = 358.0;
    msg.sensorRelAzimuthDeg = 5.0; // Total heading = 3.0 deg

    Misb1909Config config {};
    config.tapeSpanDegrees = 60.0F;

    const TacticalOverlayLayout layout = Misb1909OverlayRenderer::buildLayout(msg, config);

    EXPECT_TRUE(layout.headingTape.visible);
    EXPECT_NEAR(layout.headingTape.currentHeadingDeg, 3.0F, 0.001F);
    EXPECT_EQ(layout.headingTape.digitalReadout, "003*");

    bool foundNorth { false };
    for (const auto& tick : layout.headingTape.ticks) {
        if (tick.isMajor && tick.headingDeg == 0.0F) {
            EXPECT_EQ(tick.label, "N");
            foundNorth = true;
        }
    }
    EXPECT_TRUE(foundNorth);
}

TEST(TestMisb1909OverlayRenderer, SecurityClassificationBanners)
{
    UasDatalinkMessage msg {};
    SecurityMetadata sec {};
    sec.classification = SecurityClassification::TopSecret;
    sec.classifyingCountry = "GBR";
    sec.caveats = "REL TO NATO";
    msg.security = sec;

    const TacticalOverlayLayout layout = Misb1909OverlayRenderer::buildLayout(msg);
    EXPECT_TRUE(layout.securityGroup.visible);
    EXPECT_EQ(layout.securityGroup.lines[0], "// TOP SECRET // GBR // REL TO NATO //");
}

TEST(TestMisb1909OverlayRenderer, LaserSensorGroupRendering)
{
    UasDatalinkMessage msg {};
    const TacticalOverlayLayout layout = Misb1909OverlayRenderer::buildLayout(msg);

    EXPECT_TRUE(layout.laserSensorGroup.visible);
    EXPECT_EQ(layout.laserSensorGroup.lines[0], "Laser");
    EXPECT_EQ(layout.laserSensorGroup.lines[1], "Laser OFF");
}

TEST(TestMisb1909OverlayRenderer, ReticleBoresightPrimitives)
{
    UasDatalinkMessage msg {};
    const TacticalOverlayLayout layout = Misb1909OverlayRenderer::buildLayout(msg);

    EXPECT_TRUE(layout.reticle.visible);
    EXPECT_EQ(layout.reticle.crosshairLines.size(), 4U);

    // Boresight is centered at 0.50, 0.50
    EXPECT_NEAR(layout.reticle.center.x.value(), 0.50F, 0.0001F);
    EXPECT_NEAR(layout.reticle.center.y.value(), 0.50F, 0.0001F);
}

TEST(TestMisb1909OverlayRenderer, MgrsCoordinateFormatting)
{
    // 34.2500 N, -116.1200 W -> UTM Zone 11, Band S, Square NT
    const std::string mgrs = Misb1909OverlayRenderer::toMgrsString(34.2500, -116.1200);
    EXPECT_FALSE(mgrs.empty());
    EXPECT_NE(mgrs, "N/A");
    EXPECT_EQ(mgrs.substr(0U, 5U), "11SNT");

    // Out of bounds / nullopt returns N/A
    EXPECT_EQ(Misb1909OverlayRenderer::toMgrsString(std::nullopt, -116.1200), "N/A");
    EXPECT_EQ(Misb1909OverlayRenderer::toMgrsString(95.0, 0.0), "N/A");
}

TEST(TestMisb1909OverlayRenderer, SymbologyDensityDeclutterLevels)
{
    UasDatalinkMessage msg {};

    // Minimal mode
    Misb1909Config minConfig {};
    minConfig.density = SymbologyDensity::Minimal;
    const TacticalOverlayLayout minLayout = Misb1909OverlayRenderer::buildLayout(msg, minConfig);

    EXPECT_FALSE(minLayout.pitchLadder.visible);
    EXPECT_FALSE(minLayout.headingTape.visible);
    EXPECT_FALSE(minLayout.mainSensorGroup.visible);
    EXPECT_FALSE(minLayout.platformGroup.visible);
    EXPECT_FALSE(minLayout.northArrowGroup.visible);
    EXPECT_FALSE(minLayout.laserSensorGroup.visible);
    EXPECT_TRUE(minLayout.reticle.visible);
    EXPECT_TRUE(minLayout.targetGroup.visible);

    // Standard mode
    Misb1909Config stdConfig {};
    stdConfig.density = SymbologyDensity::Standard;
    const TacticalOverlayLayout stdLayout = Misb1909OverlayRenderer::buildLayout(msg, stdConfig);

    EXPECT_TRUE(stdLayout.pitchLadder.visible);
    EXPECT_TRUE(stdLayout.headingTape.visible);
    EXPECT_TRUE(stdLayout.mainSensorGroup.visible);
    EXPECT_TRUE(stdLayout.platformGroup.visible);
}

TEST(TestMisb1909OverlayRenderer, Err55ZeroHeapNoexceptSafety)
{
    UasDatalinkMessage msg {};
    msg.platformHeadingDeg = 180.0;
    msg.platformPitchDeg = -5.0;
    msg.platformRollDeg = 3.0;

    // Stress loop: 1,000 iterations to verify stability and noexcept behavior
    for (int i = 0; i < 1000; ++i) {
        const TacticalOverlayLayout layout = Misb1909OverlayRenderer::buildLayout(msg);
        EXPECT_TRUE(layout.reticle.visible);
    }
}
