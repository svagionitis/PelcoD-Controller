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

int main()
{
    PelcoDTest::initTestHarness();
    std::cout << "[TestProtocolBuilder] Running tests..." << std::endl;
    testMotionCommands();
    testPresets();
    testAbsolutePositioning();
    testQueries();
    std::cout << "[TestProtocolBuilder] All tests passed successfully." << std::endl;
    return 0;
}
