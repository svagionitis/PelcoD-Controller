#include "AisBitReader.h"

#include <gtest/gtest.h>

namespace Nmea {
namespace {

    TEST(TestAisBitReader, CharToSixBitConversions)
    {
        // '0' (ASCII 48) -> 0
        EXPECT_EQ(AisBitReader::charToSixBit('0'), 0U);
        // '9' (ASCII 57) -> 9
        EXPECT_EQ(AisBitReader::charToSixBit('9'), 9U);
        // ':' (ASCII 58) -> 10
        EXPECT_EQ(AisBitReader::charToSixBit(':'), 10U);
        // '?' (ASCII 63) -> 15
        EXPECT_EQ(AisBitReader::charToSixBit('?'), 15U);
        // '@' (ASCII 64) -> 16
        EXPECT_EQ(AisBitReader::charToSixBit('@'), 16U);
        // 'W' (ASCII 87) -> 39
        EXPECT_EQ(AisBitReader::charToSixBit('W'), 39U);
        // '`' (ASCII 96) -> 40
        EXPECT_EQ(AisBitReader::charToSixBit('`'), 40U);
        // 'a' (ASCII 97) -> 41
        EXPECT_EQ(AisBitReader::charToSixBit('a'), 41U);
        // 'w' (ASCII 119) -> 63
        EXPECT_EQ(AisBitReader::charToSixBit('w'), 63U);

        // Out of range
        EXPECT_EQ(AisBitReader::charToSixBit(' '), 0U);
    }

    TEST(TestAisBitReader, SixBitToAsciiConversions)
    {
        // 0 -> '@'
        EXPECT_EQ(AisBitReader::sixBitToAscii(0U), '@');
        // 1 -> 'A'
        EXPECT_EQ(AisBitReader::sixBitToAscii(1U), 'A');
        // 26 -> 'Z'
        EXPECT_EQ(AisBitReader::sixBitToAscii(26U), 'Z');
        // 32 -> ' '
        EXPECT_EQ(AisBitReader::sixBitToAscii(32U), ' ');
        // 48 -> '0'
        EXPECT_EQ(AisBitReader::sixBitToAscii(48U), '0');
        // 57 -> '9'
        EXPECT_EQ(AisBitReader::sixBitToAscii(57U), '9');
    }

    TEST(TestAisBitReader, ReadBitsAndCrossingBoundaries)
    {
        // "15" -> charToSixBit('1') = 1 (000001), charToSixBit('5') = 5 (000101)
        // Combined 12 bits: 000001 000101
        AisBitReader reader("15", 0U);
        EXPECT_EQ(reader.totalBits(), 12U);

        // Read 6 bits -> 1
        EXPECT_EQ(reader.readBits(6U), 1U);
        // Read 6 bits -> 5
        EXPECT_EQ(reader.readBits(6U), 5U);
        EXPECT_EQ(reader.remainingBits(), 0U);
    }

    TEST(TestAisBitReader, ReadSignedBitsSignExtension)
    {
        // 28-bit signed negative number
        // Let's create an armored sequence where the first bit is 1 (negative)
        // 'w' is 63 (111111)
        AisBitReader reader("ww", 0U); // 12 bits of 1s
        EXPECT_EQ(reader.readSignedBits(12U), -1);

        // Positive number: '0w' -> 000000 111111 = 63
        AisBitReader reader2("0w", 0U);
        EXPECT_EQ(reader2.readSignedBits(12U), 63);
    }

    TEST(TestAisBitReader, ReadStringWithPaddingTrimmed)
    {
        // 6-bit string "TEST@@@"
        // T = 20, E = 5, S = 19, T = 20, @ = 0, @ = 0
        std::vector<std::uint8_t> sixBits { 20U, 5U, 19U, 20U, 0U, 0U };
        AisBitReader reader(sixBits, 0U);

        const std::string text = reader.readString(6U);
        EXPECT_EQ(text, "TEST"); // Trailing '@' stripped
    }

} // namespace
} // namespace Nmea
