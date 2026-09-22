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
