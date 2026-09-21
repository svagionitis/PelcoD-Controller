/// @file TestProtocolParser.cpp
/// @brief Unit tests verifying Pelco-D response packet decoding.

#include "DeviceStatus.h"
#include "PelcoDFrame.h"
#include "ProtocolParser.h"
#include "TestHelpers.h"

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

void testDescribeFrame()
{
    using PelcoD::PelcoDFrame;
    using PelcoD::ProtocolParser;

    // 1. Empty and fragments
    assert(ProtocolParser::describeFrame(true, {}) == "Empty");
    assert(ProtocolParser::describeFrame(false, {}) == "Empty");
    assert(ProtocolParser::describeFrame(false, { 0xFFU, 0x01U }) == "Raw Frame (2 bytes)");

    // 2. 4-byte general response
    const std::vector<std::uint8_t> genFrame { 0xFFU, 0x02U, 0x0AU, 0x0CU };
    assert(ProtocolParser::describeFrame(false, genFrame) == "General Response (Addr 2, Alarms: 0x0A)");

    // 3. 18-byte query response
    std::vector<std::uint8_t> qFrame(18U, 0x00U);
    qFrame[0] = 0xFFU;
    qFrame[1] = 0x03U;
    qFrame[2] = 'S';
    qFrame[3] = 'O';
    qFrame[4] = 'N';
    qFrame[5] = 'Y';
    qFrame[17] = PelcoDFrame::calculateChecksum(&qFrame[1], 16U);
    assert(ProtocolParser::describeFrame(false, qFrame) == "Query Response (Addr 3): \"SONY\"");

    // 4. 7-byte TX PTZ commands
    const auto stopFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x00U, 0x00U, 0x00U);
    assert(ProtocolParser::describeFrame(true, stopFrame) == "PTZ Stop");

    const auto moveFrame = PelcoDFrame::createFrame(0x01U, 0x01U, 0x02U | 0x08U | 0x20U, 0x20U, 0x15U);
    // cmd2 has Right(spd 32), Up(spd 21), ZoomTele, cmd1 has FocusNear
    assert(ProtocolParser::describeFrame(true, moveFrame) == "PTZ: Right(spd 32), Up(spd 21), ZoomTele, FocusNear");

    // 5. 7-byte TX Extended commands
    const auto presetFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x07U, 0x00U, 0x05U);
    assert(ProtocolParser::describeFrame(true, presetFrame) == "GoTo Preset 5");

    const auto auxFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x09U, 0x00U, 0x02U);
    assert(ProtocolParser::describeFrame(true, auxFrame) == "Set Aux 2 ON");

    const auto qPanFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x51U, 0x00U, 0x00U);
    assert(ProtocolParser::describeFrame(true, qPanFrame) == "Query Pan Position");

    // 6. 7-byte RX Responses
    const auto ackFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x01U, 0x51U, 0x01U);
    assert(ProtocolParser::describeFrame(false, ackFrame) == "ACK (OK) for Opcode 0x51");

    const auto nakFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x01U, 0x51U, 0x00U);
    assert(ProtocolParser::describeFrame(false, nakFrame) == "NAK (Error) for Opcode 0x51");

    const auto panFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x30U, 0x39U);
    assert(ProtocolParser::describeFrame(false, panFrame) == "Pan Response: 123.45°");

    const auto tiltFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x11U, 0xD7U);
    assert(ProtocolParser::describeFrame(false, tiltFrame) == "Tilt Response: 45.67°");

    const auto zoomFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x5DU, 0x04U, 0xB0U);
    assert(ProtocolParser::describeFrame(false, zoomFrame) == "Zoom Response: 1200");

    const auto magFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x63U, 0x00U, 0x0AU);
    assert(ProtocolParser::describeFrame(false, magFrame) == "Magnification Response: 10");

    const auto devFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x6DU, 0x05U, 0x02U);
    assert(ProtocolParser::describeFrame(false, devFrame) == "Device Type Response: SW=0x05 HW=0x02");

    const auto diagFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x71U, 0x23U, 0x01U);
    assert(ProtocolParser::describeFrame(false, diagFrame) == "Diagnostics Response: Temp=35°C Sensor=0x01");
}

void testResponseClassification()
{
    // General 4-byte response
    const std::vector<std::uint8_t> genFrame { 0xFFU, 0x01U, 0x00U, 0x01U };
    assert(PelcoD::ProtocolParser::classifyResponse(genFrame) == PelcoD::ResponseClassification::General);

    // Standard Extended ACK/NAK (opcode 0x01)
    const auto ackFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x01U, 0x51U, 0x01U);
    assert(PelcoD::ProtocolParser::classifyResponse(ackFrame) == PelcoD::ResponseClassification::StandardExtendedAckNak);

    // Extended Telemetry (0x59, 0x5B, 0x5D, 0x63, 0x6D, 0x71)
    const auto panFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x10U, 0x20U);
    assert(PelcoD::ProtocolParser::classifyResponse(panFrame) == PelcoD::ResponseClassification::ExtendedTelemetry);

    const auto tiltFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x10U, 0x20U);
    assert(PelcoD::ProtocolParser::classifyResponse(tiltFrame) == PelcoD::ResponseClassification::ExtendedTelemetry);

    const auto zoomFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5DU, 0x10U, 0x20U);
    assert(PelcoD::ProtocolParser::classifyResponse(zoomFrame) == PelcoD::ResponseClassification::ExtendedTelemetry);

    const auto magFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x63U, 0x10U, 0x20U);
    assert(PelcoD::ProtocolParser::classifyResponse(magFrame) == PelcoD::ResponseClassification::ExtendedTelemetry);

    const auto devFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x6DU, 0x10U, 0x20U);
    assert(PelcoD::ProtocolParser::classifyResponse(devFrame) == PelcoD::ResponseClassification::ExtendedTelemetry);

    const auto diagFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x71U, 0x10U, 0x20U);
    assert(PelcoD::ProtocolParser::classifyResponse(diagFrame) == PelcoD::ResponseClassification::ExtendedTelemetry);

    // 18-byte Query Response
    std::vector<std::uint8_t> qFrame(18U, 0x00U);
    qFrame[0] = 0xFFU;
    assert(PelcoD::ProtocolParser::classifyResponse(qFrame) == PelcoD::ResponseClassification::QueryReply);

    // Unknown cases
    assert(PelcoD::ProtocolParser::classifyResponse({}) == PelcoD::ResponseClassification::Unknown);
    const std::vector<std::uint8_t> badSync { 0xFEU, 0x01U, 0x00U, 0x01U };
    assert(PelcoD::ProtocolParser::classifyResponse(badSync) == PelcoD::ResponseClassification::Unknown);

    const auto unknownOp = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x99U, 0x00U, 0x00U);
    assert(PelcoD::ProtocolParser::classifyResponse(unknownOp) == PelcoD::ResponseClassification::Unknown);
}

void testQueryMatching()
{
    const auto panFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x10U, 0x20U);
    const auto tiltFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x10U, 0x20U);
    const auto zoomFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5DU, 0x10U, 0x20U);
    const auto magFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x63U, 0x10U, 0x20U);
    const auto devFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x6DU, 0x10U, 0x20U);
    const auto diagFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x71U, 0x10U, 0x20U);
    std::vector<std::uint8_t> q18Frame(18U, 0x00U);
    q18Frame[0] = 0xFFU;

    // Exact tag matching
    assert(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryPan", panFrame));
    assert(!PelcoD::ProtocolParser::isResponseMatchingQuery("QueryPan", tiltFrame));

    assert(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryTilt", tiltFrame));
    assert(!PelcoD::ProtocolParser::isResponseMatchingQuery("QueryTilt", panFrame));

    assert(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryZoom", zoomFrame));
    assert(!PelcoD::ProtocolParser::isResponseMatchingQuery("QueryZoom", panFrame));

    assert(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryMagnification", magFrame));
    assert(!PelcoD::ProtocolParser::isResponseMatchingQuery("QueryMagnification", zoomFrame));

    assert(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryDeviceType", devFrame));
    assert(!PelcoD::ProtocolParser::isResponseMatchingQuery("QueryDeviceType", magFrame));

    assert(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryDiagnostics", diagFrame));
    assert(!PelcoD::ProtocolParser::isResponseMatchingQuery("QueryDiagnostics", devFrame));

    assert(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryGeneral", q18Frame));
    assert(!PelcoD::ProtocolParser::isResponseMatchingQuery("QueryGeneral", panFrame));

    // Fallback heuristic for generic queries
    assert(PelcoD::ProtocolParser::isResponseMatchingQuery("GenericQuery", panFrame));
    assert(PelcoD::ProtocolParser::isResponseMatchingQuery("GenericQuery", tiltFrame));
    assert(PelcoD::ProtocolParser::isResponseMatchingQuery("GenericQuery", q18Frame));

    // Non-matching frames and empty safety
    const auto nonTelemetry = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x03U, 0x00U, 0x01U);
    assert(!PelcoD::ProtocolParser::isResponseMatchingQuery("GenericQuery", nonTelemetry));
    assert(!PelcoD::ProtocolParser::isResponseMatchingQuery("QueryPan", {}));
}

int main()
{
    PelcoDTest::initTestHarness();
    std::cout << "[TestProtocolParser] Running tests..." << std::endl;
    testParseGeneral();
    testParseExtended();
    testParseQuery();
    testDescribeFrame();
    testResponseClassification();
    testQueryMatching();
    std::cout << "[TestProtocolParser] All tests passed successfully." << std::endl;
    return 0;
}
