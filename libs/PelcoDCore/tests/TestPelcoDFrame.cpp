/// @file TestPelcoDFrame.cpp
/// @brief Google Test unit tests for Pelco-D frame checksum, creation, validation, and stream splitting.

#include "PelcoDFrame.h"
#include "TestHelpers.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

/// @brief Verify Pelco-D 8-bit modulo 256 checksum calculation for various payload scenarios.
/// @details Tests canonical messages from the Pelco-D specification (Page 18) including camera motion,
///          stop commands, combined pan/tilt, and modulo 256 overflow wraparound.
TEST(PelcoDFrameTest, ChecksumCalculation)
{
    // Spec Page 18: Message 1 (Camera 2, Pan Left)
    // 0xFF, 0x02, 0x00, 0x04, 0x20, 0x00 -> Checksum 0x26
    const std::vector<std::uint8_t> p1 { 0x02U, 0x00U, 0x04U, 0x20U, 0x00U };
    EXPECT_EQ(PelcoD::PelcoDFrame::calculateChecksum(p1), 0x26U);

    // Spec Page 18: Message 2 (Camera 2, Stop)
    // 0xFF, 0x02, 0x00, 0x00, 0x20, 0x00 -> Checksum 0x22
    const std::vector<std::uint8_t> p2 { 0x02U, 0x00U, 0x00U, 0x20U, 0x00U };
    EXPECT_EQ(PelcoD::PelcoDFrame::calculateChecksum(p2), 0x22U);

    // Spec Page 18: Message 3 (Camera 10, Camera on, Focus far, Tilt Down)
    // 0xFF, 0x0A, 0x88, 0x90, 0x00, 0x20 -> Checksum 0x42
    const std::vector<std::uint8_t> p3 { 0x0AU, 0x88U, 0x90U, 0x00U, 0x20U };
    EXPECT_EQ(PelcoD::PelcoDFrame::calculateChecksum(p3), 0x42U);

    // Modulo 256 overflow check
    const std::vector<std::uint8_t> overflow { 0xFFU, 0x02U, 0x00U, 0x00U, 0x00U };
    EXPECT_EQ(PelcoD::PelcoDFrame::calculateChecksum(overflow), 0x01U);
}

/// @brief Verify assembly and byte layout of standard 7-byte Pelco-D frames.
/// @details Tests that createFrame produces a correctly formatted 7-byte packet starting with 0xFF
///          sync byte, correct address, command 1, command 2, data 1, data 2, and trailing checksum.
TEST(PelcoDFrameTest, CreateFrame)
{
    const auto frame = PelcoD::PelcoDFrame::createFrame(0x02U, 0x00U, 0x04U, 0x20U, 0x00U);
    ASSERT_EQ(frame.size(), 7U);
    EXPECT_EQ(frame[0], 0xFFU);
    EXPECT_EQ(frame[1], 0x02U);
    EXPECT_EQ(frame[2], 0x00U);
    EXPECT_EQ(frame[3], 0x04U);
    EXPECT_EQ(frame[4], 0x20U);
    EXPECT_EQ(frame[5], 0x00U);
    EXPECT_EQ(frame[6], 0x26U);
}

/// @brief Verify validation logic for standard 4-byte, 7-byte, and 18-byte Pelco-D frames.
/// @details Checks validation across valid packets and corruptions, including sync byte mismatches,
///          checksum errors, non-printable control characters, and null termination in query replies.
TEST(PelcoDFrameTest, FrameValidation)
{
    // Valid 4-byte general response
    const std::vector<std::uint8_t> valid4 { 0xFFU, 0x01U, 0x00U, 0x01U };
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(valid4));

    // Invalid 4-byte general response with bad checksum (0x01 + 0x05 = 0x06 != 0x99)
    const std::vector<std::uint8_t> bad4 { 0xFFU, 0x01U, 0x05U, 0x99U };
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(bad4));

    // Valid 7-byte frame
    const auto valid7 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x20U, 0x00U, 0x00U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(valid7));

    // Invalid 7-byte frame with bad checksum
    auto bad7 = valid7;
    bad7[6] = 0xAAU;
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(bad7));

    // Valid 18-byte query response (empty/null padded)
    std::vector<std::uint8_t> valid18(18U, 0x00U);
    valid18[0] = 0xFFU;
    valid18[1] = 0x01U;
    valid18[17] = PelcoD::PelcoDFrame::calculateChecksum(&valid18[1], 16U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(valid18));

    // Valid 18-byte query response with ASCII model text
    auto valid18Text = valid18;
    valid18Text[2] = 'C';
    valid18Text[3] = 'A';
    valid18Text[4] = 'M';
    valid18Text[5] = '1';
    valid18Text[17] = PelcoD::PelcoDFrame::calculateChecksum(&valid18Text[1], 16U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(valid18Text));

    // Invalid 18-byte query response: non-printable control byte
    auto bad18Ctrl = valid18Text;
    bad18Ctrl[6] = 0x10U; // Non-printable byte < 32
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(bad18Ctrl));

    // Invalid 18-byte query response: character after null terminator
    auto bad18AfterNull = valid18Text;
    bad18AfterNull[6] = 0x00U; // Null terminator
    bad18AfterNull[7] = 'X'; // Stray char after null
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(bad18AfterNull));

    // Invalid 18-byte query response: corrupted checksum at byte 17
    auto bad18Cksm = valid18Text;
    bad18Cksm[17] ^= 0xFFU; // Corrupted checksum byte
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(bad18Cksm));
}

/// @brief Verify frame validation boundary cases and non-standard buffer lengths.
/// @details Validates that inputs with invalid lengths (0, 1, 2, 3, 5, 6, 8, 17, 19 bytes)
///          or bad sync headers are rejected.
TEST(PelcoDFrameTest, FrameValidationExtendedEdgeCases)
{
    // Empty buffer
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(std::vector<std::uint8_t> {}));

    // Truncated buffers
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(std::vector<std::uint8_t> { 0xFFU }));
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(std::vector<std::uint8_t> { 0xFFU, 0x01U }));
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(std::vector<std::uint8_t> { 0xFFU, 0x01U, 0x00U }));

    // Non-standard lengths (5, 6, 8, 17, 19)
    const std::vector<std::uint8_t> len5 { 0xFFU, 0x01U, 0x00U, 0x00U, 0x01U };
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(len5));

    const std::vector<std::uint8_t> len6 { 0xFFU, 0x01U, 0x00U, 0x00U, 0x00U, 0x01U };
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(len6));

    const std::vector<std::uint8_t> len8 { 0xFFU, 0x01U, 0x00U, 0x04U, 0x20U, 0x00U, 0x25U, 0x00U };
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(len8));

    std::vector<std::uint8_t> len17(17U, 0x00U);
    len17[0] = 0xFFU;
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(len17));

    std::vector<std::uint8_t> len19(19U, 0x00U);
    len19[0] = 0xFFU;
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(len19));

    // Valid length but non-0xFF sync header
    const std::vector<std::uint8_t> badSync4 { 0xFEU, 0x01U, 0x00U, 0x01U };
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(badSync4));

    const std::vector<std::uint8_t> badSync7 { 0xAAU, 0x01U, 0x00U, 0x04U, 0x20U, 0x00U, 0x25U };
    EXPECT_FALSE(PelcoD::PelcoDFrame::isValidFrame(badSync7));
}

/// @brief Verify checksum calculation safety with boundary inputs and pointer overloads.
/// @details Ensures nullptr with 0 length returns 0 safely, empty containers return 0,
///          and multi-byte accumulations with values near 255 wrap cleanly.
TEST(PelcoDFrameTest, ChecksumEdgeCases)
{
    // Null pointer with zero length
    EXPECT_EQ(PelcoD::PelcoDFrame::calculateChecksum(nullptr, 0U), 0x00U);

    // Empty vector
    const std::vector<std::uint8_t> emptyVec {};
    EXPECT_EQ(PelcoD::PelcoDFrame::calculateChecksum(emptyVec), 0x00U);

    // Single byte
    const std::vector<std::uint8_t> singleByte { 0x42U };
    EXPECT_EQ(PelcoD::PelcoDFrame::calculateChecksum(singleByte), 0x42U);

    // Boundary byte value 0xFF
    const std::vector<std::uint8_t> ffByte { 0xFFU };
    EXPECT_EQ(PelcoD::PelcoDFrame::calculateChecksum(ffByte), 0xFFU);

    // Sum wrapping exactly to 256 (0x100 -> 0x00)
    const std::vector<std::uint8_t> wrapToZero { 0x80U, 0x80U };
    EXPECT_EQ(PelcoD::PelcoDFrame::calculateChecksum(wrapToZero), 0x00U);

    // Multiple 0xFF bytes
    const std::vector<std::uint8_t> threeFF { 0xFFU, 0xFFU, 0xFFU };
    // 255 * 3 = 765; 765 % 256 = 253 (0xFD)
    EXPECT_EQ(PelcoD::PelcoDFrame::calculateChecksum(threeFF), 0xFDU);
}

/// @brief Verify stream splitting and frame extraction from contiguous byte streams.
/// @details Tests splitting streams containing noise prefixes, concatenated 7-byte frames,
///          interleaved 4-byte responses, and 18-byte query responses.
TEST(PelcoDFrameTest, StreamSplitting)
{
    // Stream with noise bytes preceding a valid 7-byte frame, followed by a valid 4-byte reply
    const std::vector<std::uint8_t> stream {
        0x12U, 0x34U, 0x56U, // Noise
        0xFFU, 0x02U, 0x00U, 0x04U, 0x20U, 0x00U, 0x26U, // Valid 7-byte
        0xFFU, 0x01U, 0x00U, 0x01U // Valid 4-byte general reply
    };

    const auto frames = PelcoD::PelcoDFrame::splitStream(stream);
    ASSERT_EQ(frames.size(), 2U);
    ASSERT_EQ(frames[0].size(), 7U);
    EXPECT_EQ(frames[0][6], 0x26U);
    EXPECT_EQ(frames[1].size(), 4U);

    // Corrupted 7-byte frame must not be extracted as a false 4-byte frame
    const std::vector<std::uint8_t> corrupted7 { 0xFFU, 0x01U, 0x00U, 0x59U, 0x10U, 0x20U, 0x99U };
    const auto badFrames = PelcoD::PelcoDFrame::splitStream(corrupted7);
    EXPECT_TRUE(badFrames.empty());

    // Three consecutive 7-byte frames in a 21-byte stream must all be extracted as 7-byte frames
    std::vector<std::uint8_t> multiStream;
    const auto f1 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x10U, 0x20U);
    const auto f2 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x05U, 0x10U);
    const auto f3 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5DU, 0x04U, 0xB0U);
    multiStream.insert(multiStream.end(), f1.begin(), f1.end());
    multiStream.insert(multiStream.end(), f2.begin(), f2.end());
    multiStream.insert(multiStream.end(), f3.begin(), f3.end());
    ASSERT_EQ(multiStream.size(), 21U);

    const auto multiFrames = PelcoD::PelcoDFrame::splitStream(multiStream);
    ASSERT_EQ(multiFrames.size(), 3U);
    EXPECT_EQ(multiFrames[0].size(), 7U);
    EXPECT_EQ(multiFrames[1].size(), 7U);
    EXPECT_EQ(multiFrames[2].size(), 7U);

    // Valid 18-byte query frame extraction via splitStream
    std::vector<std::uint8_t> qFrame(18U, 0x00U);
    qFrame[0] = 0xFFU;
    qFrame[1] = 0x01U;
    qFrame[2] = 'S';
    qFrame[3] = 'E';
    qFrame[4] = 'N';
    qFrame[5] = 'S';
    qFrame[6] = 'O';
    qFrame[7] = 'R';
    qFrame[17] = PelcoD::PelcoDFrame::calculateChecksum(&qFrame[1], 16U);
    const auto qFrames = PelcoD::PelcoDFrame::splitStream(qFrame);
    ASSERT_EQ(qFrames.size(), 1U);
    EXPECT_EQ(qFrames[0].size(), 18U);

    // Corrupted 18-byte query frame checksum must be rejected
    auto badQFrame = qFrame;
    badQFrame[17] ^= 0x55U;
    const auto badQFrames = PelcoD::PelcoDFrame::splitStream(badQFrame);
    EXPECT_TRUE(badQFrames.empty());
}

/// @brief Verify stream splitting resilience under consecutive sync bytes and trailing partial frames.
/// @details Ensures splitStream correctly resynchronizes when encountering duplicate 0xFF sync bytes,
///          ignores trailing truncated fragments, and handles purely noisy buffers.
TEST(PelcoDFrameTest, StreamSplittingComplexCases)
{
    // Stream with consecutive 0xFF sync bytes before a valid frame
    const auto validFrame = PelcoD::PelcoDFrame::createFrame(0x02U, 0x00U, 0x04U, 0x20U, 0x00U);
    std::vector<std::uint8_t> doubleSync { 0xFFU };
    doubleSync.insert(doubleSync.end(), validFrame.begin(), validFrame.end());

    const auto resDouble = PelcoD::PelcoDFrame::splitStream(doubleSync);
    ASSERT_EQ(resDouble.size(), 1U);
    EXPECT_EQ(resDouble[0], validFrame);

    // Valid frame followed by incomplete trailing fragment
    auto streamWithTrailing = validFrame;
    streamWithTrailing.push_back(0xFFU);
    streamWithTrailing.push_back(0x02U);
    streamWithTrailing.push_back(0x00U);

    const auto resTrailing = PelcoD::PelcoDFrame::splitStream(streamWithTrailing);
    ASSERT_EQ(resTrailing.size(), 1U);
    EXPECT_EQ(resTrailing[0], validFrame);

    // Pure noise without any valid frames
    const std::vector<std::uint8_t> pureNoise { 0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U };
    const auto resNoise = PelcoD::PelcoDFrame::splitStream(pureNoise);
    EXPECT_TRUE(resNoise.empty());
}

/// @brief Verify serialization of raw Pelco-D byte frames to human-readable hex strings.
/// @details Validates default space delimiters, custom delimiters, empty buffers, and raw pointer overloads.
TEST(PelcoDFrameTest, ToHexString)
{
    const std::vector<std::uint8_t> frame { 0xFFU, 0x01U, 0x00U, 0x04U, 0x20U, 0x00U, 0x25U };

    // Default space delimiter
    EXPECT_EQ(PelcoD::PelcoDFrame::toHexString(frame), "FF 01 00 04 20 00 25");

    // Custom delimiter ':'
    EXPECT_EQ(PelcoD::PelcoDFrame::toHexString(frame, ':'), "FF:01:00:04:20:00:25");

    // No delimiter '\0'
    EXPECT_EQ(PelcoD::PelcoDFrame::toHexString(frame, '\0'), "FF010004200025");

    // Empty vector
    EXPECT_TRUE(PelcoD::PelcoDFrame::toHexString(std::vector<std::uint8_t> {}).empty());

    // Raw pointer overload
    EXPECT_EQ(PelcoD::PelcoDFrame::toHexString(frame.data(), frame.size(), '-'), "FF-01-00-04-20-00-25");
    EXPECT_TRUE(PelcoD::PelcoDFrame::toHexString(nullptr, 0U).empty());
}

/// @brief Verify parsing and decoding of hex strings into byte vectors.
/// @details Tests standard formats, unspaced representations, lowercase characters, custom delimiters,
///          hex prefixes (0x), whitespace stripping, and round-trip consistency.
TEST(PelcoDFrameTest, FromHexString)
{
    const std::vector<std::uint8_t> expected { 0xFFU, 0x01U, 0x00U, 0x04U, 0x20U, 0x00U, 0x25U };

    // Standard spaced hex
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString("FF 01 00 04 20 00 25"), expected);

    // Unspaced hex
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString("FF010004200025"), expected);

    // Lowercase hex
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString("ff 01 00 04 20 00 25"), expected);

    // Colon-delimited
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString("FF:01:00:04:20:00:25"), expected);

    // With 0x / 0X prefixes
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString("0xFF 0x01 0x00 0x04 0x20 0x00 0x25"), expected);
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString("0XFF 0X01 0X00 0X04 0X20 0X00 0X25"), expected);

    // Mixed spacing and leading/trailing whitespace
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString("  FF  01  00 04   20 00 25 \t\r\n "), expected);

    // Empty and whitespace-only
    EXPECT_TRUE(PelcoD::PelcoDFrame::fromHexString("").empty());
    EXPECT_TRUE(PelcoD::PelcoDFrame::fromHexString("   ").empty());

    // Incomplete / odd trailing nibble
    const std::vector<std::uint8_t> expectedPrefix { 0xFFU, 0x01U };
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString("FF 01 A"), expectedPrefix);

    // Non-hex characters ignored
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString("ZZ FF -- 01 !!"), expectedPrefix);

    // Round-trip test
    const auto serialized = PelcoD::PelcoDFrame::toHexString(expected);
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString(serialized), expected);
}

/// @brief Verify hex string conversion corner cases and format resiliency.
/// @details Validates handling of odd length strings, lowercase mixed characters, and raw pointer overloads.
TEST(PelcoDFrameTest, HexConversionEdgeCases)
{
    // Single nibble produces empty byte vector
    EXPECT_TRUE(PelcoD::PelcoDFrame::fromHexString("A").empty());

    // Three nibbles produce single byte
    const std::vector<std::uint8_t> oneByte { 0xABU };
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString("ABC"), oneByte);

    // Mixed cases with prefix
    const std::vector<std::uint8_t> mixedExpected { 0x0AU, 0xBCU, 0xDEU, 0xF0U };
    EXPECT_EQ(PelcoD::PelcoDFrame::fromHexString("0x0a:bC:dE:F0"), mixedExpected);

    // toHexString with large single-byte vector
    const std::vector<std::uint8_t> singleByte { 0x05U };
    EXPECT_EQ(PelcoD::PelcoDFrame::toHexString(singleByte), "05");
}

} // namespace
