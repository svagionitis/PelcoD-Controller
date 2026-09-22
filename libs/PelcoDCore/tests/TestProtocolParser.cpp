/// @file TestProtocolParser.cpp
/// @brief Google Test unit tests verifying Pelco-D response packet decoding.

#include "DeviceStatus.h"
#include "PelcoDFrame.h"
#include "ProtocolParser.h"
#include "TestHelpers.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

/// @brief Verify parsing of 4-byte general response frames into address and alarm bitmask.
/// @details Checks parseGeneral and updateStatus behavior when consuming 4-byte ACK/Alarm responses,
///          verifying active alarm flags are accurately reflected in DeviceStatus.
TEST(ProtocolParserTest, ParseGeneral)
{
    // 4-byte general reply: 0xFF, Addr 1, Alarms 0x05, Checksum 0x06
    const std::vector<std::uint8_t> frame { 0xFFU, 0x01U, 0x05U, 0x06U };
    std::uint8_t addr { 0U };
    std::uint8_t alarms { 0U };

    ASSERT_TRUE(PelcoD::ProtocolParser::parseGeneral(frame, addr, alarms));
    EXPECT_EQ(addr, 1U);
    EXPECT_EQ(alarms, 0x05U);

    PelcoD::DeviceStatus status;
    PelcoD::DeviceInfo info;
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(frame, status, info));
    EXPECT_EQ(status.address, 1U);
    EXPECT_EQ(status.alarms, 0x05U);
    EXPECT_TRUE(status.isAlarmActive(1));
    EXPECT_FALSE(status.isAlarmActive(2));
    EXPECT_TRUE(status.isAlarmActive(3));
}

/// @brief Verify decoding of 7-byte extended telemetry responses for pan, tilt, zoom, and devtype.
/// @details Checks parsePan (0x59), parseTilt (0x5B), parseZoom (0x5D), and parseDevType (0x6D),
///          along with DeviceStatus telemetry updates.
TEST(ProtocolParserTest, ParseExtended)
{
    // Pan response (Opcode 0x59): Pan = 123.45 deg = 12345 = 0x3039
    const auto panFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x30U, 0x39U);
    std::uint16_t pan { 0U };
    ASSERT_TRUE(PelcoD::ProtocolParser::parsePan(panFrame, pan));
    EXPECT_EQ(pan, 12345U);

    // Tilt response (Opcode 0x5B): Tilt = 45.67 deg = 4567 = 0x11D7
    const auto tiltFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x11U, 0xD7U);
    std::uint16_t tilt { 0U };
    ASSERT_TRUE(PelcoD::ProtocolParser::parseTilt(tiltFrame, tilt));
    EXPECT_EQ(tilt, 4567U);

    // Zoom response (Opcode 0x5D): Zoom = 1200 = 0x04B0
    const auto zoomFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5DU, 0x04U, 0xB0U);
    std::uint16_t zoom { 0U };
    ASSERT_TRUE(PelcoD::ProtocolParser::parseZoom(zoomFrame, zoom));
    EXPECT_EQ(zoom, 1200U);

    // Device Type (Opcode 0x6D): SW = 0x05, HW = 0x02
    const auto devFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x6DU, 0x05U, 0x02U);
    std::uint8_t sw { 0U };
    std::uint8_t hw { 0U };
    ASSERT_TRUE(PelcoD::ProtocolParser::parseDevType(devFrame, sw, hw));
    EXPECT_EQ(sw, 0x05U);
    EXPECT_EQ(hw, 0x02U);

    // Update Status with Pan and Tilt
    PelcoD::DeviceStatus status;
    PelcoD::DeviceInfo info;
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(panFrame, status, info));
    EXPECT_EQ(status.panCentidegrees, 12345U);
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(tiltFrame, status, info));
    EXPECT_EQ(status.tiltCentidegrees, 4567U);
}

/// @brief Verify decoding of 18-byte extended query replies and device model identification.
/// @details Checks parsing ASCII payloads, trimming trailing null characters, and updating DeviceInfo.
TEST(ProtocolParserTest, ParseQuery)
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
    ASSERT_TRUE(PelcoD::ProtocolParser::parseQuery(qFrame, payload));
    EXPECT_EQ(payload, "PELCO");

    PelcoD::DeviceStatus status;
    PelcoD::DeviceInfo info;
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(qFrame, status, info));
    EXPECT_EQ(info.modelName, "PELCO");

    // Corrupted checksum must be rejected by updateStatus
    auto badQFrame = qFrame;
    badQFrame[17] ^= 0x55U;
    EXPECT_FALSE(PelcoD::ProtocolParser::updateStatus(badQFrame, status, info));
}

/// @brief Verify human-readable description strings generated for transmit and receive frames.
/// @details Validates describeFrame on empty frames, 4-byte general replies, 18-byte query replies,
///          standard PTZ motion frames, extended commands, and telemetry responses.
TEST(ProtocolParserTest, DescribeFrame)
{
    using PelcoD::PelcoDFrame;
    using PelcoD::ProtocolParser;

    // 1. Empty and fragments
    EXPECT_EQ(ProtocolParser::describeFrame(true, {}), "Empty");
    EXPECT_EQ(ProtocolParser::describeFrame(false, {}), "Empty");
    EXPECT_EQ(ProtocolParser::describeFrame(false, { 0xFFU, 0x01U }), "Raw Frame (2 bytes)");

    // 2. 4-byte general response
    const std::vector<std::uint8_t> genFrame { 0xFFU, 0x02U, 0x0AU, 0x0CU };
    EXPECT_EQ(ProtocolParser::describeFrame(false, genFrame), "General Response (Addr 2, Alarms: 0x0A)");

    // 3. 18-byte query response
    std::vector<std::uint8_t> qFrame(18U, 0x00U);
    qFrame[0] = 0xFFU;
    qFrame[1] = 0x03U;
    qFrame[2] = 'S';
    qFrame[3] = 'O';
    qFrame[4] = 'N';
    qFrame[5] = 'Y';
    qFrame[17] = PelcoDFrame::calculateChecksum(&qFrame[1], 16U);
    EXPECT_EQ(ProtocolParser::describeFrame(false, qFrame), "Query Response (Addr 3): \"SONY\"");

    // 4. 7-byte TX PTZ commands
    const auto stopFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x00U, 0x00U, 0x00U);
    EXPECT_EQ(ProtocolParser::describeFrame(true, stopFrame), "PTZ Stop");

    const auto moveFrame = PelcoDFrame::createFrame(0x01U, 0x01U, 0x02U | 0x08U | 0x20U, 0x20U, 0x15U);
    // cmd2 has Right(spd 32), Up(spd 21), ZoomTele, cmd1 has FocusNear
    EXPECT_EQ(ProtocolParser::describeFrame(true, moveFrame), "PTZ: Right(spd 32), Up(spd 21), ZoomTele, FocusNear");

    // 5. 7-byte TX Extended commands
    const auto presetFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x07U, 0x00U, 0x05U);
    EXPECT_EQ(ProtocolParser::describeFrame(true, presetFrame), "GoTo Preset 5");

    const auto auxFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x09U, 0x00U, 0x02U);
    EXPECT_EQ(ProtocolParser::describeFrame(true, auxFrame), "Set Aux 2 ON");

    const auto qPanFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x51U, 0x00U, 0x00U);
    EXPECT_EQ(ProtocolParser::describeFrame(true, qPanFrame), "Query Pan Position");

    // 6. 7-byte RX Responses
    const auto ackFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x01U, 0x51U, 0x01U);
    EXPECT_EQ(ProtocolParser::describeFrame(false, ackFrame), "ACK (OK) for Opcode 0x51");

    const auto nakFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x01U, 0x51U, 0x00U);
    EXPECT_EQ(ProtocolParser::describeFrame(false, nakFrame), "NAK (Error) for Opcode 0x51");

    const auto panFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x30U, 0x39U);
    EXPECT_EQ(ProtocolParser::describeFrame(false, panFrame), "Pan Response: 123.45°");

    const auto tiltFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x11U, 0xD7U);
    EXPECT_EQ(ProtocolParser::describeFrame(false, tiltFrame), "Tilt Response: 45.67°");

    const auto zoomFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x5DU, 0x04U, 0xB0U);
    EXPECT_EQ(ProtocolParser::describeFrame(false, zoomFrame), "Zoom Response: 1200");

    const auto magFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x63U, 0x00U, 0x0AU);
    EXPECT_EQ(ProtocolParser::describeFrame(false, magFrame), "Magnification Response: 10");

    const auto devFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x6DU, 0x05U, 0x02U);
    EXPECT_EQ(ProtocolParser::describeFrame(false, devFrame), "Device Type Response: SW=0x05 HW=0x02");

    const auto diagFrame = PelcoDFrame::createFrame(0x01U, 0x00U, 0x71U, 0x23U, 0x01U);
    EXPECT_EQ(ProtocolParser::describeFrame(false, diagFrame), "Diagnostics Response: Temp=35°C Sensor=0x01");
}

/// @brief Verify response categorization into General, StandardExtendedAckNak, ExtendedTelemetry, or QueryReply.
/// @details Checks classifyResponse on all supported packet types and unknown/corrupted packets.
TEST(ProtocolParserTest, ResponseClassification)
{
    // General 4-byte response
    const std::vector<std::uint8_t> genFrame { 0xFFU, 0x01U, 0x00U, 0x01U };
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse(genFrame), PelcoD::ResponseClassification::General);

    // Standard Extended ACK/NAK (opcode 0x01)
    const auto ackFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x01U, 0x51U, 0x01U);
    EXPECT_EQ(
        PelcoD::ProtocolParser::classifyResponse(ackFrame), PelcoD::ResponseClassification::StandardExtendedAckNak);

    // Extended Telemetry (0x59, 0x5B, 0x5D, 0x63, 0x6D, 0x71)
    const auto panFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x10U, 0x20U);
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse(panFrame), PelcoD::ResponseClassification::ExtendedTelemetry);

    const auto tiltFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x10U, 0x20U);
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse(tiltFrame), PelcoD::ResponseClassification::ExtendedTelemetry);

    const auto zoomFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5DU, 0x10U, 0x20U);
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse(zoomFrame), PelcoD::ResponseClassification::ExtendedTelemetry);

    const auto magFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x63U, 0x10U, 0x20U);
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse(magFrame), PelcoD::ResponseClassification::ExtendedTelemetry);

    const auto devFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x6DU, 0x10U, 0x20U);
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse(devFrame), PelcoD::ResponseClassification::ExtendedTelemetry);

    const auto diagFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x71U, 0x10U, 0x20U);
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse(diagFrame), PelcoD::ResponseClassification::ExtendedTelemetry);

    // 18-byte Query Response
    std::vector<std::uint8_t> qFrame(18U, 0x00U);
    qFrame[0] = 0xFFU;
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse(qFrame), PelcoD::ResponseClassification::QueryReply);

    // Unknown cases
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse({}), PelcoD::ResponseClassification::Unknown);
    const std::vector<std::uint8_t> badSync { 0xFEU, 0x01U, 0x00U, 0x01U };
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse(badSync), PelcoD::ResponseClassification::Unknown);

    const auto unknownOp = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x99U, 0x00U, 0x00U);
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse(unknownOp), PelcoD::ResponseClassification::Unknown);
}

/// @brief Verify response correlation logic linking incoming response packets to outstanding query tags.
/// @details Tests matching logic across QueryPan, QueryTilt, QueryZoom, QueryMagnification,
///          QueryDeviceType, QueryDiagnostics, QueryGeneral, and generic fallbacks.
TEST(ProtocolParserTest, QueryMatching)
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
    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryPan", panFrame));
    EXPECT_FALSE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryPan", tiltFrame));

    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryTilt", tiltFrame));
    EXPECT_FALSE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryTilt", panFrame));

    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryZoom", zoomFrame));
    EXPECT_FALSE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryZoom", panFrame));

    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryMagnification", magFrame));
    EXPECT_FALSE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryMagnification", zoomFrame));

    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryDeviceType", devFrame));
    EXPECT_FALSE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryDeviceType", magFrame));

    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryDiagnostics", diagFrame));
    EXPECT_FALSE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryDiagnostics", devFrame));

    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryGeneral", q18Frame));
    EXPECT_FALSE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryGeneral", panFrame));

    // Fallback heuristic for generic queries
    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("GenericQuery", panFrame));
    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("GenericQuery", tiltFrame));
    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("GenericQuery", q18Frame));

    // Non-matching frames and empty safety
    const auto nonTelemetry = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x03U, 0x00U, 0x01U);
    EXPECT_FALSE(PelcoD::ProtocolParser::isResponseMatchingQuery("GenericQuery", nonTelemetry));
    EXPECT_FALSE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryPan", {}));
}

/// @brief Verify parser rejection of corrupted frames and invalid structures.
/// @details Ensures updateStatus, parsePan, parseTilt, parseZoom, and parseQuery safely fail
///          when presented with bad checksums, mismatched opcodes, or corrupted sync bytes.
TEST(ProtocolParserTest, ParseCorruptedFrames)
{
    // Valid 7-byte pan frame corrupted by checksum: updateStatus must reject it
    auto corruptedPan = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x10U, 0x20U);
    corruptedPan[6] ^= 0xAAU;

    PelcoD::DeviceStatus status;
    PelcoD::DeviceInfo info;
    EXPECT_FALSE(PelcoD::ProtocolParser::updateStatus(corruptedPan, status, info));

    // parsePan fails if sync byte is wrong
    corruptedPan[0] = 0xAAU;
    std::uint16_t panVal { 0U };
    EXPECT_FALSE(PelcoD::ProtocolParser::parsePan(corruptedPan, panVal));

    // parsePan fails if opcode is not QueryPan (e.g. Tilt response 0x5B)
    const auto tiltFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x10U, 0x20U);
    EXPECT_FALSE(PelcoD::ProtocolParser::parsePan(tiltFrame, panVal));

    // Truncated pan frame fails parsePan
    const std::vector<std::uint8_t> truncPan { corruptedPan.begin(), corruptedPan.begin() + 5 };
    EXPECT_FALSE(PelcoD::ProtocolParser::parsePan(truncPan, panVal));

    // Corrupted 4-byte general frame: updateStatus must reject it due to checksum
    const std::vector<std::uint8_t> corrupted4 { 0xFFU, 0x01U, 0x05U, 0x99U };
    EXPECT_FALSE(PelcoD::ProtocolParser::updateStatus(corrupted4, status, info));

    // Non-sync byte header in query frame
    std::vector<std::uint8_t> badSync18(18U, 0x00U);
    badSync18[0] = 0xEEU;
    std::string qPayload;
    EXPECT_FALSE(PelcoD::ProtocolParser::parseQuery(badSync18, qPayload));
}

/// @brief Verify describeFrame outputs for motion directions and various camera commands.
/// @details Validates descriptive text for Left, Right, Up, Down, Focus Far, Iris Open/Close,
///          and unknown opcodes.
TEST(ProtocolParserTest, DescribeFrameComprehensive)
{
    using PelcoD::PelcoDFrame;
    using PelcoD::ProtocolParser;

    // Pan Left
    const auto panLeft = PelcoDFrame::createFrame(1U, 0x00U, 0x04U, 0x1EU, 0x00U);
    EXPECT_EQ(ProtocolParser::describeFrame(true, panLeft), "PTZ: Left(spd 30)");

    // Tilt Down
    const auto tiltDown = PelcoDFrame::createFrame(1U, 0x00U, 0x10U, 0x00U, 0x14U);
    EXPECT_EQ(ProtocolParser::describeFrame(true, tiltDown), "PTZ: Down(spd 20)");

    // Focus Far (cmd2 bit 7 = 0x80) and Iris Open (cmd1 bit 3 = 0x08)
    const auto focusIris = PelcoDFrame::createFrame(1U, 0x08U, 0x80U, 0x00U, 0x00U);
    EXPECT_EQ(ProtocolParser::describeFrame(true, focusIris), "PTZ: FocusFar, IrisOpen");

    // Iris Close (cmd1 bit 2 = 0x04)
    const auto irisClose = PelcoDFrame::createFrame(1U, 0x04U, 0x00U, 0x00U, 0x00U);
    EXPECT_EQ(ProtocolParser::describeFrame(true, irisClose), "PTZ: IrisClose");

    // Extended command with unrecognized odd opcode (e.g. 0xEF)
    const auto unknownExtCmd = PelcoDFrame::createFrame(1U, 0x00U, 0xEFU, 0x00U, 0x00U);
    EXPECT_EQ(ProtocolParser::describeFrame(true, unknownExtCmd), "Extended Command (Cmd2=0xEF)");
}

/// @brief Verify comprehensive state and device info updates across status fields.
/// @details Validates updating magnification, diagnostic telemetry, and alarms in DeviceStatus.
TEST(ProtocolParserTest, UpdateStatusComprehensive)
{
    PelcoD::DeviceStatus status;
    PelcoD::DeviceInfo info;

    // Magnification update (0x63)
    const auto magFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x63U, 0x00U, 0x20U);
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(magFrame, status, info));
    EXPECT_EQ(status.magnification, 0x0020U);

    // Diagnostics update (0x71): temp 45°C, sensor 2
    const auto diagFrame = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x71U, 45U, 2U);
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(diagFrame, status, info));
    EXPECT_EQ(status.diagnosticTemp, 45U);
    EXPECT_EQ(status.diagnosticSensorId, 2U);

    // Alarms 1 through 8 in general response
    const std::vector<std::uint8_t> allAlarms { 0xFFU, 0x01U, 0xFFU, 0x00U }; // csum (1 + 255) % 256 = 0
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(allAlarms, status, info));
    EXPECT_EQ(status.alarms, 0xFFU);
    for (std::uint8_t i = 1U; i <= 8U; ++i) {
        EXPECT_TRUE(status.isAlarmActive(i));
    }
}

} // namespace
