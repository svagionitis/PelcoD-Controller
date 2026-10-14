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
    StandardFrame frame {};
    QueryKind queryKind { QueryKind::None };
    std::string queryTag {};
    CommandPriority priority { CommandPriority::Normal };
    std::uint32_t retryCount { 0U };
    std::chrono::steady_clock::time_point earliestDispatchTime { std::chrono::steady_clock::now() };
    std::uint64_t motionGeneration { 0U };
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

    /// @brief Enqueues a command item according to its priority with capacity enforcement.
    /// @param[in] item Command packet and metadata.
    /// @return SendResult::Success on successful insertion, SendResult::QueueFull if rejected.
    [[nodiscard]] SendResult enqueue(CommandItem item);

    /// @brief Helper to construct and enqueue a command item with zero-allocation StandardFrame.
    /// @param[in] frame Standard 7-byte protocol array.
    /// @param[in] queryKind Expected query kind.
    /// @param[in] priority Scheduling priority.
    /// @param[in] motionGeneration Generation token for motion sequencing (0 = non-motion).
    /// @return SendResult::Success on successful insertion, SendResult::QueueFull if rejected.
    [[nodiscard]] SendResult enqueue(StandardFrame frame, QueryKind queryKind,
        CommandPriority priority = CommandPriority::Normal, std::uint64_t motionGeneration = 0U);

    /// @brief Helper to construct and enqueue a command item with zero-allocation StandardFrame.
    /// @param[in] frame Standard 7-byte protocol array.
    /// @param[in] queryTag Optional query identifier.
    /// @param[in] priority Scheduling priority.
    /// @param[in] motionGeneration Generation token for motion sequencing (0 = non-motion).
    /// @return SendResult::Success on successful insertion, SendResult::QueueFull if rejected.
    [[nodiscard]] SendResult enqueue(StandardFrame frame, std::string queryTag = "",
        CommandPriority priority = CommandPriority::Normal, std::uint64_t motionGeneration = 0U);

    /// @brief Helper to construct and enqueue a command item with strongly-typed QueryKind.
    /// @param[in] frame Raw frame bytes.
    /// @param[in] queryKind Expected query kind.
    /// @param[in] priority Scheduling priority.
    /// @param[in] motionGeneration Generation token for motion sequencing (0 = non-motion).
    /// @return SendResult::Success on successful insertion, SendResult::QueueFull if rejected.
    [[nodiscard]] SendResult enqueue(std::vector<std::uint8_t> frame, QueryKind queryKind,
        CommandPriority priority = CommandPriority::Normal, std::uint64_t motionGeneration = 0U);

    /// @brief Helper to construct and enqueue a command item with string query tag.
    /// @param[in] frame Raw frame bytes.
    /// @param[in] queryTag Optional query identifier.
    /// @param[in] priority Scheduling priority.
    /// @param[in] motionGeneration Generation token for motion sequencing (0 = non-motion).
    /// @return SendResult::Success on successful insertion, SendResult::QueueFull if rejected.
    [[nodiscard]] SendResult enqueue(std::vector<std::uint8_t> frame, std::string queryTag = "",
        CommandPriority priority = CommandPriority::Normal, std::uint64_t motionGeneration = 0U);

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

    using EvictionHandler = std::function<void(const CommandItem&)>;

    /// @brief Registers a callback invoked whenever an enqueued item is dropped or evicted.
    /// @details Invoked without holding internal queue locks when items are evicted due to
    ///          queue capacity limits, urgent priority purges, or queue clearing.
    /// @param[in] handler Eviction callback function.
    void setEvictionHandler(EvictionHandler handler);

    /// @brief Checks if any low-priority commands (e.g. telemetry queries) are currently queued.
    [[nodiscard]] bool hasLowPriorityPending() const;

    /// @brief Wakes any threads blocked in popReady().
    void wakeAll();

    /// @brief Removes all queued commands.
    void clear();

    /// @brief Purges all queued motion commands and pending motion retries.
    /// @details Removes items associated with active or past motion generations, as well
    ///          as standard motion and stop frames. Preserves non-motion queries and configurations.
    /// @return Number of motion items purged from the queue.
    [[nodiscard]] std::size_t purgeMotionCommands() noexcept;

    /// @brief Current number of commands queued.
    [[nodiscard]] std::size_t size() const;

    /// @brief Checks whether the queue is empty.
    [[nodiscard]] bool empty() const;

    /// @brief Configured maximum queue capacity.
    [[nodiscard]] std::size_t maxCapacity() const noexcept;

    /// @brief Current motion generation tracked by the queue.
    [[nodiscard]] std::uint64_t currentMotionGeneration() const noexcept;

private:
    const std::size_t m_maxCapacity;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<CommandItem> m_queue;
    std::uint64_t m_currentMotionGeneration { 0U };
    EvictionHandler m_evictionHandler {};
};

} // namespace PelcoD
