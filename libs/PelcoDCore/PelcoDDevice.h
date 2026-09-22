#pragma once

/// @file PelcoDDevice.h
/// @brief Asynchronous thread-safe controller managing Pelco-D device communication.

#include "Connection.h"
#include "DeviceStatus.h"
#include "ITransport.h"
#include "PacedCommandQueue.h"
#include "PelcoDTypes.h"
#include "ProtocolBuilder.h"
#include "ProtocolParser.h"
#include "RetryPolicy.h"
#include "RxStreamAccumulator.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PelcoD {

/// @class PelcoDDevice
/// @brief High-level controller coordinating transport, paced command queue, and telemetry polling.
class PelcoDDevice {
public:
    using StatusCallback = std::function<void(const DeviceStatus& status)>;
    using TrafficCallback = std::function<void(bool isTx, const std::vector<std::uint8_t>& frame)>;
    using TimeoutCallback = std::function<void(const std::string& queryTag)>;
    using QueryCompletedCallback
        = std::function<void(const std::string& queryTag, bool success, const DeviceStatus& status)>;
    using RetryCallback = std::function<void(
        const std::string& queryTag, std::uint32_t attempt, std::uint32_t maxRetries, std::chrono::milliseconds delay)>;

    explicit PelcoDDevice(std::shared_ptr<ITransport> transport, std::uint8_t address = 1U);
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
    /// @details Thread-safe and idempotent; safe to call multiple times or if never started.
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

    /// @brief Removes a status callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    bool removeStatusCallback(CallbackId id);

    /// @brief Removes a traffic callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    bool removeTrafficCallback(CallbackId id);

    /// @brief Removes a timeout callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    bool removeTimeoutCallback(CallbackId id);

    /// @brief Removes a query completed callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    bool removeQueryCompletedCallback(CallbackId id);

    /// @brief Removes a retry callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    bool removeRetryCallback(CallbackId id);

    /// @brief Removes a query latency callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    bool removeQueryLatencyCallback(CallbackId id);

    /// @brief Removes all registered status, traffic, and timeout callbacks.
    virtual void clearCallbacks();

    [[nodiscard]] DeviceStatus getStatus() const;
    [[nodiscard]] DeviceInfo getInfo() const;

    void setTelemetryPolling(bool enable, std::uint32_t intervalMs = 1000U) noexcept;
    [[nodiscard]] bool getTelemetryPolling() const noexcept;
    void setQueryTimeoutMs(std::uint32_t timeoutMs) noexcept;
    [[nodiscard]] std::uint32_t getQueryTimeoutMs() const noexcept;

    void setRetryConfig(const RetryConfig& config) noexcept;
    [[nodiscard]] RetryConfig getRetryConfig() const noexcept;

    // Motion & Positioning
    void panLeft(std::uint8_t speed);
    void panRight(std::uint8_t speed);
    void tiltUp(std::uint8_t speed);
    void tiltDown(std::uint8_t speed);
    void stopMotion();

    void move(PanDirection panDir, std::uint8_t panSpeed, TiltDirection tiltDir, std::uint8_t tiltSpeed);

    void zoomTele();
    void zoomWide();
    void zoomStop();

    void focusNear();
    void focusFar();
    void focusStop();

    void irisOpen();
    void irisClose();
    void irisStop();

    void setPanAngle(std::uint16_t centidegrees);
    void setTiltAngle(std::uint16_t centidegrees);
    void setZoomPosition(std::uint16_t position);

    // Presets
    void setPreset(std::uint8_t presetId);
    void clearPreset(std::uint8_t presetId);
    void goToPreset(std::uint8_t presetId);
    void flip180();
    void zeroPan();

    // Auxiliaries & Zones
    void setAuxiliary(std::uint8_t auxId);
    void clearAuxiliary(std::uint8_t auxId);
    void setZoneStart(std::uint8_t zoneId);
    void setZoneEnd(std::uint8_t zoneId);
    void setZoneScan(bool enable);

    // Patterns
    void recordPatternStart(std::uint8_t patternId);
    void recordPatternStop();
    void runPattern(std::uint8_t patternId);

    // Speeds & Options
    void setZoomSpeed(std::uint8_t speed);
    void setFocusSpeed(std::uint8_t speed);
    void setAutoFocus(AutoMode mode);
    void setAutoIris(AutoMode mode);
    void setAgc(AutoMode mode);
    void setBacklightComp(SwitchState state);
    void setAutoWhiteBalance(SwitchState state);
    void setShutterSpeed(std::uint16_t speed);
    void setGain(std::uint16_t gain);
    void setAutoIrisLevel(std::uint8_t level);
    void setAutoIrisPeak(std::uint8_t peak);
    void setPhaseDelayMode(SwitchState state);
    void adjustLineLockDelay(std::uint16_t centidegrees);
    void adjustWhiteBalanceRB(std::uint16_t value);
    void adjustWhiteBalanceMG(std::uint16_t value);
    void setMagnification(std::uint16_t value, bool relative = false);
    void setBaudRate(std::uint32_t baud);
    void setZeroPosition();

    // System Commands & Queries
    void resetDefaults();
    void remoteReset();
    void queryPan();
    void queryTilt();
    void queryZoom();
    void queryMagnification();
    void queryDeviceType();
    void queryGeneral();
    void queryDiagnostics();
    void queryAll();

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

    void sendRawFrame(const std::vector<std::uint8_t>& frame);
    void sendQueryFrame(const std::vector<std::uint8_t>& frame, std::string queryTag = "Query");

protected:
    void enqueueCommand(const std::vector<std::uint8_t>& frame, std::string queryTag = "",
        CommandPriority priority = CommandPriority::Normal);
    virtual void dispatchFrame(const std::vector<std::uint8_t>& frame);
    [[nodiscard]] virtual bool isResponseMatchingQuery(
        const std::string& queryTag, const std::vector<std::uint8_t>& frame) const noexcept;
    void resolveQueryWait();

private:
    void workerLoop();
    void onDataReceived(const std::vector<std::uint8_t>& data);
    void checkQueryTimeout();

    std::shared_ptr<ITransport> m_transport;
    std::atomic<std::uint8_t> m_address { 1U };

    mutable std::recursive_mutex m_lifecycleMutex;
    std::atomic<bool> m_running { false };
    std::thread m_workerThread;

    PacedCommandQueue m_queue;
    RxStreamAccumulator m_rxAccumulator;

    std::atomic<bool> m_telemetryPolling { false };
    std::atomic<std::uint32_t> m_pollIntervalMs { 1000U };
    std::atomic<std::uint32_t> m_queryTimeoutMs { 1000U };

    mutable std::mutex m_retryMutex;
    RetryConfig m_retryConfig {};

    mutable std::mutex m_statusMutex;
    DeviceStatus m_status {};
    DeviceInfo m_info {};

    std::atomic<bool> m_awaitingResponse { false };
    std::atomic<bool> m_abortQueryWait { false };
    std::condition_variable m_responseCv;
    std::string m_pendingQueryTag;
    std::chrono::steady_clock::time_point m_querySentTime;

    template <typename CallbackT> struct CallbackEntry {
        CallbackId id { 0U };
        CallbackT cb {};
    };

    struct CallbackState {
        mutable std::mutex mutex;
        std::atomic<CallbackId> nextId { 1U };
        std::shared_ptr<const std::vector<CallbackEntry<StatusCallback>>> statusCallbacks {
            std::make_shared<const std::vector<CallbackEntry<StatusCallback>>>()
        };
        std::shared_ptr<const std::vector<CallbackEntry<TrafficCallback>>> trafficCallbacks {
            std::make_shared<const std::vector<CallbackEntry<TrafficCallback>>>()
        };
        std::shared_ptr<const std::vector<CallbackEntry<TimeoutCallback>>> timeoutCallbacks {
            std::make_shared<const std::vector<CallbackEntry<TimeoutCallback>>>()
        };
        std::shared_ptr<const std::vector<CallbackEntry<QueryCompletedCallback>>> queryCompletedCallbacks {
            std::make_shared<const std::vector<CallbackEntry<QueryCompletedCallback>>>()
        };
        std::shared_ptr<const std::vector<CallbackEntry<RetryCallback>>> retryCallbacks {
            std::make_shared<const std::vector<CallbackEntry<RetryCallback>>>()
        };
        std::shared_ptr<const std::vector<CallbackEntry<QueryLatencyCallback>>> queryLatencyCallbacks {
            std::make_shared<const std::vector<CallbackEntry<QueryLatencyCallback>>>()
        };

        template <typename CallbackT>
        static bool removeCallbackEntry(
            std::shared_ptr<const std::vector<CallbackEntry<CallbackT>>>& list, CallbackId id, std::mutex& mtx)
        {
            std::scoped_lock lock(mtx);
            const auto& current = *list;
            auto it = std::find_if(current.begin(), current.end(), [id](const auto& entry) { return entry.id == id; });
            if (it == current.end()) {
                return false;
            }
            auto nextList = std::make_shared<std::vector<CallbackEntry<CallbackT>>>();
            nextList->reserve(current.size() - 1U);
            for (const auto& entry : current) {
                if (entry.id != id) {
                    nextList->push_back(entry);
                }
            }
            list = std::move(nextList);
            return true;
        }

        bool removeStatus(CallbackId id);
        bool removeTraffic(CallbackId id);
        bool removeTimeout(CallbackId id);
        bool removeQueryCompleted(CallbackId id);
        bool removeRetry(CallbackId id);
        bool removeQueryLatency(CallbackId id);
        void clear();
    };

    template <typename CallbackT, typename RemoveMemFn>
    Connection registerCallbackHelper(CallbackT cb,
        std::shared_ptr<const std::vector<CallbackEntry<CallbackT>>> CallbackState::*listMember, RemoveMemFn removeFn);

    std::shared_ptr<CallbackState> m_callbackState { std::make_shared<CallbackState>() };
};

} // namespace PelcoD
