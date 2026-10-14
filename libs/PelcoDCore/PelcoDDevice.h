#pragma once

/// @file PelcoDDevice.h
/// @brief Asynchronous thread-safe controller managing Pelco-D device communication.

#include "AsyncWatchdog.h"
#include "CallbackDispatcher.h"
#include "CallbackGate.h"
#include "Connection.h"
#include "DeviceStatus.h"
#include "FrameExtension.h"
#include "ITransport.h"
#include "MotionSafetyGuard.h"
#include "PacedCommandQueue.h"
#include "PelcoDSession.h"
#include "PelcoDStats.h"
#include "PelcoDTypes.h"
#include "ProtocolBuilder.h"
#include "ProtocolMetrics.h"
#include "ProtocolParser.h"
#include "PtzCommands.h"
#include "RetryPolicy.h"
#include "RxStreamAccumulator.h"
#include "TelemetryCoordinator.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace PelcoD {

/// @class PelcoDDevice
/// @brief High-level controller coordinating transport, paced command queue, and telemetry polling.
class PelcoDDevice : public std::enable_shared_from_this<PelcoDDevice> {
public:
    using StatusCallback = std::function<void(const DeviceStatus& status)>;
    using TrafficCallback = std::function<void(bool isTx, const std::vector<std::uint8_t>& frame)>;
    using TimeoutCallback = std::function<void(const std::string& queryTag)>;
    using QueryCompletedCallback
        = std::function<void(const std::string& queryTag, bool success, const DeviceStatus& status)>;
    using RetryCallback = std::function<void(
        const std::string& queryTag, std::uint32_t attempt, std::uint32_t maxRetries, std::chrono::milliseconds delay)>;
    using CommandDroppedCallback = std::function<void(const std::vector<std::uint8_t>& frame, SendResult reason)>;

    explicit PelcoDDevice(std::shared_ptr<ITransport> transport, std::uint8_t address = 1U);

    /// @brief Stops the controller and waits for every in-flight transport callback to finish.
    /// @details Equivalent to stop(). After the destructor returns no transport thread is executing
    ///          inside this object, and late invocations of stale transport callbacks are rejected.
    ///          Derived classes must not rely on virtual dispatch from internal threads; protocol
    ///          extensions are installed through setFrameExt() instead (review finding C3).
    /// @warning Destroying the device from inside one of its own callbacks is undefined behaviour.
    virtual ~PelcoDDevice();

    // Non-copyable, non-movable
    PelcoDDevice(const PelcoDDevice&) = delete;
    PelcoDDevice& operator=(const PelcoDDevice&) = delete;
    PelcoDDevice(PelcoDDevice&&) = delete;
    PelcoDDevice& operator=(PelcoDDevice&&) = delete;

    /// @brief Starts worker, rx, and polling loops, opening the underlying transport.
    /// @details Thread-safe and idempotent; returns true immediately if already running.
    /// @return True if started or already running; false if transport initialization failed.
    [[nodiscard]] bool start();

    /// @brief Stops worker, rx, and polling loops, and closes the transport.
    /// @details Thread-safe and idempotent; safe to call multiple times or if never started. First
    ///          closes the session's CallbackGate and waits (without holding the lifecycle lock) until
    ///          no transport callback is executing inside the device, then joins the worker and closes
    ///          the transport. May be called from inside a transport-thread callback.
    /// @note Emits one status notification with `connected == false` if the device was connected.
    void stop();
    [[nodiscard]] bool isConnected() const noexcept;

    void setAddress(std::uint8_t address);
    [[nodiscard]] std::uint8_t getAddress() const noexcept;

    Connection addStatusCallback(StatusCallback cb);
    Connection addTrafficCallback(TrafficCallback cb);

    /// @brief Registers a traffic callback with direction filtering.
    /// @param[in] cb Function invoked on packet transmission or reception.
    /// @param[in] notifyTx Whether to notify on outbound (TX) frames.
    /// @param[in] notifyRx Whether to notify on inbound (RX) frames.
    /// @return Connection object to manage the subscription.
    Connection addTrafficCallback(TrafficCallback cb, bool notifyTx, bool notifyRx);

    /// @brief Registers a traffic callback with address and direction filtering.
    /// @param[in] addressFilter Only notify on frames matching this device address.
    /// @param[in] cb Function invoked on matching frames.
    /// @param[in] notifyTx Whether to notify on outbound (TX) frames.
    /// @param[in] notifyRx Whether to notify on inbound (RX) frames.
    /// @return Connection object to manage the subscription.
    Connection addTrafficCallback(
        std::uint8_t addressFilter, TrafficCallback cb, bool notifyTx = true, bool notifyRx = true);

    Connection addTimeoutCallback(TimeoutCallback cb);

    /// @brief Registers a callback for query completion or timeout events.
    /// @param[in] cb Callback receiving queryTag, success flag, and DeviceStatus snapshot.
    /// @return Connection object to manage the subscription.
    Connection addQueryCompletedCallback(QueryCompletedCallback cb);

    /// @brief Registers a callback for query retry attempt notifications.
    /// @param[in] cb Callback receiving queryTag, attempt number (1-based), maxRetries, and scheduled delay.
    /// @return Connection object to manage the subscription.
    Connection addRetryCallback(RetryCallback cb);

    using QueryLatencyCallback
        = std::function<void(const std::string& queryTag, std::chrono::microseconds duration, bool success)>;

    /// @brief Registers a callback for query round-trip latency and outcome notifications.
    /// @param[in] cb Callback receiving queryTag, microsecond duration, and success flag.
    /// @return Connection object to manage the subscription.
    Connection addQueryLatencyCallback(QueryLatencyCallback cb);

    /// @brief Registers a callback for dropped or rejected commands.
    /// @param[in] cb Callback receiving rejected/evicted frame and SendResult reason.
    /// @return Connection object to manage the subscription.
    Connection addCommandDroppedCallback(CommandDroppedCallback cb);

    /// @brief Removes a status callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    [[nodiscard]] bool removeStatusCallback(CallbackId id);

    /// @brief Removes a traffic callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    [[nodiscard]] bool removeTrafficCallback(CallbackId id);

    /// @brief Removes a timeout callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    [[nodiscard]] bool removeTimeoutCallback(CallbackId id);

    /// @brief Removes a query completed callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    [[nodiscard]] bool removeQueryCompletedCallback(CallbackId id);

    /// @brief Removes a retry callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    [[nodiscard]] bool removeRetryCallback(CallbackId id);

    /// @brief Removes a query latency callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    [[nodiscard]] bool removeQueryLatencyCallback(CallbackId id);

    /// @brief Removes a command dropped callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    [[nodiscard]] bool removeCommandDroppedCallback(CallbackId id);

    /// @brief Removes all registered status, traffic, and timeout callbacks.
    virtual void clearCallbacks();

    [[nodiscard]] DeviceStatus getStatus() const;
    [[nodiscard]] DeviceInfo getInfo() const;

    /// @brief Captures transport-layer and kernel-level communication statistics.
    /// @return Aggregated snapshot containing generic and kernel-level metrics.
    [[nodiscard]] Transport::TransportStatsSnapshot getTransportStats() const;

    /// @brief Creates an independent virtual transport channel for concurrent services (e.g. BusScanner).
    /// @return Shared pointer to an independent ITransport channel, or nullptr if transport is null.
    [[nodiscard]] std::shared_ptr<ITransport> createVirtualChannel();

    /// @brief Captures real-time Pelco-D protocol telemetry and error counters.
    /// @return Protocol statistics snapshot with query latency and framing metrics.
    [[nodiscard]] PelcoDProtocolStats getProtocolStats() const;

    /// @brief Resets protocol performance and diagnostic counters.
    void resetProtocolStats() noexcept;

    void setTelemetryPolling(bool enable, std::uint32_t intervalMs = 1000U) noexcept;
    [[nodiscard]] bool getTelemetryPolling() const noexcept;
    void setQueryTimeoutMs(std::uint32_t timeoutMs) noexcept;
    [[nodiscard]] std::uint32_t getQueryTimeoutMs() const noexcept;

    void setRetryConfig(const RetryConfig& config) noexcept;
    [[nodiscard]] RetryConfig getRetryConfig() const noexcept;

    /// @brief Configures the motion safety dead-man watchdog timeout.
    /// @param[in] timeout Maximum duration allowed without motion refresh before auto-stop (0ms = disabled).
    void setDeadManTimeout(std::chrono::milliseconds timeout) noexcept;

    /// @brief Retrieves the configured motion safety dead-man watchdog timeout.
    /// @return Configured watchdog duration.
    [[nodiscard]] std::chrono::milliseconds getDeadManTimeout() const noexcept;

    /// @brief Configures the comprehensive motion safety policy.
    /// @param[in] policy Safety policy configuration.
    void setMotionSafetyPolicy(const MotionSafetyPolicy& policy) noexcept;

    /// @brief Retrieves the active motion safety policy snapshot.
    /// @return Active policy.
    [[nodiscard]] MotionSafetyPolicy getMotionSafetyPolicy() const noexcept;

    using MotionSafetyCallback = CallbackDispatcher::MotionSafetyCallback;

    /// @brief Registers an observer for motion safety interventions (dead-man timeouts, runaway limits).
    /// @param[in] cb Callback receiving the intervention event and affected motion axes.
    /// @return Connection object to manage the subscription.
    [[nodiscard]] Connection addMotionSafetyCallback(MotionSafetyCallback cb);

    /// @brief Removes a motion safety intervention callback by identifier.
    /// @param[in] id Subscription identifier.
    /// @return True if callback was found and removed; false otherwise.
    [[nodiscard]] bool removeMotionSafetyCallback(CallbackId id);

    // Motion & Positioning
    [[nodiscard]] SendResult panLeft(std::uint8_t speed);
    [[nodiscard]] SendResult panRight(std::uint8_t speed);
    [[nodiscard]] SendResult tiltUp(std::uint8_t speed);
    [[nodiscard]] SendResult tiltDown(std::uint8_t speed);
    [[nodiscard]] SendResult stopMotion();

    [[nodiscard]] SendResult move(
        PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed);

    /// @brief Drives combined motion across Pan, Tilt, Zoom, Focus, and Iris (M12).
    /// @param[in] panDir Pan direction (Left, Right, Stop).
    /// @param[in] panSpeed Pan speed (0 - 64).
    /// @param[in] tiltDir Tilt direction (Up, Down, Stop).
    /// @param[in] tiltSpeed Tilt speed (0 - 63).
    /// @param[in] zoom Zoom action (Tele, Wide, Stop).
    /// @param[in] focus Focus action (Near, Far, Stop).
    /// @param[in] iris Iris action (Open, Close, Stop).
    [[nodiscard]] SendResult move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir,
        std::uint8_t tiltSpeed, ZoomAction zoom, FocusAction focus = FocusAction::Stop,
        IrisAction iris = IrisAction::Stop);

    /// @brief Controls device operational power state (opcode 0x88 On, 0x08 Off) (M12).
    /// @param[in] on True for Power On, false for Power Off.
    [[nodiscard]] SendResult setPower(bool on);

    /// @brief Configures auto or manual scan mode (opcode 0x90 Auto, 0x98 Manual) (M12).
    /// @param[in] enable True to initiate auto scan, false for manual scan.
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

    /// @brief Recalls preset with explicit transition speed.
    /// @param[in] presetId Preset number.
    /// @param[in] speed Transition speed (0 for standard speed).
    [[nodiscard]] SendResult goToPreset(std::uint8_t presetId, std::uint8_t speed);

    [[nodiscard]] SendResult flip180();
    [[nodiscard]] SendResult zeroPan();

    /// @brief Initiates a preset scan touring defined presets with dwell time (opcode 0x47).
    /// @param[in] dwellSeconds Time in seconds to dwell at each visited preset.
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

    /// @brief Sets absolute focus position in status telemetry.
    /// @param[in] pos Focus coordinate value.
    [[nodiscard]] SendResult setFocusPosition(std::uint16_t pos);

    /// @brief Sets absolute iris position in status telemetry.
    /// @param[in] pos Iris aperture coordinate value.
    [[nodiscard]] SendResult setIrisPosition(std::uint16_t pos);

    /// @brief Sets factory serial number on device identification model.
    /// @param[in] serial Serial number string.
    [[nodiscard]] SendResult setSerialNumber(const std::string& serial);

    /// @brief Queries device serial number asynchronously via 18-byte query.
    [[nodiscard]] SendResult querySerialNumber();

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

    /// @brief Activates RS-485 loopback echo mode for diagnostic testing (opcode 0x65).
    [[nodiscard]] SendResult activateEchoMode();

    /// @brief Prepares the remote device for firmware download (opcode 0x57).
    [[nodiscard]] SendResult prepareForDownload();

    /// @brief Instructs the remote device to start firmware download data reception (opcode 0x69).
    [[nodiscard]] SendResult startDownload();

    /// @brief Writes a single ASCII character to the on-screen display (opcode 0x15).
    /// @param[in] column Screen column index (0-39).
    /// @param[in] asciiChar Printable ASCII character.
    [[nodiscard]] SendResult writeCharacter(std::uint8_t column, char asciiChar);

    /// @brief Clears on-screen display characters (opcode 0x17).
    [[nodiscard]] SendResult clearScreen();

    /// @brief Sends a dummy keep-alive NOP packet to the device (opcode 0x0D).
    [[nodiscard]] SendResult sendDummy();

    /// @brief Commands screen coordinate repositioning (opcode 0x79).
    /// @param[in] panPercent Signed percentage from center (-100 to 100, positive = right).
    /// @param[in] tiltPercent Signed percentage from center (-100 to 100, positive = up).
    /// @param[in] relative If true, move is relative to current screen position; if false, absolute.
    [[nodiscard]] SendResult screenMove(std::int8_t panPercent, std::int8_t tiltPercent, bool relative = false);

    /// @brief Sends a query to request the camera's application software version (opcode 0x73, sub 0x00).
    [[nodiscard]] SendResult querySoftwareVersion();

    /// @brief Sends a query to request the camera's software build number (opcode 0x73, sub 0x02).
    [[nodiscard]] SendResult queryBuildNumber();

    /// @brief Sets device clock seconds and synchronizes (opcode 0x77, sub 0x00).
    /// @param[in] seconds Seconds (0-59).
    [[nodiscard]] SendResult setSeconds(std::uint8_t seconds);

    /// @brief Sets device clock hour and minute (opcode 0x77, sub 0x02).
    /// @param[in] hour Hour in 24-hour format (0-23).
    /// @param[in] minute Minute (0-59).
    [[nodiscard]] SendResult setHourMinute(std::uint8_t hour, std::uint8_t minute);

    /// @brief Sets device calendar month and day (opcode 0x77, sub 0x04).
    /// @param[in] month Month (1-12).
    /// @param[in] day Day of month (1-31).
    [[nodiscard]] SendResult setMonthDay(std::uint8_t month, std::uint8_t day);

    /// @brief Sets device calendar year (opcode 0x77, sub 0x06).
    /// @param[in] year Full year (e.g. 2026).
    [[nodiscard]] SendResult setYear(std::uint16_t year);

    /// @brief Synchronizes device time by sending hour/minute followed by seconds (opcode 0x77).
    /// @param[in] hour Hour in 24-hour format (0-23).
    /// @param[in] minute Minute (0-59).
    /// @param[in] second Second (0-59).
    [[nodiscard]] SendResult setTime(std::uint8_t hour, std::uint8_t minute, std::uint8_t second);

    /// @brief Sets device calendar date by sending month/day followed by year (opcode 0x77).
    /// @param[in] year Full year (e.g. 2026).
    /// @param[in] month Month (1-12).
    /// @param[in] day Day of month (1-31).
    [[nodiscard]] SendResult setDate(std::uint16_t year, std::uint8_t month, std::uint8_t day);

    /// @brief Requests time or date component from device (opcode 0x77, odd sub-opcodes).
    /// @param[in] queryType ReportSeconds (0x01), ReportHourMinute (0x03), ReportMonthDay (0x05), or ReportYear (0x07).
    [[nodiscard]] SendResult queryTime(TimeSubOpcode queryType);

    /// @brief Turns on or blinks an indicator LED via Auxiliary Set (opcode 0x09, sub 0x01).
    /// @param[in] ledIdOrColor Target LED ID or color code (0xFE Green, 0xFD Red, 0xFC Amber).
    /// @param[in] onTimeTenths Duration in tenths of seconds (0 = permanently ON).
    [[nodiscard]] SendResult setAuxLed(std::uint8_t ledIdOrColor, std::uint8_t onTimeTenths = 0U);

    /// @brief Turns on or blinks an indicator LED via Auxiliary Set (opcode 0x09, sub 0x01).
    /// @param[in] ledColor Target LED color enum.
    /// @param[in] onTimeTenths Duration in tenths of seconds (0 = permanently ON).
    [[nodiscard]] SendResult setAuxLed(AuxLedColor ledColor, std::uint8_t onTimeTenths = 0U)
    {
        return setAuxLed(static_cast<std::uint8_t>(ledColor), onTimeTenths);
    }

    /// @brief Clears or blinks off indicator LED via Auxiliary Clear (opcode 0x0B, sub 0x01).
    /// @param[in] ledIdOrColor Target LED ID or color code.
    /// @param[in] offTimeTenths Duration in tenths of seconds (0 = permanently OFF).
    [[nodiscard]] SendResult clearAuxLed(std::uint8_t ledIdOrColor, std::uint8_t offTimeTenths = 0U);

    /// @brief Clears or blinks off indicator LED via Auxiliary Clear (opcode 0x0B, sub 0x01).
    /// @param[in] ledColor Target LED color enum.
    /// @param[in] offTimeTenths Duration in tenths of seconds (0 = permanently OFF).
    [[nodiscard]] SendResult clearAuxLed(AuxLedColor ledColor, std::uint8_t offTimeTenths = 0U)
    {
        return clearAuxLed(static_cast<std::uint8_t>(ledColor), offTimeTenths);
    }

    /// @brief Queries azimuth zero offset via Everest macro (opcode 0x75, sub 0x00).
    [[nodiscard]] SendResult queryAzimuthZero();

    /// @brief Sets maximum zoom limit on device via Everest macro (opcode 0x75, sub 0x02).
    /// @param[in] limitHundredths Zoom magnification limit in hundredths (e.g. 18400 = 184x).
    [[nodiscard]] SendResult setZoomLimit(std::uint16_t limitHundredths);

    /// @brief Queries maximum zoom limit from device via Everest macro (opcode 0x75, sub 0x03).
    [[nodiscard]] SendResult queryZoomLimit();

    /// @brief Queries alarm bitmask from device via Everest macro (opcode 0x75, sub 0x05).
    [[nodiscard]] SendResult queryEverestAlarms();

    /// @brief Deletes a recorded pattern by ID via Everest macro (opcode 0x75, sub 0x07).
    /// @param[in] patternId Pattern identifier (1-8).
    [[nodiscard]] SendResult deletePattern(std::uint8_t patternId);

    /// @brief Sets manual left pan limit in centidegrees via Everest macro (opcode 0x75, sub 0x08).
    /// @param[in] centidegrees Left limit angle in hundredths of degrees (0-35999).
    [[nodiscard]] SendResult setManualLeftPanLimit(std::uint16_t centidegrees);

    /// @brief Sets manual right pan limit in centidegrees via Everest macro (opcode 0x75, sub 0x09).
    /// @param[in] centidegrees Right limit angle in hundredths of degrees (0-35999).
    [[nodiscard]] SendResult setManualRightPanLimit(std::uint16_t centidegrees);

    /// @brief Sets scan left pan limit in centidegrees via Everest macro (opcode 0x75, sub 0x0A).
    /// @param[in] centidegrees Scan left limit angle in hundredths of degrees (0-35999).
    [[nodiscard]] SendResult setScanLeftPanLimit(std::uint16_t centidegrees);

    /// @brief Sets scan right pan limit in centidegrees via Everest macro (opcode 0x75, sub 0x0B).
    /// @param[in] centidegrees Scan right limit angle in hundredths of degrees (0-35999).
    [[nodiscard]] SendResult setScanRightPanLimit(std::uint16_t centidegrees);

    /// @brief Queries pan or scan limit value by ID via Everest macro (opcode 0x75, sub 0x0C).
    /// @param[in] limitId Limit identifier (ManualLeftPan, ManualRightPan, ScanLeftPan, ScanRightPan).
    [[nodiscard]] SendResult queryLimit(EverestLimitId limitId);

    /// @brief Enables or disables manual and scan limits via Everest macro (opcode 0x75, sub 0x0E).
    /// @param[in] enable True to enable limits, false to disable.
    [[nodiscard]] SendResult enableLimits(bool enable);

    /// @brief Queries defined presets bitmask for a group of 16 presets via Everest macro (opcode 0x75, sub 0x0F).
    /// @param[in] group Preset group index (0-15).
    [[nodiscard]] SendResult queryDefinedPresets(std::uint8_t group = 0U);

    /// @brief Queries defined patterns bitmask for a group of 16 patterns via Everest macro (opcode 0x75, sub 0x11).
    /// @param[in] group Pattern group index (0-15).
    [[nodiscard]] SendResult queryDefinedPatterns(std::uint8_t group = 0U);

    /// @brief Asynchronously queries current pan angle with timeout.
    /// @param[in] timeout Maximum wait duration.
    /// @return Future resolving to pan angle in centidegrees, or throwing std::runtime_error on failure/timeout.
    [[nodiscard]] std::future<std::uint16_t> queryPanAsync(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    /// @brief Asynchronously queries current tilt angle with timeout.
    /// @param[in] timeout Maximum wait duration.
    /// @return Future resolving to tilt angle in centidegrees, or throwing std::runtime_error on failure/timeout.
    [[nodiscard]] std::future<std::uint16_t> queryTiltAsync(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    /// @brief Asynchronously queries optical zoom position with timeout.
    /// @param[in] timeout Maximum wait duration.
    /// @return Future resolving to raw zoom position, or throwing std::runtime_error on failure/timeout.
    [[nodiscard]] std::future<std::uint16_t> queryZoomAsync(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    /// @brief Asynchronously queries full device status with timeout.
    /// @param[in] timeout Maximum wait duration.
    /// @return Future resolving to updated DeviceStatus, or throwing std::runtime_error on failure/timeout.
    [[nodiscard]] std::future<DeviceStatus> queryStatusAsync(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    /// @brief Asynchronously queries application software version with timeout.
    /// @param[in] timeout Maximum wait duration.
    /// @return Future resolving to pair of {major, minor} version bytes, or throwing std::runtime_error on
    /// failure/timeout.
    [[nodiscard]] std::future<std::pair<std::uint8_t, std::uint8_t>> querySoftwareVersionAsync(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    /// @brief Asynchronously queries firmware build number with timeout.
    /// @param[in] timeout Maximum wait duration.
    /// @return Future resolving to 16-bit build number, or throwing std::runtime_error on failure/timeout.
    [[nodiscard]] std::future<std::uint16_t> queryBuildNumberAsync(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    /// @brief Asynchronously queries azimuth zero offset via Everest macro with timeout.
    /// @param[in] timeout Maximum wait duration.
    /// @return Future resolving to azimuth zero offset in centidegrees.
    [[nodiscard]] std::future<std::uint16_t> queryAzimuthZeroAsync(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    /// @brief Asynchronously queries maximum zoom limit via Everest macro with timeout.
    /// @param[in] timeout Maximum wait duration.
    /// @return Future resolving to zoom magnification limit in hundredths.
    [[nodiscard]] std::future<std::uint16_t> queryZoomLimitAsync(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    /// @brief Internal watchdog accessor used for async query timeout scheduling.
    /// @return Reference to the device's AsyncWatchdog.
    [[nodiscard]] AsyncWatchdog& getAsyncWatchdog() noexcept;

    [[nodiscard]] SendResult sendRawFrame(const std::vector<std::uint8_t>& frame);
    [[nodiscard]] SendResult sendQueryFrame(const std::vector<std::uint8_t>& frame, std::string queryTag = "Query");

protected:
    [[nodiscard]] SendResult enqueueCommand(const std::vector<std::uint8_t>& frame, std::string queryTag = "",
        CommandPriority priority = CommandPriority::Normal);

    /// @brief Installs a vendor protocol extension consulted for every received frame.
    /// @details The device shares ownership of @p ext, so the extension outlives every in-flight RX
    ///          callback even while a derived device's own members are being destroyed. Intended to be
    ///          called from a derived constructor.
    /// @param[in] ext Extension to install (may be null to remove).
    /// @return True if installed; false if a session is active (call before start()).
    [[nodiscard]] bool setFrameExt(std::shared_ptr<IFrameExtension> ext);

    void resolveQueryWait();

private:
    void initSessionHandlers();
    void pollStep();
    [[nodiscard]] std::chrono::steady_clock::time_point getPollDeadline() const noexcept;

    std::chrono::steady_clock::time_point m_nextPollTime { std::chrono::steady_clock::now() };
    bool m_lastPollingEnabled { false };

    std::shared_ptr<PelcoDSession> m_session;
    std::shared_ptr<MotionSafetyGuard> m_safetyGuard;
    std::shared_ptr<TelemetryCoordinator> m_telemetry;
    std::shared_ptr<CallbackDispatcher> m_dispatcher;
    std::unique_ptr<ProtocolMetrics> m_metrics;
    std::unique_ptr<PtzCommands> m_commands;
};

} // namespace PelcoD
