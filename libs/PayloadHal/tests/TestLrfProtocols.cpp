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
    EXPECT_FALSE(results[0].signalQualityRatio.has_value());
    EXPECT_FALSE(results[0].diodeTemperatureC.has_value());
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

TEST(TestLrfProtocols, AsciiDecoupledConfig)
{
    // Interface Segregation Principle test:
    // AsciiLrfParser can be configured directly with AsciiProtocolConfig
    // without needing any monolithic SerialLrfConfig dependency.
    AsciiProtocolConfig customConfig;
    customConfig.customArmCmd = "CUSTOM_ARM\r\n";
    customConfig.customDisarmCmd = "CUSTOM_DISARM\r\n";
    customConfig.customFireCmd = "CUSTOM_FIRE\r\n";

    AsciiLrfParser parser(customConfig);
    EXPECT_EQ(parser.config().customArmCmd, "CUSTOM_ARM\r\n");
    EXPECT_EQ(parser.config().customDisarmCmd, "CUSTOM_DISARM\r\n");
    EXPECT_EQ(parser.config().customFireCmd, "CUSTOM_FIRE\r\n");

    const auto arm = parser.buildArmCommand();
    EXPECT_EQ(std::string(arm.begin(), arm.end()), "CUSTOM_ARM\r\n");

    const auto disarm = parser.buildDisarmCommand();
    EXPECT_EQ(std::string(disarm.begin(), disarm.end()), "CUSTOM_DISARM\r\n");

    const auto fire = parser.buildFireCommand();
    EXPECT_EQ(std::string(fire.begin(), fire.end()), "CUSTOM_FIRE\r\n");

    // Also verify default-constructed parser uses standard defaults
    AsciiLrfParser defaultParser;
    const auto defArm = defaultParser.buildArmCommand();
    EXPECT_EQ(std::string(defArm.begin(), defArm.end()), "ARM\r\n");
    const auto defDisarm = defaultParser.buildDisarmCommand();
    EXPECT_EQ(std::string(defDisarm.begin(), defDisarm.end()), "DISARM\r\n");
    const auto defFire = defaultParser.buildFireCommand();
    EXPECT_EQ(std::string(defFire.begin(), defFire.end()), "FIRE\r\n");

    // Verify factory creation using SerialLrfConfig with nested AsciiProtocolConfig
    SerialLrfConfig serialCfg;
    serialCfg.protocolType = LrfProtocolType::Ascii;
    serialCfg.asciiConfig.customArmCmd = "NESTED_ARM\r\n";
    auto factoryParser = createLrfParser(serialCfg);
    ASSERT_NE(factoryParser, nullptr);
    const auto factoryArm = factoryParser->buildArmCommand();
    EXPECT_EQ(std::string(factoryArm.begin(), factoryArm.end()), "NESTED_ARM\r\n");
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
    ASSERT_TRUE(results[0].signalQualityRatio.has_value());
    EXPECT_NEAR(*results[0].signalQualityRatio, 240.0 / 255.0, 0.01);
    ASSERT_TRUE(results[0].diodeTemperatureC.has_value());
    EXPECT_NEAR(*results[0].diodeTemperatureC, 28.0, 0.01);
}

TEST(TestLrfProtocols, BinaryParseEchoReportMinimalPayload)
{
    BinaryLrfParser parser;

    // Build minimal binary echo report with only status (1 byte) + dist (4 bytes), payload length = 5:
    // Sync: 0xAA 0x55
    // MsgId: 0x10
    // Len: 5
    // Status: 0x00
    // Dist: 1500000 mm = 1500.0 m (0x0016E360)
    std::vector<uint8_t> frame = { 0xAA, 0x55, 0x10, 0x05, 0x00, 0x00, 0x16, 0xE3, 0x60 };
    const uint16_t crc = BinaryLrfParser::computeCrc16(frame.data(), frame.size());
    frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));

    auto results = parser.parseIncomingBytes(frame.data(), frame.size());
    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(results[0].valid);
    EXPECT_NEAR(results[0].slantRangeMeters, 1500.0, 0.001);
    EXPECT_FALSE(results[0].signalQualityRatio.has_value());
    EXPECT_FALSE(results[0].diodeTemperatureC.has_value());
}

TEST(TestLrfProtocols, AsciiParseNoSyntheticTelemetry)
{
    AsciiLrfParser parser;
    const std::string msg = "R: 1250.5\r\n";
    auto results = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());

    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(results[0].valid);
    EXPECT_NEAR(results[0].slantRangeMeters, 1250.5, 0.001);
    EXPECT_FALSE(results[0].signalQualityRatio.has_value());
    EXPECT_FALSE(results[0].diodeTemperatureC.has_value());
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

TEST(TestLrfProtocols, BinaryCrc16StandardVectors)
{
    // 1. Standard CCITT-FALSE test vector "123456789" -> 0x29B1
    const std::string standardVector = "123456789";
    const auto crc1
        = BinaryLrfParser::computeCrc16(reinterpret_cast<const uint8_t*>(standardVector.data()), standardVector.size());
    EXPECT_EQ(crc1, 0x29B1U);

    // 2. Empty input / null pointer safety
    EXPECT_EQ(BinaryLrfParser::computeCrc16(nullptr, 0), 0xFFFFU);
    EXPECT_EQ(BinaryLrfParser::computeCrc16(nullptr, 100), 0xFFFFU);

    const std::uint8_t dummy = 0xAA;
    EXPECT_EQ(BinaryLrfParser::computeCrc16(&dummy, 0), 0xFFFFU);

    // 3. Known packet CRC
    const std::vector<uint8_t> syncPacket = { 0xAA, 0x55, 0x01, 0x00 };
    const auto crc2 = BinaryLrfParser::computeCrc16(syncPacket.data(), syncPacket.size());
    EXPECT_EQ(crc2, 0x8012U);
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

TEST(TestLrfProtocols, AsciiParseSignedCharNoise)
{
    AsciiLrfParser parser;

    // Stream noise containing characters where static_cast<char> has negative sign (0x80..0xFF)
    // std::toupper with raw negative char invokes undefined behavior; toUpperSafe casts to unsigned char.
    const std::string noiseAndValid = "\x80\xFF\xFE\r\nERROR: \x80\r\nR: 1650.25\r\n";
    auto results
        = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(noiseAndValid.data()), noiseAndValid.size());

    ASSERT_EQ(results.size(), 3U);
    EXPECT_FALSE(results[0].valid); // Pure noise line is rejected
    EXPECT_FALSE(results[1].valid); // Error string with high-bit chars is rejected safely
    EXPECT_TRUE(results[2].valid); // Subsequent valid reading parses correctly
    EXPECT_NEAR(results[2].slantRangeMeters, 1650.25, 0.001);
}

TEST(TestLrfProtocols, BinaryPayloadLengthValidation)
{
    // 1. Oversized payload (> 255 bytes) must be rejected to prevent silent truncation of the 1-byte length field
    std::vector<uint8_t> oversizedPayload(256, 0x42);
    const auto oversizedPacket = BinaryLrfParser::buildBinaryPacket(0x01, oversizedPayload);
    EXPECT_TRUE(oversizedPacket.empty());

    // 2. Maximum supported payload (exactly 255 bytes) should be constructed cleanly
    std::vector<uint8_t> maxPayload(255, 0x7E);
    const auto validMaxPacket = BinaryLrfParser::buildBinaryPacket(0x02, maxPayload);
    ASSERT_EQ(validMaxPacket.size(), 6U + 255U);
    EXPECT_EQ(validMaxPacket[0], BinaryLrfParser::kSyncByte1);
    EXPECT_EQ(validMaxPacket[1], BinaryLrfParser::kSyncByte2);
    EXPECT_EQ(validMaxPacket[2], 0x02);
    EXPECT_EQ(validMaxPacket[3], 255U);

    // 3. Incoming malformed EchoReport with payloadLen < 5 must not produce any measurements
    BinaryLrfParser parser;
    std::vector<uint8_t> truncatedEcho = { 0xAA, 0x55, 0x10, 0x03, 0x00, 0x01, 0x02 };
    const uint16_t crcTrunc = BinaryLrfParser::computeCrc16(truncatedEcho.data(), truncatedEcho.size());
    truncatedEcho.push_back(static_cast<uint8_t>((crcTrunc >> 8) & 0xFF));
    truncatedEcho.push_back(static_cast<uint8_t>(crcTrunc & 0xFF));

    auto resultsTrunc = parser.parseIncomingBytes(truncatedEcho.data(), truncatedEcho.size());
    EXPECT_TRUE(resultsTrunc.empty());
}

TEST(TestLrfProtocols, NmeaBufferGrowthBoundedUnderCorruptedStream)
{
    NmeaLrfParser parser;
    // Feed 10,000 bytes of stream noise with no newline character
    const std::string noise(10000, 'X');
    const auto noiseResults = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(noise.data()), noise.size());
    EXPECT_TRUE(noiseResults.empty());

    // Buffer size must be strictly bounded by kMaxRxBufferSize to prevent DoS / memory exhaustion
    EXPECT_LE(parser.getRxBufferSize(), ILrfProtocolParser::kMaxRxBufferSize);

    // Stream recovery: after corrupted stream overflow, incoming valid sentence must parse successfully
    const std::string validSentence = NmeaLrfParser::formatNmeaSentence("GPLRF,1850.50,M,OK");
    auto results
        = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(validSentence.data()), validSentence.size());
    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(results[0].valid);
    EXPECT_NEAR(results[0].slantRangeMeters, 1850.50, 0.001);
}

TEST(TestLrfProtocols, AsciiBufferGrowthBoundedUnderCorruptedStream)
{
    AsciiLrfParser parser;
    // Feed 10,000 bytes of non-delimited noise
    const std::string noise(10000, 'A');
    const auto noiseResults = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(noise.data()), noise.size());
    EXPECT_TRUE(noiseResults.empty());

    EXPECT_LE(parser.getRxBufferSize(), ILrfProtocolParser::kMaxRxBufferSize);

    // Verify recovery with valid reading
    const std::string validReading = "R: 2450.25\r\n";
    auto results
        = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(validReading.data()), validReading.size());
    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(results[0].valid);
    EXPECT_NEAR(results[0].slantRangeMeters, 2450.25, 0.001);
}

TEST(TestLrfProtocols, BinaryBufferGrowthBoundedUnderCorruptedStream)
{
    BinaryLrfParser parser;
    // Feed stream noise containing false sync markers with oversized expected lengths that stall parsing
    std::vector<uint8_t> noise;
    for (int i = 0; i < 40; ++i) {
        noise.push_back(BinaryLrfParser::kSyncByte1);
        noise.push_back(BinaryLrfParser::kSyncByte2);
        noise.push_back(0x10);
        noise.push_back(255); // payloadLen = 255, expecting 261 bytes
        noise.insert(noise.end(), 200, 0xEE); // only 204 bytes provided -> stalls waiting for frame
    }
    // Total bytes = 40 * 204 = 8,160 bytes (> kMaxRxBufferSize of 4,096)
    const auto noiseResults = parser.parseIncomingBytes(noise.data(), noise.size());
    EXPECT_TRUE(noiseResults.empty());

    EXPECT_LE(parser.getRxBufferSize(), ILrfProtocolParser::kMaxRxBufferSize);

    // Verify subsequent valid packet parses cleanly
    std::vector<uint8_t> frame = { 0xAA, 0x55, 0x10, 0x07, 0x00, 0x00, 0x03, 0xE8, 0x00, 200, 22 };
    const uint16_t crc = BinaryLrfParser::computeCrc16(frame.data(), frame.size());
    frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));

    auto results = parser.parseIncomingBytes(frame.data(), frame.size());
    ASSERT_EQ(results.size(), 1U);
    EXPECT_TRUE(results[0].valid);
    EXPECT_NEAR(results[0].slantRangeMeters, 256.0, 0.001);
}

TEST(TestLrfProtocols, BinaryBatchParsingWithInterleavedNoise)
{
    BinaryLrfParser parser;

    auto makeFrame = [](uint32_t distMm) {
        std::vector<uint8_t> f = { 0xAA, 0x55, 0x10, 0x07, 0x00, static_cast<uint8_t>((distMm >> 24) & 0xFF),
            static_cast<uint8_t>((distMm >> 16) & 0xFF), static_cast<uint8_t>((distMm >> 8) & 0xFF),
            static_cast<uint8_t>(distMm & 0xFF), 220, 25 };
        const uint16_t c = BinaryLrfParser::computeCrc16(f.data(), f.size());
        f.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
        f.push_back(static_cast<uint8_t>(c & 0xFF));
        return f;
    };

    std::vector<uint8_t> batch;
    // 1. Noise chunk (500 bytes with no sync pattern)
    for (int i = 0; i < 500; ++i) {
        batch.push_back(static_cast<uint8_t>((i * 7) & 0xFF));
    }
    // 2. Frame 1 (1200.0 m)
    const auto f1 = makeFrame(1200000);
    batch.insert(batch.end(), f1.begin(), f1.end());
    // 3. Noise chunk with solitary 0xAA
    batch.insert(batch.end(), { 0xAA, 0x12, 0x34, 0x56 });
    // 4. Frame 2 (3500.0 m)
    const auto f2 = makeFrame(3500000);
    batch.insert(batch.end(), f2.begin(), f2.end());
    // 5. Trailing incomplete sync byte (0xAA)
    batch.push_back(BinaryLrfParser::kSyncByte1);

    auto results = parser.parseIncomingBytes(batch.data(), batch.size());
    ASSERT_EQ(results.size(), 2U);
    EXPECT_TRUE(results[0].valid);
    EXPECT_NEAR(results[0].slantRangeMeters, 1200.0, 0.001);
    EXPECT_TRUE(results[1].valid);
    EXPECT_NEAR(results[1].slantRangeMeters, 3500.0, 0.001);

    // Trailing sync byte 0xAA must be preserved in buffer waiting for 0x55
    EXPECT_EQ(parser.getRxBufferSize(), 1U);

    // Feed the second sync byte (0x55) and the rest of Frame 3 (500.0 m)
    auto f3 = makeFrame(500000);
    f3.erase(f3.begin()); // Drop leading 0xAA already buffered
    auto res3 = parser.parseIncomingBytes(f3.data(), f3.size());
    ASSERT_EQ(res3.size(), 1U);
    EXPECT_TRUE(res3[0].valid);
    EXPECT_NEAR(res3[0].slantRangeMeters, 500.0, 0.001);
    EXPECT_EQ(parser.getRxBufferSize(), 0U);
}

TEST(TestLrfProtocols, NmeaBatchParsingWithMultipleSentences)
{
    NmeaLrfParser parser;
    std::string batch;
    for (int i = 1; i <= 20; ++i) {
        batch += NmeaLrfParser::formatNmeaSentence("GPLRF," + std::to_string(i * 100) + ".0,M,OK");
    }
    batch += "$GPLRF,9999."; // trailing incomplete fragment

    auto results = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(batch.data()), batch.size());
    ASSERT_EQ(results.size(), 20U);
    for (std::size_t i = 0; i < 20U; ++i) {
        EXPECT_TRUE(results[i].valid);
        EXPECT_NEAR(results[i].slantRangeMeters, static_cast<double>(i + 1) * 100.0, 0.001);
    }
    EXPECT_EQ(parser.getRxBufferSize(), std::string("$GPLRF,9999.").size());

    // Complete the trailing fragment
    const std::string full = NmeaLrfParser::formatNmeaSentence("GPLRF,9999.50,M,OK");
    const std::string rem = full.substr(std::string("$GPLRF,9999.").size());
    auto resFinal = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(rem.data()), rem.size());
    ASSERT_EQ(resFinal.size(), 1U);
    EXPECT_TRUE(resFinal[0].valid);
    EXPECT_NEAR(resFinal[0].slantRangeMeters, 9999.50, 0.001);
    EXPECT_EQ(parser.getRxBufferSize(), 0U);
}

TEST(TestLrfProtocols, AsciiBatchParsingWithMultipleLines)
{
    AsciiLrfParser parser;
    std::string batch;
    for (int i = 1; i <= 20; ++i) {
        batch += "R: " + std::to_string(i * 50) + ".25\r\n";
    }
    batch += "R: 777"; // trailing incomplete

    auto results = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(batch.data()), batch.size());
    ASSERT_EQ(results.size(), 20U);
    for (std::size_t i = 0; i < 20U; ++i) {
        EXPECT_TRUE(results[i].valid);
        EXPECT_NEAR(results[i].slantRangeMeters, static_cast<double>(i + 1) * 50.0 + 0.25, 0.001);
    }
    EXPECT_EQ(parser.getRxBufferSize(), std::string("R: 777").size());

    const std::string rem = ".50\r\n";
    auto resFinal = parser.parseIncomingBytes(reinterpret_cast<const uint8_t*>(rem.data()), rem.size());
    ASSERT_EQ(resFinal.size(), 1U);
    EXPECT_TRUE(resFinal[0].valid);
    EXPECT_NEAR(resFinal[0].slantRangeMeters, 777.50, 0.001);
    EXPECT_EQ(parser.getRxBufferSize(), 0U);
}
