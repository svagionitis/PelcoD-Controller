/// @file TestNmeaTagBlock.cpp
/// @brief Unit tests for NMEA 0183 v4.00+ / IEC 61162-1 Tag Block parsing and generation.

#include "NmeaSentenceBuilder.h"
#include "NmeaSentenceParser.h"
#include "NmeaStreamAccumulator.h"
#include "NmeaTagBlockBuilder.h"
#include "NmeaTagBlockParser.h"

#include <gtest/gtest.h>

using namespace Nmea;

TEST(TestNmeaTagBlock, HasTagBlock)
{
    EXPECT_TRUE(NmeaTagBlockParser::hasTagBlock("\\s:GP001*00\\"));
    EXPECT_FALSE(NmeaTagBlockParser::hasTagBlock("$GPGGA,123519..."));
    EXPECT_FALSE(NmeaTagBlockParser::hasTagBlock(""));
}

TEST(TestNmeaTagBlock, ParseSingleTag)
{
    // Example: tag block format with checksum.
    // XOR('s', ':', 'R', 'A', 'D', 'A', 'R', '0', '1')
    NmeaTagBlock block {};
    block.sourceId = "RADAR01";
    const std::string formatted = NmeaTagBlockBuilder::build(block);
    EXPECT_FALSE(formatted.empty());

    NmeaTagBlock parsed {};
    std::string_view remainder {};
    EXPECT_TRUE(NmeaTagBlockParser::parse(formatted, parsed, remainder));
    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.sourceId, "RADAR01");
    EXPECT_TRUE(remainder.empty());
}

TEST(TestNmeaTagBlock, ParseComprehensiveTags)
{
    NmeaTagBlock input {};
    input.sourceId = "NAV_STATION";
    input.timestampEpochSec = 1727618400ULL;
    input.grouping = "1-2-9988";
    input.lineCount = 42U;
    input.destinationId = "BRIDGE_ECDIS";
    input.text = "GPS_PRIMARY";

    const std::string tb = NmeaTagBlockBuilder::build(input);
    EXPECT_FALSE(tb.empty());

    NmeaTagBlock parsed {};
    std::string_view remainder {};
    EXPECT_TRUE(NmeaTagBlockParser::parse(tb, parsed, remainder));
    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.sourceId, "NAV_STATION");
    EXPECT_EQ(parsed.timestampEpochSec, 1727618400ULL);
    EXPECT_EQ(parsed.grouping, "1-2-9988");
    EXPECT_EQ(parsed.lineCount, 42U);
    EXPECT_EQ(parsed.destinationId, "BRIDGE_ECDIS");
    EXPECT_EQ(parsed.text, "GPS_PRIMARY");
    EXPECT_TRUE(remainder.empty());
}

TEST(TestNmeaTagBlock, ChecksumRejection)
{
    NmeaTagBlock block {};
    block.sourceId = "STATION";
    std::string formatted = NmeaTagBlockBuilder::build(block);
    EXPECT_GE(formatted.size(), 6U);

    // Corrupt one character in the payload
    formatted[3] = (formatted[3] == 'X') ? 'Y' : 'X';

    NmeaTagBlock parsed {};
    std::string_view remainder {};
    EXPECT_FALSE(NmeaTagBlockParser::parse(formatted, parsed, remainder));
    EXPECT_FALSE(parsed.valid);
}

TEST(TestNmeaTagBlock, WrapSentenceAndExtract)
{
    NmeaTagBlock tb {};
    tb.sourceId = "GYRO01";
    tb.timestampEpochSec = 1609459200ULL;

    const std::string nmeaSentence = NmeaSentenceBuilder::buildHdt(145.2);
    const std::string combined = NmeaTagBlockBuilder::wrapSentence(tb, nmeaSentence);

    NmeaTagBlock parsedBlock {};
    std::string_view remainder {};
    EXPECT_TRUE(NmeaTagBlockParser::parse(combined, parsedBlock, remainder));
    EXPECT_TRUE(parsedBlock.valid);
    EXPECT_EQ(parsedBlock.sourceId, "GYRO01");
    EXPECT_EQ(parsedBlock.timestampEpochSec, 1609459200ULL);
    EXPECT_EQ(remainder, nmeaSentence);

    // Standard tokenizer and parser should transparently parse the remainder
    HdtData hdt {};
    EXPECT_TRUE(NmeaSentenceParser::parseHdt(remainder, hdt, true));
    EXPECT_DOUBLE_EQ(hdt.headingDegrees, 145.2);

    // Tokenizer directly on combined string with leading tag block should strip tag block automatically
    HdtData hdtFromCombined {};
    EXPECT_TRUE(NmeaSentenceParser::parseHdt(combined, hdtFromCombined, true));
    EXPECT_DOUBLE_EQ(hdtFromCombined.headingDegrees, 145.2);
}

TEST(TestNmeaTagBlock, StreamAccumulatorWithTagBlocks)
{
    NmeaStreamAccumulator accumulator(2048U);

    NmeaTagBlock tb {};
    tb.sourceId = "RADAR";

    TtmData ttmInput {};
    ttmInput.targetNumber = 1U;
    ttmInput.targetDistanceNmi = 2.5;
    ttmInput.bearingDegrees = 45.0;
    ttmInput.bearingReference = TtmReference::True;
    ttmInput.targetSpeedKnots = 12.0;
    ttmInput.targetCourseDegrees = 90.0;
    ttmInput.courseReference = TtmReference::True;
    ttmInput.distanceCpaNmi = 1.0;
    ttmInput.timeCpaMinutes = 5.0;
    ttmInput.speedDistanceUnits = 'K';
    ttmInput.targetName = "VESSEL1";
    ttmInput.status = TtmTargetStatus::Tracking;
    ttmInput.acquisitionType = 'A';

    const std::string rawTtm = NmeaSentenceBuilder::buildTtm(ttmInput);
    const std::string sentence = NmeaTagBlockBuilder::wrapSentence(tb, rawTtm);

    // Ingest in 3 fragmented pieces
    const std::size_t part1 = sentence.size() / 3;
    const std::size_t part2 = 2 * (sentence.size() / 3);

    auto res1 = accumulator.push(sentence.substr(0, part1), true);
    EXPECT_TRUE(res1.empty());

    auto res2 = accumulator.push(sentence.substr(part1, part2 - part1), true);
    EXPECT_TRUE(res2.empty());

    auto res3 = accumulator.push(sentence.substr(part2), true);
    ASSERT_EQ(res3.size(), 1U);
    EXPECT_EQ(res3.front(), sentence);

    // Parse extracted sentence
    TtmData ttm {};
    EXPECT_TRUE(NmeaSentenceParser::parseTtm(res3.front(), ttm, true));
    EXPECT_EQ(ttm.targetNumber, 1U);
    EXPECT_DOUBLE_EQ(ttm.targetDistanceNmi, 2.5);
    EXPECT_DOUBLE_EQ(ttm.bearingDegrees, 45.0);
}
