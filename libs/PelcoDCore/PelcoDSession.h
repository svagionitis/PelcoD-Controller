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
    using CommandDroppedHandler = std::function<void(const std::vector<std::uint8_t>& frame, SendResult reason)>;

    /// @struct SessionHandlers
    /// @brief Immutable dispatch and callback hooks bundle configured before session start.
    struct SessionHandlers {
        SessionHandlers() = default;

        FrameHandler frameHandler {};
        TrafficHandler trafficHandler {};
        TimeoutHandler timeoutHandler {};
        QueryFailedHandler queryFailedHandler {};
        RetryHandler retryHandler {};
        StateHandler stateHandler {};
        CommandDroppedHandler commandDroppedHandler {};
        PollStepHandler pollStepHandler {};
        PollDeadlineHandler pollDeadlineHandler {};
        StatusProvider statusProvider {};
    };

    /// @brief Constructs a communication session.
    /// @param[in] transport Underlying communication transport interface.
    /// @param[in] address Logical RS-485 device address (1-31).
    explicit PelcoDSession(std::shared_ptr<ITransport> transport, std::uint8_t address = 1U);

    /// @brief Constructs a communication session with pre-configured handlers.
    /// @param[in] transport Underlying communication transport interface.
    /// @param[in] address Logical RS-485 device address (1-31).
    /// @param[in] handlers Pre-configured session handlers.
    PelcoDSession(std::shared_ptr<ITransport> transport, std::uint8_t address, SessionHandlers handlers);

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

    /// @brief Submits a standard command frame for transmission.
    /// @param[in] frame Standard 7-byte protocol array.
    /// @return Result status code.
    [[nodiscard]] SendResult sendRawFrame(const StandardFrame& frame);

    /// @brief Submits a raw command frame for immediate transmission.
    /// @param[in] frame Protocol frame bytes.
    /// @return Result status code.
    [[nodiscard]] SendResult sendRawFrame(const std::vector<std::uint8_t>& frame);

    /// @brief Submits a standard query frame, registering an expectation for correlated response.
    /// @param[in] frame Standard 7-byte protocol array.
    /// @param[in] query Expected query kind or tag name.
    /// @return Result status code.
    [[nodiscard]] SendResult sendQueryFrame(const StandardFrame& frame, QueryRef query = "Query");

    /// @brief Submits a query frame, registering an expectation for correlated response.
    /// @param[in] frame Protocol frame bytes.
    /// @param[in] query Expected query kind or tag name.
    /// @return Result status code.
    [[nodiscard]] SendResult sendQueryFrame(const std::vector<std::uint8_t>& frame, QueryRef query = "Query");

    /// @brief Submits a standard command into the prioritized pacing queue.
    /// @param[in] frame Standard 7-byte protocol array.
    /// @param[in] query Expected query kind or tag name.
    /// @param[in] priority Execution priority level.
    /// @return Result status code.
    [[nodiscard]] SendResult enqueueCommand(
        const StandardFrame& frame, QueryRef query = {}, CommandPriority priority = CommandPriority::Normal);

    /// @brief Submits a raw command into the prioritized pacing queue.
    /// @param[in] frame Raw frame bytes.
    /// @param[in] query Expected query kind or tag name.
    /// @param[in] priority Execution priority level.
    /// @return Result status code.
    [[nodiscard]] SendResult enqueueCommand(const std::vector<std::uint8_t>& frame, QueryRef query = {},
        CommandPriority priority = CommandPriority::Normal);

    /// @brief Submits a standard command with explicit priority into the pacing queue.
    /// @param[in] frame Standard 7-byte protocol array.
    /// @param[in] priority Execution priority level.
    /// @return Result status code.
    [[nodiscard]] SendResult enqueueCommand(const StandardFrame& frame, CommandPriority priority);

    /// @brief Submits a raw command with explicit priority into the pacing queue.
    /// @param[in] frame Raw frame bytes.
    /// @param[in] priority Execution priority level.
    /// @return Result status code.
    [[nodiscard]] SendResult enqueueCommand(const std::vector<std::uint8_t>& frame, CommandPriority priority);

    /// @brief Purges all motion and stop commands from the prioritized command queue.
    /// @return Number of purged motion commands.
    [[nodiscard]] std::size_t purgeMotionCommands() noexcept;

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

    /// @brief Configures session event and dispatch handlers.
    /// @details Thread-safe; may only be called when the session is stopped. Handlers remain immutable during
    /// execution.
    /// @param[in] handlers Handlers bundle to install.
    /// @return True if installed; false if the session is currently running.
    [[nodiscard]] bool setHandlers(SessionHandlers handlers);

    /// @brief Checks whether low-priority commands are pending in the command queue.
    /// @return True if low priority commands are pending.
    [[nodiscard]] bool hasLowPriorityPending() const noexcept;

    /// @brief Sets active pending Everest limit ID for response correlation.
    /// @param[in] limitId Optional limit identifier.
    void setPendingLimitId(std::optional<EverestLimitId> limitId) noexcept;

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

    /// @brief Captures number of pending commands waiting in pacing queue.
    /// @return Count of queued commands.
    [[nodiscard]] std::size_t getPendingCommandCount() const noexcept;

    /// @brief Retrieves count of framing checksum errors recorded by stream accumulator.
    /// @return Accumulated checksum error count.
    [[nodiscard]] std::uint64_t getChecksumErrors() const noexcept;

    /// @brief Retrieves count of discarded unaligned sync bytes from accumulator.
    /// @return Accumulated discarded byte count.
    [[nodiscard]] std::uint64_t getDiscardedBytes() const noexcept;

    /// @brief Resets accumulator framing error and discarded byte metrics.
    void resetAccumulatorStats() noexcept;

    /// @brief Awakens command queue waiting threads.
    void wakeQueue() noexcept;

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
    void notifyCommandDropped(const StandardFrame& frame, SendResult reason);
    void notifyCommandDropped(const std::vector<std::uint8_t>& frame, SendResult reason);
    [[nodiscard]] std::shared_ptr<ITransport> activeTransport() const noexcept;

    void bindTransport(const std::shared_ptr<ITransport>& transport, const std::shared_ptr<CallbackGate>& gate);
    void unbindTransport(const std::shared_ptr<ITransport>& transport);

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

    SessionHandlers m_handlers {};
};

} // namespace PelcoD
