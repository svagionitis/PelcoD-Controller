#include "NmeaSentenceParser.h"

#include <gtest/gtest.h>

namespace Nmea {
namespace {

    TEST(TestNmeaSentenceParser, IdentifySentenceAndTalker)
    {
        EXPECT_EQ(NmeaSentenceParser::identifySentence("$GPGGA,123456*00"), NmeaSentenceId::GGA);
        EXPECT_EQ(NmeaSentenceParser::identifySentence("$GPRMC,123456*00"), NmeaSentenceId::RMC);
        EXPECT_EQ(NmeaSentenceParser::identifySentence("$HEHDT,120.5,T*00"), NmeaSentenceId::HDT);
        EXPECT_EQ(NmeaSentenceParser::identifySentence("$HETHS,120.5,A*00"), NmeaSentenceId::THS);
        EXPECT_EQ(NmeaSentenceParser::identifySentence("$RATTM,01,5.2,120.5*00"), NmeaSentenceId::TTM);
        EXPECT_EQ(NmeaSentenceParser::identifySentence("$RATLL,01,1234.56,N*00"), NmeaSentenceId::TLL);
        EXPECT_EQ(NmeaSentenceParser::identifySentence("$IIXDR,A,1.2,D,PITCH*00"), NmeaSentenceId::XDR);
        EXPECT_EQ(NmeaSentenceParser::identifySentence("!AIVDM,1,1,,B*00"), NmeaSentenceId::VDM);
        EXPECT_EQ(NmeaSentenceParser::identifySentence("$PFEC,GPcmd,1,2*00"), NmeaSentenceId::PFEC);

        EXPECT_EQ(NmeaSentenceParser::extractTalkerId("$GPGGA,..."), "GP");
        EXPECT_EQ(NmeaSentenceParser::extractTalkerId("$HEHDT,..."), "HE");
        EXPECT_EQ(NmeaSentenceParser::extractTalkerId("$RATTM,..."), "RA");
        EXPECT_EQ(NmeaSentenceParser::extractTalkerId("!AIVDM,..."), "AI");
    }

    TEST(TestNmeaSentenceParser, ParseCoordinateConversions)
    {
        double deg { 0.0 };
        // 48 deg 07.038 min North = 48 + 7.038/60 = 48.1173
        EXPECT_TRUE(NmeaSentenceParser::parseCoordinate("4807.038", "N", deg));
        EXPECT_NEAR(deg, 48.1173, 1e-4);

        // South should be negative
        EXPECT_TRUE(NmeaSentenceParser::parseCoordinate("4807.038", "S", deg));
        EXPECT_NEAR(deg, -48.1173, 1e-4);

        // 011 deg 31.000 min West = -(11 + 31/60) = -11.516666
        EXPECT_TRUE(NmeaSentenceParser::parseCoordinate("01131.0000", "W", deg));
        EXPECT_NEAR(deg, -11.516666, 1e-4);

        // East should be positive
        EXPECT_TRUE(NmeaSentenceParser::parseCoordinate("01131.0000", "E", deg));
        EXPECT_NEAR(deg, 11.516666, 1e-4);

        // Invalid format
        EXPECT_FALSE(NmeaSentenceParser::parseCoordinate("", "N", deg));
        EXPECT_FALSE(NmeaSentenceParser::parseCoordinate("123", "N", deg)); // No decimal point
        EXPECT_FALSE(NmeaSentenceParser::parseCoordinate("4807.038", "X", deg)); // Invalid hemisphere
    }

    TEST(TestNmeaSentenceParser, ParseGgaValid)
    {
        // Sample: $GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76
        const std::string sentence = "$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76\r\n";
        GgaData gga {};
        ASSERT_TRUE(NmeaSentenceParser::parseGga(sentence, gga, true));

        EXPECT_TRUE(gga.valid);
        EXPECT_EQ(gga.utcTime.hour, 9U);
        EXPECT_EQ(gga.utcTime.minute, 27U);
        EXPECT_EQ(gga.utcTime.second, 50U);
        EXPECT_EQ(gga.utcTime.millisecond, 0U);

        // 53 + 21.6802/60 = 53.361336
        EXPECT_NEAR(gga.coordinates.latitudeDeg, 53.361336, 1e-4);
        // -(6 + 30.3372/60) = -6.50562
        EXPECT_NEAR(gga.coordinates.longitudeDeg, -6.50562, 1e-4);

        EXPECT_EQ(gga.fixQuality, NmeaFixQuality::GpsFix);
        EXPECT_EQ(gga.numSatellites, 8U);
        EXPECT_NEAR(gga.hdop, 1.03, 1e-2);
        EXPECT_NEAR(gga.altitudeMeters, 61.7, 1e-1);
        EXPECT_NEAR(gga.geoidalSeparationMeters, 55.2, 1e-1);
    }

    TEST(TestNmeaSentenceParser, ParseRmcValid)
    {
        // Sample: $GPRMC,083559.00,A,4717.11437,N,00833.91522,E,12.5,77.5,091218,2.1,W,A*2B
        const std::string raw = "GPRMC,083559.00,A,4717.11437,N,00833.91522,E,12.5,77.5,091218,2.1,W,A";
        const std::string sentence = NmeaChecksum::frameSentence(raw);

        RmcData rmc {};
        ASSERT_TRUE(NmeaSentenceParser::parseRmc(sentence, rmc, true));

        EXPECT_TRUE(rmc.valid);
        EXPECT_TRUE(rmc.statusActive);
        EXPECT_EQ(rmc.utcTime.hour, 8U);
        EXPECT_EQ(rmc.utcTime.minute, 35U);
        EXPECT_EQ(rmc.utcTime.second, 59U);

        EXPECT_NEAR(rmc.coordinates.latitudeDeg, 47.0 + 17.11437 / 60.0, 1e-4);
        EXPECT_NEAR(rmc.coordinates.longitudeDeg, 8.0 + 33.91522 / 60.0, 1e-4);

        EXPECT_NEAR(rmc.speedOverGroundKnots, 12.5, 1e-2);
        EXPECT_NEAR(rmc.courseOverGroundDegrees, 77.5, 1e-2);

        EXPECT_EQ(rmc.date.day, 9U);
        EXPECT_EQ(rmc.date.month, 12U);
        EXPECT_EQ(rmc.date.year, 2018U);

        EXPECT_NEAR(rmc.magneticVariationDegrees, -2.1, 1e-2);
        EXPECT_EQ(rmc.faaMode, NmeaFaaMode::Autonomous);
    }

    TEST(TestNmeaSentenceParser, ParseHdtAndThs)
    {
        const std::string hdtSentence = "$HEHDT,341.8,T*21\r\n";
        HdtData hdt {};
        ASSERT_TRUE(NmeaSentenceParser::parseHdt(hdtSentence, hdt, true));
        EXPECT_TRUE(hdt.valid);
        EXPECT_NEAR(hdt.headingDegrees, 341.8, 1e-2);

        const std::string thsRaw = "HETHS,185.3,A";
        const std::string thsSentence = NmeaChecksum::frameSentence(thsRaw);
        ThsData ths {};
        ASSERT_TRUE(NmeaSentenceParser::parseThs(thsSentence, ths, true));
        EXPECT_TRUE(ths.valid);
        EXPECT_NEAR(ths.headingDegrees, 185.3, 1e-2);
        EXPECT_EQ(ths.mode, NmeaFaaMode::Autonomous);
    }

    TEST(TestNmeaSentenceParser, ParseTtmRadarTarget)
    {
        // $RATTM,05,12.34,045.2,T,18.5,120.0,T,1.2,-5.4,K,VESSEL_ALPHA,T,,142530.00,A
        const std::string raw = "RATTM,05,12.34,045.2,T,18.5,120.0,T,1.2,-5.4,K,VESSEL_ALPHA,T,,142530.00,A";
        const std::string sentence = NmeaChecksum::frameSentence(raw);

        TtmData ttm {};
        ASSERT_TRUE(NmeaSentenceParser::parseTtm(sentence, ttm, true));

        EXPECT_TRUE(ttm.valid);
        EXPECT_EQ(ttm.targetNumber, 5U);
        EXPECT_NEAR(ttm.targetDistanceNmi, 12.34, 1e-2);
        EXPECT_NEAR(ttm.bearingDegrees, 45.2, 1e-2);
        EXPECT_EQ(ttm.bearingReference, TtmReference::True);

        EXPECT_NEAR(ttm.targetSpeedKnots, 18.5, 1e-2);
        EXPECT_NEAR(ttm.targetCourseDegrees, 120.0, 1e-2);
        EXPECT_EQ(ttm.courseReference, TtmReference::True);

        EXPECT_NEAR(ttm.distanceCpaNmi, 1.2, 1e-2);
        EXPECT_NEAR(ttm.timeCpaMinutes, -5.4, 1e-2);
        EXPECT_EQ(ttm.speedDistanceUnits, 'K');
        EXPECT_EQ(ttm.targetName, "VESSEL_ALPHA");
        EXPECT_EQ(ttm.status, TtmTargetStatus::Tracking);
        EXPECT_FALSE(ttm.referenceTarget);

        EXPECT_EQ(ttm.utcTimeTag.hour, 14U);
        EXPECT_EQ(ttm.utcTimeTag.minute, 25U);
        EXPECT_EQ(ttm.utcTimeTag.second, 30U);
        EXPECT_EQ(ttm.acquisitionType, 'A');
    }

    TEST(TestNmeaSentenceParser, ParseTllTarget)
    {
        // $RATLL,03,3612.34,N,01425.67,E,FERRY_1,123045.00,T,
        const std::string raw = "RATLL,03,3612.34,N,01425.67,E,FERRY_1,123045.00,T,";
        const std::string sentence = NmeaChecksum::frameSentence(raw);

        TllData tll {};
        ASSERT_TRUE(NmeaSentenceParser::parseTll(sentence, tll, true));

        EXPECT_TRUE(tll.valid);
        EXPECT_EQ(tll.targetNumber, 3U);
        EXPECT_NEAR(tll.coordinates.latitudeDeg, 36.0 + 12.34 / 60.0, 1e-4);
        EXPECT_NEAR(tll.coordinates.longitudeDeg, 14.0 + 25.67 / 60.0, 1e-4);
        EXPECT_EQ(tll.targetName, "FERRY_1");
        EXPECT_EQ(tll.utcTimeTag.hour, 12U);
        EXPECT_EQ(tll.utcTimeTag.minute, 30U);
        EXPECT_EQ(tll.utcTimeTag.second, 45U);
        EXPECT_EQ(tll.status, TtmTargetStatus::Tracking);
    }

    TEST(TestNmeaSentenceParser, ParseXdrPitchRoll)
    {
        // $IIXDR,A,-2.45,D,PITCH,A,1.80,D,ROLL
        const std::string raw = "IIXDR,A,-2.45,D,PITCH,A,1.80,D,ROLL";
        const std::string sentence = NmeaChecksum::frameSentence(raw);

        XdrData xdr {};
        ASSERT_TRUE(NmeaSentenceParser::parseXdr(sentence, xdr, true));

        EXPECT_TRUE(xdr.valid);
        ASSERT_EQ(xdr.transducers.size(), 2U);

        EXPECT_EQ(xdr.transducers[0].type, 'A');
        EXPECT_NEAR(xdr.transducers[0].measurement, -2.45, 1e-2);
        EXPECT_EQ(xdr.transducers[0].units, 'D');
        EXPECT_EQ(xdr.transducers[0].id, "PITCH");

        EXPECT_EQ(xdr.transducers[1].type, 'A');
        EXPECT_NEAR(xdr.transducers[1].measurement, 1.80, 1e-2);
        EXPECT_EQ(xdr.transducers[1].units, 'D');
        EXPECT_EQ(xdr.transducers[1].id, "ROLL");
    }

} // namespace
} // namespace Nmea
