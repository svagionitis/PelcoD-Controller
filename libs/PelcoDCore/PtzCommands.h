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
class PtzCommands {
public:
    /// @brief Constructs a PtzCommands instance.
    /// @param[in] session Target communication session.
    /// @param[in] guard Motion safety watchdog guard.
    /// @param[in] telemetry Telemetry and state coordinator.
    explicit PtzCommands(std::shared_ptr<PelcoDSession> session, std::shared_ptr<MotionSafetyGuard> guard = nullptr,
        std::shared_ptr<TelemetryCoordinator> telemetry = nullptr);

    virtual ~PtzCommands() = default;

    PtzCommands(const PtzCommands&) = delete;
    PtzCommands& operator=(const PtzCommands&) = delete;
    PtzCommands(PtzCommands&&) = delete;
    PtzCommands& operator=(PtzCommands&&) = delete;

    // Motion & Positioning
    /// @brief Drives continuous pan left motion at the given speed.
    /// @param[in] speed Pan speed (0 - 64).
    /// @return Result status code.
    [[nodiscard]] SendResult panLeft(std::uint8_t speed);

    /// @brief Drives continuous pan right motion at the given speed.
    /// @param[in] speed Pan speed (0 - 64).
    /// @return Result status code.
    [[nodiscard]] SendResult panRight(std::uint8_t speed);

    /// @brief Drives continuous tilt up motion at the given speed.
    /// @param[in] speed Tilt speed (0 - 63).
    /// @return Result status code.
    [[nodiscard]] SendResult tiltUp(std::uint8_t speed);

    /// @brief Drives continuous tilt down motion at the given speed.
    /// @param[in] speed Tilt speed (0 - 63).
    /// @return Result status code.
    [[nodiscard]] SendResult tiltDown(std::uint8_t speed);

    /// @brief Halts all active pan, tilt, zoom, and focus motion immediately.
    /// @return Result status code.
    [[nodiscard]] SendResult stopMotion();

    /// @brief Drives combined pan and tilt motion.
    /// @param[in] panDir Direction of pan motion.
    /// @param[in] panSpeed Speed of pan motion (0 - 64).
    /// @param[in] tiltDir Direction of tilt motion.
    /// @param[in] tiltSpeed Speed of tilt motion (0 - 63).
    /// @return Result status code.
    [[nodiscard]] SendResult move(
        PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed);

    /// @brief Drives combined motion across Pan, Tilt, Zoom, Focus, and Iris.
    /// @param[in] panDir Pan direction (Left, Right, Stop).
    /// @param[in] panSpeed Pan speed (0 - 64).
    /// @param[in] tiltDir Tilt direction (Up, Down, Stop).
    /// @param[in] tiltSpeed Tilt speed (0 - 63).
    /// @param[in] zoom Zoom action (Tele, Wide, Stop).
    /// @param[in] focus Focus action (Near, Far, Stop).
    /// @param[in] iris Iris action (Open, Close, Stop).
    /// @return Result status code.
    [[nodiscard]] SendResult move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir,
        std::uint8_t tiltSpeed, ZoomAction zoom, FocusAction focus = FocusAction::Stop,
        IrisAction iris = IrisAction::Stop);

    /// @brief Controls device operational power state (opcode 0x88 On, 0x08 Off).
    /// @param[in] on True for Power On, false for Power Off.
    /// @return Result status code.
    [[nodiscard]] SendResult setPower(bool on);

    /// @brief Configures auto or manual scan mode (opcode 0x90 Auto, 0x98 Manual).
    /// @param[in] enable True to initiate auto scan, false for manual scan.
    /// @return Result status code.
    [[nodiscard]] SendResult setAutoScan(bool enable);

    /// @brief Drives continuous zoom tele (in) motion.
    /// @return Result status code.
    [[nodiscard]] SendResult zoomTele();

    /// @brief Drives continuous zoom wide (out) motion.
    /// @return Result status code.
    [[nodiscard]] SendResult zoomWide();

    /// @brief Halts active zoom motion.
    /// @return Result status code.
    [[nodiscard]] SendResult zoomStop();

    /// @brief Drives continuous focus near motion.
    /// @return Result status code.
    [[nodiscard]] SendResult focusNear();

    /// @brief Drives continuous focus far motion.
    /// @return Result status code.
    [[nodiscard]] SendResult focusFar();

    /// @brief Halts active focus motion.
    /// @return Result status code.
    [[nodiscard]] SendResult focusStop();

    /// @brief Drives continuous iris open motion.
    /// @return Result status code.
    [[nodiscard]] SendResult irisOpen();

    /// @brief Drives continuous iris close motion.
    /// @return Result status code.
    [[nodiscard]] SendResult irisClose();

    /// @brief Halts active iris motion.
    /// @return Result status code.
    [[nodiscard]] SendResult irisStop();

    /// @brief Positions pan axis at absolute angle in centidegrees (opcode 0x4B).
    /// @param[in] centidegrees Angle in hundredths of degrees (0 - 35999).
    /// @return Result status code.
    [[nodiscard]] SendResult setPanAngle(std::uint16_t centidegrees);

    /// @brief Positions tilt axis at absolute angle in centidegrees (opcode 0x4D).
    /// @param[in] centidegrees Angle in hundredths of degrees (0 - 35999).
    /// @return Result status code.
    [[nodiscard]] SendResult setTiltAngle(std::uint16_t centidegrees);

    /// @brief Positions zoom axis at absolute raw position (opcode 0x4F).
    /// @param[in] position Raw zoom position value.
    /// @return Result status code.
    [[nodiscard]] SendResult setZoomPosition(std::uint16_t position);

    // Presets
    /// @brief Stores current camera position into specified preset (opcode 0x03).
    /// @param[in] presetId Preset number.
    /// @return Result status code.
    [[nodiscard]] SendResult setPreset(std::uint8_t presetId);

    /// @brief Clears defined preset by number (opcode 0x05).
    /// @param[in] presetId Preset number to clear.
    /// @return Result status code.
    [[nodiscard]] SendResult clearPreset(std::uint8_t presetId);

    /// @brief Commands camera to slew directly to saved preset (opcode 0x07).
    /// @param[in] presetId Preset number to recall.
    /// @return Result status code.
    [[nodiscard]] SendResult goToPreset(std::uint8_t presetId);

    /// @brief Recalls preset with explicit transition speed.
    /// @param[in] presetId Preset number.
    /// @param[in] speed Transition speed (0 for standard speed).
    /// @return Result status code.
    [[nodiscard]] SendResult goToPreset(std::uint8_t presetId, std::uint8_t speed);

    /// @brief Performs a 180-degree pan inversion (flip).
    /// @return Result status code.
    [[nodiscard]] SendResult flip180();

    /// @brief Sets current physical pan position as digital zero reference.
    /// @return Result status code.
    [[nodiscard]] SendResult zeroPan();

    /// @brief Initiates a preset scan touring defined presets with dwell time (opcode 0x47).
    /// @param[in] dwellSeconds Time in seconds to dwell at each visited preset.
    /// @return Result status code.
    [[nodiscard]] SendResult presetScan(std::uint8_t dwellSeconds);

    // Auxiliaries & Zones
    /// @brief Activates auxiliary relay switch (opcode 0x09).
    /// @param[in] auxId Auxiliary switch identifier.
    /// @return Result status code.
    [[nodiscard]] SendResult setAuxiliary(std::uint8_t auxId);

    /// @brief Deactivates auxiliary relay switch (opcode 0x0B).
    /// @param[in] auxId Auxiliary switch identifier.
    /// @return Result status code.
    [[nodiscard]] SendResult clearAuxiliary(std::uint8_t auxId);

    /// @brief Marks start position of camera zone (opcode 0x11).
    /// @param[in] zoneId Zone identifier (1-8).
    /// @return Result status code.
    [[nodiscard]] SendResult setZoneStart(std::uint8_t zoneId);

    /// @brief Marks end position of camera zone (opcode 0x13).
    /// @param[in] zoneId Zone identifier (1-8).
    /// @return Result status code.
    [[nodiscard]] SendResult setZoneEnd(std::uint8_t zoneId);

    /// @brief Toggles zone scanning mode on or off (opcodes 0x1B/0x1D).
    /// @param[in] enable True to enable zone scan, false to disable.
    /// @return Result status code.
    [[nodiscard]] SendResult setZoneScan(bool enable);

    // Patterns
    /// @brief Begins recording a manual tour pattern (opcode 0x1F).
    /// @param[in] patternId Pattern identifier (1-8).
    /// @return Result status code.
    [[nodiscard]] SendResult recordPatternStart(std::uint8_t patternId);

    /// @brief Concludes recording of the active tour pattern (opcode 0x21).
    /// @return Result status code.
    [[nodiscard]] SendResult recordPatternStop();

    /// @brief Executes a previously recorded tour pattern (opcode 0x23).
    /// @param[in] patternId Pattern identifier to replay.
    /// @return Result status code.
    [[nodiscard]] SendResult runPattern(std::uint8_t patternId);

    // Speeds & Options
    /// @brief Configures optical zoom travel speed (opcode 0x25).
    /// @param[in] speed Speed level (0 - 3).
    /// @return Result status code.
    [[nodiscard]] SendResult setZoomSpeed(std::uint8_t speed);

    /// @brief Configures optical focus adjustment speed (opcode 0x27).
    /// @param[in] speed Speed level (0 - 3).
    /// @return Result status code.
    [[nodiscard]] SendResult setFocusSpeed(std::uint8_t speed);

    /// @brief Sets autofocus operational mode (opcode 0x2B).
    /// @param[in] mode Off, On, or Auto.
    /// @return Result status code.
    [[nodiscard]] SendResult setAutoFocus(AutoMode mode);

    /// @brief Sets auto iris operational mode (opcode 0x2D).
    /// @param[in] mode Off, On, or Auto.
    /// @return Result status code.
    [[nodiscard]] SendResult setAutoIris(AutoMode mode);

    /// @brief Configures automatic gain control (opcode 0x2F).
    /// @param[in] mode Off, On, or Auto.
    /// @return Result status code.
    [[nodiscard]] SendResult setAgc(AutoMode mode);

    /// @brief Toggles backlight compensation mode (opcode 0x31).
    /// @param[in] state Off or On.
    /// @return Result status code.
    [[nodiscard]] SendResult setBacklightComp(SwitchState state);

    /// @brief Toggles automatic white balance mode (opcode 0x33).
    /// @param[in] state Off or On.
    /// @return Result status code.
    [[nodiscard]] SendResult setAutoWhiteBalance(SwitchState state);

    /// @brief Configures electronic shutter speed (opcode 0x37).
    /// @param[in] speed Discrete shutter speed denominator.
    /// @return Result status code.
    [[nodiscard]] SendResult setShutterSpeed(std::uint16_t speed);

    /// @brief Adjusts sensor gain level (opcode 0x3F).
    /// @param[in] gain Gain level.
    /// @return Result status code.
    [[nodiscard]] SendResult setGain(std::uint16_t gain);

    /// @brief Adjusts auto-iris response reference level (opcode 0x41).
    /// @param[in] level Target reference level.
    /// @return Result status code.
    [[nodiscard]] SendResult setAutoIrisLevel(std::uint8_t level);

    /// @brief Adjusts auto-iris peak detection threshold (opcode 0x43).
    /// @param[in] peak Peak detection threshold.
    /// @return Result status code.
    [[nodiscard]] SendResult setAutoIrisPeak(std::uint8_t peak);

    /// @brief Toggles AC line-lock phase delay mode (opcode 0x35).
    /// @param[in] state Off or On.
    /// @return Result status code.
    [[nodiscard]] SendResult setPhaseDelayMode(SwitchState state);

    /// @brief Adjusts AC line-lock phase angle in centidegrees (opcode 0x39).
    /// @param[in] centidegrees Phase angle in hundredths of degrees (0 - 35999).
    /// @return Result status code.
    [[nodiscard]] SendResult adjustLineLockDelay(std::uint16_t centidegrees);

    /// @brief Fine-tunes Red/Blue white balance color components (opcode 0x3B).
    /// @param[in] value Packed adjustment offset.
    /// @return Result status code.
    [[nodiscard]] SendResult adjustWhiteBalanceRB(std::uint16_t value);

    /// @brief Fine-tunes Magenta/Green white balance color components (opcode 0x3D).
    /// @param[in] value Packed adjustment offset.
    /// @return Result status code.
    [[nodiscard]] SendResult adjustWhiteBalanceMG(std::uint16_t value);

    /// @brief Sets digital/optical magnification level (opcode 0x5F).
    /// @param[in] value Magnification in hundredths.
    /// @param[in] relative If true, step relative to current magnification; if false, absolute.
    /// @return Result status code.
    [[nodiscard]] SendResult setMagnification(std::uint16_t value, bool relative = false);

    /// @brief Commands baud rate reconfiguration on the remote receiver (opcode 0x67).
    /// @param[in] baud Target baud rate (2400, 4800, 9600, etc.).
    /// @return Result status code.
    [[nodiscard]] SendResult setBaudRate(std::uint32_t baud);

    /// @brief Sets current physical pan and tilt position as absolute zero reference (opcode 0x49).
    /// @return Result status code.
    [[nodiscard]] SendResult setZeroPosition();

    // System Commands & Queries
    /// @brief Commands the receiver to reset all settings to factory defaults (opcode 0x29).
    /// @return Result status code.
    [[nodiscard]] SendResult resetDefaults();

    /// @brief Triggers a remote soft-reboot of the camera device (opcode 0x0F).
    /// @return Result status code.
    [[nodiscard]] SendResult remoteReset();

    /// @brief Sends a query for the current pan position angle (opcode 0x51).
    /// @return Result status code.
    [[nodiscard]] SendResult queryPan();

    /// @brief Sends a query for the current tilt position angle (opcode 0x53).
    /// @return Result status code.
    [[nodiscard]] SendResult queryTilt();

    /// @brief Sends a query for the current zoom position (opcode 0x55).
    /// @return Result status code.
    [[nodiscard]] SendResult queryZoom();

    /// @brief Sends a query for the current magnification multiplier (opcode 0x61).
    /// @return Result status code.
    [[nodiscard]] SendResult queryMagnification();

    /// @brief Queries remote hardware/software model identifier (opcode 0x6B).
    /// @return Result status code.
    [[nodiscard]] SendResult queryDeviceType();

    /// @brief Queries 18-byte extended camera state and status (opcode 0x45).
    /// @return Result status code.
    [[nodiscard]] SendResult queryGeneral();

    /// @brief Queries device diagnostic alarms and self-test flags (opcode 0x6F).
    /// @return Result status code.
    [[nodiscard]] SendResult queryDiagnostics();

    /// @brief Enqueues a full polling sequence across pan, tilt, zoom, and general status.
    /// @return Result status code.
    [[nodiscard]] SendResult queryAll();

    /// @brief Queries device serial number asynchronously via 18-byte query.
    /// @return Result status code.
    [[nodiscard]] SendResult querySerialNumber();

    /// @brief Activates RS-485 loopback echo mode for diagnostic testing (opcode 0x65).
    /// @return Result status code.
    [[nodiscard]] SendResult activateEchoMode();

    /// @brief Prepares the remote device for firmware download (opcode 0x57).
    /// @return Result status code.
    [[nodiscard]] SendResult prepareForDownload();

    /// @brief Instructs the remote device to start firmware download data reception (opcode 0x69).
    /// @return Result status code.
    [[nodiscard]] SendResult startDownload();

    /// @brief Writes a single ASCII character to the on-screen display (opcode 0x15).
    /// @param[in] column Screen column index (0-39).
    /// @param[in] asciiChar Printable ASCII character.
    /// @return Result status code.
    [[nodiscard]] SendResult writeCharacter(std::uint8_t column, char asciiChar);

    /// @brief Clears on-screen display characters (opcode 0x17).
    /// @return Result status code.
    [[nodiscard]] SendResult clearScreen();

    /// @brief Sends a dummy keep-alive NOP packet to the device (opcode 0x0D).
    /// @return Result status code.
    [[nodiscard]] SendResult sendDummy();

    /// @brief Commands screen coordinate repositioning (opcode 0x79).
    /// @param[in] panPercent Signed percentage from center (-100 to 100, positive = right).
    /// @param[in] tiltPercent Signed percentage from center (-100 to 100, positive = up).
    /// @param[in] relative If true, move is relative to current screen position; if false, absolute.
    /// @return Result status code.
    [[nodiscard]] SendResult screenMove(std::int8_t panPercent, std::int8_t tiltPercent, bool relative = false);

    /// @brief Sends a query to request the camera's application software version (opcode 0x73, sub 0x00).
    /// @return Result status code.
    [[nodiscard]] SendResult querySoftwareVersion();

    /// @brief Sends a query to request the camera's software build number (opcode 0x73, sub 0x02).
    /// @return Result status code.
    [[nodiscard]] SendResult queryBuildNumber();

    /// @brief Sets device clock seconds and synchronizes (opcode 0x77, sub 0x00).
    /// @param[in] seconds Seconds (0-59).
    /// @return Result status code.
    [[nodiscard]] SendResult setSeconds(std::uint8_t seconds);

    /// @brief Sets device clock hour and minute (opcode 0x77, sub 0x02).
    /// @param[in] hour Hour in 24-hour format (0-23).
    /// @param[in] minute Minute (0-59).
    /// @return Result status code.
    [[nodiscard]] SendResult setHourMinute(std::uint8_t hour, std::uint8_t minute);

    /// @brief Sets device calendar month and day (opcode 0x77, sub 0x04).
    /// @param[in] month Month (1-12).
    /// @param[in] day Day of month (1-31).
    /// @return Result status code.
    [[nodiscard]] SendResult setMonthDay(std::uint8_t month, std::uint8_t day);

    /// @brief Sets device calendar year (opcode 0x77, sub 0x06).
    /// @param[in] year Full year (e.g. 2026).
    /// @return Result status code.
    [[nodiscard]] SendResult setYear(std::uint16_t year);

    /// @brief Synchronizes device time by sending hour/minute followed by seconds (opcode 0x77).
    /// @param[in] hour Hour in 24-hour format (0-23).
    /// @param[in] minute Minute (0-59).
    /// @param[in] second Second (0-59).
    /// @return Result status code.
    [[nodiscard]] SendResult setTime(std::uint8_t hour, std::uint8_t minute, std::uint8_t second);

    /// @brief Sets device calendar date by sending month/day followed by year (opcode 0x77).
    /// @param[in] year Full year (e.g. 2026).
    /// @param[in] month Month (1-12).
    /// @param[in] day Day of month (1-31).
    /// @return Result status code.
    [[nodiscard]] SendResult setDate(std::uint16_t year, std::uint8_t month, std::uint8_t day);

    /// @brief Requests time or date component from device (opcode 0x77, odd sub-opcodes).
    /// @param[in] queryType ReportSeconds (0x01), ReportHourMinute (0x03), ReportMonthDay (0x05), or ReportYear (0x07).
    /// @return Result status code.
    [[nodiscard]] SendResult queryTime(TimeSubOpcode queryType);

    /// @brief Turns on or blinks an indicator LED via Auxiliary Set (opcode 0x09, sub 0x01).
    /// @param[in] ledIdOrColor Target LED ID or color code (0xFE Green, 0xFD Red, 0xFC Amber).
    /// @param[in] onTimeTenths Duration in tenths of seconds (0 = permanently ON).
    /// @return Result status code.
    [[nodiscard]] SendResult setAuxLed(std::uint8_t ledIdOrColor, std::uint8_t onTimeTenths = 0U);

    /// @brief Turns on or blinks an indicator LED via Auxiliary Set (opcode 0x09, sub 0x01).
    /// @param[in] ledColor Target LED color enum.
    /// @param[in] onTimeTenths Duration in tenths of seconds (0 = permanently ON).
    /// @return Result status code.
    [[nodiscard]] SendResult setAuxLed(AuxLedColor ledColor, std::uint8_t onTimeTenths = 0U)
    {
        return setAuxLed(static_cast<std::uint8_t>(ledColor), onTimeTenths);
    }

    /// @brief Clears or blinks off indicator LED via Auxiliary Clear (opcode 0x0B, sub 0x01).
    /// @param[in] ledIdOrColor Target LED ID or color code.
    /// @param[in] offTimeTenths Duration in tenths of seconds (0 = permanently OFF).
    /// @return Result status code.
    [[nodiscard]] SendResult clearAuxLed(std::uint8_t ledIdOrColor, std::uint8_t offTimeTenths = 0U);

    /// @brief Clears or blinks off indicator LED via Auxiliary Clear (opcode 0x0B, sub 0x01).
    /// @param[in] ledColor Target LED color enum.
    /// @param[in] offTimeTenths Duration in tenths of seconds (0 = permanently OFF).
    /// @return Result status code.
    [[nodiscard]] SendResult clearAuxLed(AuxLedColor ledColor, std::uint8_t offTimeTenths = 0U)
    {
        return clearAuxLed(static_cast<std::uint8_t>(ledColor), offTimeTenths);
    }

    // Everest Macro Commands & Queries
    /// @brief Queries azimuth zero offset via Everest macro (opcode 0x75, sub 0x00).
    /// @return Result status code.
    [[nodiscard]] SendResult queryAzimuthZero();

    /// @brief Sets maximum zoom limit on device via Everest macro (opcode 0x75, sub 0x02).
    /// @param[in] limitHundredths Zoom magnification limit in hundredths (e.g. 18400 = 184x).
    /// @return Result status code.
    [[nodiscard]] SendResult setZoomLimit(std::uint16_t limitHundredths);

    /// @brief Queries maximum zoom limit from device via Everest macro (opcode 0x75, sub 0x03).
    /// @return Result status code.
    [[nodiscard]] SendResult queryZoomLimit();

    /// @brief Queries alarm bitmask from device via Everest macro (opcode 0x75, sub 0x05).
    /// @return Result status code.
    [[nodiscard]] SendResult queryEverestAlarms();

    /// @brief Deletes a recorded pattern by ID via Everest macro (opcode 0x75, sub 0x07).
    /// @param[in] patternId Pattern identifier (1-8).
    /// @return Result status code.
    [[nodiscard]] SendResult deletePattern(std::uint8_t patternId);

    /// @brief Sets manual left pan limit in centidegrees via Everest macro (opcode 0x75, sub 0x08).
    /// @param[in] centidegrees Left limit angle in hundredths of degrees (0-35999).
    /// @return Result status code.
    [[nodiscard]] SendResult setManualLeftPanLimit(std::uint16_t centidegrees);

    /// @brief Sets manual right pan limit in centidegrees via Everest macro (opcode 0x75, sub 0x09).
    /// @param[in] centidegrees Right limit angle in hundredths of degrees (0-35999).
    /// @return Result status code.
    [[nodiscard]] SendResult setManualRightPanLimit(std::uint16_t centidegrees);

    /// @brief Sets scan left pan limit in centidegrees via Everest macro (opcode 0x75, sub 0x0A).
    /// @param[in] centidegrees Scan left limit angle in hundredths of degrees (0-35999).
    /// @return Result status code.
    [[nodiscard]] SendResult setScanLeftPanLimit(std::uint16_t centidegrees);

    /// @brief Sets scan right pan limit in centidegrees via Everest macro (opcode 0x75, sub 0x0B).
    /// @param[in] centidegrees Scan right limit angle in hundredths of degrees (0-35999).
    /// @return Result status code.
    [[nodiscard]] SendResult setScanRightPanLimit(std::uint16_t centidegrees);

    /// @brief Queries pan or scan limit value by ID via Everest macro (opcode 0x75, sub 0x0C).
    /// @param[in] limitId Limit identifier (ManualLeftPan, ManualRightPan, ScanLeftPan, ScanRightPan).
    /// @return Result status code.
    [[nodiscard]] SendResult queryLimit(EverestLimitId limitId);

    /// @brief Enables or disables manual and scan limits via Everest macro (opcode 0x75, sub 0x0E).
    /// @param[in] enable True to enable limits, false to disable.
    /// @return Result status code.
    [[nodiscard]] SendResult enableLimits(bool enable);

    /// @brief Queries defined presets bitmask for a group of 16 presets via Everest macro (opcode 0x75, sub 0x0F).
    /// @param[in] group Preset group index (0-15).
    /// @return Result status code.
    [[nodiscard]] SendResult queryDefinedPresets(std::uint8_t group = 0U);

    /// @brief Queries defined patterns bitmask for a group of 16 patterns via Everest macro (opcode 0x75, sub 0x11).
    /// @param[in] group Pattern group index (0-15).
    /// @return Result status code.
    [[nodiscard]] SendResult queryDefinedPatterns(std::uint8_t group = 0U);

private:
    [[nodiscard]] SendResult dispatch(
        const StandardFrame& frame, QueryRef query = {}, CommandPriority priority = CommandPriority::Normal);
    [[nodiscard]] SendResult dispatch(const std::vector<std::uint8_t>& frame, QueryRef query = {},
        CommandPriority priority = CommandPriority::Normal);

    std::shared_ptr<PelcoDSession> m_session;
    std::shared_ptr<MotionSafetyGuard> m_guard;
    std::shared_ptr<TelemetryCoordinator> m_telemetry;
};

} // namespace PelcoD
