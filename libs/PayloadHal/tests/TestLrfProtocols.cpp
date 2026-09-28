/// @file TestLrfProtocols.cpp
/// @brief Unit tests for NMEA, ASCII, and Binary LRF wire protocol parsers.

#include "adapters/LrfProtocols.h"
#include <clocale>
#include <gtest/gtest.h>

using namespace PayloadHal;

// =============================================================================
// NMEA-0183 Parser Tests
// =============================================================================

TEST(TestLrfProtocols, NmeaChecksumCalculation)
{
    // $GPLRF,ARM*27\r\n
    // 'G' ^ 'P' ^ 'L' ^ 'R' ^ 'F' ^ ',' ^ 'A' ^ 'R' ^ 'M'
    const std::string body = "GPLRF,ARM";
    const std::uint8_t cs = NmeaLrfParser::computeNmeaChecksum(body);
    EXPECT_EQ(cs, 0x3D);
    const std::string formatted = NmeaLrfParser::formatNmeaSentence(body);
    EXPECT_EQ(formatted, "$GPLRF,ARM*3D\r\n");
}

TEST(TestLrfProtocols, NmeaParseValidSentence)
{
    NmeaLrfParser parser;
    const std::string msg = NmeaLrfParser::formatNmeaSentence("GPLRF,1250.50,M,OK");
    auto results = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());

    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(results[0].valid);
    EXPECT_NEAR(results[0].slantRangeMeters, 1250.50, 0.001);
    EXPECT_GT(results[0].signalQualityRatio, 0.9);
}

TEST(TestLrfProtocols, NmeaParseFeetConversion)
{
    NmeaLrfParser parser;
    // 1000 feet = 304.8 meters
    const std::string msg = NmeaLrfParser::formatNmeaSentence("GPLRF,1000.0,FT,OK");
    auto results = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());

    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(results[0].valid);
    EXPECT_NEAR(results[0].slantRangeMeters, 304.8, 0.01);
}

TEST(TestLrfProtocols, NmeaParseErrorOrNoTarget)
{
    NmeaLrfParser parser;
    const std::string msg = NmeaLrfParser::formatNmeaSentence("GPLRF,0.0,M,FAIL");
    auto results = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());

    ASSERT_EQ(results.size(), 1U);
    EXPECT_FALSE(results[0].valid);
    EXPECT_EQ(results[0].slantRangeMeters, 0.0);
}

TEST(TestLrfProtocols, NmeaCorruptedChecksumRejection)
{
    NmeaLrfParser parser;
    // Correct CS is 27, inject bad CS 99
    const std::string badMsg = "$GPLRF,1250.50,M,OK*99\r\n";
    auto results = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(badMsg.data()), badMsg.size());
    EXPECT_TRUE(results.empty());
}

TEST(TestLrfProtocols, NmeaStreamFragmentation)
{
    NmeaLrfParser parser;
    const std::string msg = NmeaLrfParser::formatNmeaSentence("GPLRF,2400.0,M,OK");
    ASSERT_GT(msg.size(), 6U);

    // Feed in two chunks
    auto r1 = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msg.data()), 6);
    EXPECT_TRUE(r1.empty());

    auto r2 = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msg.data() + 6), msg.size() - 6);
    ASSERT_EQ(r2.size(), 1U);
    EXPECT_TRUE(r2[0].valid);
    EXPECT_NEAR(r2[0].slantRangeMeters, 2400.0, 0.001);
}

TEST(TestLrfProtocols, NmeaCommandGeneration)
{
    NmeaLrfParser parser;
    const auto arm = parser.buildArmCommand();
    const std::string armStr(arm.begin(), arm.end());
    EXPECT_EQ(armStr, NmeaLrfParser::formatNmeaSentence("GPLRF,ARM"));

    const auto disarm = parser.buildDisarmCommand();
    const std::string disarmStr(disarm.begin(), disarm.end());
    EXPECT_EQ(disarmStr, NmeaLrfParser::formatNmeaSentence("GPLRF,DISARM"));

    const auto fire = parser.buildFireCommand();
    const std::string fireStr(fire.begin(), fire.end());
    EXPECT_EQ(fireStr, NmeaLrfParser::formatNmeaSentence("GPLRF,FIRE"));

    const auto cont5 = parser.buildContinuousCommand(LrfMode::Continuous5Hz);
    const std::string cont5Str(cont5.begin(), cont5.end());
    EXPECT_EQ(cont5Str, NmeaLrfParser::formatNmeaSentence("GPLRF,RATE,5"));
}

// =============================================================================
// ASCII Delimited Parser Tests
// =============================================================================

TEST(TestLrfProtocols, AsciiParseRColonPrefix)
{
    AsciiLrfParser parser;
    const std::string msg = "R: 1420.75\r\n";
    auto results = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());

    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(results[0].valid);
    EXPECT_NEAR(results[0].slantRangeMeters, 1420.75, 0.001);
}

TEST(TestLrfProtocols, AsciiParseDistPrefixWithUnits)
{
    AsciiLrfParser parser;
    const std::string msg = "DIST: 855.2 m\r\n";
    auto results = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());

    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(results[0].valid);
    EXPECT_NEAR(results[0].slantRangeMeters, 855.2, 0.001);
}

TEST(TestLrfProtocols, AsciiParseErrorStrings)
{
    AsciiLrfParser parser;
    const std::string msg1 = "ERROR: TARGET NOT FOUND\r\n";
    auto r1 = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msg1.data()), msg1.size());
    ASSERT_EQ(r1.size(), 1U);
    EXPECT_FALSE(r1[0].valid);

    const std::string msg2 = "NO_TARGET\r\n";
    auto r2 = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msg2.data()), msg2.size());
    ASSERT_EQ(r2.size(), 1U);
    EXPECT_FALSE(r2[0].valid);
}

TEST(TestLrfProtocols, AsciiCommands)
{
    SerialLrfConfig cfg;
    cfg.customArmCmd = "LASER_ARM\r\n";
    cfg.customDisarmCmd = "LASER_SAFE\r\n";
    AsciiLrfParser parser(cfg);

    const auto arm = parser.buildArmCommand();
    EXPECT_EQ(std::string(arm.begin(), arm.end()), "LASER_ARM\r\n");

    const auto disarm = parser.buildDisarmCommand();
    EXPECT_EQ(std::string(disarm.begin(), disarm.end()), "LASER_SAFE\r\n");

    const auto fire = parser.buildFireCommand();
    EXPECT_EQ(std::string(fire.begin(), fire.end()), "FIRE\r\n");
}

// =============================================================================
// Binary Protocol Parser Tests
// =============================================================================

TEST(TestLrfProtocols, BinaryParseEchoReport)
{
    BinaryLrfParser parser;

    // Build valid binary echo report:
    // Sync: 0xAA 0x55
    // MsgId: 0x10
    // Len: 7
    // Status: 0x00
    // Dist: 1500000 mm = 1500.0 m (0x0016E360)
    // Quality: 240
    // Temp: 28 C
    std::vector<uint8_t> frame = { 0xAA, 0x55, 0x10, 0x07, 0x00, 0x00, 0x16, 0xE3, 0x60, 240, 28 };
    const uint16_t crc = BinaryLrfParser::computeCrc16(frame.data(), frame.size());
    frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));

    auto results = parser.parseIncomingBytes(frame.data(), frame.size());
    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(results[0].valid);
    EXPECT_NEAR(results[0].slantRangeMeters, 1500.0, 0.001);
    EXPECT_NEAR(results[0].signalQualityRatio, 240.0 / 255.0, 0.01);
    EXPECT_NEAR(results[0].diodeTemperatureC, 28.0, 0.01);
}

TEST(TestLrfProtocols, BinaryCrcCorruptionRejection)
{
    BinaryLrfParser parser;
    std::vector<uint8_t> frame = {
        0xAA, 0x55, 0x10, 0x07, 0x00, 0x00, 0x16, 0xE3, 0x60, 240, 28, 0xDE, 0xAD // Invalid CRC
    };

    auto results = parser.parseIncomingBytes(frame.data(), frame.size());
    EXPECT_TRUE(results.empty());
}

TEST(TestLrfProtocols, BinaryCommands)
{
    BinaryLrfParser parser;

    const auto arm = parser.buildArmCommand();
    ASSERT_EQ(arm.size(), 6U);
    EXPECT_EQ(arm[0], 0xAA);
    EXPECT_EQ(arm[1], 0x55);
    EXPECT_EQ(arm[2], BinaryLrfParser::kCmdArm);
    EXPECT_EQ(arm[3], 0);

    const auto fire = parser.buildFireCommand();
    ASSERT_EQ(fire.size(), 6U);
    EXPECT_EQ(fire[2], BinaryLrfParser::kCmdFireSingle);

    const auto cont = parser.buildContinuousCommand(LrfMode::Continuous10Hz);
    ASSERT_EQ(cont.size(), 7U);
    EXPECT_EQ(cont[2], BinaryLrfParser::kCmdContinuous);
    EXPECT_EQ(cont[3], 1);
    EXPECT_EQ(cont[4], 10);
}

TEST(TestLrfProtocols, LocaleIndependenceUnderCommaDecimalPoint)
{
    // Save current locale
    const char* originalLocale = std::setlocale(LC_ALL, nullptr);
    std::string savedLocale = originalLocale ? originalLocale : "C";

    // Set locale where decimal separator is ',' (e.g. Greek, German)
    if (std::setlocale(LC_ALL, "el_GR.utf8") == nullptr) {
        GTEST_SKIP() << "el_GR.utf8 locale not available on system, skipping test";
    }

    struct LocaleGuard {
        std::string loc;
        ~LocaleGuard()
        {
            std::setlocale(LC_ALL, loc.c_str());
        }
    } guard { savedLocale };

    // 1. Test NMEA parsing under comma locale
    NmeaLrfParser nmeaParser;
    const std::string nmeaMsg = NmeaLrfParser::formatNmeaSentence("GPLRF,1250.50,M,OK");
    auto nmeaResults = nmeaParser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(nmeaMsg.data()), nmeaMsg.size());

    ASSERT_EQ(nmeaResults.size(), 1U);
    EXPECT_TRUE(nmeaResults[0].valid);
    EXPECT_NEAR(nmeaResults[0].slantRangeMeters, 1250.50, 0.001);

    // 2. Test ASCII parsing under comma locale
    AsciiLrfParser asciiParser;
    const std::string asciiMsg = "R: 1420.75\r\n";
    auto asciiResults
        = asciiParser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(asciiMsg.data()), asciiMsg.size());

    ASSERT_EQ(asciiResults.size(), 1U);
    EXPECT_TRUE(asciiResults[0].valid);
    EXPECT_NEAR(asciiResults[0].slantRangeMeters, 1420.75, 0.001);
}

TEST(TestLrfProtocols, SplitTokensEmptyFieldHandling)
{
    // Empty input string must yield an empty vector of tokens
    EXPECT_TRUE(NmeaLrfParser::splitTokens("", ',').empty());

    // Single delimiter yields two empty fields
    const auto twoEmpty = NmeaLrfParser::splitTokens(",", ',');
    ASSERT_EQ(twoEmpty.size(), 2U);
    EXPECT_EQ(twoEmpty[0], "");
    EXPECT_EQ(twoEmpty[1], "");

    // Multiple consecutive delimiters preserve empty fields
    const auto threeEmpty = NmeaLrfParser::splitTokens(",,", ',');
    ASSERT_EQ(threeEmpty.size(), 3U);
    EXPECT_EQ(threeEmpty[0], "");
    EXPECT_EQ(threeEmpty[1], "");
    EXPECT_EQ(threeEmpty[2], "");

    // Leading and trailing empty fields
    const auto edgeTokens = NmeaLrfParser::splitTokens(",1250.50,", ',');
    ASSERT_EQ(edgeTokens.size(), 3U);
    EXPECT_EQ(edgeTokens[0], "");
    EXPECT_EQ(edgeTokens[1], "1250.50");
    EXPECT_EQ(edgeTokens[2], "");

    // Intermediate empty and whitespace-only fields trimmed to empty
    const auto tokens = NmeaLrfParser::splitTokens("GPLRF, 1250.50 ,   , M , ", ',');
    ASSERT_EQ(tokens.size(), 5U);
    EXPECT_EQ(tokens[0], "GPLRF");
    EXPECT_EQ(tokens[1], "1250.50");
    EXPECT_EQ(tokens[2], "");
    EXPECT_EQ(tokens[3], "M");
    EXPECT_EQ(tokens[4], "");
}

TEST(TestLrfProtocols, NmeaParseOmittedFields)
{
    NmeaLrfParser parser;

    // 1. Trailing comma with status omitted: should still succeed and parse range
    const std::string msgWithTrailingComma = NmeaLrfParser::formatNmeaSentence("GPLRF,1250.50,M,");
    auto res1 = parser.parseIncomingBytes(
        reinterpret_cast<const uint8_t*>(msgWithTrailingComma.data()), msgWithTrailingComma.size());
    ASSERT_EQ(res1.size(), 1U);
    EXPECT_TRUE(res1[0].valid);
    EXPECT_NEAR(res1[0].slantRangeMeters, 1250.50, 0.001);

    // 2. Empty distance with error status: should produce valid=false measurement
    const std::string msgEmptyDist = NmeaLrfParser::formatNmeaSentence("GPLRF,,M,ERR");
    auto res2 = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msgEmptyDist.data()), msgEmptyDist.size());
    ASSERT_EQ(res2.size(), 1U);
    EXPECT_FALSE(res2[0].valid);
    EXPECT_EQ(res2[0].slantRangeMeters, 0.0);

    // 3. Unit omitted: should fall back and still parse distance
    const std::string msgOmittedUnit = NmeaLrfParser::formatNmeaSentence("GPLRF,800.25,,OK");
    auto res3
        = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msgOmittedUnit.data()), msgOmittedUnit.size());
    ASSERT_EQ(res3.size(), 1U);
    EXPECT_TRUE(res3[0].valid);
    EXPECT_NEAR(res3[0].slantRangeMeters, 800.25, 0.001);
}
