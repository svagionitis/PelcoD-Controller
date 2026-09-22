/// @file TestSonyViscaCommands.cpp
/// @brief Unit tests for SonyViscaBuilder and SonyViscaParser encoding and decoding.

#include "SonyViscaBuilder.h"
#include "SonyViscaParser.h"
#include <gtest/gtest.h>

using namespace Visca;
using namespace Visca::Sony;

/// @brief Tests Sony FCB lens zoom and focus command construction.
TEST(TestSonyViscaCommands, LensCommands)
{
    // Zoom Stop: 81 01 04 07 00 FF
    EXPECT_EQ(SonyViscaBuilder::zoomStop(1).bytes(), (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x07, 0x00, 0xFF }));

    // Zoom Direct (0x2000): 81 01 04 47 02 00 00 00 FF
    EXPECT_EQ(SonyViscaBuilder::zoomDirect(1, 0x2000).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x47, 0x02, 0x00, 0x00, 0x00, 0xFF }));

    // Zoom Tele Variable (speed 5): 81 01 04 07 25 FF
    EXPECT_EQ(SonyViscaBuilder::zoomTeleVariable(1, 5).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x07, 0x25, 0xFF }));

    // Focus Auto: 81 01 04 38 02 FF
    EXPECT_EQ(
        SonyViscaBuilder::focusAuto(1, true).bytes(), (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x38, 0x02, 0xFF }));

    // Focus Direct (0x5432): 81 01 04 48 05 04 03 02 FF
    EXPECT_EQ(SonyViscaBuilder::focusDirect(1, 0x5432).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x48, 0x05, 0x04, 0x03, 0x02, 0xFF }));
}

/// @brief Tests Sony FCB exposure and white balance command construction.
TEST(TestSonyViscaCommands, ExposureAndWhiteBalance)
{
    EXPECT_EQ(SonyViscaBuilder::exposureMode(1, SonyExposureMode::FullAuto).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x39, 0x00, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::shutterDirect(1, 0x15).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x4A, 0x00, 0x00, 0x01, 0x05, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::wbMode(1, SonyWhiteBalanceMode::ATW).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x35, 0x04, 0xFF }));
}

/// @brief Tests Sony FCB image enhancements, registers, and Block Inquiry generation.
TEST(TestSonyViscaCommands, EnhancementsAndBlockInquiries)
{
    EXPECT_EQ(SonyViscaBuilder::stabilizer(1, SonyStabilizerMode::SuperPlus).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x34, 0x05, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::defog(1, SonyDefogMode::High).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x37, 0x03, 0xFF }));

    EXPECT_EQ(SonyViscaBuilder::writeRegister(1, 0x57, 0x01).bytes(),
        (std::vector<uint8_t> { 0x81, 0x01, 0x04, 0x24, 0x57, 0x00, 0x01, 0xFF }));

    EXPECT_EQ(
        SonyViscaBuilder::blockInquiry(1, 0).bytes(), (std::vector<uint8_t> { 0x81, 0x09, 0x7E, 0x7E, 0x00, 0xFF }));
}

/// @brief Tests decoding of Block Inquiry 00 (Lens Control).
TEST(TestSonyViscaCommands, Block00Decode)
{
    SonyFCBStatus status {};
    const ViscaFrame block00 { 0x90, 0x50, 0x01, 0x02, 0x04, 0x06, 0x08, 0x03, 0x05, 0x07, 0x09, 0x01, 0x00, 0x00, 0x00,
        0xFF };

    EXPECT_TRUE(SonyViscaParser::parseBlock00(block00, status));
    EXPECT_TRUE(status.focusAuto);
    EXPECT_EQ(status.zoomPosition, 0x2468);
    EXPECT_EQ(status.focusPosition, 0x3579);
    EXPECT_EQ(status.focusNearLimit, 0x1000);
}

/// @brief Tests decoding of Block Inquiry 01 (Camera Control).
TEST(TestSonyViscaCommands, Block01Decode)
{
    SonyFCBStatus status {};
    const ViscaFrame block01 { 0x90, 0x50, 0x00,
        0x02, // WB Outdoor
        0x05, // Aperture
        0x03, // Exposure Manual
        0x12, // Shutter
        0x0E, // Iris
        0x08, // Gain
        0x07, // Exp comp pos
        0x00, 0x01, 0x05, // RGain: 0x15
        0x02, 0x0A, // BGain: 0x2A
        0xFF };

    EXPECT_TRUE(SonyViscaParser::parseBlock01(block01, status));
    EXPECT_EQ(status.exposureMode, SonyExposureMode::Manual);
    EXPECT_EQ(status.wbMode, SonyWhiteBalanceMode::Outdoor);
    EXPECT_EQ(status.shutterPosition, 0x12);
    EXPECT_EQ(status.irisPosition, 0x0E);
    EXPECT_EQ(status.gainPosition, 0x08);
}
