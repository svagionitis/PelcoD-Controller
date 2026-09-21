/// @file TestProtocolBuilder.cpp
/// @brief Unit tests verifying Pelco-D protocol frame encoding.

#include "PelcoDFrame.h"
#include "ProtocolBuilder.h"
#include "TestHelpers.h"

#include <cassert>
#include <iostream>
#include <vector>

void testMotionCommands()
{
    // Pan Left: Addr 2, speed 0x20
    const auto pLeft = PelcoD::ProtocolBuilder::buildPan(2U, PelcoD::PanDirection::Left, 0x20U);
    assert(pLeft.size() == 7U);
    assert(pLeft[0] == 0xFFU);
    assert(pLeft[1] == 0x02U);
    assert(pLeft[2] == 0x00U);
    assert(pLeft[3] == 0x04U); // Left bit
    assert(pLeft[4] == 0x20U);
    assert(pLeft[5] == 0x00U);
    assert(pLeft[6] == 0x26U);

    // Stop command
    const auto stop = PelcoD::ProtocolBuilder::buildStop(2U);
    assert(stop[3] == 0x00U);
    assert(stop[6] == 0x02U);

    // Diagonal: Up-Right (Up = 0x08, Right = 0x02 -> cmd2 = 0x0A)
    const auto diag = PelcoD::ProtocolBuilder::buildMotion(
        1U, PelcoD::PanDirection::Right, 0x10U, PelcoD::TiltDirection::Up, 0x15U);
    assert(diag[1] == 0x01U);
    assert(diag[3] == 0x0AU);
    assert(diag[4] == 0x10U);
    assert(diag[5] == 0x15U);
    assert(PelcoD::PelcoDFrame::isValidFrame(diag));

    // Zoom Tele
    const auto tele = PelcoD::ProtocolBuilder::buildZoom(1U, PelcoD::ZoomAction::Tele);
    assert(tele[3] == 0x20U);
    assert(PelcoD::PelcoDFrame::isValidFrame(tele));
}

void testPresets()
{
    // Set Preset 5
    const auto setP = PelcoD::ProtocolBuilder::buildSetPreset(1U, 5U);
    assert(setP[2] == 0x00U);
    assert(setP[3] == 0x03U);
    assert(setP[5] == 5U);
    assert(PelcoD::PelcoDFrame::isValidFrame(setP));

    // Go To Preset 1
    const auto goToP = PelcoD::ProtocolBuilder::buildGoToPreset(1U, 1U);
    assert(goToP[3] == 0x07U);
    assert(goToP[5] == 1U);
    assert(goToP[6] == 0x09U); // Addr 1 + 0x07 + 0x01 = 0x09

    // Flip 180 (Preset 0x21)
    const auto flip = PelcoD::ProtocolBuilder::buildFlip180(1U);
    assert(flip[3] == 0x07U);
    assert(flip[5] == 0x21U);
}

void testAbsolutePositioning()
{
    // Set Pan to 180.00 degrees (18000 centidegrees = 0x4650)
    const auto panPos = PelcoD::ProtocolBuilder::buildSetPan(1U, 18000U);
    assert(panPos[3] == 0x4BU);
    assert(panPos[4] == 0x46U);
    assert(panPos[5] == 0x50U);
    assert(PelcoD::PelcoDFrame::isValidFrame(panPos));

    // Set Tilt to 45.00 degrees (4500 centidegrees = 0x1194)
    const auto tiltPos = PelcoD::ProtocolBuilder::buildSetTilt(1U, 4500U);
    assert(tiltPos[3] == 0x4DU);
    assert(tiltPos[4] == 0x11U);
    assert(tiltPos[5] == 0x94U);
    assert(PelcoD::PelcoDFrame::isValidFrame(tiltPos));
}

void testQueries()
{
    const auto qPan = PelcoD::ProtocolBuilder::buildQueryPan(1U);
    assert(qPan[3] == 0x51U);
    assert(PelcoD::PelcoDFrame::isValidFrame(qPan));

    const auto qTilt = PelcoD::ProtocolBuilder::buildQueryTilt(1U);
    assert(qTilt[3] == 0x53U);
    assert(PelcoD::PelcoDFrame::isValidFrame(qTilt));

    const auto qZoom = PelcoD::ProtocolBuilder::buildQueryZoom(1U);
    assert(qZoom[3] == 0x55U);
    assert(PelcoD::PelcoDFrame::isValidFrame(qZoom));

    const auto qDev = PelcoD::ProtocolBuilder::buildQueryDevType(1U);
    assert(qDev[3] == 0x6BU);
    assert(PelcoD::PelcoDFrame::isValidFrame(qDev));
}

void testConfigurationAndSpeeds()
{
    // Zoom speed (max 3)
    const auto zSpeed = PelcoD::ProtocolBuilder::buildZoomSpeed(1U, 2U);
    assert(zSpeed[3] == 0x25U);
    assert(zSpeed[5] == 0x02U);
    assert(PelcoD::PelcoDFrame::isValidFrame(zSpeed));

    // Focus speed (max 3)
    const auto fSpeed = PelcoD::ProtocolBuilder::buildFocusSpeed(1U, 3U);
    assert(fSpeed[3] == 0x27U);
    assert(fSpeed[5] == 0x03U);
    assert(PelcoD::PelcoDFrame::isValidFrame(fSpeed));

    // Auto Focus
    const auto autoF = PelcoD::ProtocolBuilder::buildAutoFocus(1U, PelcoD::AutoMode::Auto);
    assert(autoF[3] == 0x2BU);
    assert(autoF[5] == static_cast<std::uint8_t>(PelcoD::AutoMode::Auto));
    assert(PelcoD::PelcoDFrame::isValidFrame(autoF));

    // Auto Iris
    const auto autoI = PelcoD::ProtocolBuilder::buildAutoIris(1U, PelcoD::AutoMode::On);
    assert(autoI[3] == 0x2DU);
    assert(autoI[5] == static_cast<std::uint8_t>(PelcoD::AutoMode::On));
    assert(PelcoD::PelcoDFrame::isValidFrame(autoI));

    // Agc
    const auto agc = PelcoD::ProtocolBuilder::buildAgc(1U, PelcoD::AutoMode::Off);
    assert(agc[3] == 0x2FU);
    assert(agc[5] == static_cast<std::uint8_t>(PelcoD::AutoMode::Off));
    assert(PelcoD::PelcoDFrame::isValidFrame(agc));

    // Backlight compensation
    const auto blc = PelcoD::ProtocolBuilder::buildBacklight(1U, PelcoD::SwitchState::On);
    assert(blc[3] == 0x31U);
    assert(blc[5] == 0x01U);
    assert(PelcoD::PelcoDFrame::isValidFrame(blc));

    // Auto white balance
    const auto awb = PelcoD::ProtocolBuilder::buildWhiteBalance(1U, PelcoD::SwitchState::On);
    assert(awb[3] == 0x33U);
    assert(awb[5] == 0x01U);
    assert(PelcoD::PelcoDFrame::isValidFrame(awb));

    // Reset Defaults & Remote Reset
    const auto rDef = PelcoD::ProtocolBuilder::buildResetDefaults(1U);
    assert(rDef[3] == 0x29U);
    assert(PelcoD::PelcoDFrame::isValidFrame(rDef));

    const auto rRem = PelcoD::ProtocolBuilder::buildRemoteReset(1U);
    assert(rRem[3] == 0x0FU);
    assert(PelcoD::PelcoDFrame::isValidFrame(rRem));
}

void testPatternsAndZones()
{
    // Pattern Start, Stop, Run
    const auto pStart = PelcoD::ProtocolBuilder::buildPatternStart(1U, 2U);
    assert(pStart[3] == 0x1FU);
    assert(pStart[5] == 0x02U);
    assert(PelcoD::PelcoDFrame::isValidFrame(pStart));

    const auto pStop = PelcoD::ProtocolBuilder::buildPatternStop(1U);
    assert(pStop[3] == 0x21U);
    assert(PelcoD::PelcoDFrame::isValidFrame(pStop));

    const auto pRun = PelcoD::ProtocolBuilder::buildRunPattern(1U, 2U);
    assert(pRun[3] == 0x23U);
    assert(pRun[5] == 0x02U);
    assert(PelcoD::PelcoDFrame::isValidFrame(pRun));

    // Zone Start & End
    const auto zStart = PelcoD::ProtocolBuilder::buildSetZoneStart(1U, 1U);
    assert(zStart[3] == 0x11U);
    assert(zStart[5] == 0x01U);
    assert(PelcoD::PelcoDFrame::isValidFrame(zStart));

    const auto zEnd = PelcoD::ProtocolBuilder::buildSetZoneEnd(1U, 1U);
    assert(zEnd[3] == 0x13U);
    assert(zEnd[5] == 0x01U);
    assert(PelcoD::PelcoDFrame::isValidFrame(zEnd));

    // Zone Scan On & Off
    const auto zScanOn = PelcoD::ProtocolBuilder::buildZoneScan(1U, true);
    assert(zScanOn[3] == 0x1BU);
    assert(PelcoD::PelcoDFrame::isValidFrame(zScanOn));

    const auto zScanOff = PelcoD::ProtocolBuilder::buildZoneScan(1U, false);
    assert(zScanOff[3] == 0x1DU);
    assert(PelcoD::PelcoDFrame::isValidFrame(zScanOff));
}

void test16BitCommands()
{
    // Shutter Speed: 0x0120 -> msb 0x01, lsb 0x20
    const auto shutter = PelcoD::ProtocolBuilder::buildShutterSpeed(1U, 0x0120U);
    assert(shutter[3] == 0x37U);
    assert(shutter[4] == 0x01U);
    assert(shutter[5] == 0x20U);
    assert(PelcoD::PelcoDFrame::isValidFrame(shutter));

    // Gain: 0x0250 -> msb 0x02, lsb 0x50
    const auto gain = PelcoD::ProtocolBuilder::buildGain(1U, 0x0250U);
    assert(gain[3] == 0x3FU);
    assert(gain[4] == 0x02U);
    assert(gain[5] == 0x50U);
    assert(PelcoD::PelcoDFrame::isValidFrame(gain));

    // White Balance Red/Blue: 0x0080
    const auto wbRB = PelcoD::ProtocolBuilder::buildWhiteBalanceRB(1U, 0x0080U);
    assert(wbRB[3] == 0x3BU);
    assert(wbRB[4] == 0x00U);
    assert(wbRB[5] == 0x80U);
    assert(PelcoD::PelcoDFrame::isValidFrame(wbRB));

    // White Balance Magenta/Green: 0x0090
    const auto wbMG = PelcoD::ProtocolBuilder::buildWhiteBalanceMG(1U, 0x0090U);
    assert(wbMG[3] == 0x3DU);
    assert(wbMG[4] == 0x00U);
    assert(wbMG[5] == 0x90U);
    assert(PelcoD::PelcoDFrame::isValidFrame(wbMG));
}

void testOsdAndAlarms()
{
    // Write Character 'A' at column 5
    const auto writeChar = PelcoD::ProtocolBuilder::buildWriteChar(1U, 5U, 'A');
    assert(writeChar[3] == 0x15U);
    assert(writeChar[4] == 0x05U);
    assert(writeChar[5] == static_cast<std::uint8_t>('A'));
    assert(PelcoD::PelcoDFrame::isValidFrame(writeChar));

    // Clear Screen
    const auto clrScreen = PelcoD::ProtocolBuilder::buildClearScreen(1U);
    assert(clrScreen[3] == 0x17U);
    assert(PelcoD::PelcoDFrame::isValidFrame(clrScreen));

    // Alarm Acknowledge
    const auto ack = PelcoD::ProtocolBuilder::buildAlarmAck(1U, 4U);
    assert(ack[3] == 0x19U);
    assert(ack[5] == 0x04U);
    assert(PelcoD::PelcoDFrame::isValidFrame(ack));
}

int main()
{
    PelcoDTest::initTestHarness();
    std::cout << "[TestProtocolBuilder] Running tests..." << std::endl;
    testMotionCommands();
    testPresets();
    testAbsolutePositioning();
    testQueries();
    testConfigurationAndSpeeds();
    testPatternsAndZones();
    test16BitCommands();
    testOsdAndAlarms();
    std::cout << "[TestProtocolBuilder] All tests passed successfully." << std::endl;
    return 0;
}
