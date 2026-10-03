#include "KlvEncoder.h"
#include "KlvParser.h"
#include "KlvCrc.h"
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
    original.uasLsVersion = static_cast<std::uint8_t>(16U);

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

TEST(KlvEncoderParserTest, Misb0601OffsetCorners2ByteDecoding) {
    // Manually construct a minimal ST 0601 packet containing:
    // Tag 2: Timestamp (8 bytes)
    // Tag 23: Frame Center Lat = 37.800000 deg
    // Tag 24: Frame Center Lon = -122.400000 deg
    // Tag 26: Offset Corner Lat 1 (2 bytes) = +0.005 deg offset -> 0.005 * (32767 / 0.075) = 2184
    // Tag 27: Offset Corner Lon 1 (2 bytes) = -0.002 deg offset -> -0.002 * (32767 / 0.075) = -874
    // Tag 65: Version 16
    // Tag 1: Checksum (2 bytes)
    std::vector<std::uint8_t> payload;

    // Tag 2: Timestamp
    payload.push_back(0x02U); payload.push_back(0x08U);
    for (int i = 0; i < 8; ++i) payload.push_back(0x01U);

    // Tag 23: Frame Center Lat = 37.8 deg -> 37.8 * (2147483647 / 90) = 901943132 = 0x35C28F5C
    payload.push_back(0x17U); payload.push_back(0x04U);
    payload.push_back(0x35U); payload.push_back(0xC2U); payload.push_back(0x8FU); payload.push_back(0x5CU);

    // Tag 24: Frame Center Lon = -122.4 deg -> -122.4 * (2147483647 / 180) = -1460288880 = 0xA8F5C290
    payload.push_back(0x18U); payload.push_back(0x04U);
    payload.push_back(0xA8U); payload.push_back(0xF5U); payload.push_back(0xC2U); payload.push_back(0x90U);

    // Tag 26: Offset Corner Lat 1 = 2184 = 0x0888 (2 bytes)
    payload.push_back(0x1AU); payload.push_back(0x02U);
    payload.push_back(0x08U); payload.push_back(0x88U);

    // Tag 27: Offset Corner Lon 1 = -874 = 0xFC96 (2 bytes)
    payload.push_back(0x1BU); payload.push_back(0x02U);
    payload.push_back(0xFCU); payload.push_back(0x96U);

    // Tag 65: Version 16
    payload.push_back(0x41U); payload.push_back(0x01U); payload.push_back(0x10U);

    // Tag 1 overhead
    payload.push_back(0x01U); payload.push_back(0x02U);

    std::vector<std::uint8_t> packet(kMisb0601UniversalLabel.begin(), kMisb0601UniversalLabel.end());
    packet.push_back(static_cast<std::uint8_t>(payload.size() + 2U)); // BER length
    packet.insert(packet.end(), payload.begin(), payload.end());

    const std::uint16_t crc = KlvCrc::calculate(packet.data(), packet.size());
    packet.push_back(static_cast<std::uint8_t>((crc >> 8U) & 0xFFU));
    packet.push_back(static_cast<std::uint8_t>(crc & 0xFFU));

    UasDatalinkMessage parsed;
    const KlvStatus status = KlvParser::parse(packet.data(), packet.size(), parsed, true);
    ASSERT_EQ(status, KlvStatus::Success);
    ASSERT_TRUE(parsed.cornerCoordinates.has_value());
    EXPECT_NEAR(parsed.cornerCoordinates->topLeft.latitudeDeg, 37.805, 1e-4);
    EXPECT_NEAR(parsed.cornerCoordinates->topLeft.longitudeDeg, -122.402, 1e-4);
}

TEST(KlvEncoderParserTest, Misb0102SecuritySubTagsCompliance) {
    // Construct raw MISB ST 0102 payload:
    // Tag 1 (1 byte): 0x04 (Secret)
    // Tag 2 (1 byte): 0x01 (ISO-3166-2)
    // Tag 3 (4 bytes): "NATO"
    // Tag 4 (8 bytes): "TACTICAL"
    // Tag 5 (11 bytes): "REL TO NATO"
    // Tag 6 (5 bytes): "NOFOR"
    // Tag 22 (1 byte): 0x0D (Version 13)
    std::vector<std::uint8_t> secPayload;
    secPayload.push_back(0x01U); secPayload.push_back(0x01U); secPayload.push_back(0x04U); // Tag 1: Classification
    secPayload.push_back(0x02U); secPayload.push_back(0x01U); secPayload.push_back(0x01U); // Tag 2: Coding Method
    secPayload.push_back(0x03U); secPayload.push_back(0x04U); // Tag 3: Country "NATO"
    secPayload.insert(secPayload.end(), {'N', 'A', 'T', 'O'});
    secPayload.push_back(0x04U); secPayload.push_back(0x08U); // Tag 4: SCI/SHI "TACTICAL"
    secPayload.insert(secPayload.end(), {'T', 'A', 'C', 'T', 'I', 'C', 'A', 'L'});
    secPayload.push_back(0x05U); secPayload.push_back(0x0BU); // Tag 5: Caveats "REL TO NATO"
    secPayload.insert(secPayload.end(), {'R', 'E', 'L', ' ', 'T', 'O', ' ', 'N', 'A', 'T', 'O'});
    secPayload.push_back(0x06U); secPayload.push_back(0x05U); // Tag 6: Releasing "NOFOR"
    secPayload.insert(secPayload.end(), {'N', 'O', 'F', 'O', 'R'});
    secPayload.push_back(0x16U); secPayload.push_back(0x01U); secPayload.push_back(0x0DU); // Tag 22: Version 13

    SecurityMetadata sec;
    const KlvStatus status = KlvParser::parseSecurityLocalSet(secPayload.data(), secPayload.size(), sec);
    ASSERT_EQ(status, KlvStatus::Success);
    EXPECT_EQ(sec.classification, SecurityClassification::Secret);
    EXPECT_EQ(sec.countryCodingMethod, 1U);
    EXPECT_EQ(sec.classifyingCountry, "NATO");
    EXPECT_EQ(sec.sciShiInfo, "TACTICAL");
    EXPECT_EQ(sec.caveats, "REL TO NATO");
    EXPECT_EQ(sec.releasingInstructions, "NOFOR");
    EXPECT_EQ(sec.version, 13U);
}

TEST(KlvEncoderParserTest, Misb0601MandatoryTagsEnforced) {
    UasDatalinkMessage msg;
    // Don't set security or version or timestamp; KlvEncoder must default and include them
    const auto packet = KlvEncoder::encode(msg);

    UasDatalinkMessage parsed;
    const KlvStatus status = KlvParser::parse(packet.data(), packet.size(), parsed, true);
    ASSERT_EQ(status, KlvStatus::Success);

    // Mandatory tags per MISB ST 0601:
    EXPECT_TRUE(parsed.precisionTimeStampUs.has_value());
    EXPECT_TRUE(parsed.security.has_value());
    EXPECT_TRUE(parsed.uasLsVersion.has_value());
    EXPECT_EQ(*parsed.uasLsVersion, 16U);
}

