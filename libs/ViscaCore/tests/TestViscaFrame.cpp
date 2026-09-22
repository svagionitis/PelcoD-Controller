/// @file TestViscaFrame.cpp
/// @brief Comprehensive unit tests for the ViscaFrame container, header decoding, and nibble conversion.

#include "ViscaFrame.h"
#include <gtest/gtest.h>

using namespace Visca;

/// @brief Tests default and parameterized constructor variants of ViscaFrame.
/// @details Verifies initialization from initializer list, byte vector, and raw pointer with length.
TEST(TestViscaFrame, ConstructionVariants)
{
    ViscaFrame emptyFrame;
    EXPECT_TRUE(emptyFrame.empty());
    EXPECT_EQ(emptyFrame.size(), 0U);

    ViscaFrame listFrame { 0x81, 0x01, 0x04, 0x00, 0x02, 0xFF };
    EXPECT_FALSE(listFrame.empty());
    EXPECT_EQ(listFrame.size(), 6U);
    EXPECT_EQ(listFrame[0], 0x81);
    EXPECT_EQ(listFrame[5], 0xFF);

    std::vector<uint8_t> vec { 0x90, 0x41, 0xFF };
    ViscaFrame vecFrame(vec);
    EXPECT_EQ(vecFrame.size(), 3U);
    EXPECT_EQ(vecFrame[1], 0x41);

    const uint8_t rawBytes[] = { 0x90, 0x51, 0xFF };
    ViscaFrame ptrFrame(rawBytes, sizeof(rawBytes));
    EXPECT_EQ(ptrFrame.size(), 3U);
    EXPECT_EQ(ptrFrame[1], 0x51);
}

/// @brief Validates frame boundary rules and termination checks.
/// @details Verifies that packets under 3 bytes or exceeding 16 bytes or missing 0xFF are invalid.
TEST(TestViscaFrame, ValidationRules)
{
    // Valid frame: 3 bytes ending in 0xFF with bit 7 set
    ViscaFrame valid3 { 0x90, 0x50, 0xFF };
    EXPECT_TRUE(valid3.isValid());

    // Valid frame: 16 bytes ending in 0xFF
    std::vector<uint8_t> bytes16(16, 0x00);
    bytes16[0] = 0x90;
    bytes16.back() = 0xFF;
    ViscaFrame valid16(bytes16);
    EXPECT_TRUE(valid16.isValid());

    // Invalid: Too short (< 3 bytes)
    ViscaFrame shortFrame { 0x90, 0xFF };
    EXPECT_FALSE(shortFrame.isValid());

    // Invalid: Missing 0xFF terminator
    ViscaFrame noTerm { 0x81, 0x01, 0x04, 0x00, 0x02, 0xFE };
    EXPECT_FALSE(noTerm.isValid());

    // Invalid: Missing MSB on header byte
    ViscaFrame noMsb { 0x01, 0x01, 0xFF };
    EXPECT_FALSE(noMsb.isValid());

    // Invalid: Too long (> 16 bytes)
    std::vector<uint8_t> bytes17(17, 0x00);
    bytes17[0] = 0x90;
    bytes17.back() = 0xFF;
    ViscaFrame tooLong(bytes17);
    EXPECT_FALSE(tooLong.isValid());
}

/// @brief Validates sender and receiver address decoding for commands and replies.
/// @details Tests controller commands (dest 1..7, broadcast 8) and camera responses (source 1..7).
TEST(TestViscaFrame, HeaderAddressDecoding)
{
    // Controller command to camera 1 (81 ...)
    ViscaFrame cmd1 { 0x81, 0x01, 0x04, 0x00, 0x02, 0xFF };
    EXPECT_EQ(cmd1.sourceAddress(), 0U);
    EXPECT_EQ(cmd1.destinationAddress(), 1U);
    EXPECT_FALSE(cmd1.isBroadcast());

    // Controller command to camera 3 (83 ...)
    ViscaFrame cmd3 { 0x83, 0x01, 0x04, 0x00, 0x02, 0xFF };
    EXPECT_EQ(cmd3.sourceAddress(), 0U);
    EXPECT_EQ(cmd3.destinationAddress(), 3U);
    EXPECT_FALSE(cmd3.isBroadcast());

    // Controller broadcast (88 ...)
    ViscaFrame bcast { 0x88, 0x30, 0x01, 0xFF };
    EXPECT_EQ(bcast.sourceAddress(), 0U);
    EXPECT_EQ(bcast.destinationAddress(), 8U);
    EXPECT_TRUE(bcast.isBroadcast());

    // Camera 1 response (90 ...)
    ViscaFrame resp1 { 0x90, 0x41, 0xFF };
    EXPECT_EQ(resp1.sourceAddress(), 1U);
    EXPECT_EQ(resp1.destinationAddress(), 0U);

    // Camera 4 response (C0 ...)
    ViscaFrame resp4 { 0xC0, 0x51, 0xFF };
    EXPECT_EQ(resp4.sourceAddress(), 4U);
    EXPECT_EQ(resp4.destinationAddress(), 0U);
}

/// @brief Tests detection and classification of VISCA message types.
/// @details Verifies ACK, Completion, Error, Inquiry, Network Change, and AddressSet classification.
TEST(TestViscaFrame, MessageTypeClassification)
{
    ViscaFrame ack { 0x90, 0x41, 0xFF };
    EXPECT_TRUE(ack.isAck());
    EXPECT_EQ(ack.messageType(), ViscaMessageType::Ack);
    EXPECT_EQ(ack.socket(), ViscaSocket::Socket1);

    ViscaFrame ack2 { 0x90, 0x42, 0xFF };
    EXPECT_TRUE(ack2.isAck());
    EXPECT_EQ(ack2.socket(), ViscaSocket::Socket2);

    ViscaFrame completion { 0x90, 0x51, 0xFF };
    EXPECT_TRUE(completion.isCompletion());
    EXPECT_EQ(completion.messageType(), ViscaMessageType::Completion);
    EXPECT_EQ(completion.socket(), ViscaSocket::Socket1);

    ViscaFrame error { 0x90, 0x61, 0x03, 0xFF };
    EXPECT_TRUE(error.isError());
    EXPECT_EQ(error.messageType(), ViscaMessageType::Error);
    EXPECT_EQ(error.socket(), ViscaSocket::Socket1);
    EXPECT_EQ(error.errorCode(), ViscaErrorCode::CommandBufferFull);

    ViscaFrame netChange { 0x90, 0x38, 0xFF };
    EXPECT_TRUE(netChange.isNetworkChange());
    EXPECT_EQ(netChange.messageType(), ViscaMessageType::NetworkChange);

    ViscaFrame addrSet { 0x88, 0x30, 0x01, 0xFF };
    EXPECT_EQ(addrSet.messageType(), ViscaMessageType::AddressSet);

    ViscaFrame inqCmd { 0x81, 0x09, 0x04, 0x00, 0xFF };
    EXPECT_EQ(inqCmd.messageType(), ViscaMessageType::Inquiry);

    ViscaFrame inqResp { 0x90, 0x50, 0x02, 0xFF };
    EXPECT_TRUE(inqResp.isInquiryResponse());
}

/// @brief Verifies nibble packing and unpacking helper utilities.
/// @details Tests 16-bit word nibbles and 8-bit byte nibbles across boundary and random values.
TEST(TestViscaFrame, NibbleHelpers)
{
    const uint16_t wordVal = 0x4A7F;
    const auto wordNibbles = ViscaFrame::packWordNibbles(wordVal);
    ASSERT_EQ(wordNibbles.size(), 4U);
    EXPECT_EQ(wordNibbles[0], 0x04);
    EXPECT_EQ(wordNibbles[1], 0x0A);
    EXPECT_EQ(wordNibbles[2], 0x07);
    EXPECT_EQ(wordNibbles[3], 0x0F);

    const uint16_t unpackedWord = ViscaFrame::unpackWordNibbles(wordNibbles.data());
    EXPECT_EQ(unpackedWord, wordVal);

    const uint8_t byteVal = 0x5E;
    const auto byteNibbles = ViscaFrame::packByteNibbles(byteVal);
    ASSERT_EQ(byteNibbles.size(), 2U);
    EXPECT_EQ(byteNibbles[0], 0x05);
    EXPECT_EQ(byteNibbles[1], 0x0E);

    const uint8_t unpackedByte = ViscaFrame::unpackByteNibbles(byteNibbles.data());
    EXPECT_EQ(unpackedByte, byteVal);
}

/// @brief Validates hexadecimal string formatting.
/// @details Verifies spaced hex representation formatting for debugging and logging.
TEST(TestViscaFrame, HexStringFormatting)
{
    ViscaFrame frame { 0x81, 0x01, 0x04, 0x00, 0x02, 0xFF };
    EXPECT_EQ(frame.toHexString(), "81 01 04 00 02 FF");

    ViscaFrame empty;
    EXPECT_TRUE(empty.toHexString().empty());
}

/// @brief Validates hexadecimal string parsing into ViscaFrame.
TEST(TestViscaFrame, FromHexStringParsing)
{
    const ViscaFrame expected { 0x81, 0x01, 0x04, 0x00, 0x02, 0xFF };

    // Standard spaced string
    EXPECT_EQ(ViscaFrame::fromHexString("81 01 04 00 02 FF"), expected);

    // Continuous string
    EXPECT_EQ(ViscaFrame::fromHexString("8101040002ff"), expected);

    // Delimited with colons and '0x' prefixes
    EXPECT_EQ(ViscaFrame::fromHexString("0x81:0x01:0x04:0x00:0x02:0xFF"), expected);

    // Empty and whitespace strings
    EXPECT_TRUE(ViscaFrame::fromHexString("").empty());
    EXPECT_TRUE(ViscaFrame::fromHexString("   \t\r\n").empty());
}
