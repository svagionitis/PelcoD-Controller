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

/// @brief Verify protocol frame construction for basic motion commands.
/// @details Checks pan left, stop, diagonal motion (up-right), and zoom tele commands
///          for valid byte structures and correct checksums.
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

/// @brief Verify preset management commands including Set, Go To, and Flip 180.
/// @details Validates opcode assignment in command 2, preset identifiers in data 2,
///          and correct checksum computation for preset transactions.
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

/// @brief Verify absolute positioning commands for pan and tilt angles in centidegrees.
/// @details Checks encoding of 16-bit centidegrees into MSB and LSB data bytes for opcodes 0x4B and 0x4D.
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

/// @brief Verify query command frame generation for pan, tilt, zoom, and device type.
/// @details Validates standard query opcodes: 0x51 (pan), 0x53 (tilt), 0x55 (zoom), and 0x6B (dev type).
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

/// @brief Verify camera configuration commands and speed setting frames.
/// @details Tests zoom speed, focus speed, auto-focus, auto-iris, AGC, backlight compensation,
///          white balance, and reset commands.
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

/// @brief Verify pattern record/play and zone scan configuration commands.
/// @details Checks opcode encoding for Pattern Start (0x1F), Pattern Stop (0x21),
///          Run Pattern (0x23), Set Zone Start (0x11), Set Zone End (0x13), and Zone Scan (0x1B/0x1D).
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

/// @brief Verify 16-bit extended configuration commands.
/// @details Validates two-byte data payload splitting into MSB and LSB for shutter speed (0x37),
///          gain (0x3F), white balance R/B (0x3B), and white balance M/G (0x3D).
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

/// @brief Verify on-screen display character writing, screen clear, and alarm acknowledge.
/// @details Tests Write Character (0x15) column/ASCII encoding, Clear Screen (0x17),
///          and Alarm Acknowledge (0x19).
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

/// @brief Verify auxiliary power and scan commands.
/// @details Tests buildPower on/off, buildScan auto/manual, and buildSetAux / buildClearAux opcodes.
TEST(ProtocolBuilderTest, AuxiliaryAndPowerCommands)
{
    // Power On & Off: cmd1 (byte 2) has DeviceOn (0x88) or DeviceOff (0x08)
    const auto pwrOn = PelcoD::ProtocolBuilder::buildPower(1U, true);
    EXPECT_EQ(pwrOn[2], 0x88U);
    EXPECT_EQ(pwrOn[3], 0x00U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(pwrOn));

    const auto pwrOff = PelcoD::ProtocolBuilder::buildPower(1U, false);
    EXPECT_EQ(pwrOff[2], 0x08U);
    EXPECT_EQ(pwrOff[3], 0x00U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(pwrOff));

    // Auto Scan & Manual Scan
    const auto autoScan = PelcoD::ProtocolBuilder::buildScan(1U, true);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(autoScan));
    const auto manualScan = PelcoD::ProtocolBuilder::buildScan(1U, false);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(manualScan));

    // Set Auxiliary 3 (opcode 0x09)
    const auto setAux = PelcoD::ProtocolBuilder::buildSetAux(1U, 3U);
    EXPECT_EQ(setAux[3], 0x09U);
    EXPECT_EQ(setAux[5], 0x03U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(setAux));

    // Clear Auxiliary 3 (opcode 0x0B)
    const auto clrAux = PelcoD::ProtocolBuilder::buildClearAux(1U, 3U);
    EXPECT_EQ(clrAux[3], 0x0BU);
    EXPECT_EQ(clrAux[5], 0x03U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(clrAux));
}

/// @brief Verify zero-pan and hardware azimuth zeroing command frames.
/// @details Checks buildZeroPan (opcode 0x07 with data2=0x22 or buildZeroPan opcode)
///          and buildSetZeroPosition (opcode 0x49).
TEST(ProtocolBuilderTest, ZeroPanAndAzimuthCommands)
{
    const auto zeroPan = PelcoD::ProtocolBuilder::buildZeroPan(1U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(zeroPan));

    const auto setZeroPos = PelcoD::ProtocolBuilder::buildSetZeroPosition(1U);
    EXPECT_EQ(setZeroPos[3], 0x49U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(setZeroPos));
}

/// @brief Verify auto-iris levels, peak settings, and phase delay commands.
/// @details Tests buildAutoIrisLevel, buildAutoIrisPeak, buildPhaseDelayMode, and buildLineLockDelay.
TEST(ProtocolBuilderTest, AutoIrisAndExposureControls)
{
    // Auto Iris Level (opcode 0x41, data2 = level)
    const auto irisLevel = PelcoD::ProtocolBuilder::buildAutoIrisLevel(1U, 0x1AU);
    EXPECT_EQ(irisLevel[3], 0x41U);
    EXPECT_EQ(irisLevel[5], 0x1AU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(irisLevel));

    // Auto Iris Peak (opcode 0x43, data2 = peak)
    const auto irisPeak = PelcoD::ProtocolBuilder::buildAutoIrisPeak(1U, 0x05U);
    EXPECT_EQ(irisPeak[3], 0x43U);
    EXPECT_EQ(irisPeak[5], 0x05U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(irisPeak));

    // Phase Delay Mode (opcode 0x35)
    const auto phaseDelay = PelcoD::ProtocolBuilder::buildPhaseDelayMode(1U, PelcoD::SwitchState::On);
    EXPECT_EQ(phaseDelay[3], 0x35U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(phaseDelay));

    // Line Lock Delay (opcode 0x39, 16-bit centidegrees)
    const auto lineLock = PelcoD::ProtocolBuilder::buildLineLockDelay(1U, 1200U);
    EXPECT_EQ(lineLock[3], 0x39U);
    EXPECT_EQ(lineLock[4], static_cast<std::uint8_t>(1200U >> 8U));
    EXPECT_EQ(lineLock[5], static_cast<std::uint8_t>(1200U & 0xFFU));
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(lineLock));
}

/// @brief Verify optical magnification and remote baud rate configuration builders.
/// @details Checks absolute and relative magnification (opcode 0x5F) and remote baud rate (opcode 0x67).
TEST(ProtocolBuilderTest, MagnificationAndBaudRate)
{
    // Absolute magnification: data1 = 0x00 (abs), data2 = lsb
    const auto absMag = PelcoD::ProtocolBuilder::buildSetMagnification(1U, 0x0250U, false);
    EXPECT_EQ(absMag[3], 0x5FU);
    EXPECT_EQ(absMag[4], 0x00U);
    EXPECT_EQ(absMag[5], 0x50U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(absMag));

    // Relative magnification: data1 = 0x01 (rel), data2 = lsb
    const auto relMag = PelcoD::ProtocolBuilder::buildSetMagnification(1U, 0x0050U, true);
    EXPECT_EQ(relMag[3], 0x5FU);
    EXPECT_EQ(relMag[4], 0x01U);
    EXPECT_EQ(relMag[5], 0x50U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(relMag));

    // Remote baud rate 9600 (code 0x02)
    const auto baud9600 = PelcoD::ProtocolBuilder::buildSetBaudRate(1U, 9600U);
    EXPECT_EQ(baud9600[3], 0x67U);
    EXPECT_EQ(baud9600[5], 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(baud9600));

    // Remote baud rate 2400 (code 0x00)
    const auto baud2400 = PelcoD::ProtocolBuilder::buildSetBaudRate(1U, 2400U);
    EXPECT_EQ(baud2400[3], 0x67U);
    EXPECT_EQ(baud2400[5], 0x00U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(baud2400));
}

/// @brief Verify builder methods for all supported query types.
/// @details Tests buildQueryPan, buildQueryTilt, buildQueryZoom, buildQueryMag,
///          buildQueryDevType, buildQueryGeneral, and buildQueryDiagnostics.
TEST(ProtocolBuilderTest, AllQueryVariants)
{
    const auto qPan = PelcoD::ProtocolBuilder::buildQueryPan(2U);
    EXPECT_EQ(qPan[1], 0x02U);
    EXPECT_EQ(qPan[3], 0x51U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qPan));

    const auto qTilt = PelcoD::ProtocolBuilder::buildQueryTilt(2U);
    EXPECT_EQ(qTilt[1], 0x02U);
    EXPECT_EQ(qTilt[3], 0x53U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qTilt));

    const auto qZoom = PelcoD::ProtocolBuilder::buildQueryZoom(2U);
    EXPECT_EQ(qZoom[1], 0x02U);
    EXPECT_EQ(qZoom[3], 0x55U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qZoom));

    const auto qMag = PelcoD::ProtocolBuilder::buildQueryMag(2U);
    EXPECT_EQ(qMag[1], 0x02U);
    EXPECT_EQ(qMag[3], 0x61U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qMag));

    const auto qGeneral = PelcoD::ProtocolBuilder::buildQueryGeneral(2U);
    EXPECT_EQ(qGeneral[1], 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qGeneral));

    const auto qDiag = PelcoD::ProtocolBuilder::buildQueryDiagnostics(2U);
    EXPECT_EQ(qDiag[1], 0x02U);
    EXPECT_EQ(qDiag[3], 0x6FU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qDiag));
}

/// @brief Verify buildMotion combining multiple simultaneous actions in one frame.
/// @details Tests simultaneous Pan Left, Tilt Down, Zoom Tele, Focus Far, and Iris Open.
TEST(ProtocolBuilderTest, CombinedMotionMultiAction)
{
    const auto multi
        = PelcoD::ProtocolBuilder::buildMotion(1U, PelcoD::PanDirection::Left, 0x25U, PelcoD::TiltDirection::Down,
            0x1AU, PelcoD::ZoomAction::Tele, PelcoD::FocusAction::Far, PelcoD::IrisAction::Open);

    ASSERT_EQ(multi.size(), 7U);
    EXPECT_EQ(multi[0], 0xFFU);
    EXPECT_EQ(multi[1], 0x01U);
    // Pan Left is 0x04 in cmd2; Tilt Down is 0x10 in cmd2
    // Zoom Tele is 0x20 in cmd2; Focus Far is 0x80 in cmd1 or cmd2; Iris Open is 0x02 in cmd1
    EXPECT_EQ(multi[4], 0x25U); // pan speed
    EXPECT_EQ(multi[5], 0x1AU); // tilt speed
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(multi));
}

} // namespace
