#include <gtest/gtest.h>

#include "GeoRegistrationTypes.h"
#include "KlvEncoder.h"
#include "KlvParser.h"
#include "KlvTypes.h"
#include "St1607Encoder.h"
#include "St1607Parser.h"
#include "St1607Types.h"

#include <array>
#include <vector>

using namespace Klv;

TEST(TestSt1607LocalSet, UniversalLabelIdentification) {
    EXPECT_TRUE(St1607Parser::isAmendLocalSet(AmendLocalSetUl.data(), AmendLocalSetUl.size()));
    EXPECT_TRUE(St1607Parser::isSegmentLocalSet(SegmentLocalSetUl.data(), SegmentLocalSetUl.size()));

    EXPECT_FALSE(St1607Parser::isAmendLocalSet(SegmentLocalSetUl.data(), SegmentLocalSetUl.size()));
    EXPECT_FALSE(St1607Parser::isSegmentLocalSet(AmendLocalSetUl.data(), AmendLocalSetUl.size()));

    EXPECT_FALSE(St1607Parser::isAmendLocalSet(nullptr, 0U));
    EXPECT_FALSE(St1607Parser::isSegmentLocalSet(nullptr, 15U));
}

TEST(TestSt1607LocalSet, MsidPackLocalIdRoundTrip) {
    MetadataSubstreamId msid {};
    msid.localId = 42U; // Local ID > 0, UUID truncated

    std::vector<std::uint8_t> encoded {};
    ASSERT_TRUE(St1607Encoder::encodeMsid(msid, encoded));
    EXPECT_FALSE(encoded.empty());

    MetadataSubstreamId decoded {};
    std::size_t bytesRead { 0U };
    ASSERT_TRUE(St1607Parser::parseMsid(encoded.data(), encoded.size(), decoded, bytesRead));
    EXPECT_EQ(decoded.localId, 42U);
    EXPECT_FALSE(decoded.universalId.has_value());
    EXPECT_EQ(bytesRead, encoded.size());
}

TEST(TestSt1607LocalSet, MsidPackUniversalIdRoundTrip) {
    MetadataSubstreamId msid {};
    msid.localId = 0U; // Universal ID active
    msid.universalId = std::array<std::uint8_t, 16> {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    std::vector<std::uint8_t> encoded {};
    ASSERT_TRUE(St1607Encoder::encodeMsid(msid, encoded));
    EXPECT_EQ(encoded.size(), 17U); // 1 byte localId (0) + 16 bytes UUID

    MetadataSubstreamId decoded {};
    std::size_t bytesRead { 0U };
    ASSERT_TRUE(St1607Parser::parseMsid(encoded.data(), encoded.size(), decoded, bytesRead));
    EXPECT_EQ(decoded.localId, 0U);
    ASSERT_TRUE(decoded.universalId.has_value());
    EXPECT_EQ(*decoded.universalId, *msid.universalId);
    EXPECT_EQ(bytesRead, 17U);
}

TEST(TestSt1607LocalSet, StandaloneAmendLocalSetPacket) {
    AmendLocalSet amendSet {};
    amendSet.msid.localId = 1U;
    amendSet.sensorLatitudeDeg = 34.0522;
    amendSet.sensorLongitudeDeg = -118.2437;
    amendSet.sensorTrueAltitudeM = 1250.0;
    amendSet.sensorHfovDeg = 15.5;

    std::vector<std::uint8_t> packet {};
    ASSERT_EQ(St1607Encoder::encodePacket(amendSet, packet), KlvStatus::Success);
    EXPECT_FALSE(packet.empty());

    EXPECT_TRUE(St1607Parser::isAmendLocalSet(packet.data(), packet.size()));

    AmendLocalSet decodedSet {};
    ASSERT_EQ(St1607Parser::parsePacket(packet.data(), packet.size(), decodedSet), KlvStatus::Success);

    EXPECT_EQ(decodedSet.msid.localId, 1U);
    ASSERT_TRUE(decodedSet.sensorLatitudeDeg.has_value());
    EXPECT_NEAR(*decodedSet.sensorLatitudeDeg, 34.0522, 1e-4);
    ASSERT_TRUE(decodedSet.sensorLongitudeDeg.has_value());
    EXPECT_NEAR(*decodedSet.sensorLongitudeDeg, -118.2437, 1e-4);
    ASSERT_TRUE(decodedSet.sensorTrueAltitudeM.has_value());
    EXPECT_NEAR(*decodedSet.sensorTrueAltitudeM, 1250.0, 0.5);
    ASSERT_TRUE(decodedSet.sensorHfovDeg.has_value());
    EXPECT_NEAR(*decodedSet.sensorHfovDeg, 15.5, 0.01);
}

TEST(TestSt1607LocalSet, StandaloneSegmentLocalSetPacket) {
    SegmentLocalSet segSet {};
    segSet.msid.localId = 5U;
    segSet.sensorHfovDeg = 30.0;
    segSet.sensorVfovDeg = 20.0;
    segSet.imageSourceSensor = "EO-Wide";

    std::vector<std::uint8_t> packet {};
    ASSERT_EQ(St1607Encoder::encodePacket(segSet, packet), KlvStatus::Success);
    EXPECT_FALSE(packet.empty());

    EXPECT_TRUE(St1607Parser::isSegmentLocalSet(packet.data(), packet.size()));

    SegmentLocalSet decodedSet {};
    ASSERT_EQ(St1607Parser::parsePacket(packet.data(), packet.size(), decodedSet), KlvStatus::Success);

    EXPECT_EQ(decodedSet.msid.localId, 5U);
    ASSERT_TRUE(decodedSet.sensorHfovDeg.has_value());
    EXPECT_NEAR(*decodedSet.sensorHfovDeg, 30.0, 0.01);
    ASSERT_TRUE(decodedSet.sensorVfovDeg.has_value());
    EXPECT_NEAR(*decodedSet.sensorVfovDeg, 20.0, 0.01);
    ASSERT_TRUE(decodedSet.imageSourceSensor.has_value());
    EXPECT_EQ(*decodedSet.imageSourceSensor, "EO-Wide");
}

TEST(TestSt1607LocalSet, AmendLocalSetWithGeoRegistration) {
    AmendLocalSet amendSet {};
    amendSet.msid.localId = 9U;
    amendSet.sensorLatitudeDeg = 32.1234;

    GeoRegistrationLocalSet geoReg {};
    geoReg.documentVersion = 2U;
    geoReg.algorithmName = "SIFT_MATCH";
    geoReg.algorithmVersion = "2.1";
    geoReg.pixelPoints = { { 100U, 200U, 102U, 204U } };
    amendSet.geoRegistration = geoReg;

    std::vector<std::uint8_t> encoded {};
    ASSERT_EQ(St1607Encoder::encodeAmend(amendSet, encoded), KlvStatus::Success);

    AmendLocalSet decodedSet {};
    ASSERT_EQ(St1607Parser::parseAmend(encoded.data(), encoded.size(), decodedSet), KlvStatus::Success);

    EXPECT_EQ(decodedSet.msid.localId, 9U);
    ASSERT_TRUE(decodedSet.geoRegistration.has_value());
    EXPECT_EQ(decodedSet.geoRegistration->algorithmName, "SIFT_MATCH");
    EXPECT_EQ(decodedSet.geoRegistration->pixelPoints.size(), 1U);
}

TEST(TestSt1607LocalSet, St1607RulesValidation) {
    // ST 1607.2-08: Parent Amend LS cannot nest a Segment LS
    AmendLocalSet parentAmend {};
    parentAmend.msid.localId = 1U;

    AmendLocalSet childAmend {};
    childAmend.msid.localId = 2U;
    parentAmend.childAmends.push_back(childAmend);
    EXPECT_TRUE(parentAmend.validate());

    // ST 1607.2-07 & ST 1607.2-08 compliance on Segment LS
    SegmentLocalSet parentSegment {};
    parentSegment.msid.localId = 10U;

    SegmentLocalSet childSegment {};
    childSegment.msid.localId = 11U;
    parentSegment.childSegments.push_back(childSegment);

    // Segment CAN nest Amend LS
    parentSegment.childAmends.push_back(childAmend);
    EXPECT_TRUE(parentSegment.validate());
}

TEST(TestSt1607LocalSet, UnionAndOverrideResolution) {
    UasDatalinkMessage baseMsg {};
    baseMsg.platformHeadingDeg = 90.0;
    baseMsg.sensorLatitudeDeg = 35.0;
    baseMsg.sensorLongitudeDeg = -120.0;
    baseMsg.sensorHfovDeg = 25.0;

    AmendLocalSet amendSet {};
    amendSet.msid.localId = 1U;
    amendSet.sensorLatitudeDeg = 35.001; // Amended corrected latitude
    amendSet.sensorLongitudeDeg = -120.002; // Amended corrected longitude

    UasDatalinkMessage effectiveMsg = baseMsg;
    amendSet.applyTo(effectiveMsg);

    // Unchanged base fields preserved
    ASSERT_TRUE(effectiveMsg.platformHeadingDeg.has_value());
    EXPECT_NEAR(*effectiveMsg.platformHeadingDeg, 90.0, 0.01);
    ASSERT_TRUE(effectiveMsg.sensorHfovDeg.has_value());
    EXPECT_NEAR(*effectiveMsg.sensorHfovDeg, 25.0, 0.01);

    // Overridden fields updated
    ASSERT_TRUE(effectiveMsg.sensorLatitudeDeg.has_value());
    EXPECT_NEAR(*effectiveMsg.sensorLatitudeDeg, 35.001, 1e-4);
    ASSERT_TRUE(effectiveMsg.sensorLongitudeDeg.has_value());
    EXPECT_NEAR(*effectiveMsg.sensorLongitudeDeg, -120.002, 1e-4);
}

TEST(TestSt1607LocalSet, Misb0601IntegrationTags100And101) {
    UasDatalinkMessage msg {};
    msg.precisionTimeStampUs = 1700000000000000ULL;
    msg.sensorLatitudeDeg = 30.0;
    msg.sensorLongitudeDeg = -100.0;

    // Add Tag 100 Segment
    SegmentLocalSet seg {};
    seg.msid.localId = 2U;
    seg.sensorHfovDeg = 12.0;
    msg.segments.push_back(seg);

    // Add Tag 101 Amend
    AmendLocalSet amend {};
    amend.msid.localId = 3U;
    amend.sensorLatitudeDeg = 30.0005;
    msg.amends.push_back(amend);

    const auto encodedPacket = KlvEncoder::encode(msg);
    ASSERT_FALSE(encodedPacket.empty());

    UasDatalinkMessage decodedMsg {};
    ASSERT_EQ(KlvParser::parse(encodedPacket.data(), encodedPacket.size(), decodedMsg), KlvStatus::Success);

    ASSERT_EQ(decodedMsg.segments.size(), 1U);
    EXPECT_EQ(decodedMsg.segments[0].msid.localId, 2U);
    ASSERT_TRUE(decodedMsg.segments[0].sensorHfovDeg.has_value());
    EXPECT_NEAR(*decodedMsg.segments[0].sensorHfovDeg, 12.0, 0.01);

    ASSERT_EQ(decodedMsg.amends.size(), 1U);
    EXPECT_EQ(decodedMsg.amends[0].msid.localId, 3U);
    ASSERT_TRUE(decodedMsg.amends[0].sensorLatitudeDeg.has_value());
    EXPECT_NEAR(*decodedMsg.amends[0].sensorLatitudeDeg, 30.0005, 1e-4);
}
