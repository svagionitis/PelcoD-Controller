#include <gtest/gtest.h>

#include "KlvCrc.h"
#include "KlvEncoder.h"
#include "KlvParser.h"
#include "RvtEncoder.h"
#include "RvtParser.h"
#include "RvtTypes.h"

#include <cmath>
#include <string>
#include <vector>

using namespace Klv;

TEST(TestRvtLocalSet, Crc32MpegCalculation) {
    // Basic test string: "123456789"
    // Standard MPEG-2 CRC32 for "123456789" (ASCII) is 0x0376E6E7
    const std::string testStr = "123456789";
    const auto* data = reinterpret_cast<const std::uint8_t*>(testStr.data());
    const std::uint32_t crc = KlvCrc::calculateCrc32Mpeg(data, testStr.size());

    EXPECT_EQ(crc, 0x0376E6E7U);
}

TEST(TestRvtLocalSet, UniversalLabelsAndIdentification) {
    EXPECT_TRUE(RvtParser::isRvt(kMisb0806UniversalLabel.data(), kMisb0806UniversalLabel.size()));

    std::array<std::uint8_t, 16> invalidLabel = kMisb0806UniversalLabel;
    invalidLabel[0] = 0x00U;
    EXPECT_FALSE(RvtParser::isRvt(invalidLabel.data(), invalidLabel.size()));
    EXPECT_FALSE(RvtParser::isRvt(nullptr, 0U));
}

TEST(TestRvtLocalSet, PoiEncodingAndParsing) {
    PoiPack originalPoi {};
    originalPoi.poiNumber = 42U;
    originalPoi.latitudeDeg = 34.0522;
    originalPoi.longitudeDeg = -118.2437;
    originalPoi.altitudeMslM = 1500.5;
    originalPoi.type = RvtTargetType::Target;
    originalPoi.text = "Primary Target Location";
    originalPoi.sourceIcon = "SFGPU----------";
    originalPoi.sourceId = "Sensor-Alpha-1";
    originalPoi.label = "TGT-ALPHA";
    originalPoi.operationId = "OP-RESOLUTE";

    std::vector<std::uint8_t> encoded;
    RvtEncoder::encodePoi(originalPoi, encoded);
    ASSERT_FALSE(encoded.empty());

    PoiPack decodedPoi {};
    ASSERT_TRUE(RvtParser::parsePoi(encoded.data(), encoded.size(), decodedPoi));

    EXPECT_EQ(decodedPoi.poiNumber, 42U);
    EXPECT_NEAR(decodedPoi.latitudeDeg, 34.0522, 1e-5);
    EXPECT_NEAR(decodedPoi.longitudeDeg, -118.2437, 1e-5);
    ASSERT_TRUE(decodedPoi.altitudeMslM.has_value());
    EXPECT_NEAR(*decodedPoi.altitudeMslM, 1500.5, 0.5);
    ASSERT_TRUE(decodedPoi.type.has_value());
    EXPECT_EQ(*decodedPoi.type, RvtTargetType::Target);
    ASSERT_TRUE(decodedPoi.text.has_value());
    EXPECT_EQ(*decodedPoi.text, "Primary Target Location");
    ASSERT_TRUE(decodedPoi.sourceIcon.has_value());
    EXPECT_EQ(*decodedPoi.sourceIcon, "SFGPU----------");
    ASSERT_TRUE(decodedPoi.sourceId.has_value());
    EXPECT_EQ(*decodedPoi.sourceId, "Sensor-Alpha-1");
    ASSERT_TRUE(decodedPoi.label.has_value());
    EXPECT_EQ(*decodedPoi.label, "TGT-ALPHA");
    ASSERT_TRUE(decodedPoi.operationId.has_value());
    EXPECT_EQ(*decodedPoi.operationId, "OP-RESOLUTE");
}

TEST(TestRvtLocalSet, AoiEncodingAndParsing) {
    AoiPack originalAoi {};
    originalAoi.aoiNumber = 7U;
    originalAoi.corner1Nw = GeoPoint2D { 35.120, -119.500 };
    originalAoi.corner3Se = GeoPoint2D { 35.080, -119.450 };
    originalAoi.type = RvtTargetType::Hostile;
    originalAoi.text = "Restricted Sector Bravo";
    originalAoi.sourceId = "HQ-AIR-OPS";
    originalAoi.label = "SECTOR-B";
    originalAoi.operationId = "OP-WATCHFUL";

    std::vector<std::uint8_t> encoded;
    RvtEncoder::encodeAoi(originalAoi, encoded);
    ASSERT_FALSE(encoded.empty());

    AoiPack decodedAoi {};
    ASSERT_TRUE(RvtParser::parseAoi(encoded.data(), encoded.size(), decodedAoi));

    EXPECT_EQ(decodedAoi.aoiNumber, 7U);
    EXPECT_NEAR(decodedAoi.corner1Nw.latitudeDeg, 35.120, 1e-5);
    EXPECT_NEAR(decodedAoi.corner1Nw.longitudeDeg, -119.500, 1e-5);
    EXPECT_NEAR(decodedAoi.corner3Se.latitudeDeg, 35.080, 1e-5);
    EXPECT_NEAR(decodedAoi.corner3Se.longitudeDeg, -119.450, 1e-5);
    EXPECT_EQ(decodedAoi.type, RvtTargetType::Hostile);
    ASSERT_TRUE(decodedAoi.text.has_value());
    EXPECT_EQ(*decodedAoi.text, "Restricted Sector Bravo");
    ASSERT_TRUE(decodedAoi.sourceId.has_value());
    EXPECT_EQ(*decodedAoi.sourceId, "HQ-AIR-OPS");
    ASSERT_TRUE(decodedAoi.label.has_value());
    EXPECT_EQ(*decodedAoi.label, "SECTOR-B");
    ASSERT_TRUE(decodedAoi.operationId.has_value());
    EXPECT_EQ(*decodedAoi.operationId, "OP-WATCHFUL");
}

TEST(TestRvtLocalSet, UserDefinedEncodingAndParsing) {
    UserDefinedPack originalUserDef {};
    originalUserDef.numericId = 15U;
    originalUserDef.dataType = RvtUserDataType::Uint;
    originalUserDef.data = { 0xDEU, 0xADU, 0xBEU, 0xEFU };

    std::vector<std::uint8_t> encoded;
    RvtEncoder::encodeUserDefined(originalUserDef, encoded);
    ASSERT_FALSE(encoded.empty());

    UserDefinedPack decodedUserDef {};
    ASSERT_TRUE(RvtParser::parseUserDefined(encoded.data(), encoded.size(), decodedUserDef));

    EXPECT_EQ(decodedUserDef.numericId, 15U);
    EXPECT_EQ(decodedUserDef.dataType, RvtUserDataType::Uint);
    EXPECT_EQ(decodedUserDef.data, originalUserDef.data);
}

TEST(TestRvtLocalSet, StandalonePacketRoundTripWithCrc32) {
    RvtLocalSet originalRvt {};
    originalRvt.precisionTimeStampUs = 1700000000123456ULL;
    originalRvt.platformTrueAirspeedMps = static_cast<std::uint16_t>(145U);
    originalRvt.platformIndicatedAirspeedMps = static_cast<std::uint16_t>(138U);
    originalRvt.telemetryAccuracy = static_cast<std::uint8_t>(1U);
    originalRvt.fragCircleRadiusM = static_cast<std::uint16_t>(250U);
    originalRvt.frameCode = 1234567U;
    originalRvt.version = static_cast<std::uint8_t>(2U);
    originalRvt.videoDataRate = 5000000U;
    originalRvt.digitalVideoFileFormat = "H.264";

    RvtMgrsCoord acMgrs {};
    acMgrs.zone = static_cast<std::uint8_t>(11U);
    acMgrs.bandAndGridSquare = "SMT";
    acMgrs.eastingM = 12345U;
    acMgrs.northingM = 67890U;
    originalRvt.aircraftMgrs = acMgrs;

    RvtMgrsCoord fcMgrs {};
    fcMgrs.zone = static_cast<std::uint8_t>(11U);
    fcMgrs.bandAndGridSquare = "SMT";
    fcMgrs.eastingM = 12500U;
    fcMgrs.northingM = 68000U;
    originalRvt.frameCenterMgrs = fcMgrs;

    PoiPack poi {};
    poi.poiNumber = 1U;
    poi.latitudeDeg = 36.1234;
    poi.longitudeDeg = -115.1234;
    poi.altitudeMslM = 750.0;
    originalRvt.pois.push_back(poi);

    AoiPack aoi {};
    aoi.aoiNumber = 2U;
    aoi.corner1Nw = GeoPoint2D { 36.200, -115.200 };
    aoi.corner3Se = GeoPoint2D { 36.100, -115.100 };
    aoi.type = RvtTargetType::Friendly;
    originalRvt.aois.push_back(aoi);

    const auto packet = RvtEncoder::encode(originalRvt, true);
    ASSERT_FALSE(packet.empty());

    // Check UL presence
    EXPECT_TRUE(RvtParser::isRvt(packet.data(), packet.size()));

    // Decode with checksum validation
    RvtLocalSet decodedRvt {};
    const auto status = RvtParser::parse(packet.data(), packet.size(), decodedRvt, true);
    ASSERT_EQ(status, KlvStatus::Success);

    ASSERT_TRUE(decodedRvt.precisionTimeStampUs.has_value());
    EXPECT_EQ(*decodedRvt.precisionTimeStampUs, 1700000000123456ULL);
    ASSERT_TRUE(decodedRvt.platformTrueAirspeedMps.has_value());
    EXPECT_EQ(*decodedRvt.platformTrueAirspeedMps, 145U);
    ASSERT_TRUE(decodedRvt.platformIndicatedAirspeedMps.has_value());
    EXPECT_EQ(*decodedRvt.platformIndicatedAirspeedMps, 138U);
    ASSERT_TRUE(decodedRvt.fragCircleRadiusM.has_value());
    EXPECT_EQ(*decodedRvt.fragCircleRadiusM, 250U);
    ASSERT_TRUE(decodedRvt.frameCode.has_value());
    EXPECT_EQ(*decodedRvt.frameCode, 1234567U);
    EXPECT_EQ(decodedRvt.version, 2U);
    ASSERT_TRUE(decodedRvt.videoDataRate.has_value());
    EXPECT_EQ(*decodedRvt.videoDataRate, 5000000U);
    ASSERT_TRUE(decodedRvt.digitalVideoFileFormat.has_value());
    EXPECT_EQ(*decodedRvt.digitalVideoFileFormat, "H.264");

    ASSERT_TRUE(decodedRvt.aircraftMgrs.has_value());
    EXPECT_EQ(decodedRvt.aircraftMgrs->zone, 11U);
    EXPECT_EQ(decodedRvt.aircraftMgrs->bandAndGridSquare, "SMT");
    EXPECT_EQ(decodedRvt.aircraftMgrs->eastingM, 12345U);
    EXPECT_EQ(decodedRvt.aircraftMgrs->northingM, 67890U);

    ASSERT_EQ(decodedRvt.pois.size(), 1U);
    EXPECT_EQ(decodedRvt.pois[0].poiNumber, 1U);
    EXPECT_NEAR(decodedRvt.pois[0].latitudeDeg, 36.1234, 1e-5);
    EXPECT_NEAR(decodedRvt.pois[0].longitudeDeg, -115.1234, 1e-5);

    ASSERT_EQ(decodedRvt.aois.size(), 1U);
    EXPECT_EQ(decodedRvt.aois[0].aoiNumber, 2U);
    EXPECT_EQ(decodedRvt.aois[0].type, RvtTargetType::Friendly);

    // Corrupt the CRC32 checksum byte and ensure CrcMismatch
    auto corrupted = packet;
    corrupted.back() ^= 0xFFU;
    RvtLocalSet badRvt {};
    EXPECT_EQ(RvtParser::parse(corrupted.data(), corrupted.size(), badRvt, true), KlvStatus::CrcMismatch);
}

TEST(TestRvtLocalSet, EmbeddedInUasDatalinkMessage) {
    UasDatalinkMessage originalMsg {};
    originalMsg.precisionTimeStampUs = 1600000000000000ULL;
    originalMsg.platformHeadingDeg = 180.0;
    originalMsg.sensorLatitudeDeg = 32.5;
    originalMsg.sensorLongitudeDeg = -117.2;
    originalMsg.sensorTrueAltitudeM = 5000.0;

    RvtLocalSet originalRvt {};
    originalRvt.precisionTimeStampUs = 1600000000000000ULL;
    originalRvt.platformTrueAirspeedMps = static_cast<std::uint16_t>(120U);
    originalRvt.version = static_cast<std::uint8_t>(2U);

    PoiPack poi {};
    poi.poiNumber = 101U;
    poi.latitudeDeg = 32.51;
    poi.longitudeDeg = -117.21;
    poi.altitudeMslM = 300.0;
    originalRvt.pois.push_back(poi);

    originalMsg.rvt = originalRvt;

    // Encode ST 0601 with nested ST 0806 Tag 73
    const auto klvBytes = KlvEncoder::encode(originalMsg);
    ASSERT_FALSE(klvBytes.empty());

    // Parse ST 0601 and verify nested RVT Local Set
    UasDatalinkMessage decodedMsg {};
    const auto status = KlvParser::parse(klvBytes.data(), klvBytes.size(), decodedMsg, true);
    ASSERT_EQ(status, KlvStatus::Success);

    ASSERT_TRUE(decodedMsg.sensorLatitudeDeg.has_value());
    EXPECT_NEAR(*decodedMsg.sensorLatitudeDeg, 32.5, 1e-4);

    ASSERT_TRUE(decodedMsg.rvt.has_value());
    EXPECT_EQ(decodedMsg.rvt->version, 2U);
    ASSERT_TRUE(decodedMsg.rvt->platformTrueAirspeedMps.has_value());
    EXPECT_EQ(*decodedMsg.rvt->platformTrueAirspeedMps, 120U);

    ASSERT_EQ(decodedMsg.rvt->pois.size(), 1U);
    EXPECT_EQ(decodedMsg.rvt->pois[0].poiNumber, 101U);
    EXPECT_NEAR(decodedMsg.rvt->pois[0].latitudeDeg, 32.51, 1e-5);
    EXPECT_NEAR(decodedMsg.rvt->pois[0].longitudeDeg, -117.21, 1e-5);
}
