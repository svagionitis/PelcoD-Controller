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

    TEST(TestNmeaSentenceBuilder, RoundtripGsa)
    {
        GsaData inData {};
        inData.selectionMode = 'A';
        inData.fixMode = 3U;
        inData.activeSatellitePrns = { 3U, 7U, 11U, 14U, 19U, 28U };
        inData.pdop = 1.8;
        inData.hdop = 1.0;
        inData.vdop = 1.5;
        inData.systemId = 1U;

        const std::string sentence = NmeaSentenceBuilder::buildGsa(inData, "GP");
        EXPECT_TRUE(NmeaChecksum::validate(sentence));

        GsaData outData {};
        ASSERT_TRUE(NmeaSentenceParser::parseGsa(sentence, outData, true));
        EXPECT_TRUE(outData.valid);
        EXPECT_EQ(outData.selectionMode, 'A');
        EXPECT_EQ(outData.fixMode, 3U);
        EXPECT_EQ(outData.activeSatellitePrns.size(), 6U);
        EXPECT_EQ(outData.activeSatellitePrns[0], 3U);
        EXPECT_EQ(outData.activeSatellitePrns[5], 28U);
        EXPECT_NEAR(outData.pdop, 1.8, 1e-1);
        EXPECT_NEAR(outData.hdop, 1.0, 1e-1);
        EXPECT_NEAR(outData.vdop, 1.5, 1e-1);
    }

    TEST(TestNmeaSentenceBuilder, RoundtripGsv)
    {
        GsvData inData {};
        inData.totalSentences = 1U;
        inData.sentenceNumber = 1U;
        inData.totalSatellitesInView = 2U;
        inData.satellites = {
            { 5U, 45.0, 120.0, 48.0 },
            { 9U, 30.0, 240.0, std::nullopt }
        };

        const std::string sentence = NmeaSentenceBuilder::buildGsv(inData, "GP");
        EXPECT_TRUE(NmeaChecksum::validate(sentence));

        GsvData outData {};
        ASSERT_TRUE(NmeaSentenceParser::parseGsv(sentence, outData, true));
        EXPECT_TRUE(outData.valid);
        EXPECT_EQ(outData.totalSentences, 1U);
        EXPECT_EQ(outData.totalSatellitesInView, 2U);
        ASSERT_EQ(outData.satellites.size(), 2U);
        EXPECT_EQ(outData.satellites[0].prn, 5U);
        EXPECT_NEAR(outData.satellites[0].elevationDeg, 45.0, 1e-1);
        EXPECT_TRUE(outData.satellites[0].snrDb.has_value());
        EXPECT_FALSE(outData.satellites[1].snrDb.has_value());
    }

    TEST(TestNmeaSentenceBuilder, RoundtripZda)
    {
        ZdaData inData {};
        inData.utcTime = { 14, 25, 40, 250 };
        inData.day = 29U;
        inData.month = 9U;
        inData.year = 2026U;
        inData.localZoneHours = 3;
        inData.localZoneMinutes = 0U;

        const std::string sentence = NmeaSentenceBuilder::buildZda(inData, "GP");
        EXPECT_TRUE(NmeaChecksum::validate(sentence));

        ZdaData outData {};
        ASSERT_TRUE(NmeaSentenceParser::parseZda(sentence, outData, true));
        EXPECT_TRUE(outData.valid);
        EXPECT_EQ(outData.utcTime.hour, 14U);
        EXPECT_EQ(outData.utcTime.minute, 25U);
        EXPECT_EQ(outData.utcTime.second, 40U);
        EXPECT_EQ(outData.day, 29U);
        EXPECT_EQ(outData.month, 9U);
        EXPECT_EQ(outData.year, 2026U);
        EXPECT_EQ(outData.localZoneHours, 3);
    }

    TEST(TestNmeaSentenceBuilder, RoundtripVbwAndVhw)
    {
        VbwData inVbw {};
        inVbw.longitudinalWaterSpeedKnots = 15.2;
        inVbw.transverseWaterSpeedKnots = 0.5;
        inVbw.waterSpeedStatus = 'A';
        inVbw.longitudinalGroundSpeedKnots = 15.5;
        inVbw.transverseGroundSpeedKnots = 0.6;
        inVbw.groundSpeedStatus = 'A';

        const std::string vbwSent = NmeaSentenceBuilder::buildVbw(inVbw, "VD");
        EXPECT_TRUE(NmeaChecksum::validate(vbwSent));

        VbwData outVbw {};
        ASSERT_TRUE(NmeaSentenceParser::parseVbw(vbwSent, outVbw, true));
        EXPECT_TRUE(outVbw.valid);
        EXPECT_NEAR(outVbw.longitudinalWaterSpeedKnots, 15.2, 1e-1);
        EXPECT_NEAR(outVbw.longitudinalGroundSpeedKnots, 15.5, 1e-1);

        VhwData inVhw {};
        inVhw.headingDegreesTrue = 210.5;
        inVhw.headingDegreesMagnetic = 208.1;
        inVhw.speedWaterKnots = 14.8;
        inVhw.speedWaterKmh = 27.4;

        const std::string vhwSent = NmeaSentenceBuilder::buildVhw(inVhw, "VW");
        EXPECT_TRUE(NmeaChecksum::validate(vhwSent));

        VhwData outVhw {};
        ASSERT_TRUE(NmeaSentenceParser::parseVhw(vhwSent, outVhw, true));
        EXPECT_TRUE(outVhw.valid);
        ASSERT_TRUE(outVhw.headingDegreesTrue.has_value());
        EXPECT_NEAR(*outVhw.headingDegreesTrue, 210.5, 1e-1);
    }

    TEST(TestNmeaSentenceBuilder, RoundtripDptAndDbt)
    {
        DptData inDpt {};
        inDpt.waterDepthMeters = 32.4;
        inDpt.offsetMeters = -2.1;

        const std::string dptSent = NmeaSentenceBuilder::buildDpt(inDpt, "SD");
        EXPECT_TRUE(NmeaChecksum::validate(dptSent));

        DptData outDpt {};
        ASSERT_TRUE(NmeaSentenceParser::parseDpt(dptSent, outDpt, true));
        EXPECT_TRUE(outDpt.valid);
        EXPECT_NEAR(outDpt.waterDepthMeters, 32.4, 1e-1);
        EXPECT_NEAR(outDpt.offsetMeters, -2.1, 1e-1);

        DbtData inDbt {};
        inDbt.depthFeet = 99.0;
        inDbt.depthMeters = 30.18;
        inDbt.depthFathoms = 16.5;

        const std::string dbtSent = NmeaSentenceBuilder::buildDbt(inDbt, "SD");
        EXPECT_TRUE(NmeaChecksum::validate(dbtSent));

        DbtData outDbt {};
        ASSERT_TRUE(NmeaSentenceParser::parseDbt(dbtSent, outDbt, true));
        EXPECT_TRUE(outDbt.valid);
        EXPECT_NEAR(outDbt.depthFeet, 99.0, 1e-1);
    }

    TEST(TestNmeaSentenceBuilder, RoundtripBamSentences)
    {
        Bam::AlfData inAlf {};
        inAlf.alertPriority = 'W';
        inAlf.alertCategory = 'B';
        inAlf.alertState = 'S';
        inAlf.alertIdentifier = 2005U;
        inAlf.alertInstance = 1U;
        inAlf.alertText = "High Gimbal Temp";

        const std::string alfSent = NmeaSentenceBuilder::buildAlf(inAlf, "BN");
        EXPECT_TRUE(NmeaChecksum::validate(alfSent));

        Bam::AlfData outAlf {};
        ASSERT_TRUE(NmeaSentenceParser::parseAlf(alfSent, outAlf, true));
        EXPECT_TRUE(outAlf.valid);
        EXPECT_EQ(outAlf.alertPriority, 'W');
        EXPECT_EQ(outAlf.alertIdentifier, 2005U);
        EXPECT_EQ(outAlf.alertText, "High Gimbal Temp");

        Bam::AlcData inAlc {};
        inAlc.alertCount = 1U;
        inAlc.alertEntries = { { 2005U, 1U, 2U } };

        const std::string alcSent = NmeaSentenceBuilder::buildAlc(inAlc, "BN");
        EXPECT_TRUE(NmeaChecksum::validate(alcSent));

        Bam::AlcData outAlc {};
        ASSERT_TRUE(NmeaSentenceParser::parseAlc(alcSent, outAlc, true));
        EXPECT_TRUE(outAlc.valid);
        ASSERT_EQ(outAlc.alertEntries.size(), 1U);
        EXPECT_EQ(outAlc.alertEntries[0].alertIdentifier, 2005U);

        Bam::ArcData inArc {};
        inArc.alertIdentifier = 2005U;
        inArc.alertInstance = 1U;
        inArc.command = 'Q';

        const std::string arcSent = NmeaSentenceBuilder::buildArc(inArc, "BN");
        EXPECT_TRUE(NmeaChecksum::validate(arcSent));

        Bam::ArcData outArc {};
        ASSERT_TRUE(NmeaSentenceParser::parseArc(arcSent, outArc, true));
        EXPECT_TRUE(outArc.valid);
        EXPECT_EQ(outArc.command, 'Q');

        Bam::HbtData inHbt {};
        inHbt.configuredIntervalSec = 30.0;
        inHbt.equipmentStatus = 'A';
        inHbt.sequentialSentenceId = 3U;

        const std::string hbtSent = NmeaSentenceBuilder::buildHbt(inHbt, "BN");
        EXPECT_TRUE(NmeaChecksum::validate(hbtSent));

        Bam::HbtData outHbt {};
        ASSERT_TRUE(NmeaSentenceParser::parseHbt(hbtSent, outHbt, true));
        EXPECT_TRUE(outHbt.valid);
        EXPECT_EQ(outHbt.sequentialSentenceId, 3U);
    }

    TEST(TestNmeaSentenceBuilder, BuildExtendedPfecCommands)
    {
        const std::string palSent = NmeaSentenceBuilder::buildPfecPalette(3U);
        EXPECT_TRUE(NmeaChecksum::validate(palSent));
        EXPECT_NE(palSent.find("PFEC,GPcam,p,3"), std::string::npos);

        const std::string stabOn = NmeaSentenceBuilder::buildPfecStabilization(true);
        EXPECT_TRUE(NmeaChecksum::validate(stabOn));
        EXPECT_NE(stabOn.find("PFEC,GPcam,s,on"), std::string::npos);

        const std::string stabOff = NmeaSentenceBuilder::buildPfecStabilization(false);
        EXPECT_TRUE(NmeaChecksum::validate(stabOff));
        EXPECT_NE(stabOff.find("PFEC,GPcam,s,off"), std::string::npos);

        const std::string nucSent = NmeaSentenceBuilder::buildPfecNuc();
        EXPECT_TRUE(NmeaChecksum::validate(nucSent));
        EXPECT_NE(nucSent.find("PFEC,GPcam,nuc"), std::string::npos);

        const std::string zoomSent = NmeaSentenceBuilder::buildPfecDigitalZoom(4.0);
        EXPECT_TRUE(NmeaChecksum::validate(zoomSent));
        EXPECT_NE(zoomSent.find("PFEC,GPcam,z,4.0"), std::string::npos);
    }

} // namespace
} // namespace Nmea
