#pragma once

/// @file AsyncWatchdog.h
/// @brief Centralized timer manager for asynchronous query timeouts without detached threads.

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace PelcoD {

/// @class AsyncWatchdog
/// @brief Single-thread deadline timer scheduler replacing per-query detached threads.
/// @details Maintains a priority queue of scheduled timeout deadlines. A single worker thread sleeps
///          until the earliest deadline or stop notification, executing expired callbacks outside internal
///          locks. Prevents OS thread proliferation (review finding A6 and M4).
class AsyncWatchdog final {
public:
    /// @brief Constructs and launches the watchdog worker thread.
    explicit AsyncWatchdog();

    /// @brief Destructor; terminates the watchdog worker thread safely.
    ~AsyncWatchdog();

    AsyncWatchdog(const AsyncWatchdog&) = delete;
    AsyncWatchdog& operator=(const AsyncWatchdog&) = delete;
    AsyncWatchdog(AsyncWatchdog&&) = delete;
    AsyncWatchdog& operator=(AsyncWatchdog&&) = delete;

    /// @brief Schedules a callback to be invoked after the specified timeout duration.
    /// @param[in] timeout Delay duration before callback invocation.
    /// @param[in] callback Function to execute upon timeout expiration.
    /// @return Unique 64-bit timer identifier used to cancel the timer, or 0 if stopped.
    [[nodiscard]] std::uint64_t schedule(std::chrono::milliseconds timeout, std::function<void()> callback);

    /// @brief Cancels an existing scheduled timer before expiration.
    /// @param[in] timerId Identifier returned by schedule().
    /// @return True if timer was active and successfully cancelled; false if expired or invalid.
    bool cancel(std::uint64_t timerId) noexcept;

    /// @brief Shuts down the timer manager, aborting all pending timers and joining the worker thread.
    void shutdown() noexcept;

private:
    struct TimerItem {
        std::uint64_t id { 0U };
        std::chrono::steady_clock::time_point expiry {};
        std::function<void()> cb {};
        bool cancelled { false };
    };

    struct TimerComparator {
        [[nodiscard]] bool operator()(
            const std::shared_ptr<TimerItem>& a, const std::shared_ptr<TimerItem>& b) const noexcept
        {
            return a->expiry > b->expiry;
        }
    };

    void watchdogLoop();

    mutable std::mutex m_mutex {};
    std::condition_variable m_cv {};
    std::atomic<bool> m_stopped { false };
    std::uint64_t m_nextId { 1U };

    std::priority_queue<std::shared_ptr<TimerItem>, std::vector<std::shared_ptr<TimerItem>>, TimerComparator>
        m_queue {};
    std::unordered_map<std::uint64_t, std::shared_ptr<TimerItem>> m_activeTimers {};

    std::thread m_worker {};
};

} // namespace PelcoD
