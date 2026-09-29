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
        EXPECT_EQ(NmeaSentenceParser::identifySentence("$PASHR,120000.00,045.2,T*00"), NmeaSentenceId::PASHR);

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

    TEST(TestNmeaSentenceParser, ParsePashrAttitude)
    {
        // $PASHR,120000.00,045.20,T,+03.50,-02.10,+0.12,0.05,0.05,0.10,2,1
        const std::string raw = "PASHR,120000.00,045.20,T,+03.50,-02.10,+0.12,0.05,0.05,0.10,2,1";
        const std::string sentence = NmeaChecksum::frameSentence(raw);

        PashrData pashr {};
        ASSERT_TRUE(NmeaSentenceParser::parsePashr(sentence, pashr, true));

        EXPECT_TRUE(pashr.valid);
        EXPECT_EQ(pashr.utcTime.hour, 12U);
        EXPECT_EQ(pashr.utcTime.minute, 0U);
        EXPECT_EQ(pashr.utcTime.second, 0U);
        EXPECT_NEAR(pashr.headingDegrees, 45.2, 1e-2);
        EXPECT_TRUE(pashr.isTrueHeading);
        EXPECT_NEAR(pashr.rollDegrees, 3.5, 1e-2);
        EXPECT_NEAR(pashr.pitchDegrees, -2.1, 1e-2);
        EXPECT_NEAR(pashr.heaveMeters, 0.12, 1e-2);
        EXPECT_NEAR(pashr.rollAccuracyDeg, 0.05, 1e-3);
        EXPECT_NEAR(pashr.pitchAccuracyDeg, 0.05, 1e-3);
        EXPECT_NEAR(pashr.headingAccuracyDeg, 0.10, 1e-3);
        EXPECT_EQ(pashr.gpsQualityFlag, 2U);
        EXPECT_EQ(pashr.imuStatusFlag, 1U);
    }

    TEST(TestNmeaSentenceParser, ParsePfecAttitude)
    {
        // $PFEC,GPatt,180.50,-04.20,+08.10
        const std::string raw = "PFEC,GPatt,180.50,-04.20,+08.10";
        const std::string sentence = NmeaChecksum::frameSentence(raw);

        PfecAttitudeData pfec {};
        ASSERT_TRUE(NmeaSentenceParser::parsePfecAtt(sentence, pfec, true));

        EXPECT_TRUE(pfec.valid);
        EXPECT_NEAR(pfec.yawDegrees, 180.5, 1e-2);
        EXPECT_NEAR(pfec.pitchDegrees, -4.2, 1e-2);
        EXPECT_NEAR(pfec.rollDegrees, 8.1, 1e-2);
    }

    TEST(TestNmeaSentenceParser, ParseGsaSentence)
    {
        const std::string raw = "GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1,1";
        const std::string sentence = NmeaChecksum::frameSentence(raw);

        GsaData gsa {};
        ASSERT_TRUE(NmeaSentenceParser::parseGsa(sentence, gsa, true));
        EXPECT_TRUE(gsa.valid);
        EXPECT_EQ(gsa.selectionMode, 'A');
        EXPECT_EQ(gsa.fixMode, 3U);
        EXPECT_EQ(gsa.activeSatellitePrns.size(), 5U);
        EXPECT_EQ(gsa.activeSatellitePrns[0], 4U);
        EXPECT_EQ(gsa.activeSatellitePrns[1], 5U);
        EXPECT_EQ(gsa.activeSatellitePrns[2], 9U);
        EXPECT_EQ(gsa.activeSatellitePrns[3], 12U);
        EXPECT_EQ(gsa.activeSatellitePrns[4], 24U);
        EXPECT_NEAR(gsa.pdop, 2.5, 1e-2);
        EXPECT_NEAR(gsa.hdop, 1.3, 1e-2);
        EXPECT_NEAR(gsa.vdop, 2.1, 1e-2);
        ASSERT_TRUE(gsa.systemId.has_value());
        EXPECT_EQ(*gsa.systemId, 1U);
    }

    TEST(TestNmeaSentenceParser, ParseGsvSentence)
    {
        const std::string raw = "GPGSV,2,1,08,01,40,083,46,02,17,308,41,12,07,344,,14,66,039,45,1";
        const std::string sentence = NmeaChecksum::frameSentence(raw);

        GsvData gsv {};
        ASSERT_TRUE(NmeaSentenceParser::parseGsv(sentence, gsv, true));
        EXPECT_TRUE(gsv.valid);
        EXPECT_EQ(gsv.totalSentences, 2U);
        EXPECT_EQ(gsv.sentenceNumber, 1U);
        EXPECT_EQ(gsv.totalSatellitesInView, 8U);
        ASSERT_EQ(gsv.satellites.size(), 4U);

        EXPECT_EQ(gsv.satellites[0].prn, 1U);
        EXPECT_NEAR(gsv.satellites[0].elevationDeg, 40.0, 1e-1);
        EXPECT_NEAR(gsv.satellites[0].azimuthDeg, 83.0, 1e-1);
        ASSERT_TRUE(gsv.satellites[0].snrDb.has_value());
        EXPECT_NEAR(*gsv.satellites[0].snrDb, 46.0, 1e-1);

        EXPECT_EQ(gsv.satellites[2].prn, 12U);
        EXPECT_FALSE(gsv.satellites[2].snrDb.has_value()); // Tracking without lock

        ASSERT_TRUE(gsv.signalId.has_value());
        EXPECT_EQ(*gsv.signalId, 1U);
    }

    TEST(TestNmeaSentenceParser, ParseZdaSentence)
    {
        const std::string raw = "GPZDA,201530.50,04,07,2026,02,30";
        const std::string sentence = NmeaChecksum::frameSentence(raw);

        ZdaData zda {};
        ASSERT_TRUE(NmeaSentenceParser::parseZda(sentence, zda, true));
        EXPECT_TRUE(zda.valid);
        EXPECT_EQ(zda.utcTime.hour, 20U);
        EXPECT_EQ(zda.utcTime.minute, 15U);
        EXPECT_EQ(zda.utcTime.second, 30U);
        EXPECT_EQ(zda.utcTime.millisecond, 500U);
        EXPECT_EQ(zda.day, 4U);
        EXPECT_EQ(zda.month, 7U);
        EXPECT_EQ(zda.year, 2026U);
        EXPECT_EQ(zda.localZoneHours, 2);
        EXPECT_EQ(zda.localZoneMinutes, 30U);
    }

    TEST(TestNmeaSentenceParser, ParseVbwSentence)
    {
        const std::string raw = "IIVBW,12.50,0.30,A,12.80,0.40,A,0.20,A,0.25,A";
        const std::string sentence = NmeaChecksum::frameSentence(raw);

        VbwData vbw {};
        ASSERT_TRUE(NmeaSentenceParser::parseVbw(sentence, vbw, true));
        EXPECT_TRUE(vbw.valid);
        EXPECT_NEAR(vbw.longitudinalWaterSpeedKnots, 12.5, 1e-2);
        EXPECT_NEAR(vbw.transverseWaterSpeedKnots, 0.3, 1e-2);
        EXPECT_EQ(vbw.waterSpeedStatus, 'A');
        EXPECT_NEAR(vbw.longitudinalGroundSpeedKnots, 12.8, 1e-2);
        EXPECT_NEAR(vbw.transverseGroundSpeedKnots, 0.4, 1e-2);
        EXPECT_EQ(vbw.groundSpeedStatus, 'A');
        ASSERT_TRUE(vbw.sternWaterSpeedKnots.has_value());
        EXPECT_NEAR(*vbw.sternWaterSpeedKnots, 0.2, 1e-2);
    }

    TEST(TestNmeaSentenceParser, ParseVhwSentence)
    {
        const std::string raw = "IIVHW,125.4,T,122.1,M,12.4,N,23.0,K";
        const std::string sentence = NmeaChecksum::frameSentence(raw);

        VhwData vhw {};
        ASSERT_TRUE(NmeaSentenceParser::parseVhw(sentence, vhw, true));
        EXPECT_TRUE(vhw.valid);
        ASSERT_TRUE(vhw.headingDegreesTrue.has_value());
        EXPECT_NEAR(*vhw.headingDegreesTrue, 125.4, 1e-2);
        ASSERT_TRUE(vhw.headingDegreesMagnetic.has_value());
        EXPECT_NEAR(*vhw.headingDegreesMagnetic, 122.1, 1e-2);
        ASSERT_TRUE(vhw.speedWaterKnots.has_value());
        EXPECT_NEAR(*vhw.speedWaterKnots, 12.4, 1e-2);
        ASSERT_TRUE(vhw.speedWaterKmh.has_value());
        EXPECT_NEAR(*vhw.speedWaterKmh, 23.0, 1e-2);
    }

    TEST(TestNmeaSentenceParser, ParseDptAndDbtSentences)
    {
        // DPT
        const std::string dptRaw = "SDDPT,24.5,1.5,100.0";
        const std::string dptSent = NmeaChecksum::frameSentence(dptRaw);
        DptData dpt {};
        ASSERT_TRUE(NmeaSentenceParser::parseDpt(dptSent, dpt, true));
        EXPECT_TRUE(dpt.valid);
        EXPECT_NEAR(dpt.waterDepthMeters, 24.5, 1e-2);
        EXPECT_NEAR(dpt.offsetMeters, 1.5, 1e-2);
        ASSERT_TRUE(dpt.maximumRangeScaleMeters.has_value());
        EXPECT_NEAR(*dpt.maximumRangeScaleMeters, 100.0, 1e-2);

        // DBT
        const std::string dbtRaw = "SDDBT,80.4,f,24.5,M,13.4,F";
        const std::string dbtSent = NmeaChecksum::frameSentence(dbtRaw);
        DbtData dbt {};
        ASSERT_TRUE(NmeaSentenceParser::parseDbt(dbtSent, dbt, true));
        EXPECT_TRUE(dbt.valid);
        EXPECT_NEAR(dbt.depthFeet, 80.4, 1e-2);
        EXPECT_NEAR(dbt.depthMeters, 24.5, 1e-2);
        EXPECT_NEAR(dbt.depthFathoms, 13.4, 1e-2);
    }

    TEST(TestNmeaSentenceParser, ParseBamSentences)
    {
        // ALF
        const std::string alfRaw = "BNALF,1,1,0,123045.00,A,B,V,1001,1,1,0,Thermal Overheat";
        const std::string alfSent = NmeaChecksum::frameSentence(alfRaw);
        Bam::AlfData alf {};
        ASSERT_TRUE(NmeaSentenceParser::parseAlf(alfSent, alf, true));
        EXPECT_TRUE(alf.valid);
        EXPECT_EQ(alf.alertPriority, 'A');
        EXPECT_EQ(alf.alertCategory, 'B');
        EXPECT_EQ(alf.alertState, 'V');
        EXPECT_EQ(alf.alertIdentifier, 1001U);
        EXPECT_EQ(alf.alertInstance, 1U);
        EXPECT_EQ(alf.alertText, "Thermal Overheat");

        // ALC
        const std::string alcRaw = "BNALC,1,1,0,2,1001,1,1,1002,1,2";
        const std::string alcSent = NmeaChecksum::frameSentence(alcRaw);
        Bam::AlcData alc {};
        ASSERT_TRUE(NmeaSentenceParser::parseAlc(alcSent, alc, true));
        EXPECT_TRUE(alc.valid);
        EXPECT_EQ(alc.alertCount, 2U);
        ASSERT_EQ(alc.alertEntries.size(), 2U);
        EXPECT_EQ(alc.alertEntries[0].alertIdentifier, 1001U);
        EXPECT_EQ(alc.alertEntries[1].alertIdentifier, 1002U);
        EXPECT_EQ(alc.alertEntries[1].revisionCounter, 2U);

        // ARC
        const std::string arcRaw = "BNARC,123100.00,1001,1,A";
        const std::string arcSent = NmeaChecksum::frameSentence(arcRaw);
        Bam::ArcData arc {};
        ASSERT_TRUE(NmeaSentenceParser::parseArc(arcSent, arc, true));
        EXPECT_TRUE(arc.valid);
        EXPECT_EQ(arc.alertIdentifier, 1001U);
        EXPECT_EQ(arc.command, 'A');

        // HBT
        const std::string hbtRaw = "BNHBT,60.0,A,1";
        const std::string hbtSent = NmeaChecksum::frameSentence(hbtRaw);
        Bam::HbtData hbt {};
        ASSERT_TRUE(NmeaSentenceParser::parseHbt(hbtSent, hbt, true));
        EXPECT_TRUE(hbt.valid);
        EXPECT_NEAR(hbt.configuredIntervalSec, 60.0, 1e-1);
        EXPECT_EQ(hbt.equipmentStatus, 'A');
        EXPECT_EQ(hbt.sequentialSentenceId, 1U);

        // ALR
        const std::string alrRaw = "BNALR,123045.00,1001,A,V,Thermal Overheat";
        const std::string alrSent = NmeaChecksum::frameSentence(alrRaw);
        Bam::AlrData alr {};
        ASSERT_TRUE(NmeaSentenceParser::parseAlr(alrSent, alr, true));
        EXPECT_TRUE(alr.valid);
        EXPECT_EQ(alr.alertIdentifier, 1001U);
        EXPECT_EQ(alr.condition, 'A');
        EXPECT_EQ(alr.acknowledgeState, 'V');

        // ACK
        const std::string ackRaw = "BNACK,1001";
        const std::string ackSent = NmeaChecksum::frameSentence(ackRaw);
        Bam::AckData ack {};
        ASSERT_TRUE(NmeaSentenceParser::parseAck(ackSent, ack, true));
        EXPECT_TRUE(ack.valid);
        EXPECT_EQ(ack.alertIdentifier, 1001U);
    }

} // namespace
} // namespace Nmea
