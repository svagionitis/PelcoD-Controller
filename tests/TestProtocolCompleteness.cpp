/// @file TestProtocolCompleteness.cpp
/// @brief TDD regression tests for protocol bug fixes and missing opcode coverage.
///        All tests must FAIL before the fix is applied, then PASS after.

#include "DeviceStatus.h"
#include "PelcoDFrame.h"
#include "PelcoDTypes.h"
#include "ProtocolBuilder.h"
#include "ProtocolParser.h"
#include "TestHelpers.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
// Bug 1: QueryMagnification response (0x63) silently discarded
// ---------------------------------------------------------------------------
static void testMagnificationResponseWired()
{
    // Construct a valid 7-byte 0x63 Magnification response frame.
    // Frame: FF addr 00 0x63 msb lsb csum
    const std::uint8_t addr { 0x01U };
    const std::uint16_t magValue { 0x00F0U }; // raw magnification
    const std::uint8_t msb = static_cast<std::uint8_t>(magValue >> 8U);
    const std::uint8_t lsb = static_cast<std::uint8_t>(magValue & 0xFFU);
    const std::uint8_t opcByte = static_cast<std::uint8_t>(PelcoD::ResponseOpcode::QueryMagnification);
    const std::uint8_t csum = static_cast<std::uint8_t>((addr + 0x00U + opcByte + msb + lsb) & 0xFFU);
    const std::vector<std::uint8_t> frame { 0xFFU, addr, 0x00U, opcByte, msb, lsb, csum };

    PelcoD::DeviceStatus status {};
    PelcoD::DeviceInfo info {};
    const bool ok = PelcoD::ProtocolParser::updateStatus(frame, status, info);

    assert(ok && "updateStatus must return true for valid 0x63 response");
    assert(status.magnification == magValue && "magnification must be populated from 0x63 response");

    std::cout << "  testMagnificationResponseWired: magnification=" << status.magnification << " — PASSED\n";
}

// ---------------------------------------------------------------------------
// Bug 2: buildSetZeroPosition must emit opcode 0x49, not GoToPreset
// ---------------------------------------------------------------------------
static void testSetZeroPositionOpcode()
{
    const auto frame = PelcoD::ProtocolBuilder::buildSetZeroPosition(0x01U);
    assert(frame.size() == 7U && "frame must be 7 bytes");
    // Byte [3] is cmd2 — must be 0x49
    assert(frame[3] == 0x49U && "buildSetZeroPosition must emit opcode 0x49");
    // Must NOT be GoToPreset (0x07) with data2=0x22
    assert(!(frame[3] == 0x07U && frame[5] == 0x22U) && "must not alias GoToPreset 0x22");

    std::cout << "  testSetZeroPositionOpcode: opcode=0x" << std::hex << static_cast<int>(frame[3]) << std::dec
              << " — PASSED\n";
}

// ---------------------------------------------------------------------------
// Bug 3: Enum values for Download opcodes
// ---------------------------------------------------------------------------
static void testDownloadEnumValues()
{
    assert(static_cast<std::uint8_t>(PelcoD::CommandOpcode::PrepareForDownload) == 0x57U
        && "PrepareForDownload must be 0x57");
    assert(static_cast<std::uint8_t>(PelcoD::CommandOpcode::StartDownload) == 0x69U && "StartDownload must be 0x69");

    std::cout << "  testDownloadEnumValues — PASSED\n";
}

// ---------------------------------------------------------------------------
// ACK/NAK: Standard Extended Response (0x01) must be parsed
// ---------------------------------------------------------------------------
static void testAckNakParsed()
{
    // ACK frame: FF addr 00 0x01 opcode 0x00 csum  (resp_type=0x01 ACK)
    const std::uint8_t addr { 0x01U };
    const std::uint8_t opcByte = static_cast<std::uint8_t>(PelcoD::ResponseOpcode::StandardExtended);
    const std::uint8_t echoOpcode { 0x03U }; // echoes SetPreset opcode
    const std::uint8_t ackByte { 0x01U }; // ACK
    const std::uint8_t csum = static_cast<std::uint8_t>((addr + 0x00U + opcByte + echoOpcode + ackByte) & 0xFFU);
    const std::vector<std::uint8_t> frame { 0xFFU, addr, 0x00U, opcByte, echoOpcode, ackByte, csum };

    PelcoD::DeviceStatus status {};
    PelcoD::DeviceInfo info {};
    const bool ok = PelcoD::ProtocolParser::updateStatus(frame, status, info);

    assert(ok && "updateStatus must return true for valid 0x01 ACK response");
    assert(status.lastAckOk && "lastAckOk must be true for ACK frame");
    assert(status.lastAckOpcode == echoOpcode && "lastAckOpcode must reflect echoed opcode");

    std::cout << "  testAckNakParsed — PASSED\n";
}

// ---------------------------------------------------------------------------
// New builders: verify opcode byte in position [3]
// ---------------------------------------------------------------------------
static void testNewBuilderOpcodes()
{
    struct Case {
        std::vector<std::uint8_t> frame;
        std::uint8_t expectedOpcode;
        const char* name;
    };

    const std::vector<Case> cases {
        { PelcoD::ProtocolBuilder::buildPhaseDelayMode(1U, PelcoD::SwitchState::On), 0x35U, "PhaseDelayMode" },
        { PelcoD::ProtocolBuilder::buildLineLockDelay(1U, 0x0010U), 0x39U, "LineLockDelay" },
        { PelcoD::ProtocolBuilder::buildWhiteBalanceRB(1U, 0x0010U), 0x3BU, "WhiteBalanceRB" },
        { PelcoD::ProtocolBuilder::buildWhiteBalanceMG(1U, 0x0010U), 0x3DU, "WhiteBalanceMG" },
        { PelcoD::ProtocolBuilder::buildSetMagnification(1U, 0x0100U, false), 0x5FU, "SetMagnification" },
        { PelcoD::ProtocolBuilder::buildSetBaudRate(1U, 9600U), 0x67U, "SetBaudRate" },
        { PelcoD::ProtocolBuilder::buildSetZeroPosition(1U), 0x49U, "SetZeroPosition" },
        { PelcoD::ProtocolBuilder::buildQueryDiagnostics(1U), 0x6FU, "QueryDiagnostics" },
    };

    for (const auto& c : cases) {
        assert(c.frame.size() == 7U && "all frames must be 7 bytes");
        assert(c.frame[3] == c.expectedOpcode && "opcode byte must match spec");
        std::cout << "  testNewBuilderOpcodes [" << c.name << "]: opcode=0x" << std::hex << static_cast<int>(c.frame[3])
                  << std::dec << " — PASSED\n";
    }
}

// ---------------------------------------------------------------------------
// Diagnostics response (0x71) must be parsed
// ---------------------------------------------------------------------------
static void testDiagnosticsResponseParsed()
{
    // 7-byte frame: FF addr 00 0x71 temp sensorId csum
    const std::uint8_t addr { 0x01U };
    const std::uint8_t opcByte = static_cast<std::uint8_t>(PelcoD::ResponseOpcode::QueryDiagnostics);
    const std::uint8_t temp { 0x2AU };
    const std::uint8_t sensorId { 0x05U };
    const std::uint8_t csum = static_cast<std::uint8_t>((addr + 0x00U + opcByte + temp + sensorId) & 0xFFU);
    const std::vector<std::uint8_t> frame { 0xFFU, addr, 0x00U, opcByte, temp, sensorId, csum };

    PelcoD::DeviceStatus status {};
    PelcoD::DeviceInfo info {};
    const bool ok = PelcoD::ProtocolParser::updateStatus(frame, status, info);

    assert(ok && "updateStatus must return true for valid 0x71 diagnostics response");
    assert(status.diagnosticTemp == temp && "diagnosticTemp must be populated");
    assert(status.diagnosticSensorId == sensorId && "diagnosticSensorId must be populated");

    std::cout << "  testDiagnosticsResponseParsed — PASSED\n";
}

int main()
{
    PelcoDTest::initTestHarness();
    std::cout << "[TestProtocolCompleteness] Running...\n";
    testMagnificationResponseWired();
    testSetZeroPositionOpcode();
    testDownloadEnumValues();
    testAckNakParsed();
    testNewBuilderOpcodes();
    testDiagnosticsResponseParsed();
    std::cout << "[TestProtocolCompleteness] All tests passed.\n";
    return 0;
}
