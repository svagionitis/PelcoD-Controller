/// @file PacedCommandQueue.cpp
/// @brief Implementation of prioritized paced command queue with exponential retry backoff.

#include "PacedCommandQueue.h"

#include <algorithm>
#include <cmath>
#include <glog/logging.h>
#include <random>

namespace PelcoD {

PacedCommandQueue::PacedCommandQueue(std::size_t maxCapacity)
    : m_maxCapacity { (maxCapacity > 0U) ? maxCapacity : DefaultMaxCapacity }
{
}

void PacedCommandQueue::enqueue(CommandItem item)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_queue.size() >= m_maxCapacity) {
        LOG(WARNING) << "PacedCommandQueue capacity reached (" << m_queue.size() << "/" << m_maxCapacity
                     << "): dropping oldest non-urgent command";
        auto dropIt = m_queue.end();
        for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
            if (it->priority != CommandPriority::Urgent) {
                dropIt = it;
                break;
            }
        }
        if (dropIt != m_queue.end()) {
            m_queue.erase(dropIt);
        } else {
            m_queue.pop_front();
        }
    }

    if (item.priority == CommandPriority::Urgent) {
        auto it = m_queue.begin();
        while (it != m_queue.end()) {
            if (it->priority == CommandPriority::Low) {
                it = m_queue.erase(it);
            } else {
                ++it;
            }
        }
        m_queue.push_front(std::move(item));
    } else {
        m_queue.push_back(std::move(item));
    }

    m_cv.notify_one();
}

void PacedCommandQueue::enqueue(
    std::vector<std::uint8_t> frame, std::string queryTag, CommandPriority priority)
{
    CommandItem item;
    item.frame = std::move(frame);
    item.queryTag = std::move(queryTag);
    item.priority = priority;
    item.retryCount = 0U;
    item.earliestDispatchTime = std::chrono::steady_clock::now();
    enqueue(std::move(item));
}

std::chrono::milliseconds PacedCommandQueue::scheduleRetry(
    CommandItem item, const RetryConfig& retryCfg, std::string_view logReason)
{
    item.retryCount++;
    const auto backoffDelay = calculateBackoffDelay(retryCfg, item.retryCount);
    item.earliestDispatchTime = std::chrono::steady_clock::now() + backoffDelay;

    if (!logReason.empty()) {
        LOG(INFO) << logReason << " (attempt " << item.retryCount << "/" << retryCfg.maxRetries
                  << "). Retrying in " << backoffDelay.count() << " ms";
    }

    enqueue(std::move(item));
    return backoffDelay;
}

bool PacedCommandQueue::popReady(CommandItem& outItem, const std::function<bool()>& stopPredicate,
    std::chrono::steady_clock::time_point nextPollTime, std::chrono::milliseconds defaultTimeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    while (!stopPredicate()) {
        const auto curNow = std::chrono::steady_clock::now();
        auto readyIt = m_queue.end();
        auto earliestWait = std::chrono::steady_clock::time_point::max();

        for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
            if (it->earliestDispatchTime <= curNow) {
                readyIt = it;
                break;
            }
            if (it->earliestDispatchTime < earliestWait) {
                earliestWait = it->earliestDispatchTime;
            }
        }

        if (readyIt != m_queue.end()) {
            outItem = std::move(*readyIt);
            m_queue.erase(readyIt);
            return true;
        }

        auto waitTime = defaultTimeout;
        if (nextPollTime != std::chrono::steady_clock::time_point::max()) {
            if (nextPollTime > curNow) {
                const auto pollDiff
                    = std::chrono::duration_cast<std::chrono::milliseconds>(nextPollTime - curNow);
                waitTime = std::min(waitTime, pollDiff);
            } else {
                waitTime = std::chrono::milliseconds(0);
            }
        }

        if (!m_queue.empty() && earliestWait != std::chrono::steady_clock::time_point::max()) {
            const auto backoffDiff
                = std::chrono::duration_cast<std::chrono::milliseconds>(earliestWait - curNow);
            waitTime = std::min(waitTime, std::max(backoffDiff, std::chrono::milliseconds(1)));
        }

        if (waitTime.count() <= 0) {
            return false;
        }

        m_cv.wait_for(lock, waitTime, [this, &stopPredicate, earliestWait] {
            if (stopPredicate()) {
                return true;
            }
            const auto checkNow = std::chrono::steady_clock::now();
            if (!m_queue.empty() && checkNow >= earliestWait) {
                return true;
            }
            for (const auto& q : m_queue) {
                if (q.earliestDispatchTime <= checkNow) {
                    return true;
                }
            }
            return false;
        });

        if (stopPredicate()) {
            return false;
        }

        const auto checkNow = std::chrono::steady_clock::now();
        for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
            if (it->earliestDispatchTime <= checkNow) {
                outItem = std::move(*it);
                m_queue.erase(it);
                return true;
            }
        }

        return false;
    }

    return false;
}

bool PacedCommandQueue::hasLowPriorityPending() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& item : m_queue) {
        if (item.priority == CommandPriority::Low) {
            return true;
        }
    }
    return false;
}

void PacedCommandQueue::wakeAll()
{
    m_cv.notify_all();
}

void PacedCommandQueue::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.clear();
}

std::size_t PacedCommandQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

bool PacedCommandQueue::empty() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.empty();
}

std::size_t PacedCommandQueue::maxCapacity() const noexcept
{
    return m_maxCapacity;
}

} // namespace PelcoD
