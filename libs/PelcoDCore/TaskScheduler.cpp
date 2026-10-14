#include "TaskScheduler.h"

#include <utility>

namespace PelcoD {

TaskScheduler::TaskScheduler() = default;

TaskScheduler::~TaskScheduler()
{
    stop();
}

void TaskScheduler::start()
{
    if (m_running.exchange(true, std::memory_order_relaxed)) {
        return;
    }

    m_workerThread = std::thread(&TaskScheduler::workerLoop, this);
}

void TaskScheduler::stop()
{
    if (!m_running.exchange(false, std::memory_order_relaxed)) {
        return;
    }

    m_cv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    std::scoped_lock lock(m_mutex);
    while (!m_queue.empty()) {
        m_queue.pop();
    }
    m_cancelledMap.clear();
}

TaskId TaskScheduler::schedule(std::chrono::milliseconds delay, Task task)
{
    if (!task) {
        return 0U;
    }

    const TaskId id = m_nextId.fetch_add(1U, std::memory_order_relaxed);
    const auto deadline = std::chrono::steady_clock::now() + delay;

    {
        std::scoped_lock lock(m_mutex);
        m_queue.push(ScheduledItem { id, deadline, std::chrono::milliseconds { 0 }, false, std::move(task) });
    }

    m_cv.notify_one();
    return id;
}

TaskId TaskScheduler::schedulePeriodic(std::chrono::milliseconds interval, Task task)
{
    if (!task || interval.count() <= 0) {
        return 0U;
    }

    const TaskId id = m_nextId.fetch_add(1U, std::memory_order_relaxed);
    const auto deadline = std::chrono::steady_clock::now() + interval;

    {
        std::scoped_lock lock(m_mutex);
        m_queue.push(ScheduledItem { id, deadline, interval, true, std::move(task) });
    }

    m_cv.notify_one();
    return id;
}

bool TaskScheduler::cancel(TaskId taskId)
{
    if (taskId == 0U) {
        return false;
    }

    std::scoped_lock lock(m_mutex);
    m_cancelledMap[taskId] = true;
    m_cv.notify_one();
    return true;
}

void TaskScheduler::workerLoop()
{
    while (m_running.load(std::memory_order_relaxed)) {
        std::unique_lock<std::mutex> lock(m_mutex);

        if (m_queue.empty()) {
            m_cv.wait(lock, [this] { return !m_running.load(std::memory_order_relaxed) || !m_queue.empty(); });
            if (!m_running.load(std::memory_order_relaxed)) {
                break;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const auto topDeadline = m_queue.top().deadline;

        if (now < topDeadline) {
            m_cv.wait_until(lock, topDeadline, [this, topDeadline] {
                return !m_running.load(std::memory_order_relaxed) || m_queue.empty()
                    || m_queue.top().deadline < topDeadline;
            });
            continue;
        }

        auto item = m_queue.top();
        m_queue.pop();

        const auto it = m_cancelledMap.find(item.id);
        const bool isCancelled = (it != m_cancelledMap.end() && it->second);

        if (isCancelled) {
            m_cancelledMap.erase(item.id);
            continue;
        }

        if (item.periodic) {
            ScheduledItem nextItem = item;
            nextItem.deadline = now + item.interval;
            m_queue.push(std::move(nextItem));
        }

        lock.unlock();

        if (item.task) {
            item.task();
        }
    }
}

} // namespace PelcoD
