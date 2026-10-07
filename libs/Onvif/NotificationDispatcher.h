#pragma once

/// @file NotificationDispatcher.h
/// @brief Thread-safe bounded worker pool for delivering asynchronous push notifications.
/// @details Replaces unmanaged detached threads with a bounded queue and managed worker thread,
///          remediating review finding C5 (CWE-400 thread and resource exhaustion).

#include "OnvifServerTypes.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace Onvif {

/// @struct PushDeliveryTask
/// @brief A single push notification task awaiting HTTP delivery.
struct PushDeliveryTask {
    std::string consumerUrl {};
    std::string payload {};
};

/// @class NotificationDispatcher
/// @brief Background dispatcher delivering push notification payloads to verified consumers.
/// @details Manages a bounded work queue, strictly controlled HTTP timeouts, and an RAII worker
///          thread that cleanly joins on shutdown without leaking detached threads.
class NotificationDispatcher {
public:
    /// @brief Constructs a dispatcher with the specified notification configuration.
    /// @param[in] config Notification configuration containing timeouts and queue bounds.
    explicit NotificationDispatcher(NotificationConfig config);

    /// @brief Destroys the dispatcher, ensuring the worker thread is cleanly joined.
    ~NotificationDispatcher();

    NotificationDispatcher(const NotificationDispatcher&) = delete;
    NotificationDispatcher& operator=(const NotificationDispatcher&) = delete;
    NotificationDispatcher(NotificationDispatcher&&) = delete;
    NotificationDispatcher& operator=(NotificationDispatcher&&) = delete;

    /// @brief Starts the background worker thread.
    void start();

    /// @brief Stops the background worker thread and waits for in-flight tasks to exit.
    void stop();

    /// @brief Enqueues a notification delivery task into the bounded work queue.
    /// @param[in] url Verified consumer reference endpoint URL.
    /// @param[in] payload Serialized SOAP notification payload.
    /// @return True if enqueued; false if queue was saturated and item could not be queued.
    bool enqueue(std::string url, std::string payload);

    /// @brief Queries current count of tasks waiting in the delivery queue.
    /// @return Number of pending tasks.
    [[nodiscard]] std::size_t getPendingCount() const;

    /// @brief Checks whether the background worker thread is currently active.
    /// @return True if running.
    [[nodiscard]] bool isRunning() const noexcept;

private:
    void workerLoop();

    NotificationConfig m_config;
    std::deque<PushDeliveryTask> m_queue {};
    mutable std::mutex m_mutex {};
    std::condition_variable m_cv {};
    std::atomic<bool> m_running { false };
    std::thread m_worker {};
};

} // namespace Onvif
