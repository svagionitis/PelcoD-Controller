/// @file TestSightlineTelemetryKLV.cpp
/// @brief Comprehensive unit tests for Sightline KLV Metadata module (STANAG 4609 / MISB ST 0601 / ST 0102 / ST 0903 / CoT).

#include "SightlineFraming.h"
#include "SightlineProtocolBuilder.h"
#include "SightlineProtocolParser.h"
#include "modules/SightlineKlv.h"
#include "modules/SightlineKlvBuilder.h"
#include "modules/SightlineKlvParser.h"
#include "modules/SightlineTelemetryBuilder.h"
#include "modules/SightlineTelemetryParser.h"

#include <gtest/gtest.h>

namespace Sightline {
namespace {

    /// @brief Test MISB fixed-point telemetry conversions.
    TEST(TestSightlineTelemetryKLV, TelemetryConversions)
    {
        // Latitude: [-90, 90]
        const double latDeg { 37.7749 };
        const auto latFixed = MsgSetMetadataValues::degToLat(latDeg);
        EXPECT_NEAR(MsgSetMetadataValues::latToDeg(latFixed), latDeg, 1e-6);

        // Longitude: [-180, 180]
        const double lonDeg { -122.4194 };
        const auto lonFixed = MsgSetMetadataValues::degToLon(lonDeg);
        EXPECT_NEAR(MsgSetMetadataValues::lonToDeg(lonFixed), lonDeg, 1e-6);

        // Heading: [0, 360)
        const double headingDeg { 180.5 };
        const auto headingFixed = MsgSetMetadataValues::degToHeading(headingDeg);
        EXPECT_NEAR(MsgSetMetadataValues::headingToDeg(headingFixed), headingDeg, 0.01);

        // Pitch: [-20, 20]
        const double pitchDeg { -5.25 };
        const auto pitchFixed = MsgSetMetadataValues::degToPitch(pitchDeg);
        EXPECT_NEAR(MsgSetMetadataValues::pitchToDeg(pitchFixed), pitchDeg, 0.01);

        // Roll: [-50, 50]
        const double rollDeg { 12.5 };
        const auto rollFixed = MsgSetMetadataValues::degToRoll(rollDeg);
        EXPECT_NEAR(MsgSetMetadataValues::rollToDeg(rollFixed), rollDeg, 0.01);

        // Altitude: [-900, 19000]
        const double altM { 1500.0 };
        const auto altFixed = MsgSetMetadataValues::altToMisb(altM);
        EXPECT_NEAR(MsgSetMetadataValues::misbToAlt(altFixed), altM, 0.5);

        // FOV: [0, 180]
        const double fovDeg { 30.0 };
        const auto fovFixed = MsgSetMetadataValues::degToFov(fovDeg);
        EXPECT_NEAR(MsgSetMetadataValues::fovToDeg(fovFixed), fovDeg, 0.01);

        // Azimuth: [0, 360)
        const double azDeg { 270.0 };
        const auto azFixed = MsgSetMetadataValues::degToAzimuth(azDeg);
        EXPECT_NEAR(MsgSetMetadataValues::azimuthToDeg(azFixed), azDeg, 1e-5);

        // Elevation: [-180, 180]
        const double elDeg { -45.0 };
        const auto elFixed = MsgSetMetadataValues::degToElevation(elDeg);
        EXPECT_NEAR(MsgSetMetadataValues::elevationToDeg(elFixed), elDeg, 1e-5);
    }

    /// @brief Verify SetMetadataValues (0x13, 44 bytes) build and parse round-trip.
    TEST(TestSightlineTelemetryKLV, SetMetadataValuesRoundTrip)
    {
        MsgSetMetadataValues msgIn {};
        msgIn.validDataMask = 0x0FFFU;
        msgIn.utcTime = 1609459200000000ULL; // timestamp in microseconds
        msgIn.heading = MsgSetMetadataValues::degToHeading(90.0);
        msgIn.pitch = MsgSetMetadataValues::degToPitch(-2.5);
        msgIn.roll = MsgSetMetadataValues::degToRoll(0.5);
        msgIn.lat = MsgSetMetadataValues::degToLat(45.0);
        msgIn.lon = MsgSetMetadataValues::degToLon(-93.0);
        msgIn.alt = MsgSetMetadataValues::altToMisb(250.0);
        msgIn.hfov = MsgSetMetadataValues::degToFov(15.0);
        msgIn.vfov = MsgSetMetadataValues::degToFov(10.0);
        msgIn.az = MsgSetMetadataValues::degToAzimuth(180.0);
        msgIn.el = MsgSetMetadataValues::degToElevation(-30.0);
        msgIn.sensorRoll = 0U;
        msgIn.displayId = 0x0002U;

        const auto pkt = SightlineKlvBuilder::buildSetMetadataValues(msgIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetMetadataValues);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetMetadataValues(msgIn));

        const auto payload = SightlineFraming::extractPayload(pkt);
        EXPECT_EQ(payload.size(), 44U);

        MsgSetMetadataValues msgOut {};
        ASSERT_TRUE(SightlineKlvParser::parseMetadataValues(pkt, msgOut));
        EXPECT_EQ(msgOut.validDataMask, 0x0FFFU);
        EXPECT_EQ(msgOut.utcTime, 1609459200000000ULL);
        EXPECT_EQ(msgOut.heading, msgIn.heading);
        EXPECT_EQ(msgOut.pitch, msgIn.pitch);
        EXPECT_EQ(msgOut.roll, msgIn.roll);
        EXPECT_EQ(msgOut.lat, msgIn.lat);
        EXPECT_EQ(msgOut.lon, msgIn.lon);
        EXPECT_EQ(msgOut.alt, msgIn.alt);
        EXPECT_EQ(msgOut.hfov, msgIn.hfov);
        EXPECT_EQ(msgOut.vfov, msgIn.vfov);
        EXPECT_EQ(msgOut.az, msgIn.az);
        EXPECT_EQ(msgOut.el, msgIn.el);
        EXPECT_EQ(msgOut.displayId, 0x0002U);

        // Facade equivalence
        MsgSetMetadataValues facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseMetadataValues(pkt, facadeOut));
        EXPECT_EQ(facadeOut.utcTime, msgIn.utcTime);
    }

    /// @brief Verify CurrentMetadataValues (0x8B, 42 bytes) parsing.
    TEST(TestSightlineTelemetryKLV, CurrentMetadataValuesParsing)
    {
        std::vector<std::uint8_t> payload {};
        payload.reserve(42U);
        SightlineFraming::appendU64Le(payload, 1609459200123456ULL); // utcTime
        SightlineFraming::appendU16Le(payload, 1000U);                // heading
        SightlineFraming::appendS16Le(payload, -500);                 // pitch
        SightlineFraming::appendS16Le(payload, 250);                  // roll
        SightlineFraming::appendS32Le(payload, 12345678);             // lat
        SightlineFraming::appendS32Le(payload, -87654321);            // lon
        SightlineFraming::appendU16Le(payload, 5000U);                // alt
        SightlineFraming::appendU16Le(payload, 3000U);                // hfov
        SightlineFraming::appendU16Le(payload, 2000U);                // vfov
        SightlineFraming::appendU32Le(payload, 40000U);               // az
        SightlineFraming::appendS32Le(payload, -10000);               // el
        SightlineFraming::appendU32Le(payload, 0U);                   // sensorRoll
        SightlineFraming::appendU16Le(payload, 0x0080U);              // displayId (Net1)

        const auto pkt = SightlineFraming::buildPacket(MessageId::CurrentMetadataValues, payload);

        MsgCurrentMetadataValues curOut {};
        ASSERT_TRUE(SightlineKlvParser::parseCurrentValues(pkt, curOut));
        EXPECT_EQ(curOut.utcTime, 1609459200123456ULL);
        EXPECT_EQ(curOut.heading, 1000U);
        EXPECT_EQ(curOut.pitch, -500);
        EXPECT_EQ(curOut.roll, 250);
        EXPECT_EQ(curOut.lat, 12345678);
        EXPECT_EQ(curOut.lon, -87654321);
        EXPECT_EQ(curOut.alt, 5000U);
        EXPECT_EQ(curOut.displayId, 0x0080U);

        // Facade equivalence
        MsgCurrentMetadataValues facadeCurOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseCurrentValues(pkt, facadeCurOut));
        EXPECT_EQ(facadeCurOut.utcTime, curOut.utcTime);
    }

    /// @brief Verify MetadataStaticValues (0x14) build and parse round-trip.
    TEST(TestSightlineTelemetryKLV, MetadataStaticValuesRoundTrip)
    {
        MsgMetadataStaticValues msgIn {};
        msgIn.type = StaticMetadataType::MissionId;
        msgIn.setString("OPERATION_FIREFLY");
        msgIn.displayId = 0x0002U;

        const auto pkt = SightlineKlvBuilder::buildMetadataStaticValues(msgIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::MetadataStaticValues);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildMetadataStaticValues(msgIn));

        MsgMetadataStaticValues msgOut {};
        ASSERT_TRUE(SightlineKlvParser::parseMetadataStaticValues(pkt, msgOut));
        EXPECT_EQ(msgOut.type, StaticMetadataType::MissionId);
        EXPECT_EQ(msgOut.asString(), "OPERATION_FIREFLY");
        EXPECT_EQ(msgOut.displayId, 0x0002U);

        // Facade equivalence
        MsgMetadataStaticValues facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseMetadataStaticValues(pkt, facadeOut));
        EXPECT_EQ(facadeOut.asString(), "OPERATION_FIREFLY");
    }

    /// @brief Verify SetMetadataFrameValues (0x15, 49 bytes) build and parse round-trip.
    TEST(TestSightlineTelemetryKLV, FrameValuesRoundTrip)
    {
        MsgSetMetadataFrameValues msgIn {};
        msgIn.validDataMask = 0x001FU;
        msgIn.frameCenterLat = 100000;
        msgIn.frameCenterLon = -200000;
        msgIn.frameCenterEl = 350U;
        msgIn.frameWidth = 120U;
        msgIn.slantRange = 1500U;
        msgIn.userSuppliedFlags = 0x20U; // Bit 5: OLS DTED mode enabled
        msgIn.displayId = 0x0002U;

        const auto pkt = SightlineKlvBuilder::buildSetMetadataFrameValues(msgIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetMetadataFrameValues);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetFrameValues(msgIn));

        const auto payload = SightlineFraming::extractPayload(pkt);
        EXPECT_EQ(payload.size(), 49U);

        MsgSetMetadataFrameValues msgOut {};
        ASSERT_TRUE(SightlineKlvParser::parseFrameValues(pkt, msgOut));
        EXPECT_EQ(msgOut.validDataMask, 0x001FU);
        EXPECT_EQ(msgOut.frameCenterLat, 100000);
        EXPECT_EQ(msgOut.frameCenterLon, -200000);
        EXPECT_EQ(msgOut.frameCenterEl, 350U);
        EXPECT_EQ(msgOut.frameWidth, 120U);
        EXPECT_EQ(msgOut.slantRange, 1500U);
        EXPECT_EQ(msgOut.userSuppliedFlags, 0x20U);
        EXPECT_EQ(msgOut.displayId, 0x0002U);

        // Facade equivalence
        MsgSetMetadataFrameValues facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseFrameValues(pkt, facadeOut));
        EXPECT_EQ(facadeOut.slantRange, 1500U);
    }

    /// @brief Verify SetMetadataRate (0x62, 11 bytes) build and parse round-trip.
    TEST(TestSightlineTelemetryKLV, MetadataRateRoundTrip)
    {
        MsgSetMetadataRate msgIn {};
        msgIn.enables = 0x0000000000000001ULL; // Bit 0: MISB ST 0601 enable
        msgIn.frameStep = 1U;                  // 30 Hz full rate
        msgIn.displayId = 0x0002U;

        const auto pkt = SightlineKlvBuilder::buildSetMetadataRate(msgIn);
        EXPECT_EQ(SightlineFraming::identifyMessage(pkt), MessageId::SetMetadataRate);
        EXPECT_EQ(pkt, SightlineProtocolBuilder::buildSetMetadataRate(msgIn));

        const auto payload = SightlineFraming::extractPayload(pkt);
        EXPECT_EQ(payload.size(), 11U);

        MsgSetMetadataRate msgOut {};
        ASSERT_TRUE(SightlineKlvParser::parseMetadataRate(pkt, msgOut));
        EXPECT_EQ(msgOut.enables, 1ULL);
        EXPECT_EQ(msgOut.frameStep, 1U);
        EXPECT_EQ(msgOut.displayId, 0x0002U);

        // Facade equivalence
        MsgSetMetadataRate facadeOut {};
        ASSERT_TRUE(SightlineProtocolParser::parseMetadataRate(pkt, facadeOut));
        EXPECT_EQ(facadeOut.frameStep, 1U);
    }

    /// @brief Verify TagData (0x96), TagDataRate (0x97), and TagSourceSelector (0x98).
    TEST(TestSightlineTelemetryKLV, TagControlRoundTrip)
    {
        // 0x96 TagData
        MsgTagData tagMsg {};
        tagMsg.tagId = 94U; // MI Core ID
        tagMsg.tagSubId = 0U;
        tagMsg.displayId = 0x0002U;
        tagMsg.data = { 0x01, 0x02, 0x03, 0x04 };

        const auto tagPkt = SightlineKlvBuilder::buildTagData(tagMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(tagPkt), MessageId::TagData);
        EXPECT_EQ(tagPkt, SightlineProtocolBuilder::buildTagData(tagMsg));

        MsgTagData tagOut {};
        ASSERT_TRUE(SightlineKlvParser::parseTagData(tagPkt, tagOut));
        EXPECT_EQ(tagOut.tagId, 94U);
        EXPECT_EQ(tagOut.data.size(), 4U);
        EXPECT_EQ(tagOut.data[0], 0x01);

        // 0x97 TagDataRate
        MsgTagDataRate rateMsg {};
        rateMsg.mode = 0U; // single tag
        rateMsg.tagId1 = 5U;
        rateMsg.frameStep = 10U; // decimate by 10
        rateMsg.displayId = 0x0002U;

        const auto ratePkt = SightlineKlvBuilder::buildTagDataRate(rateMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(ratePkt), MessageId::TagDataRate);
        EXPECT_EQ(ratePkt, SightlineProtocolBuilder::buildTagDataRate(rateMsg));

        MsgTagDataRate rateOut {};
        ASSERT_TRUE(SightlineKlvParser::parseTagDataRate(ratePkt, rateOut));
        EXPECT_EQ(rateOut.tagId1, 5U);
        EXPECT_EQ(rateOut.frameStep, 10U);

        // 0x98 TagSourceSelector
        MsgTagSourceSelector srcMsg {};
        srcMsg.mode = 0U;
        srcMsg.tagId1 = 13U; // Sensor Lat
        srcMsg.selector = static_cast<std::uint16_t>(TagSource::SlaProtocol);
        srcMsg.displayId = 0x0002U;

        const auto srcPkt = SightlineKlvBuilder::buildSetTagSourceSelector(srcMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(srcPkt), MessageId::TagSourceSelector);
        EXPECT_EQ(srcPkt, SightlineProtocolBuilder::buildSetTagSourceSelector(srcMsg));

        MsgTagSourceSelector srcOut {};
        ASSERT_TRUE(SightlineKlvParser::parseTagSourceSelector(srcPkt, srcOut));
        EXPECT_EQ(srcOut.tagId1, 13U);
        EXPECT_EQ(srcOut.selector, static_cast<std::uint16_t>(TagSource::SlaProtocol));
    }

    /// @brief Verify CursorOnTarget (0xB0) build and parse round-trip.
    TEST(TestSightlineTelemetryKLV, CursorOnTargetRoundTrip)
    {
        MsgCursorOnTarget cotMsg {};
        cotMsg.mode = 1U;
        cotMsg.ipAddress = 0xEFE00A0AU; // 239.224.10.10
        cotMsg.port = 1870U;
        cotMsg.rate = 30U;
        cotMsg.displayId = 0x0002U;

        const auto cotPkt = SightlineKlvBuilder::buildCursorOnTarget(cotMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(cotPkt), MessageId::CursorOnTarget);
        EXPECT_EQ(cotPkt, SightlineProtocolBuilder::buildCursorOnTarget(cotMsg));

        MsgCursorOnTarget cotOut {};
        ASSERT_TRUE(SightlineKlvParser::parseCursorOnTarget(cotPkt, cotOut));
        EXPECT_EQ(cotOut.mode, 1U);
        EXPECT_EQ(cotOut.ipAddress, 0xEFE00A0AU);
        EXPECT_EQ(cotOut.port, 1870U);
        EXPECT_EQ(cotOut.rate, 30U);
        EXPECT_EQ(cotOut.displayId, 0x0002U);
    }

    /// @brief Verify VMTI Chips (0xAD) and VMTI Fields (0xBF) build and parse.
    TEST(TestSightlineTelemetryKLV, VmtiChipsAndFieldsRoundTrip)
    {
        // 0xAD VmtiChips
        MsgVmtiChips chipsMsg {};
        chipsMsg.mode = 1U;
        chipsMsg.format = VmtiChipFormat::Jpeg;
        chipsMsg.sizeType = VmtiChipSizeType::FixedSize;
        chipsMsg.sizeHint = 64U;
        chipsMsg.maxPerFrame = 2U;
        chipsMsg.minFramesBetween = 5U;
        chipsMsg.displayId = 0x0002U;

        const auto chipsPkt = SightlineKlvBuilder::buildVmtiChips(chipsMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(chipsPkt), MessageId::VmtiChips);
        EXPECT_EQ(chipsPkt, SightlineProtocolBuilder::buildVmtiChips(chipsMsg));

        MsgVmtiChips chipsOut {};
        ASSERT_TRUE(SightlineKlvParser::parseVmtiChips(chipsPkt, chipsOut));
        EXPECT_EQ(chipsOut.mode, 1U);
        EXPECT_EQ(chipsOut.format, VmtiChipFormat::Jpeg);
        EXPECT_EQ(chipsOut.sizeHint, 64U);
        EXPECT_EQ(chipsOut.maxPerFrame, 2U);

        // 0xBF VmtiFields
        MsgVmtiFields fieldsMsg {};
        fieldsMsg.displayId = 0x0002U;
        fieldsMsg.fields = 0x0007U;
        fieldsMsg.ontologySeriesRate = 15U;

        const auto fieldsPkt = SightlineKlvBuilder::buildVmtiFields(fieldsMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(fieldsPkt), MessageId::VmtiFields);
        EXPECT_EQ(fieldsPkt, SightlineProtocolBuilder::buildVmtiFields(fieldsMsg));

        MsgVmtiFields fieldsOut {};
        ASSERT_TRUE(SightlineKlvParser::parseVmtiFields(fieldsPkt, fieldsOut));
        EXPECT_EQ(fieldsOut.displayId, 0x0002U);
        EXPECT_EQ(fieldsOut.fields, 0x0007U);
        EXPECT_EQ(fieldsOut.ontologySeriesRate, 15U);
    }

    /// @brief Verify AncillaryTextMetadata (0xAC) round-trip.
    TEST(TestSightlineTelemetryKLV, AncillaryTextRoundTrip)
    {
        MsgAncillaryTextMetadata textMsg {};
        textMsg.creationTime = 1609459200000000ULL;
        textMsg.source = "Operator";
        textMsg.originator = "GCS-ALPHA";
        textMsg.messageBody = "Target vehicle stationary at intersection.";
        textMsg.displayId = 0x0002U;

        const auto textPkt = SightlineKlvBuilder::buildAncillaryText(textMsg);
        EXPECT_EQ(SightlineFraming::identifyMessage(textPkt), MessageId::AncillaryTextMetadata);
        EXPECT_EQ(textPkt, SightlineProtocolBuilder::buildAncillaryTextMetadata(textMsg));

        MsgAncillaryTextMetadata textOut {};
        ASSERT_TRUE(SightlineKlvParser::parseAncillaryText(textPkt, textOut));
        EXPECT_EQ(textOut.creationTime, 1609459200000000ULL);
        EXPECT_EQ(textOut.source, "Operator");
        EXPECT_EQ(textOut.originator, "GCS-ALPHA");
        EXPECT_EQ(textOut.messageBody, "Target vehicle stationary at intersection.");
        EXPECT_EQ(textOut.displayId, 0x0002U);
    }

} // namespace
} // namespace Sightline
