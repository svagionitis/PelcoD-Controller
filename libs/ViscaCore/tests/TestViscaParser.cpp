/// @file TestViscaParser.cpp
/// @brief Unit tests for ViscaParser core response decoders.

#include "ViscaParser.h"
#include <gtest/gtest.h>

using namespace Visca;

/// @brief Tests parsing of ACK and Completion responses.
/// @details Verifies socket number and responding camera address parsing.
TEST(TestViscaParser, AckAndCompletionParsing)
{
    // ACK from camera 1 on socket 1: 90 41 FF
    ViscaFrame ackFrame { 0x90, 0x41, 0xFF };
    const auto ack = ViscaParser::parseAck(ackFrame);
    ASSERT_TRUE(ack.has_value());
    EXPECT_EQ(ack->cameraAddress, 1U);
    EXPECT_EQ(ack->socket, ViscaSocket::Socket1);

    // Completion from camera 2 on socket 2: A0 52 FF
    ViscaFrame compFrame { 0xA0, 0x52, 0xFF };
    const auto comp = ViscaParser::parseCompletion(compFrame);
    ASSERT_TRUE(comp.has_value());
    EXPECT_EQ(comp->cameraAddress, 2U);
    EXPECT_EQ(comp->socket, ViscaSocket::Socket2);
}

/// @brief Tests parsing of Error responses across error codes.
/// @details Verifies socket number, error code, and camera address extraction.
TEST(TestViscaParser, ErrorParsing)
{
    // Syntax error from camera 1 on socket 1: 90 61 02 FF
    ViscaFrame errFrame { 0x90, 0x61, 0x02, 0xFF };
    const auto err = ViscaParser::parseError(errFrame);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->cameraAddress, 1U);
    EXPECT_EQ(err->socket, ViscaSocket::Socket1);
    EXPECT_EQ(err->code, ViscaErrorCode::SyntaxError);

    // Command Not Executable: 90 60 41 FF
    ViscaFrame errNotExec { 0x90, 0x60, 0x41, 0xFF };
    const auto err2 = ViscaParser::parseError(errNotExec);
    ASSERT_TRUE(err2.has_value());
    EXPECT_EQ(err2->code, ViscaErrorCode::CommandNotExecutable);
}

/// @brief Tests CAM_VersionInq response parsing.
/// @details Verifies parsing Vendor ID, Model ID, and ROM version.
TEST(TestViscaParser, VersionInquiryParsing)
{
    // Version reply: 90 50 00 20 07 11 01 02 02 FF
    ViscaFrame verFrame { 0x90, 0x50, 0x00, 0x20, 0x07, 0x11, 0x01, 0x02, 0x02, 0xFF };
    const auto ver = ViscaParser::parseVersionInquiry(verFrame);
    ASSERT_TRUE(ver.has_value());
    EXPECT_EQ(ver->cameraAddress, 1U);
    EXPECT_EQ(ver->vendorId, 0x0020);
    EXPECT_EQ(ver->modelId, 0x0711);
    EXPECT_EQ(ver->romVersion, 0x0102);
    EXPECT_EQ(ver->maxSockets, 2U);
    EXPECT_TRUE(ver->isSony());
}

/// @brief Tests AddressSet reply parsing.
/// @details Verifies calculation of total camera count from (p - 1).
TEST(TestViscaParser, AddressSetParsing)
{
    // 3 cameras on bus: 88 30 04 FF -> 4 - 1 = 3
    ViscaFrame addrFrame { 0x88, 0x30, 0x04, 0xFF };
    const auto addr = ViscaParser::parseAddressSet(addrFrame);
    ASSERT_TRUE(addr.has_value());
    EXPECT_EQ(addr->cameraCount, 3U);
}

/// @brief Tests PowerInquiry parsing.
/// @details Verifies boolean power status from inquiry reply (y0 50 02/03 FF).
TEST(TestViscaParser, PowerInquiryParsing)
{
    ViscaFrame pwrOnFrame { 0x90, 0x50, 0x02, 0xFF };
    const auto onRes = ViscaParser::parsePowerInquiry(pwrOnFrame);
    ASSERT_TRUE(onRes.has_value());
    EXPECT_TRUE(*onRes);

    ViscaFrame pwrOffFrame { 0x90, 0x50, 0x03, 0xFF };
    const auto offRes = ViscaParser::parsePowerInquiry(pwrOffFrame);
    ASSERT_TRUE(offRes.has_value());
    EXPECT_FALSE(*offRes);
}

/// @brief Tests Pan/Tilt response parsing (Pan_TiltPosInq and Pan_TiltStatusInq).
TEST(TestViscaParser, PanTiltResponseParsing)
{
    // Pan/Tilt Position: Pan = 0x1234, Tilt = -0x0567 = 0xFA99
    // 90 50 01 02 03 04 0F 0A 09 09 FF
    const ViscaFrame posFrame { 0x90, 0x50, 0x01, 0x02, 0x03, 0x04, 0x0F, 0x0A, 0x09, 0x09, 0xFF };
    const auto posRes = ViscaParser::parsePanTiltPosition(posFrame);
    ASSERT_TRUE(posRes.has_value());
    EXPECT_EQ(posRes->cameraAddress, 1U);
    EXPECT_EQ(posRes->panPosition, 0x1234);
    EXPECT_EQ(posRes->tiltPosition, static_cast<int16_t>(0xFA99));

    // Invalid length or terminator
    const ViscaFrame invalidPosLen { 0x90, 0x50, 0x01, 0x02, 0x03, 0x04, 0xFF };
    EXPECT_FALSE(ViscaParser::parsePanTiltPosition(invalidPosLen).has_value());

    const ViscaFrame invalidPosTerm { 0x90, 0x50, 0x01, 0x02, 0x03, 0x04, 0x0F, 0x0A, 0x09, 0x09, 0x00 };
    EXPECT_FALSE(ViscaParser::parsePanTiltPosition(invalidPosTerm).has_value());

    // Pan/Tilt Status: Pan speed = 0x14, Tilt speed = 0x0E,
    // limits = 0x05 (Bit 0 = PanLeft, Bit 2 = TiltUp), motion = 0x01 (moving)
    const ViscaFrame statusFrame { 0x90, 0x50, 0x14, 0x0E, 0x05, 0x01, 0xFF };
    const auto statusRes = ViscaParser::parsePanTiltStatus(statusFrame);
    ASSERT_TRUE(statusRes.has_value());
    EXPECT_EQ(statusRes->cameraAddress, 1U);
    EXPECT_EQ(statusRes->panSpeed, 0x14);
    EXPECT_EQ(statusRes->tiltSpeed, 0x0E);
    EXPECT_TRUE(statusRes->panLeftLimit);
    EXPECT_FALSE(statusRes->panRightLimit);
    EXPECT_TRUE(statusRes->tiltUpLimit);
    EXPECT_FALSE(statusRes->tiltDownLimit);
    EXPECT_TRUE(statusRes->moving);
}

