/// @file TestProtocolBuilder.cpp
/// @brief Google Test unit tests verifying Pelco-D protocol frame encoding.

#include "PelcoDFrame.h"
#include "ProtocolBuilder.h"
#include "TestHelpers.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

TEST(ProtocolBuilderTest, MotionCommands)
{
    // Pan Left: Addr 2, speed 0x20
    const auto pLeft = PelcoD::ProtocolBuilder::buildPan(2U, PelcoD::PanDirection::Left, 0x20U);
    ASSERT_EQ(pLeft.size(), 7U);
    EXPECT_EQ(pLeft[0], 0xFFU);
    EXPECT_EQ(pLeft[1], 0x02U);
    EXPECT_EQ(pLeft[2], 0x00U);
    EXPECT_EQ(pLeft[3], 0x04U); // Left bit
    EXPECT_EQ(pLeft[4], 0x20U);
    EXPECT_EQ(pLeft[5], 0x00U);
    EXPECT_EQ(pLeft[6], 0x26U);

    // Stop command
    const auto stop = PelcoD::ProtocolBuilder::buildStop(2U);
    EXPECT_EQ(stop[3], 0x00U);
    EXPECT_EQ(stop[6], 0x02U);

    // Diagonal: Up-Right (Up = 0x08, Right = 0x02 -> cmd2 = 0x0A)
    const auto diag = PelcoD::ProtocolBuilder::buildMotion(
        1U, PelcoD::PanDirection::Right, 0x10U, PelcoD::TiltDirection::Up, 0x15U);
    EXPECT_EQ(diag[1], 0x01U);
    EXPECT_EQ(diag[3], 0x0AU);
    EXPECT_EQ(diag[4], 0x10U);
    EXPECT_EQ(diag[5], 0x15U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(diag));

    // Zoom Tele
    const auto tele = PelcoD::ProtocolBuilder::buildZoom(1U, PelcoD::ZoomAction::Tele);
    EXPECT_EQ(tele[3], 0x20U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(tele));
}

TEST(ProtocolBuilderTest, Presets)
{
    // Set Preset 5
    const auto setP = PelcoD::ProtocolBuilder::buildSetPreset(1U, 5U);
    EXPECT_EQ(setP[2], 0x00U);
    EXPECT_EQ(setP[3], 0x03U);
    EXPECT_EQ(setP[5], 5U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(setP));

    // Go To Preset 1
    const auto goToP = PelcoD::ProtocolBuilder::buildGoToPreset(1U, 1U);
    EXPECT_EQ(goToP[3], 0x07U);
    EXPECT_EQ(goToP[5], 1U);
    EXPECT_EQ(goToP[6], 0x09U); // Addr 1 + 0x07 + 0x01 = 0x09

    // Flip 180 (Preset 0x21)
    const auto flip = PelcoD::ProtocolBuilder::buildFlip180(1U);
    EXPECT_EQ(flip[3], 0x07U);
    EXPECT_EQ(flip[5], 0x21U);
}

TEST(ProtocolBuilderTest, AbsolutePositioning)
{
    // Set Pan to 180.00 degrees (18000 centidegrees = 0x4650)
    const auto panPos = PelcoD::ProtocolBuilder::buildSetPan(1U, 18000U);
    EXPECT_EQ(panPos[3], 0x4BU);
    EXPECT_EQ(panPos[4], 0x46U);
    EXPECT_EQ(panPos[5], 0x50U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(panPos));

    // Set Tilt to 45.00 degrees (4500 centidegrees = 0x1194)
    const auto tiltPos = PelcoD::ProtocolBuilder::buildSetTilt(1U, 4500U);
    EXPECT_EQ(tiltPos[3], 0x4DU);
    EXPECT_EQ(tiltPos[4], 0x11U);
    EXPECT_EQ(tiltPos[5], 0x94U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(tiltPos));
}

TEST(ProtocolBuilderTest, Queries)
{
    const auto qPan = PelcoD::ProtocolBuilder::buildQueryPan(1U);
    EXPECT_EQ(qPan[3], 0x51U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qPan));

    const auto qTilt = PelcoD::ProtocolBuilder::buildQueryTilt(1U);
    EXPECT_EQ(qTilt[3], 0x53U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qTilt));

    const auto qZoom = PelcoD::ProtocolBuilder::buildQueryZoom(1U);
    EXPECT_EQ(qZoom[3], 0x55U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qZoom));

    const auto qDev = PelcoD::ProtocolBuilder::buildQueryDevType(1U);
    EXPECT_EQ(qDev[3], 0x6BU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qDev));
}

TEST(ProtocolBuilderTest, ConfigurationAndSpeeds)
{
    // Zoom speed (max 3)
    const auto zSpeed = PelcoD::ProtocolBuilder::buildZoomSpeed(1U, 2U);
    EXPECT_EQ(zSpeed[3], 0x25U);
    EXPECT_EQ(zSpeed[5], 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(zSpeed));

    // Focus speed (max 3)
    const auto fSpeed = PelcoD::ProtocolBuilder::buildFocusSpeed(1U, 3U);
    EXPECT_EQ(fSpeed[3], 0x27U);
    EXPECT_EQ(fSpeed[5], 0x03U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(fSpeed));

    // Auto Focus
    const auto autoF = PelcoD::ProtocolBuilder::buildAutoFocus(1U, PelcoD::AutoMode::Auto);
    EXPECT_EQ(autoF[3], 0x2BU);
    EXPECT_EQ(autoF[5], static_cast<std::uint8_t>(PelcoD::AutoMode::Auto));
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(autoF));

    // Auto Iris
    const auto autoI = PelcoD::ProtocolBuilder::buildAutoIris(1U, PelcoD::AutoMode::On);
    EXPECT_EQ(autoI[3], 0x2DU);
    EXPECT_EQ(autoI[5], static_cast<std::uint8_t>(PelcoD::AutoMode::On));
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(autoI));

    // Agc
    const auto agc = PelcoD::ProtocolBuilder::buildAgc(1U, PelcoD::AutoMode::Off);
    EXPECT_EQ(agc[3], 0x2FU);
    EXPECT_EQ(agc[5], static_cast<std::uint8_t>(PelcoD::AutoMode::Off));
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(agc));

    // Backlight compensation
    const auto blc = PelcoD::ProtocolBuilder::buildBacklight(1U, PelcoD::SwitchState::On);
    EXPECT_EQ(blc[3], 0x31U);
    EXPECT_EQ(blc[5], 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(blc));

    // Auto white balance
    const auto awb = PelcoD::ProtocolBuilder::buildWhiteBalance(1U, PelcoD::SwitchState::On);
    EXPECT_EQ(awb[3], 0x33U);
    EXPECT_EQ(awb[5], 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(awb));

    // Reset Defaults & Remote Reset
    const auto rDef = PelcoD::ProtocolBuilder::buildResetDefaults(1U);
    EXPECT_EQ(rDef[3], 0x29U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(rDef));

    const auto rRem = PelcoD::ProtocolBuilder::buildRemoteReset(1U);
    EXPECT_EQ(rRem[3], 0x0FU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(rRem));
}

TEST(ProtocolBuilderTest, PatternsAndZones)
{
    // Pattern Start, Stop, Run
    const auto pStart = PelcoD::ProtocolBuilder::buildPatternStart(1U, 2U);
    EXPECT_EQ(pStart[3], 0x1FU);
    EXPECT_EQ(pStart[5], 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(pStart));

    const auto pStop = PelcoD::ProtocolBuilder::buildPatternStop(1U);
    EXPECT_EQ(pStop[3], 0x21U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(pStop));

    const auto pRun = PelcoD::ProtocolBuilder::buildRunPattern(1U, 2U);
    EXPECT_EQ(pRun[3], 0x23U);
    EXPECT_EQ(pRun[5], 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(pRun));

    // Zone Start & End
    const auto zStart = PelcoD::ProtocolBuilder::buildSetZoneStart(1U, 1U);
    EXPECT_EQ(zStart[3], 0x11U);
    EXPECT_EQ(zStart[5], 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(zStart));

    const auto zEnd = PelcoD::ProtocolBuilder::buildSetZoneEnd(1U, 1U);
    EXPECT_EQ(zEnd[3], 0x13U);
    EXPECT_EQ(zEnd[5], 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(zEnd));

    // Zone Scan On & Off
    const auto zScanOn = PelcoD::ProtocolBuilder::buildZoneScan(1U, true);
    EXPECT_EQ(zScanOn[3], 0x1BU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(zScanOn));

    const auto zScanOff = PelcoD::ProtocolBuilder::buildZoneScan(1U, false);
    EXPECT_EQ(zScanOff[3], 0x1DU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(zScanOff));
}

TEST(ProtocolBuilderTest, SixteenBitCommands)
{
    // Shutter Speed: 0x0120 -> msb 0x01, lsb 0x20
    const auto shutter = PelcoD::ProtocolBuilder::buildShutterSpeed(1U, 0x0120U);
    EXPECT_EQ(shutter[3], 0x37U);
    EXPECT_EQ(shutter[4], 0x01U);
    EXPECT_EQ(shutter[5], 0x20U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(shutter));

    // Gain: 0x0250 -> msb 0x02, lsb 0x50
    const auto gain = PelcoD::ProtocolBuilder::buildGain(1U, 0x0250U);
    EXPECT_EQ(gain[3], 0x3FU);
    EXPECT_EQ(gain[4], 0x02U);
    EXPECT_EQ(gain[5], 0x50U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(gain));

    // White Balance Red/Blue: 0x0080
    const auto wbRB = PelcoD::ProtocolBuilder::buildWhiteBalanceRB(1U, 0x0080U);
    EXPECT_EQ(wbRB[3], 0x3BU);
    EXPECT_EQ(wbRB[4], 0x00U);
    EXPECT_EQ(wbRB[5], 0x80U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(wbRB));

    // White Balance Magenta/Green: 0x0090
    const auto wbMG = PelcoD::ProtocolBuilder::buildWhiteBalanceMG(1U, 0x0090U);
    EXPECT_EQ(wbMG[3], 0x3DU);
    EXPECT_EQ(wbMG[4], 0x00U);
    EXPECT_EQ(wbMG[5], 0x90U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(wbMG));
}

TEST(ProtocolBuilderTest, OsdAndAlarms)
{
    // Write Character 'A' at column 5
    const auto writeChar = PelcoD::ProtocolBuilder::buildWriteChar(1U, 5U, 'A');
    EXPECT_EQ(writeChar[3], 0x15U);
    EXPECT_EQ(writeChar[4], 0x05U);
    EXPECT_EQ(writeChar[5], static_cast<std::uint8_t>('A'));
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(writeChar));

    // Clear Screen
    const auto clrScreen = PelcoD::ProtocolBuilder::buildClearScreen(1U);
    EXPECT_EQ(clrScreen[3], 0x17U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(clrScreen));

    // Alarm Acknowledge
    const auto ack = PelcoD::ProtocolBuilder::buildAlarmAck(1U, 4U);
    EXPECT_EQ(ack[3], 0x19U);
    EXPECT_EQ(ack[5], 0x04U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(ack));
}

} // namespace
