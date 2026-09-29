/// @file TestNmeaMaritimeSentences.cpp
/// @brief Unit tests for advanced maritime NMEA sentences: RSD, OSD, APB, BWC, MWV, HDG.

#include "NmeaDevice.h"
#include "NmeaSentenceBuilder.h"
#include "NmeaSentenceParser.h"
#include "Transport/BaseTransport.h"

#include <gtest/gtest.h>

using namespace Nmea;

namespace {

class MockMaritimeTransport : public Transport::BaseTransport {
public:
    MockMaritimeTransport() = default;
    ~MockMaritimeTransport() override = default;

    bool open() override
    {
        m_isOpen = true;
        return true;
    }

    void close() override
    {
        m_isOpen = false;
    }

    bool isOpen() const noexcept override
    {
        return m_isOpen;
    }

    bool sendData(const std::vector<std::uint8_t>& /*data*/) override
    {
        return true;
    }

private:
    bool m_isOpen { true };
};

} // namespace

TEST(TestNmeaMaritimeSentences, RsdRadarCursorRoundtrip)
{
    RsdData input {};
    input.cursorRangeNmi = 4.25;
    input.cursorBearingDeg = 135.5;
    input.rangeScaleNmi = 12.0;
    input.displayRotation = 'N'; // North-up

    const std::string sentence = NmeaSentenceBuilder::buildRsd(input, "RA");
    EXPECT_FALSE(sentence.empty());

    EXPECT_EQ(NmeaSentenceParser::identifySentence(sentence), NmeaSentenceId::RSD);
    EXPECT_EQ(NmeaSentenceParser::extractTalkerId(sentence), "RA");

    RsdData parsed {};
    EXPECT_TRUE(NmeaSentenceParser::parseRsd(sentence, parsed, true));
    EXPECT_TRUE(parsed.valid);
    EXPECT_NEAR(parsed.cursorRangeNmi, 4.25, 1e-4);
    EXPECT_NEAR(parsed.cursorBearingDeg, 135.5, 1e-4);
    EXPECT_NEAR(parsed.rangeScaleNmi, 12.0, 1e-4);
    EXPECT_EQ(parsed.displayRotation, 'N');
}

TEST(TestNmeaMaritimeSentences, OsdOwnShipDataRoundtrip)
{
    OsdData input {};
    input.headingDegrees = 270.4;
    input.headingValid = true;
    input.courseDegrees = 272.1;
    input.courseReference = 'P'; // Positioning system
    input.vesselSpeed = 15.3;
    input.speedReference = 'P';
    input.vesselSetDeg = 180.0;
    input.vesselDriftSpeed = 1.2;
    input.speedUnits = 'N'; // Knots

    const std::string sentence = NmeaSentenceBuilder::buildOsd(input, "RA");
    EXPECT_FALSE(sentence.empty());

    EXPECT_EQ(NmeaSentenceParser::identifySentence(sentence), NmeaSentenceId::OSD);

    OsdData parsed {};
    EXPECT_TRUE(NmeaSentenceParser::parseOsd(sentence, parsed, true));
    EXPECT_TRUE(parsed.valid);
    EXPECT_DOUBLE_EQ(parsed.headingDegrees, 270.4);
    EXPECT_DOUBLE_EQ(parsed.courseDegrees, 272.1);
    EXPECT_DOUBLE_EQ(parsed.vesselSpeed, 15.3);
    EXPECT_EQ(parsed.courseReference, 'P');
    EXPECT_EQ(parsed.speedUnits, 'N');
}

TEST(TestNmeaMaritimeSentences, ApbAutopilotRoundtrip)
{
    ApbData input {};
    input.generalWarning = false;
    input.cycleLockWarning = false;
    input.crossTrackErrorNmi = 0.045;
    input.directionToSteer = 'R';
    input.xteUnits = 'N';
    input.arrivalCircleEntered = false;
    input.perpendicularPassed = false;
    input.bearingOriginToDestDeg = 85.0;
    input.bearingOriginRef = 'T';
    input.destWaypointId = "WPT042";
    input.bearingPresentToDestDeg = 86.2;
    input.bearingPresentRef = 'T';
    input.headingToSteerDeg = 87.5;
    input.headingToSteerRef = 'T';
    input.faaMode = NmeaFaaMode::Autonomous;

    const std::string sentence = NmeaSentenceBuilder::buildApb(input, "AP");
    EXPECT_FALSE(sentence.empty());

    EXPECT_EQ(NmeaSentenceParser::identifySentence(sentence), NmeaSentenceId::APB);

    ApbData parsed {};
    EXPECT_TRUE(NmeaSentenceParser::parseApb(sentence, parsed, true));
    EXPECT_TRUE(parsed.valid);
    EXPECT_FALSE(parsed.generalWarning);
    EXPECT_NEAR(parsed.crossTrackErrorNmi, 0.045, 1e-4);
    EXPECT_EQ(parsed.directionToSteer, 'R');
    EXPECT_EQ(parsed.destWaypointId, "WPT042");
    EXPECT_DOUBLE_EQ(parsed.headingToSteerDeg, 87.5);
    EXPECT_EQ(parsed.faaMode, NmeaFaaMode::Autonomous);
}

TEST(TestNmeaMaritimeSentences, BwcWaypointRoundtrip)
{
    BwcData input {};
    input.utcTime = { 14, 30, 0, 0 };
    input.waypointCoordinates = { 37.9542, 23.6358 };
    input.bearingTrueDeg = 142.0;
    input.bearingMagneticDeg = 138.5;
    input.distanceNmi = 8.7;
    input.waypointId = "PIRAEUS_BUOY";
    input.faaMode = NmeaFaaMode::Differential;

    const std::string sentence = NmeaSentenceBuilder::buildBwc(input, "GP");
    EXPECT_FALSE(sentence.empty());

    EXPECT_EQ(NmeaSentenceParser::identifySentence(sentence), NmeaSentenceId::BWC);

    BwcData parsed {};
    EXPECT_TRUE(NmeaSentenceParser::parseBwc(sentence, parsed, true));
    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.utcTime.hour, 14);
    EXPECT_EQ(parsed.utcTime.minute, 30);
    EXPECT_NEAR(parsed.waypointCoordinates.latitudeDeg, 37.9542, 1e-3);
    EXPECT_NEAR(parsed.waypointCoordinates.longitudeDeg, 23.6358, 1e-3);
    EXPECT_DOUBLE_EQ(parsed.bearingTrueDeg, 142.0);
    EXPECT_DOUBLE_EQ(parsed.distanceNmi, 8.7);
    EXPECT_EQ(parsed.waypointId, "PIRAEUS_BUOY");
    EXPECT_EQ(parsed.faaMode, NmeaFaaMode::Differential);
}

TEST(TestNmeaMaritimeSentences, MwvWindSpeedAndAngleRoundtrip)
{
    MwvData input {};
    input.windAngleDeg = 45.0;
    input.reference = 'R'; // Relative / Apparent wind
    input.windSpeed = 18.5;
    input.speedUnits = 'N'; // Knots
    input.valid = true;

    const std::string sentence = NmeaSentenceBuilder::buildMwv(input, "WI");
    EXPECT_FALSE(sentence.empty());

    EXPECT_EQ(NmeaSentenceParser::identifySentence(sentence), NmeaSentenceId::MWV);

    MwvData parsed {};
    EXPECT_TRUE(NmeaSentenceParser::parseMwv(sentence, parsed, true));
    EXPECT_TRUE(parsed.valid);
    EXPECT_DOUBLE_EQ(parsed.windAngleDeg, 45.0);
    EXPECT_EQ(parsed.reference, 'R');
    EXPECT_DOUBLE_EQ(parsed.windSpeed, 18.5);
    EXPECT_EQ(parsed.speedUnits, 'N');
}

TEST(TestNmeaMaritimeSentences, HdgHeadingDeviationVariation)
{
    HdgData input {};
    input.magneticHeadingDeg = 120.5;
    input.hasDeviation = true;
    input.magneticDeviationDeg = -2.5; // 2.5 West
    input.hasVariation = true;
    input.magneticVariationDeg = 4.0; // 4.0 East

    const std::string sentence = NmeaSentenceBuilder::buildHdg(input, "HC");
    EXPECT_FALSE(sentence.empty());

    EXPECT_EQ(NmeaSentenceParser::identifySentence(sentence), NmeaSentenceId::HDG);

    HdgData parsed {};
    EXPECT_TRUE(NmeaSentenceParser::parseHdg(sentence, parsed, true));
    EXPECT_TRUE(parsed.valid);
    EXPECT_DOUBLE_EQ(parsed.magneticHeadingDeg, 120.5);
    EXPECT_TRUE(parsed.hasDeviation);
    EXPECT_DOUBLE_EQ(parsed.magneticDeviationDeg, -2.5);
    EXPECT_TRUE(parsed.hasVariation);
    EXPECT_DOUBLE_EQ(parsed.magneticVariationDeg, 4.0);
}

TEST(TestNmeaMaritimeSentences, NmeaDeviceMaritimeCallbacks)
{
    auto mockTransport = std::make_shared<MockMaritimeTransport>();
    NmeaDevice device(mockTransport);

    std::optional<RsdData> receivedRsd {};
    std::optional<ApbData> receivedApb {};
    std::optional<MwvData> receivedMwv {};
    std::optional<HdgData> receivedHdg {};

    device.addRsdCallback([&](const RsdData& rsd) { receivedRsd = rsd; });
    device.addApbCallback([&](const ApbData& apb) { receivedApb = apb; });
    device.addMwvCallback([&](const MwvData& mwv) { receivedMwv = mwv; });
    device.addHdgCallback([&](const HdgData& hdg) { receivedHdg = hdg; });

    // 1. Feed RSD sentence
    RsdData rsdInput {};
    rsdInput.cursorRangeNmi = 3.5;
    rsdInput.cursorBearingDeg = 92.0;
    rsdInput.rangeScaleNmi = 6.0;
    rsdInput.displayRotation = 'H';
    const std::string rsdStr = NmeaSentenceBuilder::buildRsd(rsdInput);
    device.feedRawBytes({ rsdStr.begin(), rsdStr.end() });

    ASSERT_TRUE(receivedRsd.has_value());
    EXPECT_DOUBLE_EQ(receivedRsd->cursorRangeNmi, 3.5);
    EXPECT_DOUBLE_EQ(receivedRsd->cursorBearingDeg, 92.0);
    EXPECT_EQ(device.lastRsd()->cursorRangeNmi, 3.5);

    // 2. Feed APB sentence
    ApbData apbInput {};
    apbInput.crossTrackErrorNmi = 0.12;
    apbInput.directionToSteer = 'L';
    apbInput.destWaypointId = "WAYPOINT_ALPHA";
    apbInput.headingToSteerDeg = 315.0;
    const std::string apbStr = NmeaSentenceBuilder::buildApb(apbInput);
    device.feedRawBytes({ apbStr.begin(), apbStr.end() });

    ASSERT_TRUE(receivedApb.has_value());
    EXPECT_DOUBLE_EQ(receivedApb->crossTrackErrorNmi, 0.12);
    EXPECT_EQ(receivedApb->directionToSteer, 'L');
    EXPECT_EQ(receivedApb->destWaypointId, "WAYPOINT_ALPHA");
    EXPECT_DOUBLE_EQ(receivedApb->headingToSteerDeg, 315.0);

    // 3. Feed MWV sentence
    MwvData mwvInput {};
    mwvInput.windAngleDeg = 210.0;
    mwvInput.reference = 'T';
    mwvInput.windSpeed = 25.0;
    mwvInput.speedUnits = 'N';
    mwvInput.valid = true;
    const std::string mwvStr = NmeaSentenceBuilder::buildMwv(mwvInput);
    device.feedRawBytes({ mwvStr.begin(), mwvStr.end() });

    ASSERT_TRUE(receivedMwv.has_value());
    EXPECT_DOUBLE_EQ(receivedMwv->windAngleDeg, 210.0);
    EXPECT_EQ(receivedMwv->reference, 'T');
    EXPECT_DOUBLE_EQ(receivedMwv->windSpeed, 25.0);

    // 4. Feed HDG sentence (with 3.0 deg East variation)
    HdgData hdgInput {};
    hdgInput.magneticHeadingDeg = 100.0;
    hdgInput.hasVariation = true;
    hdgInput.magneticVariationDeg = 3.0;
    const std::string hdgStr = NmeaSentenceBuilder::buildHdg(hdgInput);
    device.feedRawBytes({ hdgStr.begin(), hdgStr.end() });

    ASSERT_TRUE(receivedHdg.has_value());
    EXPECT_DOUBLE_EQ(receivedHdg->magneticHeadingDeg, 100.0);
    // HDG should have updated the own-ship nav heading to true heading (100.0 + 3.0 = 103.0)
    EXPECT_TRUE(device.navSnapshot().hasHeading);
    EXPECT_DOUBLE_EQ(device.navSnapshot().trueHeadingDegrees, 103.0);
}
