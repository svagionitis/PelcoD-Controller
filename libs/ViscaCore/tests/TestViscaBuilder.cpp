/// @file TestViscaBuilder.cpp
/// @brief Unit tests for ViscaBuilder standard command packet generation.

#include "ViscaBuilder.h"
#include <gtest/gtest.h>

using namespace Visca;

/// @brief Tests standard VISCA command packet creation.
/// @details Verifies exact byte outputs for AddressSet, IF_Clear, CommandCancel, and Inquiries.
TEST(TestViscaBuilder, StandardViscaCommands)
{
    // AddressSet: 88 30 01 FF
    const ViscaFrame addrSet = ViscaBuilder::addressSet();
    const std::vector<uint8_t> expectedAddrSet { 0x88, 0x30, 0x01, 0xFF };
    EXPECT_EQ(addrSet.bytes(), expectedAddrSet);

    // IF_Clear for camera 1: 81 01 00 01 FF
    const ViscaFrame ifClear1 = ViscaBuilder::ifClear(1);
    const std::vector<uint8_t> expectedIfClear1 { 0x81, 0x01, 0x00, 0x01, 0xFF };
    EXPECT_EQ(ifClear1.bytes(), expectedIfClear1);

    // IF_Clear broadcast: 88 01 00 01 FF
    const ViscaFrame ifClearBcast = ViscaBuilder::ifClearBroadcast();
    const std::vector<uint8_t> expectedIfClearBcast { 0x88, 0x01, 0x00, 0x01, 0xFF };
    EXPECT_EQ(ifClearBcast.bytes(), expectedIfClearBcast);

    // CommandCancel for camera 1, socket 1: 81 21 FF
    const ViscaFrame cancel1 = ViscaBuilder::commandCancel(1, ViscaSocket::Socket1);
    const std::vector<uint8_t> expectedCancel1 { 0x81, 0x21, 0xFF };
    EXPECT_EQ(cancel1.bytes(), expectedCancel1);

    // CommandCancel for camera 2, socket 2: 82 22 FF
    const ViscaFrame cancel2 = ViscaBuilder::commandCancel(2, ViscaSocket::Socket2);
    const std::vector<uint8_t> expectedCancel2 { 0x82, 0x22, 0xFF };
    EXPECT_EQ(cancel2.bytes(), expectedCancel2);

    // Version inquiry for camera 1: 81 09 00 02 FF
    const ViscaFrame verInq = ViscaBuilder::versionInquiry(1);
    const std::vector<uint8_t> expectedVerInq { 0x81, 0x09, 0x00, 0x02, 0xFF };
    EXPECT_EQ(verInq.bytes(), expectedVerInq);

    // Power On/Off
    const ViscaFrame pwrOn = ViscaBuilder::power(1, true);
    EXPECT_EQ(pwrOn.bytes(), (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x00, 0x02, 0xFF }));

    const ViscaFrame pwrOff = ViscaBuilder::power(1, false);
    EXPECT_EQ(pwrOff.bytes(), (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x00, 0x03, 0xFF }));
}

/// @brief Tests VISCA Pan/Tilt command packet generation (opcode 0x06).
TEST(TestViscaBuilder, PanTiltCommands)
{
    // Continuous drive (Up-Right at pan speed 0x10, tilt speed 0x0C): 81 01 06 01 10 0C 02 01 FF
    EXPECT_EQ(
        ViscaBuilder::panTiltDrive(1, 0x10, 0x0C, ViscaPanDirection::Right, ViscaTiltDirection::Up).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x01, 0x10, 0x0C, 0x02, 0x01, 0xFF }));

    // Stop: 81 01 06 01 00 00 03 03 FF
    EXPECT_EQ(ViscaBuilder::panTiltStop(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x01, 0x00, 0x00, 0x03, 0x03, 0xFF }));

    // Directional helpers
    EXPECT_EQ(ViscaBuilder::panTiltUp(1, 0x0E).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x01, 0x00, 0x0E, 0x03, 0x01, 0xFF }));
    EXPECT_EQ(ViscaBuilder::panTiltDown(1, 0x0E).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x01, 0x00, 0x0E, 0x03, 0x02, 0xFF }));
    EXPECT_EQ(ViscaBuilder::panTiltLeft(1, 0x14).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x01, 0x14, 0x00, 0x01, 0x03, 0xFF }));
    EXPECT_EQ(ViscaBuilder::panTiltRight(1, 0x14).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x01, 0x14, 0x00, 0x02, 0x03, 0xFF }));
    EXPECT_EQ(ViscaBuilder::panTiltUpLeft(1, 0x10, 0x0C).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x01, 0x10, 0x0C, 0x01, 0x01, 0xFF }));
    EXPECT_EQ(ViscaBuilder::panTiltUpRight(1, 0x10, 0x0C).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x01, 0x10, 0x0C, 0x02, 0x01, 0xFF }));
    EXPECT_EQ(ViscaBuilder::panTiltDownLeft(1, 0x10, 0x0C).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x01, 0x10, 0x0C, 0x01, 0x02, 0xFF }));
    EXPECT_EQ(ViscaBuilder::panTiltDownRight(1, 0x10, 0x0C).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x01, 0x10, 0x0C, 0x02, 0x02, 0xFF }));

    // Absolute Position (pan = 0x1234, tilt = -0x0567 = 0xFA99):
    // 81 01 06 02 12 0A 01 02 03 04 0F 0A 09 09 FF
    EXPECT_EQ(ViscaBuilder::panTiltAbsolute(1, 0x12, 0x0A, 0x1234, static_cast<int16_t>(0xFA99)).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x02, 0x12, 0x0A, 0x01, 0x02, 0x03, 0x04, 0x0F, 0x0A, 0x09, 0x09, 0xFF }));

    // Relative Position (deltaPan = 0x0100, deltaTilt = 0x0080):
    EXPECT_EQ(ViscaBuilder::panTiltRelative(1, 0x08, 0x06, 0x0100, 0x0080).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x03, 0x08, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0xFF }));

    // Home & Reset
    EXPECT_EQ(ViscaBuilder::panTiltHome(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x04, 0xFF }));
    EXPECT_EQ(ViscaBuilder::panTiltReset(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x05, 0xFF }));

    // Limit Set & Clear
    EXPECT_EQ(ViscaBuilder::panTiltLimitSet(1, ViscaPanTiltCorner::UpRight, 0x2000, 0x1000).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x07, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF }));
    EXPECT_EQ(ViscaBuilder::panTiltLimitClear(1, ViscaPanTiltCorner::DownLeft).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x06, 0x07, 0x01, 0x00, 0x07, 0x0F, 0x0F, 0x0F, 0x07, 0x0F, 0x0F, 0x0F, 0xFF }));

    // Inquiries
    EXPECT_EQ(ViscaBuilder::panTiltPositionInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x06, 0x12, 0xFF }));
    EXPECT_EQ(ViscaBuilder::panTiltStatusInquiry(1).bytes(),
        (std::vector<uint8_t> { 0x81, 0x09, 0x06, 0x10, 0xFF }));
}

