/// @file TestProtocolCompleteness.cpp
/// @brief TDD regression tests for protocol bug fixes, opcode completeness, and enum validity.

#include "DeviceStatus.h"
#include "PelcoDFrame.h"
#include "PelcoDTypes.h"
#include "PelcoDDevice.h"
#include "MockPelcoDDevice.h"
#include "ProtocolBuilder.h"
#include "ProtocolParser.h"
#include "TestHelpers.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

/// @brief Verify parsing and storage of QueryMagnification response packets (opcode 0x63).
/// @details Ensures that 0x63 responses are recognized by updateStatus and correctly populate
///          the magnification field in DeviceStatus.
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

/// @brief Verify buildSetZeroPosition emits correct dedicated opcode 0x49.
/// @details Ensures buildSetZeroPosition does not mistakenly alias to GoToPreset (0x07) with preset 0x22.
TEST(ProtocolCompletenessTest, SetZeroPositionOpcode)
{
    const auto frame = PelcoD::ProtocolBuilder::buildSetZeroPosition(0x01U);
    ASSERT_EQ(frame.size(), 7U);
    // Byte [3] is cmd2 — must be 0x49
    EXPECT_EQ(frame[3], 0x49U);
    // Must NOT be GoToPreset (0x07) with data2=0x22
    EXPECT_FALSE(frame[3] == 0x07U && frame[5] == 0x22U);
}

/// @brief Verify enum opcode values for firmware download commands.
/// @details Validates PrepareForDownload (0x57) and StartDownload (0x69) enum values.
TEST(ProtocolCompletenessTest, DownloadEnumValues)
{
    EXPECT_EQ(static_cast<std::uint8_t>(PelcoD::CommandOpcode::PrepareForDownload), 0x57U);
    EXPECT_EQ(static_cast<std::uint8_t>(PelcoD::CommandOpcode::StartDownload), 0x69U);
}

/// @brief Verify parsing and status tracking of Standard Extended ACK/NAK frames (opcode 0x01).
/// @details Tests that opcode 0x01 updates lastAckOk and lastAckOpcode correctly in DeviceStatus.
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

/// @brief Verify newly introduced ProtocolBuilder opcodes match specification values.
/// @details Tests PhaseDelayMode, LineLockDelay, WhiteBalanceRB, WhiteBalanceMG, SetMagnification,
///          SetBaudRate, SetZeroPosition, and QueryDiagnostics frames for byte 3 opcode matching.
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

/// @brief Verify diagnostics response telemetry parsing and status population.
/// @details Validates parsing 0x71 diagnostics frames and populating diagnosticTemp and diagnosticSensorId.
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

/// @brief Verify protocol enums have expected integer values and distinct enumerators.
/// @details Validates PanDirection, TiltDirection, ZoomAction, FocusAction, IrisAction, AutoMode, and SwitchState.
TEST(ProtocolCompletenessTest, EnumConversionsAndRanges)
{
    // PanDirection
    EXPECT_NE(PelcoD::PanDirection::Stop, PelcoD::PanDirection::Left);
    EXPECT_NE(PelcoD::PanDirection::Left, PelcoD::PanDirection::Right);

    // TiltDirection
    EXPECT_NE(PelcoD::TiltDirection::Stop, PelcoD::TiltDirection::Up);
    EXPECT_NE(PelcoD::TiltDirection::Up, PelcoD::TiltDirection::Down);

    // ZoomAction
    EXPECT_NE(PelcoD::ZoomAction::Stop, PelcoD::ZoomAction::Tele);
    EXPECT_NE(PelcoD::ZoomAction::Tele, PelcoD::ZoomAction::Wide);

    // FocusAction
    EXPECT_NE(PelcoD::FocusAction::Stop, PelcoD::FocusAction::Far);
    EXPECT_NE(PelcoD::FocusAction::Far, PelcoD::FocusAction::Near);

    // IrisAction
    EXPECT_NE(PelcoD::IrisAction::Stop, PelcoD::IrisAction::Open);
    EXPECT_NE(PelcoD::IrisAction::Open, PelcoD::IrisAction::Close);

    // AutoMode
    EXPECT_NE(PelcoD::AutoMode::Off, PelcoD::AutoMode::On);
    EXPECT_NE(PelcoD::AutoMode::On, PelcoD::AutoMode::Auto);

    // SwitchState
    EXPECT_NE(PelcoD::SwitchState::Off, PelcoD::SwitchState::On);
}

/// @brief Verify DeviceStatus alarm bit checking and degree calculation conversions.
/// @details Tests isAlarmActive across bits 1 to 8, boundary conditions (index 0, index 9),
///          and panDegrees / tiltDegrees floating point transformations.
TEST(ProtocolCompletenessTest, DeviceStatusAlarmBits)
{
    PelcoD::DeviceStatus status {};

    // No alarms initially
    for (std::uint8_t i = 1U; i <= 8U; ++i) {
        EXPECT_FALSE(status.isAlarmActive(i));
    }

    // Out-of-bounds alarm queries
    EXPECT_FALSE(status.isAlarmActive(0U));
    EXPECT_FALSE(status.isAlarmActive(9U));
    EXPECT_FALSE(status.isAlarmActive(255U));

    // Test alarm bits 1, 4, 8
    status.alarms = 0x89U; // 1000 1001 binary -> Alarms 1, 4, 8
    EXPECT_TRUE(status.isAlarmActive(1U));
    EXPECT_FALSE(status.isAlarmActive(2U));
    EXPECT_FALSE(status.isAlarmActive(3U));
    EXPECT_TRUE(status.isAlarmActive(4U));
    EXPECT_FALSE(status.isAlarmActive(5U));
    EXPECT_FALSE(status.isAlarmActive(6U));
    EXPECT_FALSE(status.isAlarmActive(7U));
    EXPECT_TRUE(status.isAlarmActive(8U));

    // Degree conversions
    status.panCentidegrees = 35999U;
    EXPECT_DOUBLE_EQ(status.panDegrees(), 359.99);

    status.tiltCentidegrees = 1234U;
    EXPECT_DOUBLE_EQ(status.tiltDegrees(), 12.34);
}

/// @brief Verify describeFrame correctly parses IrisOpen and IrisClose actions.
/// @details TDD regression test exposing bug where IrisOpen bit was tested with 0x08 instead of 0x02.
TEST(ProtocolCompletenessTest, DescribeFrameIrisOpenAndClose)
{
    const auto openFrame = PelcoD::ProtocolBuilder::buildIris(1U, PelcoD::IrisAction::Open);
    const auto closeFrame = PelcoD::ProtocolBuilder::buildIris(1U, PelcoD::IrisAction::Close);

    const std::string descOpen = PelcoD::ProtocolParser::describeFrame(true, openFrame);
    const std::string descClose = PelcoD::ProtocolParser::describeFrame(true, closeFrame);

    EXPECT_NE(descOpen.find("IrisOpen"), std::string::npos) << "Got: " << descOpen;
    EXPECT_NE(descClose.find("IrisClose"), std::string::npos) << "Got: " << descClose;
}

/// @brief Verify builder and disassembly for Phase 1 extended commands: PresetScan, Download, EchoMode.
TEST(ProtocolCompletenessTest, Phase1BuildersAndDisassembly)
{
    const std::uint8_t addr { 0x02U };

    // 1. PresetScan (opcode 0x47, data2 = dwell)
    const auto scanFrame = PelcoD::ProtocolBuilder::buildPresetScan(addr, 5U);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(scanFrame));
    EXPECT_EQ(scanFrame[1], addr);
    EXPECT_EQ(scanFrame[2], 0x00U);
    EXPECT_EQ(scanFrame[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::PresetScan));
    EXPECT_EQ(scanFrame[4], 0x00U);
    EXPECT_EQ(scanFrame[5], 5U);
    const std::string descScan = PelcoD::ProtocolParser::describeFrame(true, scanFrame);
    EXPECT_NE(descScan.find("Preset Scan"), std::string::npos) << "Got: " << descScan;
    EXPECT_NE(descScan.find("5s"), std::string::npos) << "Got: " << descScan;

    // 2. PrepareForDownload (opcode 0x57)
    const auto prepFrame = PelcoD::ProtocolBuilder::buildPrepareForDownload(addr);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(prepFrame));
    EXPECT_EQ(prepFrame[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::PrepareForDownload));
    const std::string descPrep = PelcoD::ProtocolParser::describeFrame(true, prepFrame);
    EXPECT_NE(descPrep.find("Prepare For Download"), std::string::npos) << "Got: " << descPrep;

    // 3. StartDownload (opcode 0x69)
    const auto startFrame = PelcoD::ProtocolBuilder::buildStartDownload(addr);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(startFrame));
    EXPECT_EQ(startFrame[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::StartDownload));
    const std::string descStart = PelcoD::ProtocolParser::describeFrame(true, startFrame);
    EXPECT_NE(descStart.find("Start Download"), std::string::npos) << "Got: " << descStart;

    // 4. EchoMode (opcode 0x65)
    const auto echoFrame = PelcoD::ProtocolBuilder::buildEchoMode(addr);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(echoFrame));
    EXPECT_EQ(echoFrame[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::EchoMode));
    const std::string descEcho = PelcoD::ProtocolParser::describeFrame(true, echoFrame);
    EXPECT_NE(descEcho.find("Activate Echo Mode"), std::string::npos) << "Got: " << descEcho;

    // 5. OSD WriteChar and ClearScreen disassembly
    const auto writeCharFrame = PelcoD::ProtocolBuilder::buildWriteChar(addr, 10U, 'A');
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(writeCharFrame));
    const std::string descChar = PelcoD::ProtocolParser::describeFrame(true, writeCharFrame);
    EXPECT_NE(descChar.find("Write Char 'A' at Col 10"), std::string::npos) << "Got: " << descChar;

    const auto clearScreenFrame = PelcoD::ProtocolBuilder::buildClearScreen(addr);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(clearScreenFrame));
    const std::string descClear = PelcoD::ProtocolParser::describeFrame(true, clearScreenFrame);
    EXPECT_NE(descClear.find("Clear Screen"), std::string::npos) << "Got: " << descClear;
}

/// @brief Verify PelcoDDevice high-level wrappers for Phase 1 commands.
TEST(ProtocolCompletenessTest, PelcoDDevicePhase1Wrappers)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::vector<std::vector<std::uint8_t>> sentFrames;
    std::mutex mtx;
    device.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>& frame) {
        if (isTx) {
            std::scoped_lock lock(mtx);
            sentFrames.push_back(frame);
        }
    });

    ASSERT_TRUE(device.start());

    device.presetScan(10U);
    device.prepareForDownload();
    device.startDownload();
    device.activateEchoMode();
    device.writeCharacter(5U, 'X');
    device.clearScreen();

    // Allow queue to dispatch commands
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    device.stop();

    std::scoped_lock lock(mtx);
    ASSERT_GE(sentFrames.size(), 6U);

    // Verify opcodes of the sent frames
    EXPECT_EQ(sentFrames[0][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::PresetScan));
    EXPECT_EQ(sentFrames[0][5], 10U);
    EXPECT_EQ(sentFrames[1][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::PrepareForDownload));
    EXPECT_EQ(sentFrames[2][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::StartDownload));
    EXPECT_EQ(sentFrames[3][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::EchoMode));
    EXPECT_EQ(sentFrames[4][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::WriteCharacter));
    EXPECT_EQ(sentFrames[4][4], 5U);
    EXPECT_EQ(sentFrames[4][5], static_cast<std::uint8_t>('X'));
    EXPECT_EQ(sentFrames[5][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::ClearScreen));
}

/// @brief Verify builder and disassembly for Phase 2 standalone commands: Dummy (0x0D) and ScreenMove (0x79).
TEST(ProtocolCompletenessTest, Phase2BuildersAndDisassembly)
{
    const std::uint8_t addr { 0x03U };

    // 1. Dummy (opcode 0x0D)
    const auto dummyFrame = PelcoD::ProtocolBuilder::buildDummy(addr);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(dummyFrame));
    EXPECT_EQ(dummyFrame[1], addr);
    EXPECT_EQ(dummyFrame[2], 0x00U);
    EXPECT_EQ(dummyFrame[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::Dummy));
    EXPECT_EQ(dummyFrame[4], 0x00U);
    EXPECT_EQ(dummyFrame[5], 0x00U);
    const std::string descDummy = PelcoD::ProtocolParser::describeFrame(true, dummyFrame);
    EXPECT_NE(descDummy.find("Dummy / Ping"), std::string::npos) << "Got: " << descDummy;

    // 2. ScreenMove Absolute (opcode 0x79, cmd1=0x00)
    const auto screenAbs = PelcoD::ProtocolBuilder::buildScreenMove(addr, 50, -30, false);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(screenAbs));
    EXPECT_EQ(screenAbs[1], addr);
    EXPECT_EQ(screenAbs[2], 0x00U); // Abs
    EXPECT_EQ(screenAbs[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::ScreenMove));
    EXPECT_EQ(static_cast<std::int8_t>(screenAbs[4]), 50);
    EXPECT_EQ(static_cast<std::int8_t>(screenAbs[5]), -30);
    const std::string descAbs = PelcoD::ProtocolParser::describeFrame(true, screenAbs);
    EXPECT_NE(descAbs.find("Screen Move (Abs, Pan 50%, Tilt -30%)"), std::string::npos) << "Got: " << descAbs;

    // 3. ScreenMove Relative (opcode 0x79, cmd1=0x01)
    const auto screenRel = PelcoD::ProtocolBuilder::buildScreenMove(addr, -25, 40, true);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(screenRel));
    EXPECT_EQ(screenRel[1], addr);
    EXPECT_EQ(screenRel[2], 0x01U); // Rel
    EXPECT_EQ(screenRel[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::ScreenMove));
    EXPECT_EQ(static_cast<std::int8_t>(screenRel[4]), -25);
    EXPECT_EQ(static_cast<std::int8_t>(screenRel[5]), 40);
    const std::string descRel = PelcoD::ProtocolParser::describeFrame(true, screenRel);
    EXPECT_NE(descRel.find("Screen Move (Rel, Pan -25%, Tilt 40%)"), std::string::npos) << "Got: " << descRel;
}

/// @brief Verify PelcoDDevice high-level wrappers for Phase 2 commands.
TEST(ProtocolCompletenessTest, PelcoDDevicePhase2Wrappers)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::vector<std::vector<std::uint8_t>> sentFrames;
    std::mutex mtx;
    device.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>& frame) {
        if (isTx) {
            std::scoped_lock lock(mtx);
            sentFrames.push_back(frame);
        }
    });

    ASSERT_TRUE(device.start());

    device.sendDummy();
    device.screenMove(-50, 75, true);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    device.stop();

    std::scoped_lock lock(mtx);
    ASSERT_GE(sentFrames.size(), 2U);

    // Frame 0: Dummy (0x0D)
    EXPECT_EQ(sentFrames[0][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::Dummy));

    // Frame 1: Screen Move (0x79, cmd1=0x01 rel)
    EXPECT_EQ(sentFrames[1][2], 0x01U);
    EXPECT_EQ(sentFrames[1][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::ScreenMove));
    EXPECT_EQ(static_cast<std::int8_t>(sentFrames[1][4]), -50);
    EXPECT_EQ(static_cast<std::int8_t>(sentFrames[1][5]), 75);
}

} // namespace
