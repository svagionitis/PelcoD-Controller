#include "NmeaChecksum.h"
#include "gateway/NmeaGateway.h"
#include "n2k/N2kDecoder.h"
#include "n2k/N2kEncoder.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace Nmea::Gateway {

TEST(TestNmeaGateway, N2kToNmea0183PositionAndCogSog)
{
    GatewayConfig cfg {};
    cfg.enableN2kTo0183 = true;
    cfg.enable0183ToN2k = false;
    cfg.defaultDecimationInterval = std::chrono::milliseconds(0); // No decimation for test

    NmeaGateway gateway(cfg);

    std::vector<std::string> emittedSentences {};
    gateway.setSentenceOutputCallback([&](std::string_view s) { emittedSentences.emplace_back(s); });

    // 1. Send Position Rapid (PGN 129025)
    N2k::PositionRapid pos {};
    pos.latitudeDeg = 37.7749;
    pos.longitudeDeg = -122.4194;
    pos.isValid = true;

    const auto posFrame = N2k::N2kEncoder::encodePositionRapid(pos);
    gateway.onCanFrame(posFrame);

    ASSERT_FALSE(emittedSentences.empty());
    // Should emit GGA
    EXPECT_NE(emittedSentences.front().find("GGA"), std::string::npos);
    EXPECT_TRUE(NmeaChecksum::validate(emittedSentences.front()));

    // 2. Send CogSog Rapid (PGN 129026)
    emittedSentences.clear();
    N2k::CogSogRapid cogSog {};
    cogSog.cogDegrees = 180.0;
    cogSog.sogKnots = 12.5;
    cogSog.hasCog = true;
    cogSog.hasSog = true;

    const auto cogFrame = N2k::N2kEncoder::encodeCogSogRapid(cogSog);
    gateway.onCanFrame(cogFrame);

    ASSERT_FALSE(emittedSentences.empty());
    // Should emit RMC
    EXPECT_NE(emittedSentences.front().find("RMC"), std::string::npos);
    EXPECT_TRUE(NmeaChecksum::validate(emittedSentences.front()));
}

TEST(TestNmeaGateway, N2kToNmea0183HeadingAttitudeAndWind)
{
    GatewayConfig cfg {};
    cfg.enableN2kTo0183 = true;
    cfg.defaultDecimationInterval = std::chrono::milliseconds(0);

    NmeaGateway gateway(cfg);
    std::vector<std::string> emittedSentences {};
    gateway.setSentenceOutputCallback([&](std::string_view s) { emittedSentences.emplace_back(s); });

    // Heading
    N2k::VesselHeading hdg {};
    hdg.headingDegrees = 142.5;
    hdg.hasHeading = true;
    gateway.onCanFrame(N2k::N2kEncoder::encodeVesselHeading(hdg));

    ASSERT_FALSE(emittedSentences.empty());
    EXPECT_NE(emittedSentences.back().find("HDT"), std::string::npos);
    EXPECT_TRUE(NmeaChecksum::validate(emittedSentences.back()));

    // Attitude
    emittedSentences.clear();
    N2k::Attitude att {};
    att.pitchDegrees = 4.2;
    att.rollDegrees = -1.8;
    att.hasPitch = true;
    att.hasRoll = true;
    gateway.onCanFrame(N2k::N2kEncoder::encodeAttitude(att));

    ASSERT_FALSE(emittedSentences.empty());
    EXPECT_NE(emittedSentences.back().find("XDR"), std::string::npos);
    EXPECT_TRUE(NmeaChecksum::validate(emittedSentences.back()));

    // Wind
    emittedSentences.clear();
    N2k::WindData wind {};
    wind.windSpeedKnots = 15.0;
    wind.windSpeedMps = 15.0 * 0.514444;
    wind.windAngleDegrees = 60.0;
    wind.reference = N2k::WindReference::Apparent;
    wind.hasWindSpeed = true;
    wind.hasWindAngle = true;
    gateway.onCanFrame(N2k::N2kEncoder::encodeWindData(wind));

    ASSERT_FALSE(emittedSentences.empty());
    EXPECT_NE(emittedSentences.back().find("MWV"), std::string::npos);
    EXPECT_TRUE(NmeaChecksum::validate(emittedSentences.back()));
}

TEST(TestNmeaGateway, Nmea0183ToN2kConversion)
{
    GatewayConfig cfg {};
    cfg.enable0183ToN2k = true;
    cfg.defaultDecimationInterval = std::chrono::milliseconds(0);

    NmeaGateway gateway(cfg);
    std::vector<N2k::CanFrame> emittedFrames {};
    gateway.setCanFrameOutputCallback([&](const N2k::CanFrame& f) { emittedFrames.push_back(f); });

    // 1. Feed GGA
    const auto ggaStr = NmeaChecksum::frameSentence("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    gateway.onSentence(ggaStr);

    ASSERT_FALSE(emittedFrames.empty());
    auto hdr = N2k::N2kHeader::fromCanId(emittedFrames.back().id);
    EXPECT_EQ(hdr.pgn, static_cast<std::uint32_t>(N2k::Pgn::PositionRapidUpdate));

    N2k::PositionRapid pos {};
    ASSERT_TRUE(N2k::N2kDecoder::parsePgn129025(emittedFrames.back().data.data(), emittedFrames.back().dlc, pos));
    EXPECT_TRUE(pos.isValid);
    EXPECT_NEAR(pos.latitudeDeg, 48.1173, 1e-4);
    EXPECT_NEAR(pos.longitudeDeg, 11.5166, 1e-4);

    // 2. Feed HDT
    emittedFrames.clear();
    const auto hdtStr = NmeaChecksum::frameSentence("HEHDT,245.5,T");
    gateway.onSentence(hdtStr);

    ASSERT_FALSE(emittedFrames.empty());
    hdr = N2k::N2kHeader::fromCanId(emittedFrames.back().id);
    EXPECT_EQ(hdr.pgn, static_cast<std::uint32_t>(N2k::Pgn::VesselHeading));

    N2k::VesselHeading decodedHdg {};
    ASSERT_TRUE(
        N2k::N2kDecoder::parsePgn127250(emittedFrames.back().data.data(), emittedFrames.back().dlc, decodedHdg));
    EXPECT_TRUE(decodedHdg.hasHeading);
    EXPECT_NEAR(decodedHdg.headingDegrees, 245.5, 0.05);

    // 3. Feed XDR
    emittedFrames.clear();
    const auto xdrStr = NmeaChecksum::frameSentence("IIXDR,A,6.5,D,PITCH,A,-3.5,D,ROLL");
    gateway.onSentence(xdrStr);

    ASSERT_FALSE(emittedFrames.empty());
    hdr = N2k::N2kHeader::fromCanId(emittedFrames.back().id);
    EXPECT_EQ(hdr.pgn, static_cast<std::uint32_t>(N2k::Pgn::Attitude));

    N2k::Attitude att {};
    ASSERT_TRUE(N2k::N2kDecoder::parsePgn127257(emittedFrames.back().data.data(), emittedFrames.back().dlc, att));
    EXPECT_TRUE(att.hasPitch);
    EXPECT_TRUE(att.hasRoll);
    EXPECT_NEAR(att.pitchDegrees, 6.5, 0.05);
    EXPECT_NEAR(att.rollDegrees, -3.5, 0.05);
}

TEST(TestNmeaGateway, RateDecimationThrottling)
{
    GatewayConfig cfg {};
    cfg.enableN2kTo0183 = true;
    cfg.defaultDecimationInterval = std::chrono::milliseconds(500); // 500 ms limit

    NmeaGateway gateway(cfg);
    int sentenceCount { 0 };
    gateway.setSentenceOutputCallback([&](std::string_view) { ++sentenceCount; });

    N2k::VesselHeading hdg {};
    hdg.headingDegrees = 100.0;
    hdg.hasHeading = true;
    const auto frame = N2k::N2kEncoder::encodeVesselHeading(hdg);

    // Blast 5 frames immediately
    for (int i = 0; i < 5; ++i) {
        gateway.onCanFrame(frame);
    }

    // Only the first one should pass through; remaining 4 dropped by rate decimation
    EXPECT_EQ(sentenceCount, 1);
    EXPECT_EQ(gateway.stats().throttledDrops, 4U);
    EXPECT_EQ(gateway.stats().sentencesEmitted, 1U);
}

TEST(TestNmeaGateway, N2kToNmea0183RudderAndSystemTime)
{
    GatewayConfig cfg {};
    cfg.enableN2kTo0183 = true;
    cfg.defaultDecimationInterval = std::chrono::milliseconds(0);

    NmeaGateway gateway(cfg);
    std::vector<std::string> emitted {};
    gateway.setSentenceOutputCallback([&](std::string_view s) { emitted.emplace_back(s); });

    // 1. Rudder PGN 127245
    N2k::RudderData rudder {};
    rudder.positionDegrees = -8.5;
    rudder.hasPosition = true;
    gateway.onCanFrame(N2k::N2kEncoder::encodeRudder(rudder));

    ASSERT_FALSE(emitted.empty());
    EXPECT_NE(emitted.back().find("RSA"), std::string::npos);
    EXPECT_TRUE(NmeaChecksum::validate(emitted.back()));
    ASSERT_TRUE(gateway.lastRudder().has_value());
    EXPECT_NEAR(gateway.lastRudder()->positionDegrees, -8.5, 0.05);

    // 2. System Time PGN 126992
    emitted.clear();
    N2k::SystemTimeData sysTime {};
    sysTime.systemDateDays = 20614U; // 2026-06-10
    sysTime.secondsSinceMidnight = 57612.0; // 16:00:12
    sysTime.hasDate = true;
    sysTime.hasTime = true;
    gateway.onCanFrame(N2k::N2kEncoder::encodeSystemTime(sysTime));

    ASSERT_FALSE(emitted.empty());
    EXPECT_NE(emitted.back().find("ZDA"), std::string::npos);
    EXPECT_TRUE(NmeaChecksum::validate(emitted.back()));
    ASSERT_TRUE(gateway.lastSystemTime().has_value());
    EXPECT_EQ(gateway.lastSystemTime()->systemDateDays, 20614U);

    // 3. Mag Variation PGN 127258
    emitted.clear();
    N2k::MagneticVariation magVar {};
    magVar.variationDegrees = -2.5;
    magVar.hasVariation = true;
    gateway.onCanFrame(N2k::N2kEncoder::encodeMagneticVariation(magVar));

    ASSERT_FALSE(emitted.empty());
    EXPECT_NE(emitted.back().find("HDG"), std::string::npos);
    EXPECT_TRUE(NmeaChecksum::validate(emitted.back()));
    ASSERT_TRUE(gateway.lastMagVariation().has_value());
    EXPECT_NEAR(gateway.lastMagVariation()->variationDegrees, -2.5, 0.05);
}

TEST(TestNmeaGateway, Nmea0183ToN2kRudderAndSystemTime)
{
    GatewayConfig cfg {};
    cfg.enable0183ToN2k = true;
    cfg.defaultDecimationInterval = std::chrono::milliseconds(0);

    NmeaGateway gateway(cfg);
    std::vector<N2k::CanFrame> emitted {};
    gateway.setCanFrameOutputCallback([&](const N2k::CanFrame& f) { emitted.push_back(f); });

    // 1. Feed RSA
    const auto rsaStr = NmeaChecksum::frameSentence("IIRSA,12.5,A,,V");
    gateway.onSentence(rsaStr);

    ASSERT_FALSE(emitted.empty());
    auto hdr = N2k::N2kHeader::fromCanId(emitted.back().id);
    EXPECT_EQ(hdr.pgn, static_cast<std::uint32_t>(N2k::Pgn::Rudder));

    N2k::RudderData rOut {};
    ASSERT_TRUE(N2k::N2kDecoder::parsePgn127245(emitted.back().data.data(), emitted.back().dlc, rOut));
    EXPECT_TRUE(rOut.hasPosition);
    EXPECT_NEAR(rOut.positionDegrees, 12.5, 0.05);

    // 2. Feed ZDA
    emitted.clear();
    const auto zdaStr = NmeaChecksum::frameSentence("GPZDA,123000.00,15,08,2026,00,00");
    gateway.onSentence(zdaStr);

    ASSERT_FALSE(emitted.empty());
    hdr = N2k::N2kHeader::fromCanId(emitted.back().id);
    EXPECT_EQ(hdr.pgn, static_cast<std::uint32_t>(N2k::Pgn::SystemTime));

    N2k::SystemTimeData tOut {};
    ASSERT_TRUE(N2k::N2kDecoder::parsePgn126992(emitted.back().data.data(), emitted.back().dlc, tOut));
    EXPECT_TRUE(tOut.hasTime);
    EXPECT_TRUE(tOut.hasDate);
    EXPECT_NEAR(tOut.secondsSinceMidnight, 12.0 * 3600.0 + 30.0 * 60.0, 0.5);
}
} // namespace Nmea::Gateway
