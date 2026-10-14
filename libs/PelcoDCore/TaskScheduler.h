#pragma once

/// @file TaskScheduler.h
/// @brief Unified monotonic-deadline timer and task scheduling engine.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace PelcoD {

using TaskId = std::uint64_t;

/// @class TaskScheduler
/// @brief Centralized monotonic-deadline scheduler running periodic and delayed tasks on a single thread.
class TaskScheduler final {
public:
    using Task = std::function<void()>;

    TaskScheduler();
    ~TaskScheduler();

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;
    TaskScheduler(TaskScheduler&&) = delete;
    TaskScheduler& operator=(TaskScheduler&&) = delete;

    /// @brief Starts the background scheduling thread.
    void start();

    /// @brief Stops the scheduling thread and drains or purges tasks.
    void stop();

    /// @brief Schedules a one-shot task to execute after delay.
    /// @param[in] delay Duration before task execution.
    /// @param[in] task Function to execute.
    /// @return Unique task handle.
    [[nodiscard]] TaskId schedule(std::chrono::milliseconds delay, Task task);

    /// @brief Schedules a recurring task to execute periodically.
    /// @param[in] interval Repetition period.
    /// @param[in] task Function to execute.
    /// @return Unique task handle.
    [[nodiscard]] TaskId schedulePeriodic(std::chrono::milliseconds interval, Task task);

    /// @brief Cancels a previously scheduled task.
    /// @param[in] taskId Handle returned by schedule.
    /// @return True if task was found and cancelled; false otherwise.
    [[nodiscard]] bool cancel(TaskId taskId);

private:
    struct ScheduledItem {
        TaskId id { 0U };
        std::chrono::steady_clock::time_point deadline {};
        std::chrono::milliseconds interval { 0 };
        bool periodic { false };
        Task task {};

        bool operator>(const ScheduledItem& other) const noexcept
        {
            return deadline > other.deadline;
        }
    };

    void workerLoop();

    std::atomic<bool> m_running { false };
    std::atomic<TaskId> m_nextId { 1U };
    std::thread m_workerThread {};
    std::mutex m_mutex {};
    std::condition_variable m_cv {};

    std::priority_queue<ScheduledItem, std::vector<ScheduledItem>, std::greater<ScheduledItem>> m_queue {};
    std::unordered_map<TaskId, bool> m_cancelledMap {};
};

} // namespace PelcoD
