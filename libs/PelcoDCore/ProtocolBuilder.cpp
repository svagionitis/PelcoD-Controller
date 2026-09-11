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

std::vector<std::uint8_t> ProtocolBuilder::buildSetPreset(std::uint8_t address, std::uint8_t presetId)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetPreset), 0x00U, presetId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildClearPreset(std::uint8_t address, std::uint8_t presetId)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::ClearPreset), 0x00U, presetId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildGoToPreset(std::uint8_t address, std::uint8_t presetId)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::GoToPreset), 0x00U, presetId);
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
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetAuxiliary), 0x00U, auxId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildClearAux(std::uint8_t address, std::uint8_t auxId)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::ClearAuxiliary), 0x00U, auxId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetZoneStart(std::uint8_t address, std::uint8_t zoneId)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetZoneStart), 0x00U, zoneId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetZoneEnd(std::uint8_t address, std::uint8_t zoneId)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetZoneEnd), 0x00U, zoneId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildZoneScan(std::uint8_t address, bool enable)
{
    const auto opcode = enable ? CommandOpcode::ZoneScanOn : CommandOpcode::ZoneScanOff;
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(opcode), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildPatternStart(std::uint8_t address, std::uint8_t patternId)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::RecordPatternStart), 0x00U, patternId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildPatternStop(std::uint8_t address)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::RecordPatternStop), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildRunPattern(std::uint8_t address, std::uint8_t patternId)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::RunPattern), 0x00U, patternId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildZoomSpeed(std::uint8_t address, std::uint8_t speed)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetZoomSpeed), 0x00U, speed & 0x03U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildFocusSpeed(std::uint8_t address, std::uint8_t speed)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetFocusSpeed), 0x00U, speed & 0x03U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildResetDefaults(std::uint8_t address)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::ResetDefaults), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildRemoteReset(std::uint8_t address)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::RemoteReset), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildAutoFocus(std::uint8_t address, AutoMode mode)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::AutoFocus), 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> ProtocolBuilder::buildAutoIris(std::uint8_t address, AutoMode mode)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::AutoIris), 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> ProtocolBuilder::buildAgc(std::uint8_t address, AutoMode mode)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::Agc), 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> ProtocolBuilder::buildBacklight(std::uint8_t address, SwitchState state)
{
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::BacklightComp), 0x00U,
        static_cast<std::uint8_t>(state));
}

std::vector<std::uint8_t> ProtocolBuilder::buildWhiteBalance(std::uint8_t address, SwitchState state)
{
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::AutoWhiteBalance), 0x00U,
        static_cast<std::uint8_t>(state));
}

std::vector<std::uint8_t> ProtocolBuilder::buildShutterSpeed(std::uint8_t address, std::uint16_t speed)
{
    const std::uint8_t msb = static_cast<std::uint8_t>((speed >> 8U) & 0xFFU);
    const std::uint8_t lsb = static_cast<std::uint8_t>(speed & 0xFFU);
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetShutterSpeed), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildGain(std::uint8_t address, std::uint16_t gain)
{
    const std::uint8_t msb = static_cast<std::uint8_t>((gain >> 8U) & 0xFFU);
    const std::uint8_t lsb = static_cast<std::uint8_t>(gain & 0xFFU);
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::AdjustGain), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildAutoIrisLevel(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::AdjustAutoIrisLevel), 0x00U, level);
}

std::vector<std::uint8_t> ProtocolBuilder::buildAutoIrisPeak(std::uint8_t address, std::uint8_t peak)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::AdjustAutoIrisPeak), 0x00U, peak);
}

std::vector<std::uint8_t> ProtocolBuilder::buildPhaseDelayMode(std::uint8_t address, SwitchState state)
{
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::PhaseDelayMode), 0x00U,
        static_cast<std::uint8_t>(state));
}

std::vector<std::uint8_t> ProtocolBuilder::buildLineLockDelay(std::uint8_t address, std::uint16_t centidegrees)
{
    const std::uint8_t msb = static_cast<std::uint8_t>((centidegrees >> 8U) & 0xFFU);
    const std::uint8_t lsb = static_cast<std::uint8_t>(centidegrees & 0xFFU);
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::AdjustLineLock), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildWhiteBalanceRB(std::uint8_t address, std::uint16_t value)
{
    const std::uint8_t msb = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    const std::uint8_t lsb = static_cast<std::uint8_t>(value & 0xFFU);
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::AdjustWbRedBlue), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildWhiteBalanceMG(std::uint8_t address, std::uint16_t value)
{
    const std::uint8_t msb = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    const std::uint8_t lsb = static_cast<std::uint8_t>(value & 0xFFU);
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::AdjustWbMg), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetPan(std::uint8_t address, std::uint16_t centidegrees)
{
    const std::uint8_t msb = static_cast<std::uint8_t>((centidegrees >> 8U) & 0xFFU);
    const std::uint8_t lsb = static_cast<std::uint8_t>(centidegrees & 0xFFU);
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetPanPosition), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetTilt(std::uint8_t address, std::uint16_t centidegrees)
{
    const std::uint8_t msb = static_cast<std::uint8_t>((centidegrees >> 8U) & 0xFFU);
    const std::uint8_t lsb = static_cast<std::uint8_t>(centidegrees & 0xFFU);
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetTiltPosition), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetZoom(std::uint8_t address, std::uint16_t position)
{
    const std::uint8_t msb = static_cast<std::uint8_t>((position >> 8U) & 0xFFU);
    const std::uint8_t lsb = static_cast<std::uint8_t>(position & 0xFFU);
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetZoomPosition), msb, lsb);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetZeroPosition(std::uint8_t address)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetZeroPosition), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildSetMagnification(
    std::uint8_t address, std::uint16_t value, bool relative)
{
    // data1: 0x00=absolute, 0x01=relative (rel/abs bit per spec §5.48)
    const std::uint8_t relAbs = relative ? 0x01U : 0x00U;
    const std::uint8_t lsb = static_cast<std::uint8_t>(value & 0xFFU);
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetMagnification), relAbs, lsb);
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
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::SetBaudRate), 0x00U, baudCode);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryPan(std::uint8_t address)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::QueryPanPosition), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryTilt(std::uint8_t address)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::QueryTiltPosition), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryZoom(std::uint8_t address)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::QueryZoomPosition), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryMag(std::uint8_t address)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::QueryMagnification), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryDevType(std::uint8_t address)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::QueryDeviceType), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryGeneral(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::Query), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildWriteChar(std::uint8_t address, std::uint8_t column, char asciiChar)
{
    return PelcoDFrame::createFrame(address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::WriteCharacter),
        column & 0x3FU, static_cast<std::uint8_t>(asciiChar));
}

std::vector<std::uint8_t> ProtocolBuilder::buildClearScreen(std::uint8_t address)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::ClearScreen), 0x00U, 0x00U);
}

std::vector<std::uint8_t> ProtocolBuilder::buildAlarmAck(std::uint8_t address, std::uint8_t alarmId)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::AlarmAcknowledge), 0x00U, alarmId);
}

std::vector<std::uint8_t> ProtocolBuilder::buildQueryDiagnostics(std::uint8_t address)
{
    return PelcoDFrame::createFrame(
        address, 0x00U, static_cast<std::uint8_t>(CommandOpcode::QueryDiagnostics), 0x00U, 0x00U);
}

} // namespace PelcoD
