/// @file PacedCommandQueue.cpp
/// @brief Implementation of prioritized paced command queue with exponential retry backoff.

#include "PacedCommandQueue.h"
#include "PelcoDFrame.h"

#include <algorithm>
#include <glog/logging.h>

namespace PelcoD {

PacedCommandQueue::PacedCommandQueue(std::size_t maxCapacity)
    : m_maxCapacity { (maxCapacity > 0U) ? maxCapacity : DefaultMaxCapacity }
{
}

void PacedCommandQueue::setEvictionHandler(EvictionHandler handler)
{
    std::scoped_lock lock { m_mutex };
    m_evictionHandler = std::move(handler);
}

SendResult PacedCommandQueue::enqueue(CommandItem item)
{
    if (item.frame[0] != PelcoDFrame::SyncByte) {
        return SendResult::InvalidFrame;
    }

    std::vector<CommandItem> evictedItems {};
    EvictionHandler handler {};
    SendResult result { SendResult::Success };

    {
        std::scoped_lock lock(m_mutex);

        if (item.motionGeneration > 0U) {
            m_currentMotionGeneration = std::max(m_currentMotionGeneration, item.motionGeneration);
            auto it = m_queue.begin();
            while (it != m_queue.end()) {
                if (it->motionGeneration > 0U && it->motionGeneration < m_currentMotionGeneration) {
                    it = m_queue.erase(it);
                } else {
                    ++it;
                }
            }
        }

        if (m_queue.size() >= m_maxCapacity) {
            if (item.priority == CommandPriority::Urgent) {
                LOG(WARNING) << "PacedCommandQueue capacity reached (" << m_queue.size() << "/" << m_maxCapacity
                             << "): urgent command evicting oldest non-urgent command";
                auto dropIt = m_queue.end();
                for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
                    if (it->priority != CommandPriority::Urgent) {
                        dropIt = it;
                        break;
                    }
                }
                if (dropIt != m_queue.end()) {
                    evictedItems.push_back(std::move(*dropIt));
                    m_queue.erase(dropIt);
                } else {
                    evictedItems.push_back(std::move(m_queue.front()));
                    m_queue.pop_front();
                }
            } else if (item.priority == CommandPriority::Normal) {
                auto dropIt = m_queue.end();
                for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
                    if (it->priority != CommandPriority::Urgent) {
                        dropIt = it;
                        break;
                    }
                }
                if (dropIt != m_queue.end()) {
                    evictedItems.push_back(std::move(*dropIt));
                    m_queue.erase(dropIt);
                } else {
                    LOG(WARNING)
                        << "PacedCommandQueue capacity reached with all urgent commands: rejecting normal command";
                    evictedItems.push_back(std::move(item));
                    result = SendResult::QueueFull;
                }
            } else {
                auto dropIt = m_queue.end();
                for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
                    if (it->priority == CommandPriority::Low) {
                        dropIt = it;
                        break;
                    }
                }
                if (dropIt != m_queue.end()) {
                    evictedItems.push_back(std::move(*dropIt));
                    m_queue.erase(dropIt);
                } else {
                    LOG(WARNING)
                        << "PacedCommandQueue capacity reached with higher priority commands: rejecting low command";
                    evictedItems.push_back(std::move(item));
                    result = SendResult::QueueFull;
                }
            }
        }

        if (result == SendResult::Success) {
            if (item.priority == CommandPriority::Urgent) {
                auto it = m_queue.begin();
                while (it != m_queue.end()) {
                    if (it->priority == CommandPriority::Low) {
                        evictedItems.push_back(std::move(*it));
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

        handler = m_evictionHandler;
    }

    if (handler && !evictedItems.empty()) {
        for (const auto& evicted : evictedItems) {
            handler(evicted);
        }
    }

    return result;
}

SendResult PacedCommandQueue::enqueue(
    StandardFrame frame, QueryKind queryKind, CommandPriority priority, std::uint64_t motionGeneration)
{
    CommandItem item {};
    item.frame = frame;
    item.queryKind = queryKind;
    item.queryTag = std::string(toQueryTagName(queryKind));
    item.priority = priority;
    item.retryCount = 0U;
    item.earliestDispatchTime = std::chrono::steady_clock::now();
    item.motionGeneration = motionGeneration;
    return enqueue(std::move(item));
}

SendResult PacedCommandQueue::enqueue(
    StandardFrame frame, std::string queryTag, CommandPriority priority, std::uint64_t motionGeneration)
{
    CommandItem item {};
    item.frame = frame;
    item.queryKind = parseQueryKind(queryTag);
    item.queryTag = std::move(queryTag);
    item.priority = priority;
    item.retryCount = 0U;
    item.earliestDispatchTime = std::chrono::steady_clock::now();
    item.motionGeneration = motionGeneration;
    return enqueue(std::move(item));
}

SendResult PacedCommandQueue::enqueue(
    std::vector<std::uint8_t> frame, QueryKind queryKind, CommandPriority priority, std::uint64_t motionGeneration)
{
    if (frame.empty() || frame[0] != PelcoDFrame::SyncByte || frame.size() > PelcoDFrame::StandardFrameSize) {
        return SendResult::InvalidFrame;
    }
    StandardFrame stdFrame {};
    for (std::size_t i { 0U }; i < frame.size(); ++i) {
        stdFrame[i] = frame[i];
    }
    return enqueue(stdFrame, queryKind, priority, motionGeneration);
}

SendResult PacedCommandQueue::enqueue(
    std::vector<std::uint8_t> frame, std::string queryTag, CommandPriority priority, std::uint64_t motionGeneration)
{
    if (frame.empty() || frame[0] != PelcoDFrame::SyncByte || frame.size() > PelcoDFrame::StandardFrameSize) {
        return SendResult::InvalidFrame;
    }
    StandardFrame stdFrame {};
    for (std::size_t i { 0U }; i < frame.size(); ++i) {
        stdFrame[i] = frame[i];
    }
    return enqueue(stdFrame, std::move(queryTag), priority, motionGeneration);
}

std::chrono::milliseconds PacedCommandQueue::scheduleRetry(
    CommandItem item, const RetryConfig& retryCfg, std::string_view logReason)
{
    {
        std::scoped_lock lock(m_mutex);
        if (item.motionGeneration > 0U && item.motionGeneration < m_currentMotionGeneration) {
            LOG(INFO) << "Cancelling retry for obsolete motion command (gen " << item.motionGeneration << " < "
                      << m_currentMotionGeneration << ")";
            return std::chrono::milliseconds { 0 };
        }
    }

    item.retryCount++;
    const auto backoffDelay = calculateBackoffDelay(retryCfg, item.retryCount);
    item.earliestDispatchTime = std::chrono::steady_clock::now() + backoffDelay;

    if (!logReason.empty()) {
        LOG(INFO) << logReason << " (attempt " << item.retryCount << "/" << retryCfg.maxRetries << "). Retrying in "
                  << backoffDelay.count() << " ms";
    }

    static_cast<void>(enqueue(std::move(item)));
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

        auto it = m_queue.begin();
        while (it != m_queue.end()) {
            if (it->motionGeneration > 0U && it->motionGeneration < m_currentMotionGeneration) {
                it = m_queue.erase(it);
                continue;
            }
            if (readyIt == m_queue.end() && it->earliestDispatchTime <= curNow) {
                readyIt = it;
            }
            if (it->earliestDispatchTime < earliestWait) {
                earliestWait = it->earliestDispatchTime;
            }
            ++it;
        }

        if (readyIt != m_queue.end()) {
            outItem = std::move(*readyIt);
            m_queue.erase(readyIt);
            return true;
        }

        auto waitTime = defaultTimeout;
        if (nextPollTime != std::chrono::steady_clock::time_point::max()) {
            if (nextPollTime > curNow) {
                const auto pollDiff = std::chrono::duration_cast<std::chrono::milliseconds>(nextPollTime - curNow);
                waitTime = std::min(waitTime, pollDiff);
            } else {
                waitTime = std::chrono::milliseconds(0);
            }
        }

        if (!m_queue.empty() && earliestWait != std::chrono::steady_clock::time_point::max()) {
            const auto backoffDiff = std::chrono::duration_cast<std::chrono::milliseconds>(earliestWait - curNow);
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
        auto checkIt = m_queue.begin();
        while (checkIt != m_queue.end()) {
            if (checkIt->motionGeneration > 0U && checkIt->motionGeneration < m_currentMotionGeneration) {
                checkIt = m_queue.erase(checkIt);
                continue;
            }
            if (checkIt->earliestDispatchTime <= checkNow) {
                outItem = std::move(*checkIt);
                m_queue.erase(checkIt);
                return true;
            }
            ++checkIt;
        }

        return false;
    }

    return false;
}

bool PacedCommandQueue::hasLowPriorityPending() const
{
    std::scoped_lock lock(m_mutex);
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
    std::vector<CommandItem> evictedItems {};
    EvictionHandler handler {};
    {
        std::scoped_lock lock(m_mutex);
        evictedItems.assign(std::make_move_iterator(m_queue.begin()), std::make_move_iterator(m_queue.end()));
        m_queue.clear();
        handler = m_evictionHandler;
    }
    if (handler && !evictedItems.empty()) {
        for (const auto& evicted : evictedItems) {
            handler(evicted);
        }
    }
}

std::size_t PacedCommandQueue::purgeMotionCommands() noexcept
{
    std::scoped_lock lock(m_mutex);
    std::size_t purgedCount { 0U };
    auto it = m_queue.begin();
    while (it != m_queue.end()) {
        if (it->motionGeneration > 0U || PelcoDFrame::isStandardMotion(it->frame)
            || PelcoDFrame::isStandardStop(it->frame)) {
            it = m_queue.erase(it);
            ++purgedCount;
        } else {
            ++it;
        }
    }
    return purgedCount;
}

std::size_t PacedCommandQueue::size() const
{
    std::scoped_lock lock(m_mutex);
    return m_queue.size();
}

bool PacedCommandQueue::empty() const
{
    std::scoped_lock lock(m_mutex);
    return m_queue.empty();
}

std::size_t PacedCommandQueue::maxCapacity() const noexcept
{
    return m_maxCapacity;
}

std::uint64_t PacedCommandQueue::currentMotionGeneration() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_currentMotionGeneration;
}

} // namespace PelcoD
