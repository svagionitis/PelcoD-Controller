#pragma once

/// @file PelcoDSession.h
/// @brief Core communication session executing transport I/O, framing, queuing, and query correlation.

#include "AsyncWatchdog.h"
#include "CallbackGate.h"
#include "FrameExtension.h"
#include "ITransport.h"
#include "PacedCommandQueue.h"
#include "PelcoDTypes.h"
#include "RetryPolicy.h"
#include "RxStreamAccumulator.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace PelcoD {

/// @class PelcoDSession
/// @brief Core asynchronous session coordinating transport lifecycle, framing, and command dispatch.
/// @details Owns a dedicated worker thread, priority command queue, stream accumulator, and admission
///          barrier, executing outbound command pacing and correlating inbound query responses.
class PelcoDSession final : public std::enable_shared_from_this<PelcoDSession> {
public:
    using FrameHandler = std::function<void(const std::vector<std::uint8_t>& frame, const std::string& queryTag,
        std::chrono::microseconds durationUs, std::optional<EverestLimitId> limitId)>;
    using TrafficHandler = std::function<void(bool isTx, const std::vector<std::uint8_t>& frame)>;
    using TimeoutHandler = std::function<void(const std::string& queryTag)>;
    using QueryFailedHandler = std::function<void(const std::string& queryTag, std::chrono::microseconds durationUs)>;
    using RetryHandler = std::function<void(
        const std::string& queryTag, std::uint32_t attempt, std::uint32_t maxRetries, std::chrono::milliseconds delay)>;
    using StateHandler = std::function<void(TransportState state, const std::string& msg)>;
    using PollStepHandler = std::function<void()>;
    using PollDeadlineHandler = std::function<std::chrono::steady_clock::time_point()>;
    using StatusProvider = std::function<DeviceStatus()>;

    /// @brief Constructs a communication session.
    /// @param[in] transport Underlying communication transport interface.
    /// @param[in] address Logical RS-485 device address (1-31).
    explicit PelcoDSession(std::shared_ptr<ITransport> transport, std::uint8_t address = 1U);

    /// @brief Destructor stopping the worker strand and closing the session admission gate.
    ~PelcoDSession();

    PelcoDSession(const PelcoDSession&) = delete;
    PelcoDSession& operator=(const PelcoDSession&) = delete;
    PelcoDSession(PelcoDSession&&) = delete;
    PelcoDSession& operator=(PelcoDSession&&) = delete;

    /// @brief Starts session worker thread and opens underlying transport.
    /// @return True if session successfully opened; false on transport error.
    [[nodiscard]] bool start();

    /// @brief Stops worker thread, closes admission gate, and shuts down transport.
    void stop();

    /// @brief Reports whether transport link is connected and session is running.
    /// @return True if operational.
    [[nodiscard]] bool isConnected() const noexcept;

    /// @brief Updates device address.
    /// @param[in] address Logical device address.
    void setAddress(std::uint8_t address) noexcept;

    /// @brief Retrieves currently assigned device address.
    /// @return Logical device address.
    [[nodiscard]] std::uint8_t getAddress() const noexcept;

    /// @brief Submits a raw command frame for immediate transmission.
    /// @param[in] frame Protocol frame bytes.
    /// @return Result status code.
    [[nodiscard]] SendResult sendRawFrame(const std::vector<std::uint8_t>& frame);

    /// @brief Submits a query frame with strongly-typed QueryKind.
    /// @param[in] frame Protocol frame bytes.
    /// @param[in] queryKind Strongly-typed query category.
    /// @return Result status code.
    [[nodiscard]] SendResult sendQueryFrame(const std::vector<std::uint8_t>& frame, QueryKind queryKind);

    /// @brief Submits a query frame, registering an expectation for correlated response.
    /// @param[in] frame Protocol frame bytes.
    /// @param[in] queryTag Tag identifying expected response.
    /// @return Result status code.
    [[nodiscard]] SendResult sendQueryFrame(
        const std::vector<std::uint8_t>& frame, const std::string& queryTag = "Query");

    /// @brief Submits a command into the prioritized pacing queue with strongly-typed QueryKind.
    /// @param[in] frame Raw frame bytes.
    /// @param[in] queryKind Expected query kind.
    /// @param[in] priority Execution priority level.
    /// @return Result status code.
    [[nodiscard]] SendResult enqueueCommand(const std::vector<std::uint8_t>& frame, QueryKind queryKind,
        CommandPriority priority = CommandPriority::Normal);

    /// @brief Submits a command into the prioritized pacing queue.
    /// @param[in] frame Raw frame bytes.
    /// @param[in] queryTag Optional query identifier.
    /// @param[in] priority Execution priority level.
    /// @return Result status code.
    [[nodiscard]] SendResult enqueueCommand(const std::vector<std::uint8_t>& frame, const std::string& queryTag = "",
        CommandPriority priority = CommandPriority::Normal);

    /// @brief Installs a vendor protocol extension hook.
    /// @param[in] ext Shared pointer to frame extension.
    /// @return True if successfully installed.
    [[nodiscard]] bool setFrameExt(std::shared_ptr<IFrameExtension> ext);

    /// @brief Resolves active query wait condition without waiting for timeout.
    void resolveQueryWait();

    /// @brief Configures query timeout duration.
    /// @param[in] timeoutMs Maximum duration in milliseconds to wait for a response.
    void setQueryTimeoutMs(std::uint32_t timeoutMs) noexcept;

    /// @brief Retrieves the configured query timeout duration.
    /// @return Query timeout in milliseconds.
    [[nodiscard]] std::uint32_t getQueryTimeoutMs() const noexcept;

    /// @brief Configures retry policy.
    /// @param[in] config Policy parameters.
    void setRetryConfig(const RetryConfig& config) noexcept;

    /// @brief Retrieves active retry configuration.
    /// @return Active policy parameters.
    [[nodiscard]] RetryConfig getRetryConfig() const noexcept;

    /// @brief Registers incoming frame dispatch handler.
    /// @param[in] handler Callback function.
    void setFrameHandler(FrameHandler handler);

    /// @brief Registers traffic observation handler.
    /// @param[in] handler Callback function.
    void setTrafficHandler(TrafficHandler handler);

    /// @brief Registers query timeout handler.
    /// @param[in] handler Callback function.
    void setTimeoutHandler(TimeoutHandler handler);

    /// @brief Registers query failure handler.
    /// @param[in] handler Callback function.
    void setQueryFailedHandler(QueryFailedHandler handler);

    /// @brief Registers retry event handler.
    /// @param[in] handler Callback function.
    void setRetryHandler(RetryHandler handler);

    /// @brief Registers transport link state change handler.
    /// @param[in] handler Callback function.
    void setStateHandler(StateHandler handler);

    using CommandDroppedHandler = std::function<void(const std::vector<std::uint8_t>& frame, SendResult reason)>;

    /// @brief Registers command dropped notification handler.
    /// @param[in] handler Callback function.
    void setCommandDroppedHandler(CommandDroppedHandler handler);

    /// @brief Registers periodic telemetry poll step handler and optional deadline provider.
    /// @param[in] handler Callback function.
    /// @param[in] deadlineHandler Callback providing the next poll deadline time point.
    void setPollStepHandler(PollStepHandler handler, PollDeadlineHandler deadlineHandler = nullptr);

    /// @brief Registers status provider used by protocol extensions.
    /// @param[in] provider Function returning a current DeviceStatus snapshot.
    void setStatusProvider(StatusProvider provider);

    /// @brief Checks whether low-priority commands are pending in the command queue.
    /// @return True if low priority commands are pending.
    [[nodiscard]] bool hasLowPriorityPending() const noexcept;

    /// @brief Wakes all waiting threads on the command queue.
    void wakeQueue() noexcept;

    /// @brief Sets active pending Everest limit ID for response correlation.
    /// @param[in] limitId Optional limit identifier.
    void setPendingLimitId(std::optional<EverestLimitId> limitId) noexcept;

    /// @brief Retrieves currently pending Everest limit ID.
    /// @return Optional limit identifier.
    [[nodiscard]] std::optional<EverestLimitId> getPendingLimitId() const noexcept;

    /// @brief Grants access to internal AsyncWatchdog.
    /// @return Reference to watchdog instance.
    [[nodiscard]] AsyncWatchdog& getAsyncWatchdog() noexcept
    {
        return *m_asyncWatchdog;
    }

    /// @brief Grants access to internal PacedCommandQueue.
    /// @return Reference to queue instance.
    [[nodiscard]] PacedCommandQueue& getQueue() noexcept
    {
        return m_queue;
    }

    /// @brief Grants access to internal RxStreamAccumulator.
    /// @return Reference to accumulator instance.
    [[nodiscard]] RxStreamAccumulator& getAccumulator() noexcept
    {
        return m_rxAccumulator;
    }

    /// @brief Captures underlying transport statistics snapshot.
    /// @return Transport telemetry snapshot.
    [[nodiscard]] Transport::TransportStatsSnapshot getTransportStats() const;

    /// @brief Creates an independent virtual transport channel for concurrent services (e.g. BusScanner).
    /// @return Shared pointer to an independent ITransport channel, or nullptr if transport is null.
    [[nodiscard]] std::shared_ptr<ITransport> createVirtualChannel();

private:
    void workerLoop();
    void onDataReceived(const std::vector<std::uint8_t>& data);
    void onTransportState(TransportState state, const std::string& msg);
    void markConnected();
    void markDisconnected();
    void checkQueryTimeout();
    void dispatchFrame(const std::vector<std::uint8_t>& frame);
    [[nodiscard]] bool isResponseMatchingQuery(
        QueryKind kind, const std::string& queryTag, const std::vector<std::uint8_t>& frame) const noexcept;
    void notifyQueryFailed(
        const std::string& queryTag, std::chrono::microseconds durationUs = std::chrono::microseconds { 0 });
    void notifyCommandDropped(const std::vector<std::uint8_t>& frame, SendResult reason);
    [[nodiscard]] std::shared_ptr<ITransport> activeTransport() const noexcept;

    std::shared_ptr<ITransport> m_transport;
    std::shared_ptr<TransportMultiplexer> m_multiplexer {};
    std::shared_ptr<ITransport> m_sessionChannel {};
    std::atomic<std::uint8_t> m_address { 1U };
    std::atomic<bool> m_running { false };
    std::atomic<bool> m_connected { false };
    std::thread m_workerThread;
    mutable std::recursive_mutex m_lifecycleMutex;

    std::shared_ptr<CallbackGate> m_rxGate {};
    std::shared_ptr<IFrameExtension> m_frameExt {};

    PacedCommandQueue m_queue;
    RxStreamAccumulator m_rxAccumulator;
    std::unique_ptr<AsyncWatchdog> m_asyncWatchdog {};
    std::atomic<std::uint64_t> m_motionGeneration { 0U };

    std::atomic<std::uint32_t> m_queryTimeoutMs { 1000U };
    mutable std::mutex m_retryMutex;
    RetryConfig m_retryConfig {};

    mutable std::mutex m_sessionMutex;
    std::atomic<bool> m_awaitingResponse { false };
    std::atomic<bool> m_abortQueryWait { false };
    std::condition_variable m_responseCv;
    QueryKind m_pendingQueryKind { QueryKind::None };
    std::string m_pendingQueryTag {};
    std::optional<EverestLimitId> m_pendingLimitId { std::nullopt };
    std::chrono::steady_clock::time_point m_querySentTime {};

    mutable std::mutex m_handlerMutex;
    FrameHandler m_frameHandler {};
    TrafficHandler m_trafficHandler {};
    TimeoutHandler m_timeoutHandler {};
    QueryFailedHandler m_queryFailedHandler {};
    RetryHandler m_retryHandler {};
    StateHandler m_stateHandler {};
    CommandDroppedHandler m_commandDroppedHandler {};
    PollStepHandler m_pollStepHandler {};
    PollDeadlineHandler m_pollDeadlineHandler {};
    StatusProvider m_statusProvider {};
};

} // namespace PelcoD
