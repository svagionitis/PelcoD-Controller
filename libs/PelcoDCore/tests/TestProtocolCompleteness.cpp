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
        { PelcoD::ProtocolBuilder::buildSetMagnification(1U, 0x0100U, false).value(), 0x5FU, "SetMagnification" },
        { PelcoD::ProtocolBuilder::buildSetBaudRate(1U, 9600U).value(), 0x67U, "SetBaudRate" },
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

/// @brief Verify builder and disassembly for Phase 3 commands: Version Info (0x73) and Time Macro (0x77).
TEST(ProtocolCompletenessTest, Phase3BuildersAndDisassembly)
{
    const std::uint8_t addr { 0x01U };

    // 1. Version Info Queries (0x73)
    const auto qSw = PelcoD::ProtocolBuilder::buildQuerySoftwareVersion(addr);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qSw));
    EXPECT_EQ(qSw[1], addr);
    EXPECT_EQ(qSw[2], static_cast<std::uint8_t>(PelcoD::VersionInfoSubOpcode::RequestSoftwareVersion));
    EXPECT_EQ(qSw[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::VersionInfo));
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(true, qSw).find("Query Software Version"), std::string::npos);

    const auto qBuild = PelcoD::ProtocolBuilder::buildQueryBuildNumber(addr);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qBuild));
    EXPECT_EQ(qBuild[1], addr);
    EXPECT_EQ(qBuild[2], static_cast<std::uint8_t>(PelcoD::VersionInfoSubOpcode::RequestBuildNumber));
    EXPECT_EQ(qBuild[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::VersionInfo));
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(true, qBuild).find("Query Build Number"), std::string::npos);

    // 2. Time Macro Sets (0x77)
    const auto setSec = PelcoD::ProtocolBuilder::buildSetSeconds(addr, 45U);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(setSec));
    EXPECT_EQ(setSec[2], static_cast<std::uint8_t>(PelcoD::TimeSubOpcode::SetSeconds));
    EXPECT_EQ(setSec[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::TimeMacro));
    EXPECT_EQ(setSec[5], 45U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(true, setSec).find("Set Seconds (45s)"), std::string::npos);

    const auto setHm = PelcoD::ProtocolBuilder::buildSetHourMinute(addr, 14U, 30U);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(setHm));
    EXPECT_EQ(setHm[2], static_cast<std::uint8_t>(PelcoD::TimeSubOpcode::SetHourMinute));
    EXPECT_EQ(setHm[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::TimeMacro));
    EXPECT_EQ(setHm[4], 14U);
    EXPECT_EQ(setHm[5], 30U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(true, setHm).find("Set Hour/Minute (14:30)"), std::string::npos);

    const auto setMd = PelcoD::ProtocolBuilder::buildSetMonthDay(addr, 9U, 28U);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(setMd));
    EXPECT_EQ(setMd[2], static_cast<std::uint8_t>(PelcoD::TimeSubOpcode::SetMonthDay));
    EXPECT_EQ(setMd[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::TimeMacro));
    EXPECT_EQ(setMd[4], 9U);
    EXPECT_EQ(setMd[5], 28U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(true, setMd).find("Set Month/Day (9/28)"), std::string::npos);

    const auto setYr = PelcoD::ProtocolBuilder::buildSetYear(addr, 2026U);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(setYr));
    EXPECT_EQ(setYr[2], static_cast<std::uint8_t>(PelcoD::TimeSubOpcode::SetYear));
    EXPECT_EQ(setYr[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::TimeMacro));
    EXPECT_EQ(setYr[4], static_cast<std::uint8_t>(2026U >> 8U));
    EXPECT_EQ(setYr[5], static_cast<std::uint8_t>(2026U & 0xFFU));
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(true, setYr).find("Set Year (2026)"), std::string::npos);

    // 3. Time Macro Queries (0x77 odd sub-opcodes)
    const auto qSec = PelcoD::ProtocolBuilder::buildQueryTime(addr, PelcoD::TimeSubOpcode::ReportSeconds);
    EXPECT_EQ(qSec[2], 0x01U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(true, qSec).find("Query Seconds"), std::string::npos);

    const auto qHm = PelcoD::ProtocolBuilder::buildQueryTime(addr, PelcoD::TimeSubOpcode::ReportHourMinute);
    EXPECT_EQ(qHm[2], 0x03U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(true, qHm).find("Query Hour/Minute"), std::string::npos);

    const auto qMd = PelcoD::ProtocolBuilder::buildQueryTime(addr, PelcoD::TimeSubOpcode::ReportMonthDay);
    EXPECT_EQ(qMd[2], 0x05U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(true, qMd).find("Query Month/Day"), std::string::npos);

    const auto qYr = PelcoD::ProtocolBuilder::buildQueryTime(addr, PelcoD::TimeSubOpcode::ReportYear);
    EXPECT_EQ(qYr[2], 0x07U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(true, qYr).find("Query Year"), std::string::npos);
}

/// @brief Verify Phase 3 response parsing, telemetry updates, and query matching.
TEST(ProtocolCompletenessTest, Phase3ResponsesAndParsing)
{
    const std::uint8_t addr { 0x01U };

    // 1. Software Version Response (0x73, sub 0x01): major=2, minor=5 -> "2.5"
    const auto swResp = PelcoD::PelcoDFrame::createFrame(addr,
        static_cast<std::uint8_t>(PelcoD::VersionInfoSubOpcode::SoftwareVersionResponse),
        static_cast<std::uint8_t>(PelcoD::ResponseOpcode::VersionInfo), 2U, 5U);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(swResp));
    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("QuerySoftwareVersion", swResp));
    EXPECT_FALSE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryBuildNumber", swResp));

    PelcoD::VersionInfoSubOpcode verSub {};
    std::uint8_t d1 { 0U };
    std::uint8_t d2 { 0U };
    ASSERT_TRUE(PelcoD::ProtocolParser::parseVersionInfo(swResp, verSub, d1, d2));
    EXPECT_EQ(verSub, PelcoD::VersionInfoSubOpcode::SoftwareVersionResponse);
    EXPECT_EQ(d1, 2U);
    EXPECT_EQ(d2, 5U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(false, swResp).find("Software Version Response: 2.5"), std::string::npos);

    PelcoD::DeviceStatus status {};
    PelcoD::DeviceInfo info {};
    EXPECT_TRUE(PelcoD::ProtocolParser::updateStatus(swResp, status, info));
    EXPECT_EQ(info.softwareMajor, 2U);
    EXPECT_EQ(info.softwareMinor, 5U);

    // 2. Build Number Response (0x73, sub 0x03): build 1234 (0x04D2)
    const auto buildResp = PelcoD::PelcoDFrame::createFrame(addr,
        static_cast<std::uint8_t>(PelcoD::VersionInfoSubOpcode::BuildNumberResponse),
        static_cast<std::uint8_t>(PelcoD::ResponseOpcode::VersionInfo), 0x04U, 0xD2U);
    ASSERT_TRUE(PelcoD::PelcoDFrame::isValidFrame(buildResp));
    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryBuildNumber", buildResp));
    EXPECT_FALSE(PelcoD::ProtocolParser::isResponseMatchingQuery("QuerySoftwareVersion", buildResp));

    ASSERT_TRUE(PelcoD::ProtocolParser::parseVersionInfo(buildResp, verSub, d1, d2));
    EXPECT_EQ(verSub, PelcoD::VersionInfoSubOpcode::BuildNumberResponse);
    EXPECT_EQ(d1, 0x04U);
    EXPECT_EQ(d2, 0xD2U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(false, buildResp).find("Build Number Response: 1234"), std::string::npos);

    EXPECT_TRUE(PelcoD::ProtocolParser::updateStatus(buildResp, status, info));
    EXPECT_EQ(info.buildNumber, 1234U);

    // 3. Time Responses (0x77, odd subs)
    PelcoD::TimeSubOpcode timeSub {};
    const auto timeSecResp = PelcoD::PelcoDFrame::createFrame(addr,
        static_cast<std::uint8_t>(PelcoD::TimeSubOpcode::ReportSeconds),
        static_cast<std::uint8_t>(PelcoD::ResponseOpcode::TimeMacro), 0x00U, 45U);
    ASSERT_TRUE(PelcoD::ProtocolParser::parseTimeResponse(timeSecResp, timeSub, d1, d2));
    EXPECT_EQ(timeSub, PelcoD::TimeSubOpcode::ReportSeconds);
    EXPECT_EQ(d2, 45U);
    EXPECT_TRUE(PelcoD::ProtocolParser::isResponseMatchingQuery("QueryTime", timeSecResp));
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(false, timeSecResp).find("Seconds Response: 45s"), std::string::npos);

    const auto timeHmResp = PelcoD::PelcoDFrame::createFrame(addr,
        static_cast<std::uint8_t>(PelcoD::TimeSubOpcode::ReportHourMinute),
        static_cast<std::uint8_t>(PelcoD::ResponseOpcode::TimeMacro), 14U, 30U);
    ASSERT_TRUE(PelcoD::ProtocolParser::parseTimeResponse(timeHmResp, timeSub, d1, d2));
    EXPECT_EQ(timeSub, PelcoD::TimeSubOpcode::ReportHourMinute);
    EXPECT_EQ(d1, 14U);
    EXPECT_EQ(d2, 30U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(false, timeHmResp).find("Hour/Minute Response: 14:30"), std::string::npos);

    const auto timeMdResp = PelcoD::PelcoDFrame::createFrame(addr,
        static_cast<std::uint8_t>(PelcoD::TimeSubOpcode::ReportMonthDay),
        static_cast<std::uint8_t>(PelcoD::ResponseOpcode::TimeMacro), 9U, 28U);
    ASSERT_TRUE(PelcoD::ProtocolParser::parseTimeResponse(timeMdResp, timeSub, d1, d2));
    EXPECT_EQ(timeSub, PelcoD::TimeSubOpcode::ReportMonthDay);
    EXPECT_EQ(d1, 9U);
    EXPECT_EQ(d2, 28U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(false, timeMdResp).find("Month/Day Response: 9/28"), std::string::npos);

    const auto timeYrResp = PelcoD::PelcoDFrame::createFrame(addr,
        static_cast<std::uint8_t>(PelcoD::TimeSubOpcode::ReportYear),
        static_cast<std::uint8_t>(PelcoD::ResponseOpcode::TimeMacro), 0x07U, 0xEAU);
    ASSERT_TRUE(PelcoD::ProtocolParser::parseTimeResponse(timeYrResp, timeSub, d1, d2));
    EXPECT_EQ(timeSub, PelcoD::TimeSubOpcode::ReportYear);
    const auto yr = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
    EXPECT_EQ(yr, 2026U);
    EXPECT_NE(PelcoD::ProtocolParser::describeFrame(false, timeYrResp).find("Year Response: 2026"), std::string::npos);
}

/// @brief Verify PelcoDDevice high-level wrappers for Phase 3 Version Info and Time macro commands.
TEST(ProtocolCompletenessTest, PelcoDDevicePhase3Wrappers)
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

    device.querySoftwareVersion();
    device.queryBuildNumber();
    device.setSeconds(15U);
    device.setHourMinute(10U, 45U);
    device.setMonthDay(11U, 20U);
    device.setYear(2025U);
    device.setTime(8U, 30U, 0U);
    device.setDate(2026U, 7U, 4U);
    device.queryTime(PelcoD::TimeSubOpcode::ReportHourMinute);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    device.stop();

    std::scoped_lock lock(mtx);
    ASSERT_GE(sentFrames.size(), 11U);

    // Frame 0: QuerySoftwareVersion (0x73, sub 0x00)
    EXPECT_EQ(sentFrames[0][2], 0x00U);
    EXPECT_EQ(sentFrames[0][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::VersionInfo));

    // Frame 1: QueryBuildNumber (0x73, sub 0x02)
    EXPECT_EQ(sentFrames[1][2], 0x02U);
    EXPECT_EQ(sentFrames[1][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::VersionInfo));

    // Frame 2: setSeconds(15) -> 0x77 sub 0x00, d2=15
    EXPECT_EQ(sentFrames[2][2], 0x00U);
    EXPECT_EQ(sentFrames[2][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::TimeMacro));
    EXPECT_EQ(sentFrames[2][5], 15U);

    // Frame 3: setHourMinute(10, 45) -> 0x77 sub 0x02, d1=10, d2=45
    EXPECT_EQ(sentFrames[3][2], 0x02U);
    EXPECT_EQ(sentFrames[3][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::TimeMacro));
    EXPECT_EQ(sentFrames[3][4], 10U);
    EXPECT_EQ(sentFrames[3][5], 45U);

    // Frame 4: setMonthDay(11, 20) -> 0x77 sub 0x04, d1=11, d2=20
    EXPECT_EQ(sentFrames[4][2], 0x04U);
    EXPECT_EQ(sentFrames[4][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::TimeMacro));
    EXPECT_EQ(sentFrames[4][4], 11U);
    EXPECT_EQ(sentFrames[4][5], 20U);

    // Frame 5: setYear(2025) -> 0x77 sub 0x06
    EXPECT_EQ(sentFrames[5][2], 0x06U);
    EXPECT_EQ(sentFrames[5][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::TimeMacro));
    const auto yr2025 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(sentFrames[5][4]) << 8U) | sentFrames[5][5]);
    EXPECT_EQ(yr2025, 2025U);

    // Frame 6 & 7: setTime(8, 30, 0) -> setHourMinute then setSeconds
    EXPECT_EQ(sentFrames[6][2], 0x02U);
    EXPECT_EQ(sentFrames[6][4], 8U);
    EXPECT_EQ(sentFrames[6][5], 30U);
    EXPECT_EQ(sentFrames[7][2], 0x00U);
    EXPECT_EQ(sentFrames[7][5], 0U);

    // Frame 8 & 9: setDate(2026, 7, 4) -> setMonthDay then setYear
    EXPECT_EQ(sentFrames[8][2], 0x04U);
    EXPECT_EQ(sentFrames[8][4], 7U);
    EXPECT_EQ(sentFrames[8][5], 4U);
    EXPECT_EQ(sentFrames[9][2], 0x06U);
    const auto yr2026 = static_cast<std::uint16_t>((static_cast<std::uint16_t>(sentFrames[9][4]) << 8U) | sentFrames[9][5]);
    EXPECT_EQ(yr2026, 2026U);

    // Frame 10: queryTime(ReportHourMinute) -> 0x77 sub 0x03
    EXPECT_EQ(sentFrames[10][2], 0x03U);
    EXPECT_EQ(sentFrames[10][3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::TimeMacro));

    const auto info = device.getInfo();
    EXPECT_EQ(info.softwareMajor, 1U);
    EXPECT_EQ(info.softwareMinor, 2U);
    EXPECT_EQ(info.buildNumber, 345U);
}

/// @brief Verify asynchronous futures for software version and build number queries.
TEST(ProtocolCompletenessTest, PelcoDDevicePhase3AsyncQueries)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    ASSERT_TRUE(device.start());

    auto verFuture = device.querySoftwareVersionAsync(std::chrono::milliseconds(2000));
    ASSERT_EQ(verFuture.wait_for(std::chrono::milliseconds(1500)), std::future_status::ready);
    const auto [maj, min] = verFuture.get();
    EXPECT_EQ(maj, 1U);
    EXPECT_EQ(min, 2U);

    auto buildFuture = device.queryBuildNumberAsync(std::chrono::milliseconds(2000));
    ASSERT_EQ(buildFuture.wait_for(std::chrono::milliseconds(1500)), std::future_status::ready);
    const auto build = buildFuture.get();
    EXPECT_EQ(build, 345U);

    device.stop();
}

/// @brief Verify Auxiliary Indicator LED and Everest macro frame builders and disassembler.
TEST(ProtocolCompletenessTest, Phase4BuildersAndDisassembly)
{
    // Aux Indicator LED: Set
    const auto setAuxLedFrame = PelcoD::ProtocolBuilder::buildSetAuxLed(
        1U, PelcoD::AuxLedColor::Amber, 25U);
    ASSERT_EQ(setAuxLedFrame.size(), 7U);
    EXPECT_EQ(setAuxLedFrame[2], static_cast<std::uint8_t>(PelcoD::AuxSubOpcode::Led));
    EXPECT_EQ(setAuxLedFrame[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::SetAuxiliary));
    EXPECT_EQ(setAuxLedFrame[4], 25U);
    EXPECT_EQ(setAuxLedFrame[5], static_cast<std::uint8_t>(PelcoD::AuxLedColor::Amber));
    const auto descSetLed = PelcoD::ProtocolParser::describeFrame(true, setAuxLedFrame);
    EXPECT_NE(descSetLed.find("Set Aux LED"), std::string::npos);

    // Aux Indicator LED: Clear
    const auto clearAuxLedFrame = PelcoD::ProtocolBuilder::buildClearAuxLed(
        1U, PelcoD::AuxLedColor::Red, 10U);
    ASSERT_EQ(clearAuxLedFrame.size(), 7U);
    EXPECT_EQ(clearAuxLedFrame[2], static_cast<std::uint8_t>(PelcoD::AuxSubOpcode::Led));
    EXPECT_EQ(clearAuxLedFrame[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::ClearAuxiliary));
    EXPECT_EQ(clearAuxLedFrame[4], 10U);
    EXPECT_EQ(clearAuxLedFrame[5], static_cast<std::uint8_t>(PelcoD::AuxLedColor::Red));
    const auto descClearLed = PelcoD::ProtocolParser::describeFrame(true, clearAuxLedFrame);
    EXPECT_NE(descClearLed.find("Clear Aux LED"), std::string::npos);

    // Everest: QueryAzimuthZero
    const auto qAzZero = PelcoD::ProtocolBuilder::buildQueryAzimuthZero(1U);
    ASSERT_EQ(qAzZero.size(), 7U);
    EXPECT_EQ(qAzZero[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::QueryAzimuthZero));
    EXPECT_EQ(qAzZero[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::Everest));

    // Everest: SetZoomLimit
    const auto setZoomLim = PelcoD::ProtocolBuilder::buildSetZoomLimit(1U, 24000U);
    ASSERT_EQ(setZoomLim.size(), 7U);
    EXPECT_EQ(setZoomLim[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::SetZoomLimit));
    EXPECT_EQ(setZoomLim[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::Everest));
    EXPECT_EQ(setZoomLim[4], static_cast<std::uint8_t>((24000U >> 8U) & 0xFFU));
    EXPECT_EQ(setZoomLim[5], static_cast<std::uint8_t>(24000U & 0xFFU));

    // Everest: QueryZoomLimit
    const auto qZoomLim = PelcoD::ProtocolBuilder::buildQueryZoomLimit(1U);
    EXPECT_EQ(qZoomLim[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::QueryZoomLimit));
    EXPECT_EQ(qZoomLim[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::Everest));

    // Everest: QueryAlarms
    const auto qAlarms = PelcoD::ProtocolBuilder::buildQueryEverestAlarms(1U);
    EXPECT_EQ(qAlarms[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::QueryAlarms));
    EXPECT_EQ(qAlarms[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::Everest));

    // Everest: DeletePattern
    const auto delPat = PelcoD::ProtocolBuilder::buildDeletePattern(1U, 4U);
    EXPECT_EQ(delPat[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::DeletePattern));
    EXPECT_EQ(delPat[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::Everest));
    EXPECT_EQ(delPat[5], 4U);

    // Everest: SetManualLeftPanLimit & SetManualRightPanLimit
    const auto setManL = PelcoD::ProtocolBuilder::buildSetManualLeftPanLimit(1U, 1500U);
    EXPECT_EQ(setManL[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::SetManualLeftPanLimit));
    EXPECT_EQ(setManL[3], static_cast<std::uint8_t>(PelcoD::CommandOpcode::Everest));
    const auto setManR = PelcoD::ProtocolBuilder::buildSetManualRightPanLimit(1U, 34500U);
    EXPECT_EQ(setManR[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::SetManualRightPanLimit));

    // Everest: SetScanLeftPanLimit & SetScanRightPanLimit
    const auto setScanL = PelcoD::ProtocolBuilder::buildSetScanLeftPanLimit(1U, 2500U);
    EXPECT_EQ(setScanL[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::SetScanLeftPanLimit));
    const auto setScanR = PelcoD::ProtocolBuilder::buildSetScanRightPanLimit(1U, 33500U);
    EXPECT_EQ(setScanR[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::SetScanRightPanLimit));

    // Everest: QueryLimit
    const auto qLim = PelcoD::ProtocolBuilder::buildQueryLimit(1U, PelcoD::EverestLimitId::ScanRightPan);
    EXPECT_EQ(qLim[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::QueryLimit));
    EXPECT_EQ(qLim[5], static_cast<std::uint8_t>(PelcoD::EverestLimitId::ScanRightPan));

    // Everest: EnableLimits
    const auto enLim = PelcoD::ProtocolBuilder::buildEnableLimits(1U, true);
    EXPECT_EQ(enLim[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::EnableLimits));
    EXPECT_EQ(enLim[5], 0x01U);

    // Everest: QueryDefinedPresets & QueryDefinedPatterns
    const auto qPresets = PelcoD::ProtocolBuilder::buildQueryDefinedPresets(1U);
    EXPECT_EQ(qPresets[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::QueryDefinedPresets));
    const auto qPatterns = PelcoD::ProtocolBuilder::buildQueryDefinedPatterns(1U);
    EXPECT_EQ(qPatterns[2], static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::QueryDefinedPatterns));
}

/// @brief Verify parsing and status updating for Everest responses.
TEST(ProtocolCompletenessTest, Phase4ResponsesAndParsing)
{
    // Everest AzimuthZeroResponse: val = 18000 = 0x4650
    const std::vector<std::uint8_t> azFrame = PelcoD::PelcoDFrame::createFrame(
        1U,
        static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::AzimuthZeroResponse),
        static_cast<std::uint8_t>(PelcoD::ResponseOpcode::Everest),
        0x46U, 0x50U);

    PelcoD::DeviceStatus status {};
    PelcoD::DeviceInfo info {};
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(azFrame, status, info));
    EXPECT_EQ(status.azimuthZeroOffsetCentidegrees, 18000U);

    // Everest ZoomLimitResponse: val = 20000 = 0x4E20
    const std::vector<std::uint8_t> zlFrame = PelcoD::PelcoDFrame::createFrame(
        1U,
        static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::ZoomLimitResponse),
        static_cast<std::uint8_t>(PelcoD::ResponseOpcode::Everest),
        0x4EU, 0x20U);
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(zlFrame, status, info));
    EXPECT_EQ(status.zoomLimit, 20000U);

    // Everest AlarmsResponse: data2 = 0x0F
    const std::vector<std::uint8_t> almFrame = PelcoD::PelcoDFrame::createFrame(
        1U,
        static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::AlarmsResponse),
        static_cast<std::uint8_t>(PelcoD::ResponseOpcode::Everest),
        0x00U, 0x0FU);
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(almFrame, status, info));
    EXPECT_EQ(status.everestAlarms, 0x0FU);
    EXPECT_EQ(status.alarms, 0x00U);

    // Everest DefinedPresetsResponse: mask = 0x0055
    const std::vector<std::uint8_t> dpFrame = PelcoD::PelcoDFrame::createFrame(
        1U,
        static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::DefinedPresetsResponse),
        static_cast<std::uint8_t>(PelcoD::ResponseOpcode::Everest),
        0x00U, 0x55U);
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(dpFrame, status, info));
    EXPECT_EQ(status.definedPresetsMask, 0x0055U);

    // Everest DefinedPatternsResponse: mask = 0x000F
    const std::vector<std::uint8_t> patFrame = PelcoD::PelcoDFrame::createFrame(
        1U,
        static_cast<std::uint8_t>(PelcoD::EverestSubOpcode::DefinedPatternsResponse),
        static_cast<std::uint8_t>(PelcoD::ResponseOpcode::Everest),
        0x00U, 0x0FU);
    ASSERT_TRUE(PelcoD::ProtocolParser::updateStatus(patFrame, status, info));
    EXPECT_EQ(status.definedPatternsMask, 0x000FU);

    // Classification test
    EXPECT_EQ(PelcoD::ProtocolParser::classifyResponse(azFrame),
        PelcoD::ResponseClassification::ExtendedTelemetry);
}

/// @brief Verify high-level PelcoDDevice methods for Phase 4 Aux LED and Everest commands.
TEST(ProtocolCompletenessTest, PelcoDDevicePhase4Wrappers)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    std::mutex mtx;
    std::vector<std::vector<std::uint8_t>> sentFrames;

    device.addTrafficCallback([&](bool isTx, const std::vector<std::uint8_t>& frame) {
        if (isTx) {
            std::scoped_lock lock(mtx);
            sentFrames.push_back(frame);
        }
    });

    ASSERT_TRUE(device.start());

    device.setAuxLed(PelcoD::AuxLedColor::Amber, 20U);
    device.clearAuxLed(PelcoD::AuxLedColor::Red, 0U);
    device.queryAzimuthZero();
    device.setZoomLimit(22000U);
    device.queryZoomLimit();
    device.queryEverestAlarms();
    device.deletePattern(2U);
    device.setManualLeftPanLimit(1100U);
    device.setManualRightPanLimit(34900U);
    device.setScanLeftPanLimit(2100U);
    device.setScanRightPanLimit(33900U);
    device.queryLimit(PelcoD::EverestLimitId::ScanLeftPan);
    device.enableLimits(true);
    device.queryDefinedPresets();
    device.queryDefinedPatterns();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    device.stop();

    std::scoped_lock lock(mtx);
    ASSERT_GE(sentFrames.size(), 15U);

    // Verify status updated from replies received from mock device
    const auto status = device.getStatus();
    EXPECT_EQ(status.azimuthZeroOffsetCentidegrees, 500U); // default mock offset
    EXPECT_EQ(status.zoomLimit, 22000U); // updated by setZoomLimit
    EXPECT_EQ(status.definedPresetsMask, 0x0007U); // default mock mask
    EXPECT_EQ(status.definedPatternsMask, 0x0001U); // 0x0003 with pattern 2 deleted
}

/// @brief Verify asynchronous futures for Everest queries.
TEST(ProtocolCompletenessTest, PelcoDDevicePhase4AsyncQueries)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice device(mock, 1U);

    ASSERT_TRUE(device.start());

    auto azFuture = device.queryAzimuthZeroAsync(std::chrono::milliseconds(2000));
    ASSERT_EQ(azFuture.wait_for(std::chrono::milliseconds(1500)), std::future_status::ready);
    const auto azOffset = azFuture.get();
    EXPECT_EQ(azOffset, 500U);

    auto zlFuture = device.queryZoomLimitAsync(std::chrono::milliseconds(2000));
    ASSERT_EQ(zlFuture.wait_for(std::chrono::milliseconds(1500)), std::future_status::ready);
    const auto zl = zlFuture.get();
    EXPECT_EQ(zl, 18400U);

    device.stop();
}

} // namespace


