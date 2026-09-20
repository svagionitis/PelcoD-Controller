#pragma once

/// @file PacedCommandQueue.h
/// @brief Thread-safe prioritized command queue with scheduled retry backoff.

#include "PelcoDTypes.h"
#include "RetryPolicy.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace PelcoD {

/// @struct CommandItem
/// @brief Single schedulable command packet with priority, query tag, and timing.
struct CommandItem {
    std::vector<std::uint8_t> frame {};
    std::string queryTag {};
    CommandPriority priority { CommandPriority::Normal };
    std::uint32_t retryCount { 0U };
    std::chrono::steady_clock::time_point earliestDispatchTime { std::chrono::steady_clock::now() };
};

/// @class PacedCommandQueue
/// @brief Thread-safe multi-priority command queue enforcing bounded capacity,
///        priority-aware ordering, and delayed retry scheduling.
class PacedCommandQueue {
public:
    static constexpr std::size_t DefaultMaxCapacity { 256U };

    explicit PacedCommandQueue(std::size_t maxCapacity = DefaultMaxCapacity);
    ~PacedCommandQueue() = default;

    // Non-copyable, non-movable
    PacedCommandQueue(const PacedCommandQueue&) = delete;
    PacedCommandQueue& operator=(const PacedCommandQueue&) = delete;
    PacedCommandQueue(PacedCommandQueue&&) = delete;
    PacedCommandQueue& operator=(PacedCommandQueue&&) = delete;

    /// @brief Enqueues a command item according to its priority.
    /// @param[in] item Command packet and metadata.
    void enqueue(CommandItem item);

    /// @brief Helper to construct and enqueue a command item.
    /// @param[in] frame Raw frame bytes.
    /// @param[in] queryTag Optional query identifier.
    /// @param[in] priority Scheduling priority.
    void enqueue(std::vector<std::uint8_t> frame, std::string queryTag = "",
        CommandPriority priority = CommandPriority::Normal);

    /// @brief Schedules a failed command for exponential backoff retransmission.
    /// @param[in,out] item Command item being retried (retryCount incremented).
    /// @param[in] retryCfg Retry policy settings.
    /// @param[in] logReason Diagnostic log string.
    /// @return Scheduled backoff delay in milliseconds.
    std::chrono::milliseconds scheduleRetry(
        CommandItem item, const RetryConfig& retryCfg, std::string_view logReason = "");

    /// @brief Pops the next ready command item whose earliestDispatchTime <= now.
    /// @details If no items are ready, blocks waiting up to nextPollTime or earliest wait,
    ///          or until stopPredicate returns true.
    /// @param[out] outItem Popped ready command item.
    /// @param[in] stopPredicate Function returning true if waiting should abort.
    /// @param[in] nextPollTime Optional next telemetry poll deadline.
    /// @param[in] defaultTimeout Maximum idle wait duration before re-evaluating.
    /// @return True if an item was popped; false if stopPredicate fired or queue was empty.
    [[nodiscard]] bool popReady(CommandItem& outItem, const std::function<bool()>& stopPredicate,
        std::chrono::steady_clock::time_point nextPollTime = std::chrono::steady_clock::time_point::max(),
        std::chrono::milliseconds defaultTimeout = std::chrono::milliseconds(100));

    /// @brief Checks if any low-priority commands (e.g. telemetry queries) are currently queued.
    [[nodiscard]] bool hasLowPriorityPending() const;

    /// @brief Wakes any threads blocked in popReady().
    void wakeAll();

    /// @brief Removes all queued commands.
    void clear();

    /// @brief Current number of commands queued.
    [[nodiscard]] std::size_t size() const;

    /// @brief Checks whether the queue is empty.
    [[nodiscard]] bool empty() const;

    /// @brief Configured maximum queue capacity.
    [[nodiscard]] std::size_t maxCapacity() const noexcept;

private:
    const std::size_t m_maxCapacity;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<CommandItem> m_queue;
};

} // namespace PelcoD
