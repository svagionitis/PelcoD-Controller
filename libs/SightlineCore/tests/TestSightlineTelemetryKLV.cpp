/// @file TestSightlineTelemetryKLV.cpp
/// @brief Unit tests for Sightline platform telemetry, MISB KLV metadata, and streaming builder and parser.

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
#include "modules/SightlineKlvBuilder.h"
#include "modules/SightlineKlvParser.h"
#include "modules/SightlineTelemetryBuilder.h"
#include "modules/SightlineTelemetryParser.h"

#include <gtest/gtest.h>

namespace Sightline {
namespace {

    /// @brief Verify metadata and KLV commands serialization.
    TEST(TestSightlineTelemetryKLV, BuildMetadataKLV)
    {
        MsgSetMetadataValues metaMsg {};
        metaMsg.platformLatitudeDeg = 37.7749;
        metaMsg.platformLongitudeDeg = -122.4194;
        metaMsg.platformAltitudeMeters = 1500.0;
        metaMsg.platformHeadingDeg = 180.5;
        metaMsg.platformPitchDeg = -5.2;
        metaMsg.platformRollDeg = 1.1;
        metaMsg.sensorHorizontalFovDeg = 30.0;
        metaMsg.sensorVerticalFovDeg = 20.0;
        const auto metaPkt = SightlineKlvBuilder::buildSetMetadataValues(metaMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(metaPkt), MessageId::SetMetadataValues);

        // Verify facade equivalence
        const auto facadeMetaPkt = SightlineProtocolBuilder::buildSetMetadataValues(metaMsg);
        EXPECT_EQ(metaPkt, facadeMetaPkt);

        MsgMetadataStaticValues statMsg {};
        statMsg.missionId = "TASK_ALPHA";
        statMsg.platformTailNumber = "N12345";
        statMsg.securityClassification = "UNCLASSIFIED";
        const auto statPkt = SightlineKlvBuilder::buildMetadataStaticValues(statMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(statPkt), MessageId::MetadataStaticValues);

        MsgSetMetadataRate rateMsg {};
        rateMsg.metadataType = 0U;
        rateMsg.ratePeriod = 2U;
        const auto ratePkt = SightlineKlvBuilder::buildSetMetadataRate(rateMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(ratePkt), MessageId::SetMetadataRate);

        MsgSetTelemetryDestination destMsg {};
        destMsg.clientIndex = 0U;
        destMsg.clientIpAddress = 0xC0A801C8U; // 192.168.1.200
        destMsg.clientPort = 14002U;
        destMsg.flags = 0x01U;
        const auto destPkt = SightlineTelemetryBuilder::buildSetTelemetryDest(destMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(destPkt), MessageId::SetTelemetryDestination);

        MsgCursorOnTarget cotMsg {};
        cotMsg.enable = 1U;
        cotMsg.broadcastPort = 1870U;
        cotMsg.uid = "VEHICLE_1";
        cotMsg.cotType = "a-f-G-E-V-C";
        const auto cotPkt = SightlineKlvBuilder::buildCursorOnTarget(cotMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(cotPkt), MessageId::CursorOnTarget);
    }

    /// @brief Verify platform metadata values deserialization (0x8B).
    TEST(TestSightlineTelemetryKLV, ParseMetadataValues)
    {
        std::vector<std::uint8_t> payload {};
        payload.reserve(64U);
        SightlineFraming::appendDouble64Le(payload, 37.7749);
        SightlineFraming::appendDouble64Le(payload, -122.4194);
        SightlineFraming::appendDouble64Le(payload, 1500.0);
        SightlineFraming::appendDouble64Le(payload, 180.5);
        SightlineFraming::appendDouble64Le(payload, -5.2);
        SightlineFraming::appendDouble64Le(payload, 1.1);
        SightlineFraming::appendDouble64Le(payload, 30.0);
        SightlineFraming::appendDouble64Le(payload, 20.0);

        const auto pkt = SightlineFraming::buildPacket(MessageId::CurrentMetadataValues, payload);

        MsgSetMetadataValues out {};
        ASSERT_TRUE(SightlineKlvParser::parseMetadataValues(pkt, out));
        EXPECT_DOUBLE_EQ(out.platformLatitudeDeg, 37.7749);
        EXPECT_DOUBLE_EQ(out.platformLongitudeDeg, -122.4194);
        EXPECT_DOUBLE_EQ(out.platformAltitudeMeters, 1500.0);
        EXPECT_DOUBLE_EQ(out.platformHeadingDeg, 180.5);
        EXPECT_DOUBLE_EQ(out.platformPitchDeg, -5.2);
        EXPECT_DOUBLE_EQ(out.platformRollDeg, 1.1);
        EXPECT_DOUBLE_EQ(out.sensorHorizontalFovDeg, 30.0);
        EXPECT_DOUBLE_EQ(out.sensorVerticalFovDeg, 20.0);

        // Verify facade equivalence
        MsgSetMetadataValues facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseMetadataValues(pkt, facadeOut));
        EXPECT_DOUBLE_EQ(facadeOut.platformLatitudeDeg, 37.7749);
    }

    /// @brief Verify static metadata values and rate round-trip query and parse.
    TEST(TestSightlineTelemetryKLV, StaticValuesAndRateRoundTrip)
    {
        const auto getStaticPkt = SightlineKlvBuilder::buildGetMetadataStaticValues();
        EXPECT_EQ(SightlineFraming::identifyMessage(getStaticPkt), MessageId::GetParameters);
        EXPECT_EQ(getStaticPkt, SightlineProtocolBuilder::buildGetMetadataStaticValues());

        const auto getRatePkt = SightlineKlvBuilder::buildGetMetadataRate();
        EXPECT_EQ(SightlineFraming::identifyMessage(getRatePkt), MessageId::GetParameters);
        EXPECT_EQ(getRatePkt, SightlineProtocolBuilder::buildGetMetadataRate());

        const auto getDestPkt = SightlineTelemetryBuilder::buildGetTelemetryDest();
        EXPECT_EQ(SightlineFraming::identifyMessage(getDestPkt), MessageId::GetParameters);
        EXPECT_EQ(getDestPkt, SightlineProtocolBuilder::buildGetTelemetryDest());

        MsgMetadataStaticValues staticIn {};
        staticIn.missionId = "RECON_01";
        staticIn.platformTailNumber = "N12345";
        staticIn.securityClassification = "SECRET";

        const auto staticPkt = SightlineKlvBuilder::buildMetadataStaticValues(staticIn);
        MsgMetadataStaticValues staticOut {};
        ASSERT_TRUE(SightlineKlvParser::parseMetadataStaticValues(staticPkt, staticOut));
        EXPECT_EQ(staticOut.missionId, "RECON_01");
        EXPECT_EQ(staticOut.platformTailNumber, "N12345");
        EXPECT_EQ(staticOut.securityClassification, "SECRET");

        MsgMetadataStaticValues facadeStaticOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseMetadataStaticValues(staticPkt, facadeStaticOut));
        EXPECT_EQ(facadeStaticOut.missionId, "RECON_01");

        MsgSetMetadataRate rateIn {};
        rateIn.metadataType = 0U;
        rateIn.ratePeriod = 2U;

        const auto ratePkt = SightlineKlvBuilder::buildSetMetadataRate(rateIn);
        MsgSetMetadataRate rateOut {};
        ASSERT_TRUE(SightlineKlvParser::parseMetadataRate(ratePkt, rateOut));
        EXPECT_EQ(rateOut.metadataType, 0U);
        EXPECT_EQ(rateOut.ratePeriod, 2U);

        MsgSetMetadataRate facadeRateOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseMetadataRate(ratePkt, facadeRateOut));
        EXPECT_EQ(facadeRateOut.ratePeriod, 2U);
    }

} // namespace
} // namespace Sightline
