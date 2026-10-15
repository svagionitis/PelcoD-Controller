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
class PelcoDDevice : public PtzCommands, public std::enable_shared_from_this<PelcoDDevice> {
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
    [[nodiscard]] SendResult sendQueryFrame(const std::vector<std::uint8_t>& frame, QueryRef query = "Query");

protected:
    [[nodiscard]] SendResult enqueueCommand(const std::vector<std::uint8_t>& frame, QueryRef query = {},
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
    struct DeviceCoreBundle {
        std::shared_ptr<PelcoDSession> session;
        std::shared_ptr<MotionSafetyGuard> safetyGuard;
        std::shared_ptr<TelemetryCoordinator> telemetry;
        std::shared_ptr<CallbackDispatcher> dispatcher;
        std::unique_ptr<ProtocolMetrics> metrics;
    };

    explicit PelcoDDevice(DeviceCoreBundle bundle, std::uint8_t address);

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
};

} // namespace PelcoD
