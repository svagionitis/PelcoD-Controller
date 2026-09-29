#include "n2k/N2kDecoder.h"
#include "n2k/N2kFastPacketAssembler.h"
#include "n2k/N2kTypes.h"

#include <cstring>
#include <gtest/gtest.h>

namespace Nmea::N2k {

TEST(TestN2kDecoder, Pgn129025PositionRapid)
{
    PositionRapid posIn {};
    posIn.latitudeDeg = 37.7749295;
    posIn.longitudeDeg = -122.4194155;
    posIn.isValid = true;

    const auto encoded = N2kDecoder::encodePgn129025(posIn);
    ASSERT_EQ(encoded.size(), 8U);

    PositionRapid posOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn129025(encoded.data(), encoded.size(), posOut));
    EXPECT_TRUE(posOut.isValid);
    EXPECT_NEAR(posOut.latitudeDeg, 37.7749295, 1e-6);
    EXPECT_NEAR(posOut.longitudeDeg, -122.4194155, 1e-6);

    // Test Data Unavailable Sentinel (0x7FFFFFFF)
    PositionRapid posUnavail {};
    posUnavail.isValid = false;
    const auto unavailBytes = N2kDecoder::encodePgn129025(posUnavail);
    PositionRapid posDecodedUnavail {};
    ASSERT_TRUE(N2kDecoder::parsePgn129025(unavailBytes.data(), unavailBytes.size(), posDecodedUnavail));
    EXPECT_FALSE(posDecodedUnavail.isValid);
}

TEST(TestN2kDecoder, Pgn129026CogSogRapid)
{
    CogSogRapid cogSogIn {};
    cogSogIn.sid = 12U;
    cogSogIn.cogDegrees = 245.5;
    cogSogIn.sogKnots = 18.4;
    cogSogIn.cogReference = HeadingReference::True;
    cogSogIn.hasCog = true;
    cogSogIn.hasSog = true;

    const auto encoded = N2kDecoder::encodePgn129026(cogSogIn);
    ASSERT_EQ(encoded.size(), 8U);

    CogSogRapid cogSogOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn129026(encoded.data(), encoded.size(), cogSogOut));
    EXPECT_EQ(cogSogOut.sid, 12U);
    EXPECT_TRUE(cogSogOut.hasCog);
    EXPECT_TRUE(cogSogOut.hasSog);
    EXPECT_NEAR(cogSogOut.cogDegrees, 245.5, 0.05);
    EXPECT_NEAR(cogSogOut.sogKnots, 18.4, 0.05);
    EXPECT_EQ(cogSogOut.cogReference, HeadingReference::True);
}

TEST(TestN2kDecoder, Pgn127250VesselHeading)
{
    VesselHeading hdgIn {};
    hdgIn.sid = 5U;
    hdgIn.headingDegrees = 135.2;
    hdgIn.deviationDegrees = -1.5;
    hdgIn.variationDegrees = 3.2;
    hdgIn.reference = HeadingReference::Magnetic;
    hdgIn.hasHeading = true;
    hdgIn.hasDeviation = true;
    hdgIn.hasVariation = true;

    const auto encoded = N2kDecoder::encodePgn127250(hdgIn);
    ASSERT_EQ(encoded.size(), 8U);

    VesselHeading hdgOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn127250(encoded.data(), encoded.size(), hdgOut));
    EXPECT_EQ(hdgOut.sid, 5U);
    EXPECT_TRUE(hdgOut.hasHeading);
    EXPECT_TRUE(hdgOut.hasDeviation);
    EXPECT_TRUE(hdgOut.hasVariation);
    EXPECT_NEAR(hdgOut.headingDegrees, 135.2, 0.05);
    EXPECT_NEAR(hdgOut.deviationDegrees, -1.5, 0.05);
    EXPECT_NEAR(hdgOut.variationDegrees, 3.2, 0.05);
    EXPECT_EQ(hdgOut.reference, HeadingReference::Magnetic);
}

TEST(TestN2kDecoder, Pgn127257Attitude)
{
    Attitude attIn {};
    attIn.sid = 8U;
    attIn.yawDegrees = 45.0;
    attIn.pitchDegrees = 3.5;
    attIn.rollDegrees = -2.1;
    attIn.hasYaw = true;
    attIn.hasPitch = true;
    attIn.hasRoll = true;

    const auto encoded = N2kDecoder::encodePgn127257(attIn);
    ASSERT_EQ(encoded.size(), 8U);

    Attitude attOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn127257(encoded.data(), encoded.size(), attOut));
    EXPECT_EQ(attOut.sid, 8U);
    EXPECT_TRUE(attOut.hasYaw);
    EXPECT_TRUE(attOut.hasPitch);
    EXPECT_TRUE(attOut.hasRoll);
    EXPECT_NEAR(attOut.yawDegrees, 45.0, 0.05);
    EXPECT_NEAR(attOut.pitchDegrees, 3.5, 0.05);
    EXPECT_NEAR(attOut.rollDegrees, -2.1, 0.05);
}

TEST(TestN2kDecoder, Pgn130306WindData)
{
    WindData windIn {};
    windIn.sid = 2U;
    windIn.windSpeedMps = 10.5;
    windIn.windAngleDegrees = 65.0;
    windIn.reference = WindReference::Apparent;
    windIn.hasWindSpeed = true;
    windIn.hasWindAngle = true;

    const auto encoded = N2kDecoder::encodePgn130306(windIn);
    ASSERT_EQ(encoded.size(), 6U);

    WindData windOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn130306(encoded.data(), encoded.size(), windOut));
    EXPECT_EQ(windOut.sid, 2U);
    EXPECT_TRUE(windOut.hasWindSpeed);
    EXPECT_TRUE(windOut.hasWindAngle);
    EXPECT_NEAR(windOut.windSpeedMps, 10.5, 0.05);
    EXPECT_NEAR(windOut.windAngleDegrees, 65.0, 0.05);
    EXPECT_EQ(windOut.reference, WindReference::Apparent);
}

TEST(TestN2kDecoder, Pgn129038AisClassAPosition)
{
    // 28-byte payload representing AIS Class A message
    std::vector<std::uint8_t> payload(28U, 0xFFU);
    payload[0] = 1U; // messageId = 1
    payload[1] = 0x00; // repeat = 0, navStatus = 0

    const std::uint32_t mmsi = 235001234U;
    std::memcpy(&payload[2], &mmsi, sizeof(mmsi));

    // Longitude: -122.4194155 * 1e7 = -1224194155 = 0xB7071F95
    const std::int32_t lonRaw = -1224194155;
    std::memcpy(&payload[6], &lonRaw, sizeof(lonRaw));

    // Latitude: 37.7749295 * 1e7 = 377749295 = 0x1683F72F
    const std::int32_t latRaw = 377749295;
    std::memcpy(&payload[10], &latRaw, sizeof(latRaw));

    // COG = 180.0 deg -> rad = pi -> raw = pi / 0.0001 = 31416 = 0x7AA8
    const std::uint16_t cogRaw = 31416U;
    std::memcpy(&payload[15], &cogRaw, sizeof(cogRaw));

    // SOG = 10.0 m/s -> raw = 1000 = 0x03E8
    const std::uint16_t sogRaw = 1000U;
    std::memcpy(&payload[17], &sogRaw, sizeof(sogRaw));

    // Heading = 180.0 deg -> 31416
    std::memcpy(&payload[20], &cogRaw, sizeof(cogRaw));

    AisClassAPosition aisOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn129038(payload.data(), payload.size(), aisOut));
    EXPECT_EQ(aisOut.messageId, 1U);
    EXPECT_EQ(aisOut.mmsi, 235001234U);
    EXPECT_TRUE(aisOut.positionValid);
    EXPECT_NEAR(aisOut.longitudeDeg, -122.4194155, 1e-6);
    EXPECT_NEAR(aisOut.latitudeDeg, 37.7749295, 1e-6);
    EXPECT_TRUE(aisOut.hasCog);
    EXPECT_NEAR(aisOut.cogDegrees, 180.0, 0.1);
    EXPECT_TRUE(aisOut.hasSog);
    EXPECT_NEAR(aisOut.sogKnots, 10.0 * 1.943844, 0.1);
    EXPECT_TRUE(aisOut.hasHeading);
    EXPECT_NEAR(aisOut.headingDegrees, 180.0, 0.1);
}

TEST(TestN2kDecoder, SplitAndReassembleFastPacket)
{
    N2kHeader hdr {};
    hdr.pgn = static_cast<std::uint32_t>(Pgn::AisClassAPositionReport);
    hdr.sourceAddress = 77U;

    std::vector<std::uint8_t> payload(40U);
    for (std::size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<std::uint8_t>(i ^ 0xAA);
    }

    const auto frames = N2kDecoder::splitFastPacket(hdr, payload.data(), payload.size(), 14U);
    ASSERT_GT(frames.size(), 1U);

    N2kFastPacketAssembler assembler;
    std::optional<N2kMessage> reassembled;
    for (const auto& frame : frames) {
        reassembled = assembler.processCanFrame(frame);
    }

    ASSERT_TRUE(reassembled.has_value());
    EXPECT_EQ(reassembled->header.pgn, static_cast<std::uint32_t>(Pgn::AisClassAPositionReport));
    EXPECT_EQ(reassembled->header.sourceAddress, 77U);
    EXPECT_EQ(reassembled->payload, payload);
}

TEST(TestN2kDecoder, Pgn127245Rudder)
{
    RudderData in {};
    in.instance = 0U;
    in.directionOrder = RudderDirectionOrder::MoveToStarboard;
    in.positionDegrees = 15.5;
    in.angleOrderDegrees = 16.0;
    in.hasPosition = true;
    in.hasAngleOrder = true;

    const auto encoded = N2kDecoder::encodePgn127245(in);
    ASSERT_EQ(encoded.size(), 8U);

    RudderData out {};
    ASSERT_TRUE(N2kDecoder::parsePgn127245(encoded.data(), encoded.size(), out));
    EXPECT_EQ(out.instance, 0U);
    EXPECT_EQ(out.directionOrder, RudderDirectionOrder::MoveToStarboard);
    EXPECT_TRUE(out.hasPosition);
    EXPECT_TRUE(out.hasAngleOrder);
    EXPECT_NEAR(out.positionDegrees, 15.5, 0.05);
    EXPECT_NEAR(out.angleOrderDegrees, 16.0, 0.05);
}

TEST(TestN2kDecoder, Pgn127258MagneticVariation)
{
    MagneticVariation in {};
    in.sid = 1U;
    in.source = VariationSource::Calculation;
    in.ageOfServiceDays = 1500U;
    in.variationDegrees = -4.5; // 4.5 West
    in.hasVariation = true;

    const auto encoded = N2kDecoder::encodePgn127258(in);
    ASSERT_EQ(encoded.size(), 8U);

    MagneticVariation out {};
    ASSERT_TRUE(N2kDecoder::parsePgn127258(encoded.data(), encoded.size(), out));
    EXPECT_EQ(out.sid, 1U);
    EXPECT_EQ(out.source, VariationSource::Calculation);
    EXPECT_EQ(out.ageOfServiceDays, 1500U);
    EXPECT_TRUE(out.hasVariation);
    EXPECT_NEAR(out.variationDegrees, -4.5, 0.05);
}

TEST(TestN2kDecoder, Pgn126992SystemTime)
{
    SystemTimeData in {};
    in.sid = 42U;
    in.timeSource = 0U;
    in.systemDateDays = 20350U;
    in.secondsSinceMidnight = 43200.5; // 12:00:00.5
    in.hasTime = true;
    in.hasDate = true;

    const auto encoded = N2kDecoder::encodePgn126992(in);
    ASSERT_EQ(encoded.size(), 8U);

    SystemTimeData out {};
    ASSERT_TRUE(N2kDecoder::parsePgn126992(encoded.data(), encoded.size(), out));
    EXPECT_EQ(out.sid, 42U);
    EXPECT_EQ(out.timeSource, 0U);
    EXPECT_TRUE(out.hasTime);
    EXPECT_TRUE(out.hasDate);
    EXPECT_EQ(out.systemDateDays, 20350U);
    EXPECT_NEAR(out.secondsSinceMidnight, 43200.5, 0.01);
}

TEST(TestN2kDecoder, Pgn126993Heartbeat)
{
    HeartbeatData in {};
    in.transmitIntervalMs = 1000U;
    in.sequenceCounter = 55U;
    in.equipmentStatus = 0U;
    in.valid = true;

    const auto encoded = N2kDecoder::encodePgn126993(in);
    ASSERT_EQ(encoded.size(), 8U);

    HeartbeatData out {};
    ASSERT_TRUE(N2kDecoder::parsePgn126993(encoded.data(), encoded.size(), out));
    EXPECT_EQ(out.transmitIntervalMs, 1000U);
    EXPECT_EQ(out.sequenceCounter, 55U);
    EXPECT_EQ(out.equipmentStatus, 0U);
}

TEST(TestN2kDecoder, Pgn126464PgnList)
{
    PgnListData in {};
    in.isTransmitList = true;
    in.pgnList = { 129025U, 129026U, 127250U, 127257U, 127245U };

    const auto encoded = N2kDecoder::encodePgn126464(in);
    ASSERT_EQ(encoded.size(), 1U + 5U * 3U);

    PgnListData out {};
    ASSERT_TRUE(N2kDecoder::parsePgn126464(encoded.data(), encoded.size(), out));
    EXPECT_TRUE(out.isTransmitList);
    ASSERT_EQ(out.pgnList.size(), 5U);
    EXPECT_EQ(out.pgnList[0], 129025U);
    EXPECT_EQ(out.pgnList[1], 129026U);
    EXPECT_EQ(out.pgnList[2], 127250U);
    EXPECT_EQ(out.pgnList[3], 127257U);
    EXPECT_EQ(out.pgnList[4], 127245U);
}

} // namespace Nmea::N2k
