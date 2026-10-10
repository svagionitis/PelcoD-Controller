/// @file ProtocolBuilder.cpp
/// @brief Implementation of Pelco-D protocol frame builder.

#include "ProtocolBuilder.h"
#include "PelcoDFrame.h"

#include <algorithm>

namespace PelcoD {

std::vector<std::uint8_t> ProtocolBuilder::buildMotion(std::uint8_t address, PanDirection panDir, std::uint8_t panSpeed,
    TiltDirection tiltDir, std::uint8_t tiltSpeed, ZoomAction zoom, FocusAction focus, IrisAction iris)
{
    std::uint8_t cmd1 { 0x00U };
    std::uint8_t cmd2 { 0x00U };

    // Cmd1: Focus near, Iris open/close
    if (focus == FocusAction::Near) {
        cmd1 |= static_cast<std::uint8_t>(FocusAction::Near);
    }
    if (iris == IrisAction::Open) {
        cmd1 |= static_cast<std::uint8_t>(IrisAction::Open);
    } else if (iris == IrisAction::Close) {
        cmd1 |= static_cast<std::uint8_t>(IrisAction::Close);
    }

    // Cmd2: Focus far, Zoom, Tilt, Pan
    if (focus == FocusAction::Far) {
        cmd2 |= static_cast<std::uint8_t>(FocusAction::Far);
    }
    if (zoom == ZoomAction::Tele) {
        cmd2 |= static_cast<std::uint8_t>(ZoomAction::Tele);
    } else if (zoom == ZoomAction::Wide) {
        cmd2 |= static_cast<std::uint8_t>(ZoomAction::Wide);
    }

    if (tiltDir == TiltDirection::Up) {
        cmd2 |= static_cast<std::uint8_t>(TiltDirection::Up);
    } else if (tiltDir == TiltDirection::Down) {
        cmd2 |= static_cast<std::uint8_t>(TiltDirection::Down);
    }

    if (panDir == PanDirection::Right) {
        cmd2 |= static_cast<std::uint8_t>(PanDirection::Right);
    } else if (panDir == PanDirection::Left) {
        cmd2 |= static_cast<std::uint8_t>(PanDirection::Left);
    }

    // Harden speeds: tilt saturates at 0x3F, pan saturates at 0x40 (Turbo)
    const std::uint8_t validPanSpeed = std::min(panSpeed, static_cast<std::uint8_t>(0x40U));
    const std::uint8_t validTiltSpeed = std::min(tiltSpeed, static_cast<std::uint8_t>(0x3FU));

    return PelcoDFrame::createFrame(address, cmd1, cmd2, validPanSpeed, validTiltSpeed);
}

std::vector<std::uint8_t> ProtocolBuilder::buildStop(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x00U, 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildPan(std::uint8_t address, PanDirection dir, std::uint8_t speed)
{
    return buildMotion(address, dir, speed, TiltDirection::Stop, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildTilt(std::uint8_t address, TiltDirection dir, std::uint8_t speed)
{
    return buildMotion(address, PanDirection::Stop, 0x00U, dir, speed);
}

std::vector<std::uint8_t> ProtocolBuilder::buildZoom(std::uint8_t address, ZoomAction action)
{
    return buildMotion(address, PanDirection::Stop, 0x00U, TiltDirection::Stop, 0x00U, action);
}

std::vector<std::uint8_t> ProtocolBuilder::buildFocus(std::uint8_t address, FocusAction action)
{
    return buildMotion(address, PanDirection::Stop, 0x00U, TiltDirection::Stop, 0x00U, ZoomAction::Stop, action);
}

std::vector<std::uint8_t> ProtocolBuilder::buildIris(std::uint8_t address, IrisAction action)
{
    return buildMotion(
        address, PanDirection::Stop, 0x00U, TiltDirection::Stop, 0x00U, ZoomAction::Stop, FocusAction::Stop, action);
}

std::vector<std::uint8_t> ProtocolBuilder::buildPower(std::uint8_t address, bool on)
{
    const std::uint8_t cmd1
        = on ? static_cast<std::uint8_t>(ScanSense::DeviceOn) : static_cast<std::uint8_t>(ScanSense::DeviceOff);
    return PelcoDFrame::createFrame(address, cmd1, 0x00U, 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildScan(std::uint8_t address, bool autoScan)
{
    const std::uint8_t cmd1 = autoScan ? static_cast<std::uint8_t>(ScanSense::AutoScanOn)
                                       : static_cast<std::uint8_t>(ScanSense::ManualScanOn);
    return PelcoDFrame::createFrame(address, cmd1, 0x00U, 0x00U, 0x00U);
}

namespace {

/// @brief Constructs a standard Pelco-D command frame with Command 1 set to 0x00.
[[nodiscard]] inline std::vector<std::uint8_t> buildStandardCmd(
    std::uint8_t address, CommandOpcode opcode, std::uint8_t data1 = 0x00U, std::uint8_t data2 = 0x00U)
{
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(opcode), data1, data2);
}

/// @brief Constructs a standard Pelco-D command frame with a 16-bit big-endian payload (data1=msb, data2=lsb).
[[nodiscard]] inline std::vector<std::uint8_t> build16BitCmd(
    std::uint8_t address, CommandOpcode opcode, std::uint16_t value)
{
    const auto msb = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    const auto lsb = static_cast<std::uint8_t>(value & 0xFFU);
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(opcode), msb, lsb);
}

} // namespace

std::vector<std::uint8_t> ProtocolBuilder::buildSetPreset(std::uint8_t address, std::uint8_t presetId)
{
    return buildStandardCmd(address, CommandOpcode::SetPreset, 0x00U, presetId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildClearPreset(std::uint8_t address, std::uint8_t presetId)
{
    return buildStandardCmd(address, CommandOpcode::ClearPreset, 0x00U, presetId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildGoToPreset(std::uint8_t address, std::uint8_t presetId)
{
    return buildStandardCmd(address, CommandOpcode::GoToPreset, 0x00U, presetId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildFlip180(std::uint8_t address)
{
    return buildGoToPreset(address, 0x21U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildZeroPan(std::uint8_t address)
{
    return buildGoToPreset(address, 0x22U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildPresetScan(std::uint8_t address, std::uint8_t dwellSeconds)
{
    return buildStandardCmd(address, CommandOpcode::PresetScan, 0x00U, dwellSeconds);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetAux(std::uint8_t address, std::uint8_t auxId)
{
    return buildStandardCmd(address, CommandOpcode::SetAuxiliary, 0x00U, auxId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildClearAux(std::uint8_t address, std::uint8_t auxId)
{
    return buildStandardCmd(address, CommandOpcode::ClearAuxiliary, 0x00U, auxId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetZoneStart(std::uint8_t address, std::uint8_t zoneId)
{
    return buildStandardCmd(address, CommandOpcode::SetZoneStart, 0x00U, zoneId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetZoneEnd(std::uint8_t address, std::uint8_t zoneId)
{
    return buildStandardCmd(address, CommandOpcode::SetZoneEnd, 0x00U, zoneId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildZoneScan(std::uint8_t address, bool enable)
{
    const auto opcode = enable ? CommandOpcode::ZoneScanOn : CommandOpcode::ZoneScanOff;
    return buildStandardCmd(address, opcode);
}

std::vector<std::uint8_t> ProtocolBuilder::buildPatternStart(std::uint8_t address, std::uint8_t patternId)
{
    return buildStandardCmd(address, CommandOpcode::RecordPatternStart, 0x00U, patternId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildPatternStop(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::RecordPatternStop);
}

std::vector<std::uint8_t> ProtocolBuilder::buildRunPattern(std::uint8_t address, std::uint8_t patternId)
{
    return buildStandardCmd(address, CommandOpcode::RunPattern, 0x00U, patternId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildZoomSpeed(std::uint8_t address, std::uint8_t speed)
{
    const std::uint8_t satSpeed = std::min(speed, static_cast<std::uint8_t>(0x03U));
    return buildStandardCmd(address, CommandOpcode::SetZoomSpeed, 0x00U, satSpeed);
}

std::vector<std::uint8_t> ProtocolBuilder::buildFocusSpeed(std::uint8_t address, std::uint8_t speed)
{
    const std::uint8_t satSpeed = std::min(speed, static_cast<std::uint8_t>(0x03U));
    return buildStandardCmd(address, CommandOpcode::SetFocusSpeed, 0x00U, satSpeed);
}

std::vector<std::uint8_t> ProtocolBuilder::buildResetDefaults(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::ResetDefaults);
}

std::vector<std::uint8_t> ProtocolBuilder::buildRemoteReset(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::RemoteReset);
}

std::vector<std::uint8_t> ProtocolBuilder::buildDummy(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::Dummy);
}

std::vector<std::uint8_t> ProtocolBuilder::buildAutoFocus(std::uint8_t address, AutoMode mode)
{
    return buildStandardCmd(address, CommandOpcode::AutoFocus, 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> ProtocolBuilder::buildAutoIris(std::uint8_t address, AutoMode mode)
{
    return buildStandardCmd(address, CommandOpcode::AutoIris, 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> ProtocolBuilder::buildAgc(std::uint8_t address, AutoMode mode)
{
    return buildStandardCmd(address, CommandOpcode::Agc, 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> ProtocolBuilder::buildBacklight(std::uint8_t address, SwitchState state)
{
    return buildStandardCmd(address, CommandOpcode::BacklightComp, 0x00U, static_cast<std::uint8_t>(state));
}

std::vector<std::uint8_t> ProtocolBuilder::buildWhiteBalance(std::uint8_t address, SwitchState state)
{
    return buildStandardCmd(address, CommandOpcode::AutoWhiteBalance, 0x00U, static_cast<std::uint8_t>(state));
}

std::vector<std::uint8_t> ProtocolBuilder::buildShutterSpeed(std::uint8_t address, std::uint16_t speed)
{
    return build16BitCmd(address, CommandOpcode::SetShutterSpeed, speed);
}

std::vector<std::uint8_t> ProtocolBuilder::buildGain(std::uint8_t address, std::uint16_t gain)
{
    return build16BitCmd(address, CommandOpcode::AdjustGain, gain);
}

std::vector<std::uint8_t> ProtocolBuilder::buildAutoIrisLevel(std::uint8_t address, std::uint8_t level)
{
    return buildStandardCmd(address, CommandOpcode::AdjustAutoIrisLevel, 0x00U, level);
}

std::vector<std::uint8_t> ProtocolBuilder::buildAutoIrisPeak(std::uint8_t address, std::uint8_t peak)
{
    return buildStandardCmd(address, CommandOpcode::AdjustAutoIrisPeak, 0x00U, peak);
}

std::vector<std::uint8_t> ProtocolBuilder::buildPhaseDelayMode(std::uint8_t address, SwitchState state)
{
    return buildStandardCmd(address, CommandOpcode::PhaseDelayMode, 0x00U, static_cast<std::uint8_t>(state));
}

std::vector<std::uint8_t> ProtocolBuilder::buildLineLockDelay(std::uint8_t address, std::uint16_t centidegrees)
{
    return build16BitCmd(address, CommandOpcode::AdjustLineLock, centidegrees);
}

std::vector<std::uint8_t> ProtocolBuilder::buildWhiteBalanceRB(std::uint8_t address, std::uint16_t value)
{
    return build16BitCmd(address, CommandOpcode::AdjustWbRedBlue, value);
}

std::vector<std::uint8_t> ProtocolBuilder::buildWhiteBalanceMG(std::uint8_t address, std::uint16_t value)
{
    return build16BitCmd(address, CommandOpcode::AdjustWbMg, value);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetPan(std::uint8_t address, std::uint16_t centidegrees)
{
    return build16BitCmd(address, CommandOpcode::SetPanPosition, centidegrees);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetTilt(std::uint8_t address, std::uint16_t centidegrees)
{
    return build16BitCmd(address, CommandOpcode::SetTiltPosition, centidegrees);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetZoom(std::uint8_t address, std::uint16_t position)
{
    return build16BitCmd(address, CommandOpcode::SetZoomPosition, position);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetZeroPosition(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::SetZeroPosition);
}

std::optional<std::vector<std::uint8_t>> ProtocolBuilder::buildSetMagnification(
    std::uint8_t address, std::uint16_t value, bool relative)
{
    if (relative) {
        return std::nullopt;
    }
    return build16BitCmd(address, CommandOpcode::SetMagnification, value);
}

std::optional<std::vector<std::uint8_t>> ProtocolBuilder::buildSetBaudRate(std::uint8_t address, std::uint32_t baud)
{
    // Spec-defined baud codes (§5.52): 2400=0, 4800=1, 9600=2, 19200=3, 38400=4, 115200=5
    std::uint8_t baudCode { 0x00U };
    switch (baud) {
    case 2400U:
        baudCode = 0x00U;
        break;
    case 4800U:
        baudCode = 0x01U;
        break;
    case 9600U:
        baudCode = 0x02U;
        break;
    case 19200U:
        baudCode = 0x03U;
        break;
    case 38400U:
        baudCode = 0x04U;
        break;
    case 115200U:
        baudCode = 0x05U;
        break;
    default:
        return std::nullopt;
    }
    return buildStandardCmd(address, CommandOpcode::SetBaudRate, 0x00U, baudCode);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryPan(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::QueryPanPosition);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryTilt(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::QueryTiltPosition);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryZoom(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::QueryZoomPosition);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryMag(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::QueryMagnification);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryDevType(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::QueryDeviceType);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryGeneral(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::Query);
}

std::vector<std::uint8_t> ProtocolBuilder::buildWriteChar(std::uint8_t address, std::uint8_t column, char asciiChar)
{
    const std::uint8_t satCol = std::min(column, static_cast<std::uint8_t>(0x3FU));
    return buildStandardCmd(address, CommandOpcode::WriteCharacter, satCol,
        static_cast<std::uint8_t>(static_cast<unsigned char>(asciiChar)));
}

std::vector<std::uint8_t> ProtocolBuilder::buildClearScreen(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::ClearScreen);
}

std::vector<std::uint8_t> ProtocolBuilder::buildAlarmAck(std::uint8_t address, std::uint8_t alarmId)
{
    return buildStandardCmd(address, CommandOpcode::AlarmAcknowledge, 0x00U, alarmId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryDiagnostics(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::QueryDiagnostics);
}

std::vector<std::uint8_t> ProtocolBuilder::buildPrepareForDownload(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::PrepareForDownload);
}

std::vector<std::uint8_t> ProtocolBuilder::buildStartDownload(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::StartDownload);
}

std::vector<std::uint8_t> ProtocolBuilder::buildEchoMode(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::EchoMode);
}

std::vector<std::uint8_t> ProtocolBuilder::buildScreenMove(
    std::uint8_t address, std::int8_t panPercent, std::int8_t tiltPercent, bool relative)
{
    const std::uint8_t cmd1 = relative ? 0x01U : 0x00U;
    const auto safePan = std::clamp(panPercent, static_cast<std::int8_t>(-100), static_cast<std::int8_t>(100));
    const auto safeTilt = std::clamp(tiltPercent, static_cast<std::int8_t>(-100), static_cast<std::int8_t>(100));
    const auto d1 = static_cast<std::uint8_t>(safePan);
    const auto d2 = static_cast<std::uint8_t>(safeTilt);
    return PelcoDFrame::createFrame(address, cmd1, static_cast<std::uint8_t>(CommandOpcode::ScreenMove), d1, d2);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQuerySoftwareVersion(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(VersionInfoSubOpcode::RequestSoftwareVersion),
        static_cast<std::uint8_t>(CommandOpcode::VersionInfo), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryBuildNumber(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(VersionInfoSubOpcode::RequestBuildNumber),
        static_cast<std::uint8_t>(CommandOpcode::VersionInfo), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetSeconds(std::uint8_t address, std::uint8_t seconds)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(TimeSubOpcode::SetSeconds),
        static_cast<std::uint8_t>(CommandOpcode::TimeMacro), 0x00U, seconds);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetHourMinute(
    std::uint8_t address, std::uint8_t hour, std::uint8_t minute)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(TimeSubOpcode::SetHourMinute),
        static_cast<std::uint8_t>(CommandOpcode::TimeMacro), hour, minute);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetMonthDay(
    std::uint8_t address, std::uint8_t month, std::uint8_t day)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(TimeSubOpcode::SetMonthDay),
        static_cast<std::uint8_t>(CommandOpcode::TimeMacro), month, day);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetYear(std::uint8_t address, std::uint16_t year)
{
    const auto msb = static_cast<std::uint8_t>((year >> 8U) & 0xFFU);
    const auto lsb = static_cast<std::uint8_t>(year & 0xFFU);
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(TimeSubOpcode::SetYear),
        static_cast<std::uint8_t>(CommandOpcode::TimeMacro), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryTime(std::uint8_t address, TimeSubOpcode queryType)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(queryType),
        static_cast<std::uint8_t>(CommandOpcode::TimeMacro), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetAuxLed(
    std::uint8_t address, std::uint8_t ledIdOrColor, std::uint8_t onTimeTenths)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(AuxSubOpcode::Led),
        static_cast<std::uint8_t>(CommandOpcode::SetAuxiliary), onTimeTenths, ledIdOrColor);
}

std::vector<std::uint8_t> ProtocolBuilder::buildClearAuxLed(
    std::uint8_t address, std::uint8_t ledIdOrColor, std::uint8_t offTimeTenths)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(AuxSubOpcode::Led),
        static_cast<std::uint8_t>(CommandOpcode::ClearAuxiliary), offTimeTenths, ledIdOrColor);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryAzimuthZero(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::QueryAzimuthZero),
        static_cast<std::uint8_t>(CommandOpcode::Everest), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetZoomLimit(std::uint8_t address, std::uint16_t limitHundredths)
{
    const auto msb = static_cast<std::uint8_t>((limitHundredths >> 8U) & 0xFFU);
    const auto lsb = static_cast<std::uint8_t>(limitHundredths & 0xFFU);
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::SetZoomLimit),
        static_cast<std::uint8_t>(CommandOpcode::Everest), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryZoomLimit(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::QueryZoomLimit),
        static_cast<std::uint8_t>(CommandOpcode::Everest), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryEverestAlarms(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::QueryAlarms),
        static_cast<std::uint8_t>(CommandOpcode::Everest), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildDeletePattern(std::uint8_t address, std::uint8_t patternId)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::DeletePattern),
        static_cast<std::uint8_t>(CommandOpcode::Everest), 0x00U, patternId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetManualLeftPanLimit(std::uint8_t address, std::uint16_t centidegrees)
{
    const auto msb = static_cast<std::uint8_t>((centidegrees >> 8U) & 0xFFU);
    const auto lsb = static_cast<std::uint8_t>(centidegrees & 0xFFU);
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::SetManualLeftPanLimit),
        static_cast<std::uint8_t>(CommandOpcode::Everest), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetManualRightPanLimit(std::uint8_t address, std::uint16_t centidegrees)
{
    const auto msb = static_cast<std::uint8_t>((centidegrees >> 8U) & 0xFFU);
    const auto lsb = static_cast<std::uint8_t>(centidegrees & 0xFFU);
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::SetManualRightPanLimit),
        static_cast<std::uint8_t>(CommandOpcode::Everest), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetScanLeftPanLimit(std::uint8_t address, std::uint16_t centidegrees)
{
    const auto msb = static_cast<std::uint8_t>((centidegrees >> 8U) & 0xFFU);
    const auto lsb = static_cast<std::uint8_t>(centidegrees & 0xFFU);
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::SetScanLeftPanLimit),
        static_cast<std::uint8_t>(CommandOpcode::Everest), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetScanRightPanLimit(std::uint8_t address, std::uint16_t centidegrees)
{
    const auto msb = static_cast<std::uint8_t>((centidegrees >> 8U) & 0xFFU);
    const auto lsb = static_cast<std::uint8_t>(centidegrees & 0xFFU);
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::SetScanRightPanLimit),
        static_cast<std::uint8_t>(CommandOpcode::Everest), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryLimit(std::uint8_t address, EverestLimitId limitId)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::QueryLimit),
        static_cast<std::uint8_t>(CommandOpcode::Everest), 0x00U, static_cast<std::uint8_t>(limitId));
}

std::vector<std::uint8_t> ProtocolBuilder::buildEnableLimits(std::uint8_t address, bool enable)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::EnableLimits),
        static_cast<std::uint8_t>(CommandOpcode::Everest), 0x00U, enable ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryDefinedPresets(std::uint8_t address, std::uint8_t group)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::QueryDefinedPresets),
        static_cast<std::uint8_t>(CommandOpcode::Everest), 0x00U, group);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryDefinedPatterns(std::uint8_t address, std::uint8_t group)
{
    return PelcoDFrame::createFrame(address, static_cast<std::uint8_t>(EverestSubOpcode::QueryDefinedPatterns),
        static_cast<std::uint8_t>(CommandOpcode::Everest), 0x00U, group);
}

} // namespace PelcoD
