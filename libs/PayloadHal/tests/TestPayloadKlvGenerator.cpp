/// @file TestPayloadKlvGenerator.cpp
/// @brief Unit tests for PayloadKlvGenerator STANAG 4609 / MISB ST 0601 telemetry generation.

#include "PayloadKlvGenerator.h"
#include "Klv/KlvParser.h"
#include "sim/SimulatedPayload.h"

#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>

namespace PayloadHal {
namespace {

    TEST(TestPayloadKlvGenerator, MetadataAndPlatformNavigation)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        PayloadKlvConfig config;
        config.missionId = "RECON_ALPHA";
        config.platformTailNumber = "HAWK_01";
        config.platformDesignation = "TACTICAL_UAV";
        config.imageSourceSensor = "VISIBLE_HD";
        config.uasLsVersion = 16U;

        PayloadKlvGenerator generator(payload, config);

        PlatformNavData nav;
        nav.position = { 37.7749, -122.4194, 2500.0 };
        nav.headingDeg = 45.0;
        nav.pitchDeg = 3.5;
        nav.rollDeg = -2.0;

        const uint64_t customTimeUs = 1680000000123456ULL;
        auto msg = generator.buildMessage(nav, customTimeUs);

        ASSERT_TRUE(msg.precisionTimeStampUs.has_value());
        EXPECT_EQ(*msg.precisionTimeStampUs, customTimeUs);

        ASSERT_TRUE(msg.missionId.has_value());
        EXPECT_EQ(*msg.missionId, "RECON_ALPHA");

        ASSERT_TRUE(msg.platformTailNumber.has_value());
        EXPECT_EQ(*msg.platformTailNumber, "HAWK_01");

        ASSERT_TRUE(msg.platformDesignation.has_value());
        EXPECT_EQ(*msg.platformDesignation, "TACTICAL_UAV");

        ASSERT_TRUE(msg.imageSourceSensor.has_value());
        EXPECT_EQ(*msg.imageSourceSensor, "VISIBLE_HD");

        ASSERT_TRUE(msg.platformHeadingDeg.has_value());
        EXPECT_NEAR(*msg.platformHeadingDeg, 45.0, 1e-4);

        ASSERT_TRUE(msg.platformPitchDeg.has_value());
        EXPECT_NEAR(*msg.platformPitchDeg, 3.5, 1e-4);

        ASSERT_TRUE(msg.platformRollDeg.has_value());
        EXPECT_NEAR(*msg.platformRollDeg, -2.0, 1e-4);

        ASSERT_TRUE(msg.sensorLatitudeDeg.has_value());
        EXPECT_NEAR(*msg.sensorLatitudeDeg, 37.7749, 1e-4);

        ASSERT_TRUE(msg.sensorLongitudeDeg.has_value());
        EXPECT_NEAR(*msg.sensorLongitudeDeg, -122.4194, 1e-4);

        ASSERT_TRUE(msg.sensorTrueAltitudeM.has_value());
        EXPECT_NEAR(*msg.sensorTrueAltitudeM, 2500.0, 1e-1);

        ASSERT_TRUE(msg.uasLsVersion.has_value());
        EXPECT_EQ(*msg.uasLsVersion, 16U);
    }

    TEST(TestPayloadKlvGenerator, GimbalAndCameraTelemetryMapping)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        // Set gimbal pan = 120.0 deg, tilt = -15.0 deg
        ASSERT_TRUE(payload->panTilt()->setAbsoluteAngles(120.0, -15.0));

        PayloadKlvGenerator generator(payload);

        PlatformNavData nav;
        nav.position = { 34.0, -118.0, 1000.0 };
        nav.headingDeg = 0.0;

        auto msg = generator.buildMessage(nav);

        ASSERT_TRUE(msg.sensorRelAzimuthDeg.has_value());
        EXPECT_NEAR(*msg.sensorRelAzimuthDeg, 120.0, 0.5);

        ASSERT_TRUE(msg.sensorRelElevationDeg.has_value());
        EXPECT_NEAR(*msg.sensorRelElevationDeg, -15.0, 0.5);

        ASSERT_TRUE(msg.sensorHfovDeg.has_value());
        EXPECT_GT(*msg.sensorHfovDeg, 0.0);

        ASSERT_TRUE(msg.sensorVfovDeg.has_value());
        EXPECT_GT(*msg.sensorVfovDeg, 0.0);
    }

    TEST(TestPayloadKlvGenerator, SlantRangeAndTargetProjection)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        // Platform 1000m altitude, tilt down -30 deg
        ASSERT_TRUE(payload->panTilt()->setAbsoluteAngles(0.0, -30.0));

        PayloadKlvGenerator generator(payload);

        PlatformNavData nav;
        nav.position = { 37.0, -122.0, 1000.0 };
        nav.headingDeg = 0.0;

        auto msg = generator.buildMessage(nav);

        // Even without physical LRF, slant range is derived from ground intersection
        ASSERT_TRUE(msg.slantRangeM.has_value());
        // For 1000m altitude at -30 deg, slant range = 1000 / sin(30 deg) = 2000m
        EXPECT_NEAR(*msg.slantRangeM, 2000.0, 50.0);

        // Target width should be computed based on HFOV
        ASSERT_TRUE(msg.targetWidthM.has_value());
        EXPECT_GT(*msg.targetWidthM, 0.0);

        // Frame center should be north of platform
        ASSERT_TRUE(msg.frameCenterLatDeg.has_value());
        EXPECT_GT(*msg.frameCenterLatDeg, 37.0);

        ASSERT_TRUE(msg.frameCenterLonDeg.has_value());
        EXPECT_NEAR(*msg.frameCenterLonDeg, -122.0, 0.01);
    }

    TEST(TestPayloadKlvGenerator, FrustumFootprintCorners)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        ASSERT_TRUE(payload->panTilt()->setAbsoluteAngles(0.0, -25.0));

        PayloadKlvConfig config;
        config.enableFrustumCorners = true;
        PayloadKlvGenerator generator(payload, config);

        PlatformNavData nav;
        nav.position = { 37.0, -122.0, 1000.0 };
        nav.headingDeg = 0.0;

        auto msg = generator.buildMessage(nav);

        ASSERT_TRUE(msg.cornerCoordinates.has_value());
        // Check corner 1 (topLeft)
        EXPECT_NE(msg.cornerCoordinates->topLeft.latitudeDeg, 0.0);
        EXPECT_NE(msg.cornerCoordinates->topLeft.longitudeDeg, 0.0);
    }

    TEST(TestPayloadKlvGenerator, RoundtripPacketEncodingAndParsing)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        ASSERT_TRUE(payload->panTilt()->setAbsoluteAngles(45.0, -20.0));

        PayloadKlvConfig config;
        config.missionId = "ROUNDTRIP_TEST";
        config.platformTailNumber = "N12345";
        config.uasLsVersion = 12U;
        PayloadKlvGenerator generator(payload, config);

        PlatformNavData nav;
        nav.position = { 32.5, -117.2, 1200.0 };
        nav.headingDeg = 180.0;
        nav.pitchDeg = 1.0;
        nav.rollDeg = -1.0;

        const auto packet = generator.generatePacket(nav);
        ASSERT_FALSE(packet.empty());
        EXPECT_GT(packet.size(), 20U);

        // Validate Universal Label prefix
        EXPECT_TRUE(Klv::KlvParser::isMisb0601(packet.data(), packet.size()));

        // Parse with KlvParser
        Klv::UasDatalinkMessage parsedMsg;
        const auto status = Klv::KlvParser::parse(packet.data(), packet.size(), parsedMsg, true);
        ASSERT_EQ(status, Klv::KlvStatus::Success);

        ASSERT_TRUE(parsedMsg.missionId.has_value());
        EXPECT_EQ(*parsedMsg.missionId, "ROUNDTRIP_TEST");

        ASSERT_TRUE(parsedMsg.platformTailNumber.has_value());
        EXPECT_EQ(*parsedMsg.platformTailNumber, "N12345");

        ASSERT_TRUE(parsedMsg.platformHeadingDeg.has_value());
        EXPECT_NEAR(*parsedMsg.platformHeadingDeg, 180.0, 0.01);

        ASSERT_TRUE(parsedMsg.sensorLatitudeDeg.has_value());
        EXPECT_NEAR(*parsedMsg.sensorLatitudeDeg, 32.5, 1e-4);

        ASSERT_TRUE(parsedMsg.sensorLongitudeDeg.has_value());
        EXPECT_NEAR(*parsedMsg.sensorLongitudeDeg, -117.2, 1e-4);

        ASSERT_TRUE(parsedMsg.sensorRelAzimuthDeg.has_value());
        EXPECT_NEAR(*parsedMsg.sensorRelAzimuthDeg, 45.0, 0.1);

        ASSERT_TRUE(parsedMsg.sensorRelElevationDeg.has_value());
        EXPECT_NEAR(*parsedMsg.sensorRelElevationDeg, -20.0, 0.1);
    }

} // namespace
} // namespace PayloadHal
