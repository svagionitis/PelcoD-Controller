/// @file TestProtocolCompleteness.cpp
/// @brief TDD regression tests for protocol bug fixes and missing opcode coverage.

#include "DeviceStatus.h"
#include "PelcoDFrame.h"
#include "PelcoDTypes.h"
#include "ProtocolBuilder.h"
#include "ProtocolParser.h"
#include "TestHelpers.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Bug 1: QueryMagnification response (0x63) silently discarded
// ---------------------------------------------------------------------------
TEST(ProtocolCompletenessTest, MagnificationResponseWired)
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

    EXPECT_TRUE(ok);
    EXPECT_EQ(status.magnification, magValue);
}

// ---------------------------------------------------------------------------
// Bug 2: buildSetZeroPosition must emit opcode 0x49, not GoToPreset
// ---------------------------------------------------------------------------
TEST(ProtocolCompletenessTest, SetZeroPositionOpcode)
{
    const auto frame = PelcoD::ProtocolBuilder::buildSetZeroPosition(0x01U);
    ASSERT_EQ(frame.size(), 7U);
    // Byte [3] is cmd2 — must be 0x49
    EXPECT_EQ(frame[3], 0x49U);
    // Must NOT be GoToPreset (0x07) with data2=0x22
    EXPECT_FALSE(frame[3] == 0x07U && frame[5] == 0x22U);
}

// ---------------------------------------------------------------------------
// Bug 3: Enum values for Download opcodes
// ---------------------------------------------------------------------------
TEST(ProtocolCompletenessTest, DownloadEnumValues)
{
    EXPECT_EQ(static_cast<std::uint8_t>(PelcoD::CommandOpcode::PrepareForDownload), 0x57U);
    EXPECT_EQ(static_cast<std::uint8_t>(PelcoD::CommandOpcode::StartDownload), 0x69U);
}

// ---------------------------------------------------------------------------
// ACK/NAK: Standard Extended Response (0x01) must be parsed
// ---------------------------------------------------------------------------
TEST(ProtocolCompletenessTest, AckNakParsed)
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

    EXPECT_TRUE(ok);
    EXPECT_TRUE(status.lastAckOk);
    EXPECT_EQ(status.lastAckOpcode, echoOpcode);
}

// ---------------------------------------------------------------------------
// New builders: verify opcode byte in position [3]
// ---------------------------------------------------------------------------
TEST(ProtocolCompletenessTest, NewBuilderOpcodes)
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
        ASSERT_EQ(c.frame.size(), 7U) << "Case: " << c.name;
        EXPECT_EQ(c.frame[3], c.expectedOpcode) << "Case: " << c.name;
    }
}

// ---------------------------------------------------------------------------
// Diagnostics response (0x71) must be parsed
// ---------------------------------------------------------------------------
TEST(ProtocolCompletenessTest, DiagnosticsResponseParsed)
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

    EXPECT_TRUE(ok);
    EXPECT_EQ(status.diagnosticTemp, temp);
    EXPECT_EQ(status.diagnosticSensorId, sensorId);
}

} // namespace
