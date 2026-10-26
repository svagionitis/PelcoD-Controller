#include "PtzCommands.h"
#include "PelcoDFrame.h"

#include <cstdint>

#include <memory>
#include <utility>
#include <vector>

namespace PelcoD {

PtzCommands::PtzCommands(std::shared_ptr<PelcoDSession> session, std::shared_ptr<MotionSafetyGuard> guard,
    std::shared_ptr<TelemetryCoordinator> telemetry)
    : m_session { std::move(session) }
    , m_guard { std::move(guard) }
    , m_telemetry { std::move(telemetry) }
{
}

SendResult PtzCommands::dispatch(const StandardFrame& frame, QueryRef query, CommandPriority priority)
{
    if (m_guard) {
        return m_guard->dispatch(frame, std::move(query), priority);
    }
    if (!m_session) {
        return SendResult::TransportClosed;
    }
    return m_session->enqueueCommand(frame, std::move(query), priority);
}

SendResult PtzCommands::dispatch(const std::vector<std::uint8_t>& frame, QueryRef query, CommandPriority priority)
{
    if (m_guard) {
        return m_guard->dispatch(frame, std::move(query), priority);
    }
    if (!m_session) {
        return SendResult::TransportClosed;
    }
    return m_session->enqueueCommand(frame, std::move(query), priority);
}

SendResult PtzCommands::panLeft(std::uint8_t speed)
{
    if (m_telemetry) {
        m_telemetry->setActivePreset(0U);
    }
    return dispatch(ProtocolBuilder::buildPanFrame(m_session->getAddress(), PanDirection::Left, speed));
}

SendResult PtzCommands::panRight(std::uint8_t speed)
{
    if (m_telemetry) {
        m_telemetry->setActivePreset(0U);
    }
    return dispatch(ProtocolBuilder::buildPanFrame(m_session->getAddress(), PanDirection::Right, speed));
}

SendResult PtzCommands::tiltUp(std::uint8_t speed)
{
    if (m_telemetry) {
        m_telemetry->setActivePreset(0U);
    }
    return dispatch(ProtocolBuilder::buildTiltFrame(m_session->getAddress(), TiltDirection::Up, speed));
}

SendResult PtzCommands::tiltDown(std::uint8_t speed)
{
    if (m_telemetry) {
        m_telemetry->setActivePreset(0U);
    }
    return dispatch(ProtocolBuilder::buildTiltFrame(m_session->getAddress(), TiltDirection::Down, speed));
}

SendResult PtzCommands::stopMotion()
{
    return dispatch(ProtocolBuilder::buildStopFrame(m_session->getAddress()), "", CommandPriority::Urgent);
}

SendResult PtzCommands::move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed)
{
    if (m_telemetry) {
        m_telemetry->setActivePreset(0U);
    }
    return dispatch(ProtocolBuilder::buildMotionFrame(m_session->getAddress(), panDir, panSpeed, tiltDir, tiltSpeed));
}

SendResult PtzCommands::move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed,
    ZoomAction zoom, FocusAction focus, IrisAction iris)
{
    if (m_telemetry) {
        m_telemetry->setActivePreset(0U);
    }
    return dispatch(ProtocolBuilder::buildMotionFrame(
        m_session->getAddress(), panDir, panSpeed, tiltDir, tiltSpeed, zoom, focus, iris));
}

SendResult PtzCommands::setPower(bool on)
{
    return dispatch(ProtocolBuilder::buildPowerFrame(m_session->getAddress(), on));
}

SendResult PtzCommands::setAutoScan(bool enable)
{
    return dispatch(ProtocolBuilder::buildScanFrame(m_session->getAddress(), enable));
}

SendResult PtzCommands::zoomTele()
{
    return dispatch(ProtocolBuilder::buildZoomFrame(m_session->getAddress(), ZoomAction::Tele));
}

SendResult PtzCommands::zoomWide()
{
    return dispatch(ProtocolBuilder::buildZoomFrame(m_session->getAddress(), ZoomAction::Wide));
}

SendResult PtzCommands::zoomStop()
{
    return dispatch(
        ProtocolBuilder::buildZoomFrame(m_session->getAddress(), ZoomAction::Stop), "", CommandPriority::Urgent);
}

SendResult PtzCommands::focusNear()
{
    return dispatch(ProtocolBuilder::buildFocusFrame(m_session->getAddress(), FocusAction::Near));
}

SendResult PtzCommands::focusFar()
{
    return dispatch(ProtocolBuilder::buildFocusFrame(m_session->getAddress(), FocusAction::Far));
}

SendResult PtzCommands::focusStop()
{
    return dispatch(
        ProtocolBuilder::buildFocusFrame(m_session->getAddress(), FocusAction::Stop), "", CommandPriority::Urgent);
}

SendResult PtzCommands::irisOpen()
{
    return dispatch(ProtocolBuilder::buildIrisFrame(m_session->getAddress(), IrisAction::Open));
}

SendResult PtzCommands::irisClose()
{
    return dispatch(ProtocolBuilder::buildIrisFrame(m_session->getAddress(), IrisAction::Close));
}

SendResult PtzCommands::irisStop()
{
    return dispatch(
        ProtocolBuilder::buildIrisFrame(m_session->getAddress(), IrisAction::Stop), "", CommandPriority::Urgent);
}

SendResult PtzCommands::setPanAngle(std::uint16_t centidegrees)
{
    return dispatch(ProtocolBuilder::buildSetPan(m_session->getAddress(), centidegrees));
}

SendResult PtzCommands::setTiltAngle(std::uint16_t centidegrees)
{
    return dispatch(ProtocolBuilder::buildSetTilt(m_session->getAddress(), centidegrees));
}

SendResult PtzCommands::setZoomPosition(std::uint16_t position)
{
    return dispatch(ProtocolBuilder::buildSetZoom(m_session->getAddress(), position));
}

SendResult PtzCommands::setPreset(std::uint8_t presetId)
{
    return dispatch(ProtocolBuilder::buildSetPresetFrame(m_session->getAddress(), presetId));
}

SendResult PtzCommands::clearPreset(std::uint8_t presetId)
{
    if (m_telemetry && m_telemetry->getStatus().activePreset == presetId) {
        m_telemetry->setActivePreset(0U);
    }
    return dispatch(ProtocolBuilder::buildClearPresetFrame(m_session->getAddress(), presetId));
}

SendResult PtzCommands::goToPreset(std::uint8_t presetId)
{
    if (m_telemetry) {
        m_telemetry->setActivePreset(presetId);
    }
    return dispatch(ProtocolBuilder::buildGoToPresetFrame(m_session->getAddress(), presetId));
}

SendResult PtzCommands::goToPreset(std::uint8_t presetId, std::uint8_t speed)
{
    if (m_telemetry) {
        m_telemetry->setActivePreset(presetId);
    }
    if (speed > 0U) {
        const auto res = dispatch(ProtocolBuilder::buildPanFrame(m_session->getAddress(), PanDirection::Stop, speed));
        if (res != SendResult::Success) {
            return res;
        }
    }
    return dispatch(ProtocolBuilder::buildGoToPresetFrame(m_session->getAddress(), presetId));
}

SendResult PtzCommands::flip180()
{
    return dispatch(ProtocolBuilder::buildFlip180(m_session->getAddress()));
}

SendResult PtzCommands::zeroPan()
{
    return dispatch(ProtocolBuilder::buildZeroPan(m_session->getAddress()));
}

SendResult PtzCommands::presetScan(std::uint8_t dwellSeconds)
{
    return dispatch(ProtocolBuilder::buildPresetScan(m_session->getAddress(), dwellSeconds));
}

SendResult PtzCommands::setAuxiliary(std::uint8_t auxId)
{
    return dispatch(ProtocolBuilder::buildSetAux(m_session->getAddress(), auxId));
}

SendResult PtzCommands::clearAuxiliary(std::uint8_t auxId)
{
    return dispatch(ProtocolBuilder::buildClearAux(m_session->getAddress(), auxId));
}

SendResult PtzCommands::setZoneStart(std::uint8_t zoneId)
{
    return dispatch(ProtocolBuilder::buildSetZoneStart(m_session->getAddress(), zoneId));
}

SendResult PtzCommands::setZoneEnd(std::uint8_t zoneId)
{
    return dispatch(ProtocolBuilder::buildSetZoneEnd(m_session->getAddress(), zoneId));
}

SendResult PtzCommands::setZoneScan(bool enable)
{
    return dispatch(ProtocolBuilder::buildZoneScan(m_session->getAddress(), enable));
}

SendResult PtzCommands::recordPatternStart(std::uint8_t patternId)
{
    return dispatch(ProtocolBuilder::buildPatternStart(m_session->getAddress(), patternId));
}

SendResult PtzCommands::recordPatternStop()
{
    return dispatch(ProtocolBuilder::buildPatternStop(m_session->getAddress()));
}

SendResult PtzCommands::runPattern(std::uint8_t patternId)
{
    return dispatch(ProtocolBuilder::buildRunPattern(m_session->getAddress(), patternId));
}

SendResult PtzCommands::setZoomSpeed(std::uint8_t speed)
{
    const auto satSpeed = std::min(speed, static_cast<std::uint8_t>(0x03U));
    if (m_telemetry) {
        m_telemetry->setSpeeds(satSpeed, m_telemetry->getStatus().focusSpeed);
    }
    return dispatch(ProtocolBuilder::buildZoomSpeed(m_session->getAddress(), satSpeed));
}

SendResult PtzCommands::setFocusSpeed(std::uint8_t speed)
{
    const auto satSpeed = std::min(speed, static_cast<std::uint8_t>(0x03U));
    if (m_telemetry) {
        m_telemetry->setSpeeds(m_telemetry->getStatus().zoomSpeed, satSpeed);
    }
    return dispatch(ProtocolBuilder::buildFocusSpeed(m_session->getAddress(), satSpeed));
}

SendResult PtzCommands::setAutoFocus(AutoMode mode)
{
    if (m_telemetry) {
        m_telemetry->setAutoFocus(mode);
    }
    return dispatch(ProtocolBuilder::buildAutoFocus(m_session->getAddress(), mode));
}

SendResult PtzCommands::setAutoIris(AutoMode mode)
{
    if (m_telemetry) {
        m_telemetry->setAutoIris(mode);
    }
    return dispatch(ProtocolBuilder::buildAutoIris(m_session->getAddress(), mode));
}

SendResult PtzCommands::setAgc(AutoMode mode)
{
    if (m_telemetry) {
        m_telemetry->setAgc(mode);
    }
    return dispatch(ProtocolBuilder::buildAgc(m_session->getAddress(), mode));
}

SendResult PtzCommands::setBacklightComp(SwitchState state)
{
    if (m_telemetry) {
        m_telemetry->setBacklightComp(state);
    }
    return dispatch(ProtocolBuilder::buildBacklight(m_session->getAddress(), state));
}

SendResult PtzCommands::setAutoWhiteBalance(SwitchState state)
{
    if (m_telemetry) {
        m_telemetry->setAutoWhiteBalance(state);
    }
    return dispatch(ProtocolBuilder::buildWhiteBalance(m_session->getAddress(), state));
}

SendResult PtzCommands::setShutterSpeed(std::uint16_t speed)
{
    return dispatch(ProtocolBuilder::buildShutterSpeed(m_session->getAddress(), speed));
}

SendResult PtzCommands::setGain(std::uint16_t gain)
{
    return dispatch(ProtocolBuilder::buildGain(m_session->getAddress(), gain));
}

SendResult PtzCommands::setAutoIrisLevel(std::uint8_t level)
{
    return dispatch(ProtocolBuilder::buildAutoIrisLevel(m_session->getAddress(), level));
}

SendResult PtzCommands::setAutoIrisPeak(std::uint8_t peak)
{
    return dispatch(ProtocolBuilder::buildAutoIrisPeak(m_session->getAddress(), peak));
}

SendResult PtzCommands::setPhaseDelayMode(SwitchState state)
{
    return dispatch(ProtocolBuilder::buildPhaseDelayMode(m_session->getAddress(), state));
}

SendResult PtzCommands::adjustLineLockDelay(std::uint16_t centidegrees)
{
    return dispatch(ProtocolBuilder::buildLineLockDelay(m_session->getAddress(), centidegrees));
}

SendResult PtzCommands::adjustWhiteBalanceRB(std::uint16_t value)
{
    return dispatch(ProtocolBuilder::buildWhiteBalanceRB(m_session->getAddress(), value));
}

SendResult PtzCommands::adjustWhiteBalanceMG(std::uint16_t value)
{
    return dispatch(ProtocolBuilder::buildWhiteBalanceMG(m_session->getAddress(), value));
}

SendResult PtzCommands::setMagnification(std::uint16_t value, bool relative)
{
    const auto frameOpt = ProtocolBuilder::buildSetMagnification(m_session->getAddress(), value, relative);
    if (!frameOpt.has_value()) {
        return SendResult::InvalidFrame;
    }
    return dispatch(*frameOpt);
}

SendResult PtzCommands::setBaudRate(std::uint32_t baud)
{
    const auto frameOpt = ProtocolBuilder::buildSetBaudRate(m_session->getAddress(), baud);
    if (!frameOpt.has_value()) {
        return SendResult::InvalidFrame;
    }
    return dispatch(*frameOpt);
}

SendResult PtzCommands::setZeroPosition()
{
    return dispatch(ProtocolBuilder::buildSetZeroPosition(m_session->getAddress()));
}

SendResult PtzCommands::resetDefaults()
{
    return dispatch(ProtocolBuilder::buildResetDefaults(m_session->getAddress()));
}

SendResult PtzCommands::remoteReset()
{
    return dispatch(ProtocolBuilder::buildRemoteReset(m_session->getAddress()));
}

SendResult PtzCommands::queryPan()
{
    return dispatch(ProtocolBuilder::buildQueryPanFrame(m_session->getAddress()), QueryKind::Pan, CommandPriority::Low);
}

SendResult PtzCommands::queryTilt()
{
    return dispatch(
        ProtocolBuilder::buildQueryTiltFrame(m_session->getAddress()), QueryKind::Tilt, CommandPriority::Low);
}

SendResult PtzCommands::queryZoom()
{
    return dispatch(
        ProtocolBuilder::buildQueryZoomFrame(m_session->getAddress()), QueryKind::Zoom, CommandPriority::Low);
}

SendResult PtzCommands::queryMagnification()
{
    return dispatch(
        ProtocolBuilder::buildQueryMag(m_session->getAddress()), QueryKind::Magnification, CommandPriority::Low);
}

SendResult PtzCommands::queryDeviceType()
{
    return dispatch(
        ProtocolBuilder::buildQueryDevType(m_session->getAddress()), QueryKind::DeviceType, CommandPriority::Low);
}

SendResult PtzCommands::queryGeneral()
{
    return dispatch(
        ProtocolBuilder::buildQueryGeneral(m_session->getAddress()), QueryKind::General, CommandPriority::Low);
}

SendResult PtzCommands::queryDiagnostics()
{
    return dispatch(
        ProtocolBuilder::buildQueryDiagnostics(m_session->getAddress()), QueryKind::Diagnostics, CommandPriority::Low);
}

SendResult PtzCommands::queryAll()
{
    const auto rPan = queryPan();
    if (rPan != SendResult::Success) {
        return rPan;
    }
    const auto rTilt = queryTilt();
    if (rTilt != SendResult::Success) {
        return rTilt;
    }
    return queryZoom();
}

SendResult PtzCommands::querySerialNumber()
{
    return dispatch(
        ProtocolBuilder::buildQueryGeneral(m_session->getAddress()), QueryKind::SerialNumber, CommandPriority::Low);
}

SendResult PtzCommands::activateEchoMode()
{
    return dispatch(ProtocolBuilder::buildEchoMode(m_session->getAddress()));
}

SendResult PtzCommands::prepareForDownload()
{
    return dispatch(ProtocolBuilder::buildPrepareForDownload(m_session->getAddress()));
}

SendResult PtzCommands::startDownload()
{
    return dispatch(ProtocolBuilder::buildStartDownload(m_session->getAddress()));
}

SendResult PtzCommands::writeCharacter(std::uint8_t column, char asciiChar)
{
    return dispatch(ProtocolBuilder::buildWriteChar(m_session->getAddress(), column, asciiChar));
}

SendResult PtzCommands::clearScreen()
{
    return dispatch(ProtocolBuilder::buildClearScreen(m_session->getAddress()));
}

SendResult PtzCommands::sendDummy()
{
    return dispatch(ProtocolBuilder::buildDummy(m_session->getAddress()));
}

SendResult PtzCommands::screenMove(std::int8_t panPercent, std::int8_t tiltPercent, bool relative)
{
    return dispatch(ProtocolBuilder::buildScreenMove(m_session->getAddress(), panPercent, tiltPercent, relative));
}

SendResult PtzCommands::querySoftwareVersion()
{
    return dispatch(ProtocolBuilder::buildQuerySoftwareVersion(m_session->getAddress()), QueryKind::SoftwareVersion,
        CommandPriority::Low);
}

SendResult PtzCommands::queryBuildNumber()
{
    return dispatch(
        ProtocolBuilder::buildQueryBuildNumber(m_session->getAddress()), QueryKind::BuildNumber, CommandPriority::Low);
}

SendResult PtzCommands::setSeconds(std::uint8_t seconds)
{
    return dispatch(ProtocolBuilder::buildSetSeconds(m_session->getAddress(), seconds));
}

SendResult PtzCommands::setHourMinute(std::uint8_t hour, std::uint8_t minute)
{
    return dispatch(ProtocolBuilder::buildSetHourMinute(m_session->getAddress(), hour, minute));
}

SendResult PtzCommands::setMonthDay(std::uint8_t month, std::uint8_t day)
{
    return dispatch(ProtocolBuilder::buildSetMonthDay(m_session->getAddress(), month, day));
}

SendResult PtzCommands::setYear(std::uint16_t year)
{
    return dispatch(ProtocolBuilder::buildSetYear(m_session->getAddress(), year));
}

SendResult PtzCommands::setTime(std::uint8_t hour, std::uint8_t minute, std::uint8_t second)
{
    const auto res = setHourMinute(hour, minute);
    if (res != SendResult::Success) {
        return res;
    }
    return setSeconds(second);
}

SendResult PtzCommands::setDate(std::uint16_t year, std::uint8_t month, std::uint8_t day)
{
    const auto res = setMonthDay(month, day);
    if (res != SendResult::Success) {
        return res;
    }
    return setYear(year);
}

SendResult PtzCommands::queryTime(TimeSubOpcode queryType)
{
    return dispatch(
        ProtocolBuilder::buildQueryTime(m_session->getAddress(), queryType), QueryKind::Time, CommandPriority::Low);
}

SendResult PtzCommands::setAuxLed(std::uint8_t ledIdOrColor, std::uint8_t onTimeTenths)
{
    return dispatch(ProtocolBuilder::buildSetAuxLed(m_session->getAddress(), ledIdOrColor, onTimeTenths));
}

SendResult PtzCommands::clearAuxLed(std::uint8_t ledIdOrColor, std::uint8_t offTimeTenths)
{
    return dispatch(ProtocolBuilder::buildClearAuxLed(m_session->getAddress(), ledIdOrColor, offTimeTenths));
}

SendResult PtzCommands::queryAzimuthZero()
{
    return dispatch(
        ProtocolBuilder::buildQueryAzimuthZero(m_session->getAddress()), QueryKind::AzimuthZero, CommandPriority::Low);
}

SendResult PtzCommands::setZoomLimit(std::uint16_t limitHundredths)
{
    return dispatch(ProtocolBuilder::buildSetZoomLimit(m_session->getAddress(), limitHundredths));
}

SendResult PtzCommands::queryZoomLimit()
{
    return dispatch(
        ProtocolBuilder::buildQueryZoomLimit(m_session->getAddress()), QueryKind::ZoomLimit, CommandPriority::Low);
}

SendResult PtzCommands::queryEverestAlarms()
{
    return dispatch(ProtocolBuilder::buildQueryEverestAlarms(m_session->getAddress()), QueryKind::EverestAlarms,
        CommandPriority::Low);
}

SendResult PtzCommands::deletePattern(std::uint8_t patternId)
{
    return dispatch(ProtocolBuilder::buildDeletePattern(m_session->getAddress(), patternId));
}

SendResult PtzCommands::setManualLeftPanLimit(std::uint16_t centidegrees)
{
    if (m_telemetry) {
        m_telemetry->setPanLimit(true, centidegrees);
    }
    return dispatch(ProtocolBuilder::buildSetManualLeftPanLimit(m_session->getAddress(), centidegrees));
}

SendResult PtzCommands::setManualRightPanLimit(std::uint16_t centidegrees)
{
    if (m_telemetry) {
        m_telemetry->setPanLimit(false, centidegrees);
    }
    return dispatch(ProtocolBuilder::buildSetManualRightPanLimit(m_session->getAddress(), centidegrees));
}

SendResult PtzCommands::setScanLeftPanLimit(std::uint16_t centidegrees)
{
    if (m_telemetry) {
        m_telemetry->setScanLimit(true, centidegrees);
    }
    return dispatch(ProtocolBuilder::buildSetScanLeftPanLimit(m_session->getAddress(), centidegrees));
}

SendResult PtzCommands::setScanRightPanLimit(std::uint16_t centidegrees)
{
    if (m_telemetry) {
        m_telemetry->setScanLimit(false, centidegrees);
    }
    return dispatch(ProtocolBuilder::buildSetScanRightPanLimit(m_session->getAddress(), centidegrees));
}

SendResult PtzCommands::queryLimit(EverestLimitId limitId)
{
    m_session->setPendingLimitId(limitId);
    return dispatch(
        ProtocolBuilder::buildQueryLimit(m_session->getAddress(), limitId), QueryKind::Limit, CommandPriority::Low);
}

SendResult PtzCommands::enableLimits(bool enable)
{
    return dispatch(ProtocolBuilder::buildEnableLimits(m_session->getAddress(), enable));
}

SendResult PtzCommands::queryDefinedPresets(std::uint8_t group)
{
    return dispatch(ProtocolBuilder::buildQueryDefinedPresets(m_session->getAddress(), group),
        QueryKind::DefinedPresets, CommandPriority::Low);
}

SendResult PtzCommands::queryDefinedPatterns(std::uint8_t group)
{
    return dispatch(ProtocolBuilder::buildQueryDefinedPatterns(m_session->getAddress(), group),
        QueryKind::DefinedPatterns, CommandPriority::Low);
}

} // namespace PelcoD
