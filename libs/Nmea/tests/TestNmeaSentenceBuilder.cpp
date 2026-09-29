#include "NmeaSentenceBuilder.h"
#include "NmeaSentenceParser.h"

#include <gtest/gtest.h>

namespace Nmea {
namespace {

    TEST(TestNmeaSentenceBuilder, RoundtripHdt)
    {
        const std::string sentence = NmeaSentenceBuilder::buildHdt(284.6, "HE");
        EXPECT_TRUE(NmeaChecksum::validate(sentence));

        HdtData parsed {};
        ASSERT_TRUE(NmeaSentenceParser::parseHdt(sentence, parsed, true));
        EXPECT_TRUE(parsed.valid);
        EXPECT_NEAR(parsed.headingDegrees, 284.6, 1e-1);
    }

    TEST(TestNmeaSentenceBuilder, RoundtripThs)
    {
        const std::string sentence = NmeaSentenceBuilder::buildThs(172.9, NmeaFaaMode::Autonomous, "HE");
        EXPECT_TRUE(NmeaChecksum::validate(sentence));

        ThsData parsed {};
        ASSERT_TRUE(NmeaSentenceParser::parseThs(sentence, parsed, true));
        EXPECT_TRUE(parsed.valid);
        EXPECT_NEAR(parsed.headingDegrees, 172.9, 1e-1);
        EXPECT_EQ(parsed.mode, NmeaFaaMode::Autonomous);
    }

    TEST(TestNmeaSentenceBuilder, RoundtripGga)
    {
        GgaData inData {};
        inData.utcTime = { 10, 15, 30, 0 };
        inData.coordinates = { 37.7749, -122.4194 };
        inData.fixQuality = NmeaFixQuality::DgpsFix;
        inData.numSatellites = 12U;
        inData.hdop = 0.9;
        inData.altitudeMeters = 45.2;
        inData.geoidalSeparationMeters = -32.1;
        inData.dgpsAgeSeconds = 1.2;
        inData.dgpsStationId = 104U;

        const std::string sentence = NmeaSentenceBuilder::buildGga(inData, "GP");
        EXPECT_TRUE(NmeaChecksum::validate(sentence));

        GgaData outData {};
        ASSERT_TRUE(NmeaSentenceParser::parseGga(sentence, outData, true));
        EXPECT_TRUE(outData.valid);
        EXPECT_EQ(outData.utcTime.hour, 10U);
        EXPECT_EQ(outData.utcTime.minute, 15U);
        EXPECT_EQ(outData.utcTime.second, 30U);
        EXPECT_NEAR(outData.coordinates.latitudeDeg, 37.7749, 1e-3);
        EXPECT_NEAR(outData.coordinates.longitudeDeg, -122.4194, 1e-3);
        EXPECT_EQ(outData.fixQuality, NmeaFixQuality::DgpsFix);
        EXPECT_EQ(outData.numSatellites, 12U);
        EXPECT_NEAR(outData.altitudeMeters, 45.2, 1e-1);
    }

    TEST(TestNmeaSentenceBuilder, RoundtripRmc)
    {
        RmcData inData {};
        inData.utcTime = { 18, 45, 12, 0 };
        inData.statusActive = true;
        inData.coordinates = { -22.9068, -43.1729 }; // Rio de Janeiro
        inData.speedOverGroundKnots = 14.8;
        inData.courseOverGroundDegrees = 210.5;
        inData.date = { 29, 9, 2026 };
        inData.magneticVariationDegrees = -12.4;
        inData.faaMode = NmeaFaaMode::Autonomous;

        const std::string sentence = NmeaSentenceBuilder::buildRmc(inData, "GN");
        EXPECT_TRUE(NmeaChecksum::validate(sentence));

        RmcData outData {};
        ASSERT_TRUE(NmeaSentenceParser::parseRmc(sentence, outData, true));
        EXPECT_TRUE(outData.valid);
        EXPECT_TRUE(outData.statusActive);
        EXPECT_NEAR(outData.coordinates.latitudeDeg, -22.9068, 1e-3);
        EXPECT_NEAR(outData.coordinates.longitudeDeg, -43.1729, 1e-3);
        EXPECT_NEAR(outData.speedOverGroundKnots, 14.8, 1e-1);
        EXPECT_NEAR(outData.courseOverGroundDegrees, 210.5, 1e-1);
        EXPECT_EQ(outData.date.day, 29U);
        EXPECT_EQ(outData.date.month, 9U);
        EXPECT_EQ(outData.date.year, 2026U);
    }

    TEST(TestNmeaSentenceBuilder, RoundtripTtm)
    {
        TtmData inData {};
        inData.targetNumber = 42U;
        inData.targetDistanceNmi = 6.45;
        inData.bearingDegrees = 112.5;
        inData.bearingReference = TtmReference::True;
        inData.targetSpeedKnots = 22.0;
        inData.targetCourseDegrees = 95.0;
        inData.courseReference = TtmReference::True;
        inData.distanceCpaNmi = 0.8;
        inData.timeCpaMinutes = 4.2;
        inData.speedDistanceUnits = 'K';
        inData.targetName = "SPEEDBOAT_3";
        inData.status = TtmTargetStatus::Tracking;
        inData.referenceTarget = false;
        inData.utcTimeTag = { 15, 20, 10, 0 };
        inData.acquisitionType = 'M';

        const std::string sentence = NmeaSentenceBuilder::buildTtm(inData, "RA");
        EXPECT_TRUE(NmeaChecksum::validate(sentence));

        TtmData outData {};
        ASSERT_TRUE(NmeaSentenceParser::parseTtm(sentence, outData, true));
        EXPECT_TRUE(outData.valid);
        EXPECT_EQ(outData.targetNumber, 42U);
        EXPECT_NEAR(outData.targetDistanceNmi, 6.45, 1e-2);
        EXPECT_NEAR(outData.bearingDegrees, 112.5, 1e-1);
        EXPECT_NEAR(outData.targetSpeedKnots, 22.0, 1e-1);
        EXPECT_EQ(outData.targetName, "SPEEDBOAT_3");
        EXPECT_EQ(outData.status, TtmTargetStatus::Tracking);
        EXPECT_EQ(outData.acquisitionType, 'M');
    }

    TEST(TestNmeaSentenceBuilder, RoundtripXdrPitchRoll)
    {
        const std::string sentence = NmeaSentenceBuilder::buildXdrPitchRoll(3.14, -1.59, "II");
        EXPECT_TRUE(NmeaChecksum::validate(sentence));

        XdrData outData {};
        ASSERT_TRUE(NmeaSentenceParser::parseXdr(sentence, outData, true));
        EXPECT_TRUE(outData.valid);
        ASSERT_EQ(outData.transducers.size(), 2U);
        EXPECT_NEAR(outData.transducers[0].measurement, 3.14, 1e-2);
        EXPECT_EQ(outData.transducers[0].id, "PITCH");
        EXPECT_NEAR(outData.transducers[1].measurement, -1.59, 1e-2);
        EXPECT_EQ(outData.transducers[1].id, "ROLL");
    }

} // namespace
} // namespace Nmea
