/// @file ProtocolBuilder.cpp
/// @brief Implementation of Pelco-D protocol frame builder.

#include "ProtocolBuilder.h"
#include "PelcoDFrame.h"

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

    // Harden speeds: tilt max is 0x3F, pan max is 0x3F or 0x40 for turbo
    const std::uint8_t validPanSpeed = (panSpeed == 0x40U) ? 0x40U : static_cast<std::uint8_t>(panSpeed & 0x3FU);
    const std::uint8_t validTiltSpeed = static_cast<std::uint8_t>(tiltSpeed & 0x3FU);

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
    return buildStandardCmd(address, CommandOpcode::SetZoomSpeed, 0x00U, static_cast<std::uint8_t>(speed & 0x03U));
}

std::vector<std::uint8_t> ProtocolBuilder::buildFocusSpeed(std::uint8_t address, std::uint8_t speed)
{
    return buildStandardCmd(address, CommandOpcode::SetFocusSpeed, 0x00U, static_cast<std::uint8_t>(speed & 0x03U));
}

std::vector<std::uint8_t> ProtocolBuilder::buildResetDefaults(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::ResetDefaults);
}

std::vector<std::uint8_t> ProtocolBuilder::buildRemoteReset(std::uint8_t address)
{
    return buildStandardCmd(address, CommandOpcode::RemoteReset);
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

std::vector<std::uint8_t> ProtocolBuilder::buildSetMagnification(
    std::uint8_t address, std::uint16_t value, bool relative)
{
    // data1: 0x00=absolute, 0x01=relative (rel/abs bit per spec §5.48)
    const std::uint8_t relAbs = relative ? 0x01U : 0x00U;
    const auto lsb = static_cast<std::uint8_t>(value & 0xFFU);
    return buildStandardCmd(address, CommandOpcode::SetMagnification, relAbs, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetBaudRate(std::uint8_t address, std::uint32_t baud)
{
    // Spec-defined baud codes (§5.52): 2400=0, 4800=1, 9600=2, 19200=3, 38400=4, 115200=5
    std::uint8_t baudCode { 0x00U };
    if (baud >= 115200U) {
        baudCode = 0x05U;
    } else if (baud >= 38400U) {
        baudCode = 0x04U;
    } else if (baud >= 19200U) {
        baudCode = 0x03U;
    } else if (baud >= 9600U) {
        baudCode = 0x02U;
    } else if (baud >= 4800U) {
        baudCode = 0x01U;
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
    return buildStandardCmd(address, CommandOpcode::WriteCharacter, static_cast<std::uint8_t>(column & 0x3FU),
        static_cast<std::uint8_t>(asciiChar));
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

} // namespace PelcoD
