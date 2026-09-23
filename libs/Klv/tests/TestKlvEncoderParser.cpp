#include "KlvEncoder.h"
#include "KlvParser.h"
#include <gtest/gtest.h>

using namespace Klv;

TEST(KlvEncoderParserTest, FullRoundTrip) {
    UasDatalinkMessage original;
    original.precisionTimeStampUs = 1695500000123456ULL;
    original.missionId = "RECON_ALPHA_01";
    original.platformTailNumber = "HAWK-7";
    original.platformHeadingDeg = 145.2;
    original.platformPitchDeg = -4.5;
    original.platformRollDeg = 1.8;
    original.platformDesignation = "PTZ-MAST-100";
    original.imageSourceSensor = "SONY-FCB-EV9520L";
    original.imageCoordinateSystem = "WGS-84";
    original.sensorLatitudeDeg = 37.774929;
    original.sensorLongitudeDeg = -122.419416;
    original.sensorTrueAltitudeM = 1250.5;
    original.sensorHfovDeg = 32.4;
    original.sensorVfovDeg = 18.2;
    original.sensorRelAzimuthDeg = 45.6;
    original.sensorRelElevationDeg = -15.3;
    original.sensorRelRollDeg = 0.0;
    original.slantRangeM = 4850.0;
    original.targetWidthM = 150.0;
    original.frameCenterLatDeg = 37.801234;
    original.frameCenterLonDeg = -122.398765;
    original.frameCenterElevM = 15.0;

    FrustumCorners corners;
    corners.topLeft = { 37.805, -122.402 };
    corners.topRight = { 37.807, -122.395 };
    corners.bottomRight = { 37.798, -122.394 };
    corners.bottomLeft = { 37.796, -122.401 };
    original.cornerCoordinates = corners;

    SecurityMetadata sec;
    sec.classification = SecurityClassification::Secret;
    sec.classifyingCountry = "NATO";
    sec.sciShiInfo = "TACTICAL";
    sec.caveats = "REL TO NATO";
    original.security = sec;
    original.uasLsVersion = 16U;

    // 1. Encode
    const std::vector<std::uint8_t> encodedPacket = KlvEncoder::encode(original);
    ASSERT_GE(encodedPacket.size(), kUniversalLabelSize + 4U);

    // 2. Parse
    UasDatalinkMessage parsed;
    const KlvStatus status = KlvParser::parse(encodedPacket.data(), encodedPacket.size(), parsed, true);
    ASSERT_EQ(status, KlvStatus::Success);

    // 3. Verify values
    ASSERT_TRUE(parsed.precisionTimeStampUs.has_value());
    EXPECT_EQ(*parsed.precisionTimeStampUs, *original.precisionTimeStampUs);

    ASSERT_TRUE(parsed.missionId.has_value());
    EXPECT_EQ(*parsed.missionId, *original.missionId);

    ASSERT_TRUE(parsed.platformTailNumber.has_value());
    EXPECT_EQ(*parsed.platformTailNumber, *original.platformTailNumber);

    ASSERT_TRUE(parsed.platformHeadingDeg.has_value());
    EXPECT_NEAR(*parsed.platformHeadingDeg, *original.platformHeadingDeg, 0.01);

    ASSERT_TRUE(parsed.platformPitchDeg.has_value());
    EXPECT_NEAR(*parsed.platformPitchDeg, *original.platformPitchDeg, 0.01);

    ASSERT_TRUE(parsed.platformRollDeg.has_value());
    EXPECT_NEAR(*parsed.platformRollDeg, *original.platformRollDeg, 0.01);

    ASSERT_TRUE(parsed.sensorLatitudeDeg.has_value());
    EXPECT_NEAR(*parsed.sensorLatitudeDeg, *original.sensorLatitudeDeg, 1e-6);

    ASSERT_TRUE(parsed.sensorLongitudeDeg.has_value());
    EXPECT_NEAR(*parsed.sensorLongitudeDeg, *original.sensorLongitudeDeg, 1e-6);

    ASSERT_TRUE(parsed.sensorTrueAltitudeM.has_value());
    EXPECT_NEAR(*parsed.sensorTrueAltitudeM, *original.sensorTrueAltitudeM, 0.5);

    ASSERT_TRUE(parsed.sensorHfovDeg.has_value());
    EXPECT_NEAR(*parsed.sensorHfovDeg, *original.sensorHfovDeg, 0.01);

    ASSERT_TRUE(parsed.sensorVfovDeg.has_value());
    EXPECT_NEAR(*parsed.sensorVfovDeg, *original.sensorVfovDeg, 0.01);

    ASSERT_TRUE(parsed.sensorRelAzimuthDeg.has_value());
    EXPECT_NEAR(*parsed.sensorRelAzimuthDeg, *original.sensorRelAzimuthDeg, 0.01);

    ASSERT_TRUE(parsed.sensorRelElevationDeg.has_value());
    EXPECT_NEAR(*parsed.sensorRelElevationDeg, *original.sensorRelElevationDeg, 0.01);

    ASSERT_TRUE(parsed.slantRangeM.has_value());
    EXPECT_NEAR(*parsed.slantRangeM, *original.slantRangeM, 2.0);

    ASSERT_TRUE(parsed.frameCenterLatDeg.has_value());
    EXPECT_NEAR(*parsed.frameCenterLatDeg, *original.frameCenterLatDeg, 1e-6);

    ASSERT_TRUE(parsed.frameCenterLonDeg.has_value());
    EXPECT_NEAR(*parsed.frameCenterLonDeg, *original.frameCenterLonDeg, 1e-6);

    ASSERT_TRUE(parsed.cornerCoordinates.has_value());
    EXPECT_NEAR(parsed.cornerCoordinates->topLeft.latitudeDeg, corners.topLeft.latitudeDeg, 1e-6);
    EXPECT_NEAR(parsed.cornerCoordinates->bottomRight.longitudeDeg, corners.bottomRight.longitudeDeg, 1e-6);

    ASSERT_TRUE(parsed.security.has_value());
    EXPECT_EQ(parsed.security->classification, SecurityClassification::Secret);
    EXPECT_EQ(parsed.security->classifyingCountry, "NATO");

    ASSERT_TRUE(parsed.uasLsVersion.has_value());
    EXPECT_EQ(*parsed.uasLsVersion, 16U);
}

TEST(KlvEncoderParserTest, CrcCorruptionRejection) {
    UasDatalinkMessage msg;
    msg.platformHeadingDeg = 180.0;
    auto packet = KlvEncoder::encode(msg);

    // Corrupt one data byte
    packet[packet.size() - 5] ^= 0x55U;

    UasDatalinkMessage parsed;
    EXPECT_EQ(KlvParser::parse(packet.data(), packet.size(), parsed, true), KlvStatus::CrcMismatch);
}

TEST(KlvEncoderParserTest, InvalidUniversalLabelRejection) {
    UasDatalinkMessage msg;
    msg.platformHeadingDeg = 90.0;
    auto packet = KlvEncoder::encode(msg);

    packet[0] = 0xAAU; // Corrupt first byte of Universal Label

    UasDatalinkMessage parsed;
    EXPECT_EQ(KlvParser::parse(packet.data(), packet.size(), parsed, true), KlvStatus::InvalidUniversalLabel);
}
