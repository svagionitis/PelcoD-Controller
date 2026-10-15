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
    [[nodiscard]] SendResult panLeft(std::uint8_t speed);
    [[nodiscard]] SendResult panRight(std::uint8_t speed);
    [[nodiscard]] SendResult tiltUp(std::uint8_t speed);
    [[nodiscard]] SendResult tiltDown(std::uint8_t speed);
    [[nodiscard]] SendResult stopMotion();

    [[nodiscard]] SendResult move(
        PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed);
    [[nodiscard]] SendResult move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir,
        std::uint8_t tiltSpeed, ZoomAction zoom, FocusAction focus = FocusAction::Stop,
        IrisAction iris = IrisAction::Stop);

    [[nodiscard]] SendResult setPower(bool on);
    [[nodiscard]] SendResult setAutoScan(bool enable);

    [[nodiscard]] SendResult zoomTele();
    [[nodiscard]] SendResult zoomWide();
    [[nodiscard]] SendResult zoomStop();

    [[nodiscard]] SendResult focusNear();
    [[nodiscard]] SendResult focusFar();
    [[nodiscard]] SendResult focusStop();

    [[nodiscard]] SendResult irisOpen();
    [[nodiscard]] SendResult irisClose();
    [[nodiscard]] SendResult irisStop();

    [[nodiscard]] SendResult setPanAngle(std::uint16_t centidegrees);
    [[nodiscard]] SendResult setTiltAngle(std::uint16_t centidegrees);
    [[nodiscard]] SendResult setZoomPosition(std::uint16_t position);

    // Presets
    [[nodiscard]] SendResult setPreset(std::uint8_t presetId);
    [[nodiscard]] SendResult clearPreset(std::uint8_t presetId);
    [[nodiscard]] SendResult goToPreset(std::uint8_t presetId);
    [[nodiscard]] SendResult goToPreset(std::uint8_t presetId, std::uint8_t speed);

    [[nodiscard]] SendResult flip180();
    [[nodiscard]] SendResult zeroPan();
    [[nodiscard]] SendResult presetScan(std::uint8_t dwellSeconds);

    // Auxiliaries & Zones
    [[nodiscard]] SendResult setAuxiliary(std::uint8_t auxId);
    [[nodiscard]] SendResult clearAuxiliary(std::uint8_t auxId);
    [[nodiscard]] SendResult setZoneStart(std::uint8_t zoneId);
    [[nodiscard]] SendResult setZoneEnd(std::uint8_t zoneId);
    [[nodiscard]] SendResult setZoneScan(bool enable);

    // Patterns
    [[nodiscard]] SendResult recordPatternStart(std::uint8_t patternId);
    [[nodiscard]] SendResult recordPatternStop();
    [[nodiscard]] SendResult runPattern(std::uint8_t patternId);

    // Speeds & Options
    [[nodiscard]] SendResult setZoomSpeed(std::uint8_t speed);
    [[nodiscard]] SendResult setFocusSpeed(std::uint8_t speed);
    [[nodiscard]] SendResult setAutoFocus(AutoMode mode);
    [[nodiscard]] SendResult setAutoIris(AutoMode mode);
    [[nodiscard]] SendResult setAgc(AutoMode mode);
    [[nodiscard]] SendResult setBacklightComp(SwitchState state);
    [[nodiscard]] SendResult setAutoWhiteBalance(SwitchState state);
    [[nodiscard]] SendResult setShutterSpeed(std::uint16_t speed);
    [[nodiscard]] SendResult setGain(std::uint16_t gain);
    [[nodiscard]] SendResult setAutoIrisLevel(std::uint8_t level);
    [[nodiscard]] SendResult setAutoIrisPeak(std::uint8_t peak);
    [[nodiscard]] SendResult setPhaseDelayMode(SwitchState state);
    [[nodiscard]] SendResult adjustLineLockDelay(std::uint16_t centidegrees);
    [[nodiscard]] SendResult adjustWhiteBalanceRB(std::uint16_t value);
    [[nodiscard]] SendResult adjustWhiteBalanceMG(std::uint16_t value);
    [[nodiscard]] SendResult setMagnification(std::uint16_t value, bool relative = false);
    [[nodiscard]] SendResult setBaudRate(std::uint32_t baud);
    [[nodiscard]] SendResult setZeroPosition();

    // System Commands & Queries
    [[nodiscard]] SendResult resetDefaults();
    [[nodiscard]] SendResult remoteReset();
    [[nodiscard]] SendResult queryPan();
    [[nodiscard]] SendResult queryTilt();
    [[nodiscard]] SendResult queryZoom();
    [[nodiscard]] SendResult queryMagnification();
    [[nodiscard]] SendResult queryDeviceType();
    [[nodiscard]] SendResult queryGeneral();
    [[nodiscard]] SendResult queryDiagnostics();
    [[nodiscard]] SendResult queryAll();
    [[nodiscard]] SendResult querySerialNumber();

    [[nodiscard]] SendResult activateEchoMode();
    [[nodiscard]] SendResult prepareForDownload();
    [[nodiscard]] SendResult startDownload();

    [[nodiscard]] SendResult writeCharacter(std::uint8_t column, char asciiChar);
    [[nodiscard]] SendResult clearScreen();
    [[nodiscard]] SendResult sendDummy();
    [[nodiscard]] SendResult screenMove(std::int8_t panPercent, std::int8_t tiltPercent, bool relative = false);

    [[nodiscard]] SendResult querySoftwareVersion();
    [[nodiscard]] SendResult queryBuildNumber();

    [[nodiscard]] SendResult setSeconds(std::uint8_t seconds);
    [[nodiscard]] SendResult setHourMinute(std::uint8_t hour, std::uint8_t minute);
    [[nodiscard]] SendResult setMonthDay(std::uint8_t month, std::uint8_t day);
    [[nodiscard]] SendResult setYear(std::uint16_t year);
    [[nodiscard]] SendResult setTime(std::uint8_t hour, std::uint8_t minute, std::uint8_t second);
    [[nodiscard]] SendResult setDate(std::uint16_t year, std::uint8_t month, std::uint8_t day);
    [[nodiscard]] SendResult queryTime(TimeSubOpcode queryType);

    [[nodiscard]] SendResult setAuxLed(std::uint8_t ledIdOrColor, std::uint8_t onTimeTenths = 0U);
    [[nodiscard]] SendResult clearAuxLed(std::uint8_t ledIdOrColor, std::uint8_t offTimeTenths = 0U);

    // Everest Macro Commands & Queries
    [[nodiscard]] SendResult queryAzimuthZero();
    [[nodiscard]] SendResult setZoomLimit(std::uint16_t limitHundredths);
    [[nodiscard]] SendResult queryZoomLimit();
    [[nodiscard]] SendResult queryEverestAlarms();
    [[nodiscard]] SendResult deletePattern(std::uint8_t patternId);
    [[nodiscard]] SendResult setManualLeftPanLimit(std::uint16_t centidegrees);
    [[nodiscard]] SendResult setManualRightPanLimit(std::uint16_t centidegrees);
    [[nodiscard]] SendResult setScanLeftPanLimit(std::uint16_t centidegrees);
    [[nodiscard]] SendResult setScanRightPanLimit(std::uint16_t centidegrees);
    [[nodiscard]] SendResult queryLimit(EverestLimitId limitId);
    [[nodiscard]] SendResult enableLimits(bool enable);
    [[nodiscard]] SendResult queryDefinedPresets(std::uint8_t group = 0U);
    [[nodiscard]] SendResult queryDefinedPatterns(std::uint8_t group = 0U);

private:
    [[nodiscard]] SendResult dispatch(
        const StandardFrame& frame, QueryKind queryKind, CommandPriority priority = CommandPriority::Normal);
    [[nodiscard]] SendResult dispatch(const StandardFrame& frame, const std::string& queryTag = "",
        CommandPriority priority = CommandPriority::Normal);
    [[nodiscard]] SendResult dispatch(const std::vector<std::uint8_t>& frame, QueryKind queryKind,
        CommandPriority priority = CommandPriority::Normal);
    [[nodiscard]] SendResult dispatch(const std::vector<std::uint8_t>& frame, const std::string& queryTag = "",
        CommandPriority priority = CommandPriority::Normal);

    std::shared_ptr<PelcoDSession> m_session;
    std::shared_ptr<MotionSafetyGuard> m_guard;
    std::shared_ptr<TelemetryCoordinator> m_telemetry;
};

} // namespace PelcoD
