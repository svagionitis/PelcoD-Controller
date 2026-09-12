/// @file TestProtocolParser.cpp
/// @brief Unit tests verifying Pelco-D response packet decoding.

#include "DeviceStatus.h"
#include "PelcoDFrame.h"
#include "ProtocolParser.h"

#include <cassert>
#include <iostream>
#include <vector>

void testParseGeneral()
{
    // 4-byte general reply: 0xFF, Addr 1, Alarms 0x05, Checksum
    const std::vector<std::uint8_t> frame { 0xFFU, 0x01U, 0x05U, 0x06U };
    std::uint8_t addr { 0U };
    std::uint8_t alarms { 0U };

    assert(PelcoD::ProtocolParser::parseGeneral(frame, addr, alarms));
    assert(addr == 1U);
    assert(alarms == 0x05U);

    PelcoD::DeviceStatus status;
    PelcoD::DeviceInfo info;
    assert(PelcoD::ProtocolParser::updateStatus(frame, status, info));
    assert(status.address == 1U);
    assert(status.alarms == 0x05U);
    assert(status.isAlarmActive(1));
    assert(!status.isAlarmActive(2));
    assert(status.isAlarmActive(3));
}

void testParseExtended()
{
    // Pan response (Opcode 0x59): Pan = 123.45 deg = 12345 = 0x3039
    const auto panFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x30U, 0x39U);
    std::uint16_t pan { 0U };
    assert(PelcoD::ProtocolParser::parsePan(panFrame, pan));
    assert(pan == 12345U);

    // Tilt response (Opcode 0x5B): Tilt = 45.67 deg = 4567 = 0x11D7
    const auto tiltFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x11U, 0xD7U);
    std::uint16_t tilt { 0U };
    assert(PelcoD::ProtocolParser::parseTilt(tiltFrame, tilt));
    assert(tilt == 4567U);

    // Zoom response (Opcode 0x5D): Zoom = 1200 = 0x04B0
    const auto zoomFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5DU, 0x04U, 0xB0U);
    std::uint16_t zoom { 0U };
    assert(PelcoD::ProtocolParser::parseZoom(zoomFrame, zoom));
    assert(zoom == 1200U);

    // Device Type (Opcode 0x6D): SW = 0x05, HW = 0x02
    const auto devFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x6DU, 0x05U, 0x02U);
    std::uint8_t sw { 0U };
    std::uint8_t hw { 0U };
    assert(PelcoD::ProtocolParser::parseDevType(devFrame, sw, hw));
    assert(sw == 0x05U);
    assert(hw == 0x02U);

    // Update Status with Pan and Tilt
    PelcoD::DeviceStatus status;
    PelcoD::DeviceInfo info;
    assert(PelcoD::ProtocolParser::updateStatus(panFrame, status, info));
    assert(status.panCentidegrees == 12345U);
    assert(PelcoD::ProtocolParser::updateStatus(tiltFrame, status, info));
    assert(status.tiltCentidegrees == 4567U);
}

void testParseQuery()
{
    // 18-byte query reply
    std::vector<std::uint8_t> qFrame(18U, 0x00U);
    qFrame[0] = 0xFFU;
    qFrame[1] = 0x01U;
    qFrame[2] = 'P';
    qFrame[3] = 'E';
    qFrame[4] = 'L';
    qFrame[5] = 'C';
    qFrame[6] = 'O';
    qFrame[17] = PelcoD::PelcoDFrame::calculateChecksum(&qFrame[1], 16U);

    std::string payload;
    assert(PelcoD::ProtocolParser::parseQuery(qFrame, payload));
    assert(payload == "PELCO");

    PelcoD::DeviceStatus status;
    PelcoD::DeviceInfo info;
    assert(PelcoD::ProtocolParser::updateStatus(qFrame, status, info));
    assert(info.modelName == "PELCO");

    // Corrupted checksum must be rejected by updateStatus
    auto badQFrame = qFrame;
    badQFrame[17] ^= 0x55U;
    assert(!PelcoD::ProtocolParser::updateStatus(badQFrame, status, info));
}

int main()
{
    std::cout << "[TestProtocolParser] Running tests..." << std::endl;
    testParseGeneral();
    testParseExtended();
    testParseQuery();
    std::cout << "[TestProtocolParser] All tests passed successfully." << std::endl;
    return 0;
}
