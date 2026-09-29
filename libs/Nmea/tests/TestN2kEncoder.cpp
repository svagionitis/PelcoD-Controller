#include "n2k/N2kDecoder.h"
#include "n2k/N2kEncoder.h"
#include "n2k/N2kFastPacketAssembler.h"
#include "n2k/N2kTypes.h"

#include <gtest/gtest.h>

namespace Nmea::N2k {

TEST(TestN2kEncoder, Pgn129025PositionRapid)
{
    PositionRapid posIn {};
    posIn.latitudeDeg = 40.712776;
    posIn.longitudeDeg = -74.005974;
    posIn.isValid = true;

    const CanFrame frame = N2kEncoder::encodePositionRapid(posIn, 0x42U, 2U);
    EXPECT_EQ(frame.dlc, 8U);

    const auto hdr = N2kHeader::fromCanId(frame.id);
    EXPECT_EQ(hdr.priority, 2U);
    EXPECT_EQ(hdr.sourceAddress, 0x42U);
    EXPECT_EQ(hdr.pgn, static_cast<std::uint32_t>(Pgn::PositionRapidUpdate));

    PositionRapid posOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn129025(frame.data.data(), frame.dlc, posOut));
    EXPECT_TRUE(posOut.isValid);
    EXPECT_NEAR(posOut.latitudeDeg, 40.712776, 1e-6);
    EXPECT_NEAR(posOut.longitudeDeg, -74.005974, 1e-6);

    // Test invalid / sentinel
    PositionRapid posInvalid {};
    posInvalid.isValid = false;
    const CanFrame invalidFrame = N2kEncoder::encodePositionRapid(posInvalid);
    PositionRapid posOutInvalid {};
    ASSERT_TRUE(N2kDecoder::parsePgn129025(invalidFrame.data.data(), invalidFrame.dlc, posOutInvalid));
    EXPECT_FALSE(posOutInvalid.isValid);
}

TEST(TestN2kEncoder, Pgn129026CogSogRapid)
{
    CogSogRapid cogSogIn {};
    cogSogIn.sid = 5U;
    cogSogIn.cogDegrees = 185.2;
    cogSogIn.sogKnots = 22.8;
    cogSogIn.cogReference = HeadingReference::True;
    cogSogIn.hasCog = true;
    cogSogIn.hasSog = true;

    const CanFrame frame = N2kEncoder::encodeCogSogRapid(cogSogIn, 0x1CU, 3U);
    EXPECT_EQ(frame.dlc, 8U);

    const auto hdr = N2kHeader::fromCanId(frame.id);
    EXPECT_EQ(hdr.priority, 3U);
    EXPECT_EQ(hdr.sourceAddress, 0x1CU);
    EXPECT_EQ(hdr.pgn, static_cast<std::uint32_t>(Pgn::CogSogRapidUpdate));

    CogSogRapid cogSogOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn129026(frame.data.data(), frame.dlc, cogSogOut));
    EXPECT_EQ(cogSogOut.sid, 5U);
    EXPECT_TRUE(cogSogOut.hasCog);
    EXPECT_TRUE(cogSogOut.hasSog);
    EXPECT_NEAR(cogSogOut.cogDegrees, 185.2, 0.05);
    EXPECT_NEAR(cogSogOut.sogKnots, 22.8, 0.05);
}

TEST(TestN2kEncoder, Pgn127250VesselHeading)
{
    VesselHeading hdgIn {};
    hdgIn.sid = 9U;
    hdgIn.headingDegrees = 312.4;
    hdgIn.deviationDegrees = -1.2;
    hdgIn.variationDegrees = 3.5;
    hdgIn.reference = HeadingReference::Magnetic;
    hdgIn.hasHeading = true;
    hdgIn.hasDeviation = true;
    hdgIn.hasVariation = true;

    const CanFrame frame = N2kEncoder::encodeVesselHeading(hdgIn, 0x30U, 2U);
    EXPECT_EQ(frame.dlc, 8U);

    const auto hdr = N2kHeader::fromCanId(frame.id);
    EXPECT_EQ(hdr.priority, 2U);
    EXPECT_EQ(hdr.sourceAddress, 0x30U);
    EXPECT_EQ(hdr.pgn, static_cast<std::uint32_t>(Pgn::VesselHeading));

    VesselHeading hdgOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn127250(frame.data.data(), frame.dlc, hdgOut));
    EXPECT_EQ(hdgOut.sid, 9U);
    EXPECT_TRUE(hdgOut.hasHeading);
    EXPECT_TRUE(hdgOut.hasDeviation);
    EXPECT_TRUE(hdgOut.hasVariation);
    EXPECT_NEAR(hdgOut.headingDegrees, 312.4, 0.05);
    EXPECT_NEAR(hdgOut.deviationDegrees, -1.2, 0.05);
    EXPECT_NEAR(hdgOut.variationDegrees, 3.5, 0.05);
    EXPECT_EQ(hdgOut.reference, HeadingReference::Magnetic);
}

TEST(TestN2kEncoder, Pgn127257Attitude)
{
    Attitude attIn {};
    attIn.sid = 14U;
    attIn.yawDegrees = 45.0;
    attIn.pitchDegrees = 7.5;
    attIn.rollDegrees = -4.2;
    attIn.hasYaw = true;
    attIn.hasPitch = true;
    attIn.hasRoll = true;

    const CanFrame frame = N2kEncoder::encodeAttitude(attIn, 0x25U, 3U);
    EXPECT_EQ(frame.dlc, 8U);

    Attitude attOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn127257(frame.data.data(), frame.dlc, attOut));
    EXPECT_EQ(attOut.sid, 14U);
    EXPECT_TRUE(attOut.hasYaw);
    EXPECT_TRUE(attOut.hasPitch);
    EXPECT_TRUE(attOut.hasRoll);
    EXPECT_NEAR(attOut.yawDegrees, 45.0, 0.05);
    EXPECT_NEAR(attOut.pitchDegrees, 7.5, 0.05);
    EXPECT_NEAR(attOut.rollDegrees, -4.2, 0.05);
}

TEST(TestN2kEncoder, Pgn130306WindData)
{
    WindData windIn {};
    windIn.sid = 2U;
    windIn.windSpeedKnots = 16.5;
    windIn.windSpeedMps = 16.5 * 0.514444;
    windIn.windAngleDegrees = 85.0;
    windIn.reference = WindReference::Apparent;
    windIn.hasWindSpeed = true;
    windIn.hasWindAngle = true;

    const CanFrame frame = N2kEncoder::encodeWindData(windIn, 0x10U, 3U);
    EXPECT_EQ(frame.dlc, 8U);

    WindData windOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn130306(frame.data.data(), frame.dlc, windOut));
    EXPECT_EQ(windOut.sid, 2U);
    EXPECT_TRUE(windOut.hasWindSpeed);
    EXPECT_TRUE(windOut.hasWindAngle);
    EXPECT_NEAR(windOut.windSpeedKnots, 16.5, 0.1);
    EXPECT_NEAR(windOut.windAngleDegrees, 85.0, 0.05);
    EXPECT_EQ(windOut.reference, WindReference::Apparent);
}

TEST(TestN2kEncoder, Pgn129038AisClassAPositionFastPacket)
{
    AisClassAPosition aisIn {};
    aisIn.messageId = 1U;
    aisIn.repeatIndicator = 0U;
    aisIn.navStatus = 0U; // Under way using engine
    aisIn.mmsi = 211234567U;
    aisIn.latitudeDeg = 54.3210;
    aisIn.longitudeDeg = 10.1234;
    aisIn.positionValid = true;
    aisIn.cogDegrees = 75.3;
    aisIn.hasCog = true;
    aisIn.sogKnots = 14.2;
    aisIn.hasSog = true;
    aisIn.headingDegrees = 76.0;
    aisIn.hasHeading = true;
    aisIn.rateOfTurnDegPerSec = 0.5;
    aisIn.hasRateOfTurn = true;

    const auto frames = N2kEncoder::encodeAisClassAPosition(aisIn, 0x23U, 3U);
    ASSERT_GE(frames.size(), 4U);

    // Reassemble through N2kFastPacketAssembler
    N2kFastPacketAssembler assembler {};
    std::optional<N2kMessage> reassembled {};

    for (const auto& f : frames) {
        auto res = assembler.processCanFrame(f);
        if (res.has_value()) {
            reassembled = res;
        }
    }

    ASSERT_TRUE(reassembled.has_value());
    EXPECT_EQ(reassembled->header.pgn, static_cast<std::uint32_t>(Pgn::AisClassAPositionReport));
    EXPECT_EQ(reassembled->header.sourceAddress, 0x23U);

    AisClassAPosition aisOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn129038(reassembled->payload.data(), reassembled->payload.size(), aisOut));
    EXPECT_EQ(aisOut.mmsi, 211234567U);
    EXPECT_TRUE(aisOut.positionValid);
    EXPECT_NEAR(aisOut.latitudeDeg, 54.3210, 1e-5);
    EXPECT_NEAR(aisOut.longitudeDeg, 10.1234, 1e-5);
    EXPECT_TRUE(aisOut.hasCog);
    EXPECT_NEAR(aisOut.cogDegrees, 75.3, 0.1);
    EXPECT_TRUE(aisOut.hasSog);
    EXPECT_NEAR(aisOut.sogKnots, 14.2, 0.1);
    EXPECT_TRUE(aisOut.hasHeading);
    EXPECT_NEAR(aisOut.headingDegrees, 76.0, 0.1);
}

TEST(TestN2kEncoder, Pgn129039AisClassBPositionFastPacket)
{
    AisClassBPosition aisIn {};
    aisIn.mmsi = 366999888U;
    aisIn.latitudeDeg = 25.7617;
    aisIn.longitudeDeg = -80.1918;
    aisIn.positionValid = true;
    aisIn.cogDegrees = 120.0;
    aisIn.hasCog = true;
    aisIn.sogKnots = 8.5;
    aisIn.hasSog = true;
    aisIn.headingDegrees = 118.0;
    aisIn.hasHeading = true;

    const auto frames = N2kEncoder::encodeAisClassBPosition(aisIn, 0x23U, 7U);
    ASSERT_GE(frames.size(), 3U);

    N2kFastPacketAssembler assembler {};
    std::optional<N2kMessage> reassembled {};

    for (const auto& f : frames) {
        auto res = assembler.processCanFrame(f);
        if (res.has_value()) {
            reassembled = res;
        }
    }

    ASSERT_TRUE(reassembled.has_value());
    EXPECT_EQ(reassembled->header.pgn, static_cast<std::uint32_t>(Pgn::AisClassBPositionReport));

    AisClassBPosition aisOut {};
    ASSERT_TRUE(N2kDecoder::parsePgn129039(reassembled->payload.data(), reassembled->payload.size(), aisOut));
    EXPECT_EQ(aisOut.mmsi, 366999888U);
    EXPECT_TRUE(aisOut.positionValid);
    EXPECT_NEAR(aisOut.latitudeDeg, 25.7617, 1e-5);
    EXPECT_NEAR(aisOut.longitudeDeg, -80.1918, 1e-5);
    EXPECT_TRUE(aisOut.hasCog);
    EXPECT_NEAR(aisOut.cogDegrees, 120.0, 0.1);
    EXPECT_TRUE(aisOut.hasSog);
    EXPECT_NEAR(aisOut.sogKnots, 8.5, 0.1);
}

} // namespace Nmea::N2k
