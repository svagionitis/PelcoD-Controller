/// @file AsyncWatchdog.cpp
/// @brief Implementation of centralized asynchronous timer manager.

#include "AsyncWatchdog.h"

#include <algorithm>
#include <exception>

namespace PelcoD {

AsyncWatchdog::AsyncWatchdog()
    : m_worker { &AsyncWatchdog::watchdogLoop, this }
{
}

AsyncWatchdog::~AsyncWatchdog()
{
    shutdown();
}

std::uint64_t AsyncWatchdog::schedule(std::chrono::milliseconds timeout, std::function<void()> callback)
{
    if (!callback) {
        return 0U;
    }

    const auto expiry = std::chrono::steady_clock::now() + timeout;
    std::shared_ptr<TimerItem> item {};
    std::uint64_t id { 0U };

    {
        std::scoped_lock lock(m_mutex);
        if (m_stopped.load()) {
            return 0U;
        }

        id = m_nextId++;
        item = std::make_shared<TimerItem>();
        item->id = id;
        item->expiry = expiry;
        item->cb = std::move(callback);
        item->cancelled = false;

        m_queue.push(item);
        m_activeTimers[id] = item;
    }

    m_cv.notify_all();
    return id;
}

bool AsyncWatchdog::cancel(std::uint64_t timerId) noexcept
{
    if (timerId == 0U) {
        return false;
    }

    std::scoped_lock lock(m_mutex);
    const auto it = m_activeTimers.find(timerId);
    if (it == m_activeTimers.end()) {
        return false;
    }

    it->second->cancelled = true;
    m_activeTimers.erase(it);
    return true;
}

void AsyncWatchdog::shutdown() noexcept
{
    {
        std::scoped_lock lock(m_mutex);
        if (m_stopped.load()) {
            return;
        }
        m_stopped.store(true);
        for (const auto& pair : m_activeTimers) {
            if (pair.second) {
                pair.second->cancelled = true;
            }
        }
        m_activeTimers.clear();
        while (!m_queue.empty()) {
            m_queue.pop();
        }
    }

    m_cv.notify_all();

    if (m_worker.joinable()) {
        try {
            m_worker.join();
        } catch (...) {
        }
    }
}

void AsyncWatchdog::watchdogLoop()
{
    while (!m_stopped.load()) {
        std::shared_ptr<TimerItem> expiredItem {};

        {
            std::unique_lock<std::mutex> lock(m_mutex);

            while (!m_stopped.load() && m_queue.empty()) {
                m_cv.wait(lock);
            }

            if (m_stopped.load()) {
                break;
            }

            const auto now = std::chrono::steady_clock::now();
            auto top = m_queue.top();

            if (top->cancelled) {
                m_queue.pop();
                continue;
            }

            if (top->expiry <= now) {
                expiredItem = top;
                m_queue.pop();
                m_activeTimers.erase(top->id);
            } else {
                const auto waitDuration = top->expiry - now;
                m_cv.wait_for(lock, waitDuration);
                continue;
            }
        }

        if (expiredItem && !expiredItem->cancelled && expiredItem->cb) {
            try {
                expiredItem->cb();
            } catch (...) {
            }
        }
    }
}

} // namespace PelcoD
