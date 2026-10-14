#pragma once

/// @file PtzCommands.h
/// @brief Strongly-typed command encoder and dispatcher submitting frames to PelcoDSession.

#include "MotionSafetyGuard.h"
#include "PelcoDSession.h"
#include "PelcoDTypes.h"
#include "ProtocolBuilder.h"
#include "TelemetryCoordinator.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace PelcoD {

/// @class PtzCommands
/// @brief Command encoder facade translating high-level PTZ operations into Pelco-D protocol frames.
/// @details Coordinates with MotionSafetyGuard to track motion lifetimes, synchronizes optical and
///          preset states with TelemetryCoordinator, and returns SendResult status.
class PtzCommands final {
public:
    /// @brief Constructs a PtzCommands instance.
    /// @param[in] session Target communication session.
    /// @param[in] guard Motion safety watchdog guard.
    /// @param[in] telemetry Telemetry and state coordinator.
    explicit PtzCommands(std::shared_ptr<PelcoDSession> session, std::shared_ptr<MotionSafetyGuard> guard = nullptr,
        std::shared_ptr<TelemetryCoordinator> telemetry = nullptr);

    ~PtzCommands() = default;

    PtzCommands(const PtzCommands&) = delete;
    PtzCommands& operator=(const PtzCommands&) = delete;
    PtzCommands(PtzCommands&&) = delete;
    PtzCommands& operator=(PtzCommands&&) = delete;

    // Motion & Positioning
    SendResult panLeft(std::uint8_t speed);
    SendResult panRight(std::uint8_t speed);
    SendResult tiltUp(std::uint8_t speed);
    SendResult tiltDown(std::uint8_t speed);
    SendResult stopMotion();

    SendResult move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed);
    SendResult move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed,
        ZoomAction zoom, FocusAction focus = FocusAction::Stop, IrisAction iris = IrisAction::Stop);

    SendResult setPower(bool on);
    SendResult setAutoScan(bool enable);

    SendResult zoomTele();
    SendResult zoomWide();
    SendResult zoomStop();

    SendResult focusNear();
    SendResult focusFar();
    SendResult focusStop();

    SendResult irisOpen();
    SendResult irisClose();
    SendResult irisStop();

    SendResult setPanAngle(std::uint16_t centidegrees);
    SendResult setTiltAngle(std::uint16_t centidegrees);
    SendResult setZoomPosition(std::uint16_t position);

    // Presets
    SendResult setPreset(std::uint8_t presetId);
    SendResult clearPreset(std::uint8_t presetId);
    SendResult goToPreset(std::uint8_t presetId);
    SendResult goToPreset(std::uint8_t presetId, std::uint8_t speed);

    SendResult flip180();
    SendResult zeroPan();
    SendResult presetScan(std::uint8_t dwellSeconds);

    // Auxiliaries & Zones
    SendResult setAuxiliary(std::uint8_t auxId);
    SendResult clearAuxiliary(std::uint8_t auxId);
    SendResult setZoneStart(std::uint8_t zoneId);
    SendResult setZoneEnd(std::uint8_t zoneId);
    SendResult setZoneScan(bool enable);

    // Patterns
    SendResult recordPatternStart(std::uint8_t patternId);
    SendResult recordPatternStop();
    SendResult runPattern(std::uint8_t patternId);

    // Speeds & Options
    SendResult setZoomSpeed(std::uint8_t speed);
    SendResult setFocusSpeed(std::uint8_t speed);
    SendResult setAutoFocus(AutoMode mode);
    SendResult setAutoIris(AutoMode mode);
    SendResult setAgc(AutoMode mode);
    SendResult setBacklightComp(SwitchState state);
    SendResult setAutoWhiteBalance(SwitchState state);
    SendResult setShutterSpeed(std::uint16_t speed);
    SendResult setGain(std::uint16_t gain);
    SendResult setAutoIrisLevel(std::uint8_t level);
    SendResult setAutoIrisPeak(std::uint8_t peak);
    SendResult setPhaseDelayMode(SwitchState state);
    SendResult adjustLineLockDelay(std::uint16_t centidegrees);
    SendResult adjustWhiteBalanceRB(std::uint16_t value);
    SendResult adjustWhiteBalanceMG(std::uint16_t value);
    SendResult setMagnification(std::uint16_t value, bool relative = false);
    SendResult setBaudRate(std::uint32_t baud);
    SendResult setZeroPosition();

    SendResult setFocusPosition(std::uint16_t pos);
    SendResult setIrisPosition(std::uint16_t pos);

    // System Commands & Queries
    SendResult resetDefaults();
    SendResult remoteReset();
    SendResult queryPan();
    SendResult queryTilt();
    SendResult queryZoom();
    SendResult queryMagnification();
    SendResult queryDeviceType();
    SendResult queryGeneral();
    SendResult queryDiagnostics();
    SendResult queryAll();
    SendResult querySerialNumber();

    SendResult activateEchoMode();
    SendResult prepareForDownload();
    SendResult startDownload();

    SendResult writeCharacter(std::uint8_t column, char asciiChar);
    SendResult clearScreen();
    SendResult sendDummy();
    SendResult screenMove(std::int8_t panPercent, std::int8_t tiltPercent, bool relative = false);

    SendResult querySoftwareVersion();
    SendResult queryBuildNumber();

    SendResult setSeconds(std::uint8_t seconds);
    SendResult setHourMinute(std::uint8_t hour, std::uint8_t minute);
    SendResult setMonthDay(std::uint8_t month, std::uint8_t day);
    SendResult setYear(std::uint16_t year);
    SendResult setTime(std::uint8_t hour, std::uint8_t minute, std::uint8_t second);
    SendResult setDate(std::uint16_t year, std::uint8_t month, std::uint8_t day);
    SendResult queryTime(TimeSubOpcode queryType);

    SendResult setAuxLed(std::uint8_t ledIdOrColor, std::uint8_t onTimeTenths = 0U);
    SendResult clearAuxLed(std::uint8_t ledIdOrColor, std::uint8_t offTimeTenths = 0U);

    // Everest Macro Commands & Queries
    SendResult queryAzimuthZero();
    SendResult setZoomLimit(std::uint16_t limitHundredths);
    SendResult queryZoomLimit();
    SendResult queryEverestAlarms();
    SendResult deletePattern(std::uint8_t patternId);
    SendResult setManualLeftPanLimit(std::uint16_t centidegrees);
    SendResult setManualRightPanLimit(std::uint16_t centidegrees);
    SendResult setScanLeftPanLimit(std::uint16_t centidegrees);
    SendResult setScanRightPanLimit(std::uint16_t centidegrees);
    SendResult queryLimit(EverestLimitId limitId);
    SendResult enableLimits(bool enable);
    SendResult queryDefinedPresets(std::uint8_t group = 0U);
    SendResult queryDefinedPatterns(std::uint8_t group = 0U);

private:
    [[nodiscard]] SendResult dispatch(const std::vector<std::uint8_t>& frame, QueryKind queryKind,
        CommandPriority priority = CommandPriority::Normal);
    [[nodiscard]] SendResult dispatch(const std::vector<std::uint8_t>& frame, const std::string& queryTag = "",
        CommandPriority priority = CommandPriority::Normal);

    std::shared_ptr<PelcoDSession> m_session;
    std::shared_ptr<MotionSafetyGuard> m_guard;
    std::shared_ptr<TelemetryCoordinator> m_telemetry;
};

} // namespace PelcoD
