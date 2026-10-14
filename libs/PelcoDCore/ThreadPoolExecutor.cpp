#include "ThreadPoolExecutor.h"

#include <utility>

namespace PelcoD {

ThreadPoolExecutor::ThreadPoolExecutor(std::size_t threadCount, std::size_t maxQueueCapacity)
    : m_threadCount { (threadCount == 0U) ? 1U : threadCount }
    , m_maxCapacity { (maxQueueCapacity == 0U) ? DefaultQueueCapacity : maxQueueCapacity }
{
}

ThreadPoolExecutor::~ThreadPoolExecutor()
{
    stop();
}

void ThreadPoolExecutor::start()
{
    if (m_running.exchange(true, std::memory_order_relaxed)) {
        return;
    }

    m_workers.reserve(m_threadCount);
    for (std::size_t i { 0U }; i < m_threadCount; ++i) {
        m_workers.emplace_back(&ThreadPoolExecutor::workerLoop, this);
    }
}

void ThreadPoolExecutor::stop()
{
    if (!m_running.exchange(false, std::memory_order_relaxed)) {
        return;
    }

    m_cv.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();

    std::scoped_lock lock(m_mutex);
    while (!m_tasks.empty()) {
        m_tasks.pop();
    }
}

bool ThreadPoolExecutor::submit(Task task)
{
    if (!task || !m_running.load(std::memory_order_relaxed)) {
        return false;
    }

    {
        std::scoped_lock lock(m_mutex);
        if (m_tasks.size() >= m_maxCapacity) {
            return false;
        }
        m_tasks.push(std::move(task));
    }

    m_cv.notify_one();
    return true;
}

std::size_t ThreadPoolExecutor::getThreadCount() const noexcept
{
    return m_threadCount;
}

void ThreadPoolExecutor::workerLoop()
{
    while (m_running.load(std::memory_order_relaxed)) {
        Task task {};
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_running.load(std::memory_order_relaxed) || !m_tasks.empty(); });

            if (!m_running.load(std::memory_order_relaxed) && m_tasks.empty()) {
                break;
            }

            if (!m_tasks.empty()) {
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
        }

        if (task) {
            task();
        }
    }
}

} // namespace PelcoD
