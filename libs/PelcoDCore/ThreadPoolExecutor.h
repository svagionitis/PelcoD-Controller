#pragma once

/// @file ThreadPoolExecutor.h
/// @brief Bounded, thread-safe thread pool executor for CPU and I/O worker tasks.

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace PelcoD {

/// @class ThreadPoolExecutor
/// @brief Bounded fixed-worker pool preventing unbounded thread proliferation.
class ThreadPoolExecutor final {
public:
    using Task = std::function<void()>;

    static constexpr std::size_t DefaultThreadCount { 2U };
    static constexpr std::size_t DefaultQueueCapacity { 64U };

    explicit ThreadPoolExecutor(
        std::size_t threadCount = DefaultThreadCount, std::size_t maxQueueCapacity = DefaultQueueCapacity);
    ~ThreadPoolExecutor();

    ThreadPoolExecutor(const ThreadPoolExecutor&) = delete;
    ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) = delete;
    ThreadPoolExecutor(ThreadPoolExecutor&&) = delete;
    ThreadPoolExecutor& operator=(ThreadPoolExecutor&&) = delete;

    /// @brief Starts the thread pool workers.
    void start();

    /// @brief Gracefully terminates workers after running queued tasks.
    void stop();

    /// @brief Enqueues a task for worker execution.
    /// @param[in] task Work function to invoke.
    /// @return True if enqueued; false if pool is full or stopped.
    [[nodiscard]] bool submit(Task task);

    /// @brief Checks number of configured worker threads.
    /// @return Thread count.
    [[nodiscard]] std::size_t getThreadCount() const noexcept;

private:
    void workerLoop();

    std::size_t m_threadCount { DefaultThreadCount };
    std::size_t m_maxCapacity { DefaultQueueCapacity };
    std::atomic<bool> m_running { false };

    std::mutex m_mutex {};
    std::condition_variable m_cv {};
    std::queue<Task> m_tasks {};
    std::vector<std::thread> m_workers {};
};

} // namespace PelcoD
