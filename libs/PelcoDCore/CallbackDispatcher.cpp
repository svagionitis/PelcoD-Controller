#include "CallbackDispatcher.h"

#include <glog/logging.h>
#include <utility>

namespace PelcoD {

namespace {

    template <typename CallbackT, typename... Args>
    void invokeCallbackSafely(const char* channelName, const CallbackT& cb, Args&&... args) noexcept
    {
        try {
            cb(std::forward<Args>(args)...);
        } catch (const std::exception& e) {
            LOG(WARNING) << "CallbackDispatcher: exception in " << channelName << ": " << e.what();
        } catch (...) {
            LOG(WARNING) << "CallbackDispatcher: unknown exception in " << channelName;
        }
    }

} // namespace

CallbackDispatcher::CallbackDispatcher() = default;

CallbackDispatcher::~CallbackDispatcher()
{
    clearCallbacks();
}

template <typename CallbackT, typename RemoveMemFn>
Connection CallbackDispatcher::registerHelper(CallbackT cb,
    std::shared_ptr<const std::vector<CallbackEntry<CallbackT>>> CallbackDispatcher::*listMember, RemoveMemFn removeFn)
{
    if (!cb) {
        return Connection {};
    }
    const auto id = m_nextId.fetch_add(1U, std::memory_order_relaxed);
    auto gate = std::make_shared<CallbackGate>();

    {
        std::scoped_lock lock(m_mutex);
        const auto& current = *(this->*listMember);
        auto nextList = std::make_shared<std::vector<CallbackEntry<CallbackT>>>();
        nextList->reserve(current.size() + 1U);
        for (const auto& entry : current) {
            nextList->push_back(entry);
        }
        nextList->push_back(CallbackEntry<CallbackT> { id, std::move(cb), gate });
        this->*listMember = std::move(nextList);
    }

    std::weak_ptr<CallbackDispatcher> weakSelf = weak_from_this();
    return Connection([weakSelf, id, removeFn, gate]() {
        if (auto self = weakSelf.lock()) {
            (self.get()->*removeFn)(id);
        } else if (gate) {
            gate->close();
        }
    });
}

Connection CallbackDispatcher::addStatusCallback(StatusCallback cb)
{
    return registerHelper(
        std::move(cb), &CallbackDispatcher::m_statusCallbacks, &CallbackDispatcher::removeStatusCallback);
}

Connection CallbackDispatcher::addTrafficCallback(TrafficCallback cb)
{
    return registerHelper(
        std::move(cb), &CallbackDispatcher::m_trafficCallbacks, &CallbackDispatcher::removeTrafficCallback);
}

Connection CallbackDispatcher::addTrafficCallback(TrafficCallback cb, bool notifyTx, bool notifyRx)
{
    if (!cb) {
        return Connection {};
    }
    return addTrafficCallback(
        [cb = std::move(cb), notifyTx, notifyRx](bool isTx, const std::vector<std::uint8_t>& frame) {
            if (isTx && !notifyTx) {
                return;
            }
            if (!isTx && !notifyRx) {
                return;
            }
            cb(isTx, frame);
        });
}

Connection CallbackDispatcher::addTrafficCallback(
    std::uint8_t addressFilter, TrafficCallback cb, bool notifyTx, bool notifyRx)
{
    if (!cb) {
        return Connection {};
    }
    return addTrafficCallback(
        [cb = std::move(cb), addressFilter, notifyTx, notifyRx](bool isTx, const std::vector<std::uint8_t>& frame) {
            if (isTx && !notifyTx) {
                return;
            }
            if (!isTx && !notifyRx) {
                return;
            }
            if (frame.size() >= 2U && frame[1] != addressFilter) {
                return;
            }
            cb(isTx, frame);
        });
}

Connection CallbackDispatcher::addTimeoutCallback(TimeoutCallback cb)
{
    return registerHelper(
        std::move(cb), &CallbackDispatcher::m_timeoutCallbacks, &CallbackDispatcher::removeTimeoutCallback);
}

Connection CallbackDispatcher::addQueryCompletedCallback(QueryCompletedCallback cb)
{
    return registerHelper(std::move(cb), &CallbackDispatcher::m_queryCompletedCallbacks,
        &CallbackDispatcher::removeQueryCompletedCallback);
}

Connection CallbackDispatcher::addRetryCallback(RetryCallback cb)
{
    return registerHelper(
        std::move(cb), &CallbackDispatcher::m_retryCallbacks, &CallbackDispatcher::removeRetryCallback);
}

Connection CallbackDispatcher::addQueryLatencyCallback(QueryLatencyCallback cb)
{
    return registerHelper(
        std::move(cb), &CallbackDispatcher::m_queryLatencyCallbacks, &CallbackDispatcher::removeQueryLatencyCallback);
}

bool CallbackDispatcher::removeStatusCallback(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate;
    const bool found = removeEntry(m_statusCallbacks, id, m_mutex, gate);
    if (found && gate) {
        gate->close();
    }
    return found;
}

bool CallbackDispatcher::removeTrafficCallback(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate;
    const bool found = removeEntry(m_trafficCallbacks, id, m_mutex, gate);
    if (found && gate) {
        gate->close();
    }
    return found;
}

bool CallbackDispatcher::removeTimeoutCallback(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate;
    const bool found = removeEntry(m_timeoutCallbacks, id, m_mutex, gate);
    if (found && gate) {
        gate->close();
    }
    return found;
}

bool CallbackDispatcher::removeQueryCompletedCallback(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate;
    const bool found = removeEntry(m_queryCompletedCallbacks, id, m_mutex, gate);
    if (found && gate) {
        gate->close();
    }
    return found;
}

bool CallbackDispatcher::removeRetryCallback(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate;
    const bool found = removeEntry(m_retryCallbacks, id, m_mutex, gate);
    if (found && gate) {
        gate->close();
    }
    return found;
}

bool CallbackDispatcher::removeQueryLatencyCallback(CallbackId id)
{
    std::shared_ptr<CallbackGate> gate;
    const bool found = removeEntry(m_queryLatencyCallbacks, id, m_mutex, gate);
    if (found && gate) {
        gate->close();
    }
    return found;
}

void CallbackDispatcher::clearCallbacks()
{
    std::vector<std::shared_ptr<CallbackGate>> gatesToClose;
    {
        std::scoped_lock lock(m_mutex);
        for (const auto& entry : *m_statusCallbacks) {
            if (entry.gate)
                gatesToClose.push_back(entry.gate);
        }
        for (const auto& entry : *m_trafficCallbacks) {
            if (entry.gate)
                gatesToClose.push_back(entry.gate);
        }
        for (const auto& entry : *m_timeoutCallbacks) {
            if (entry.gate)
                gatesToClose.push_back(entry.gate);
        }
        for (const auto& entry : *m_queryCompletedCallbacks) {
            if (entry.gate)
                gatesToClose.push_back(entry.gate);
        }
        for (const auto& entry : *m_retryCallbacks) {
            if (entry.gate)
                gatesToClose.push_back(entry.gate);
        }
        for (const auto& entry : *m_queryLatencyCallbacks) {
            if (entry.gate)
                gatesToClose.push_back(entry.gate);
        }
        m_statusCallbacks = std::make_shared<const std::vector<CallbackEntry<StatusCallback>>>();
        m_trafficCallbacks = std::make_shared<const std::vector<CallbackEntry<TrafficCallback>>>();
        m_timeoutCallbacks = std::make_shared<const std::vector<CallbackEntry<TimeoutCallback>>>();
        m_queryCompletedCallbacks = std::make_shared<const std::vector<CallbackEntry<QueryCompletedCallback>>>();
        m_retryCallbacks = std::make_shared<const std::vector<CallbackEntry<RetryCallback>>>();
        m_queryLatencyCallbacks = std::make_shared<const std::vector<CallbackEntry<QueryLatencyCallback>>>();
    }
    for (auto& gate : gatesToClose) {
        if (gate) {
            gate->close();
        }
    }
}

void CallbackDispatcher::notifyStatus(const DeviceStatus& status)
{
    std::shared_ptr<const std::vector<CallbackEntry<StatusCallback>>> snapshot;
    {
        std::scoped_lock lock(m_mutex);
        snapshot = m_statusCallbacks;
    }
    for (const auto& entry : *snapshot) {
        if (entry.gate) {
            const CallbackGate::Pass pass { *entry.gate };
            if (pass) {
                invokeCallbackSafely("StatusCallback", entry.cb, status);
            }
        }
    }
}

void CallbackDispatcher::notifyTraffic(bool isTx, const std::vector<std::uint8_t>& frame)
{
    std::shared_ptr<const std::vector<CallbackEntry<TrafficCallback>>> snapshot;
    {
        std::scoped_lock lock(m_mutex);
        snapshot = m_trafficCallbacks;
    }
    for (const auto& entry : *snapshot) {
        if (entry.gate) {
            const CallbackGate::Pass pass { *entry.gate };
            if (pass) {
                invokeCallbackSafely("TrafficCallback", entry.cb, isTx, frame);
            }
        }
    }
}

void CallbackDispatcher::notifyTimeout(const std::string& queryTag)
{
    std::shared_ptr<const std::vector<CallbackEntry<TimeoutCallback>>> snapshot;
    {
        std::scoped_lock lock(m_mutex);
        snapshot = m_timeoutCallbacks;
    }
    for (const auto& entry : *snapshot) {
        if (entry.gate) {
            const CallbackGate::Pass pass { *entry.gate };
            if (pass) {
                invokeCallbackSafely("TimeoutCallback", entry.cb, queryTag);
            }
        }
    }
}

void CallbackDispatcher::notifyQueryCompleted(const std::string& queryTag, bool success, const DeviceStatus& status)
{
    std::shared_ptr<const std::vector<CallbackEntry<QueryCompletedCallback>>> snapshot;
    {
        std::scoped_lock lock(m_mutex);
        snapshot = m_queryCompletedCallbacks;
    }
    for (const auto& entry : *snapshot) {
        if (entry.gate) {
            const CallbackGate::Pass pass { *entry.gate };
            if (pass) {
                invokeCallbackSafely("QueryCompletedCallback", entry.cb, queryTag, success, status);
            }
        }
    }
}

void CallbackDispatcher::notifyRetry(
    const std::string& queryTag, std::uint32_t attempt, std::uint32_t maxRetries, std::chrono::milliseconds delay)
{
    std::shared_ptr<const std::vector<CallbackEntry<RetryCallback>>> snapshot;
    {
        std::scoped_lock lock(m_mutex);
        snapshot = m_retryCallbacks;
    }
    for (const auto& entry : *snapshot) {
        if (entry.gate) {
            const CallbackGate::Pass pass { *entry.gate };
            if (pass) {
                invokeCallbackSafely("RetryCallback", entry.cb, queryTag, attempt, maxRetries, delay);
            }
        }
    }
}

void CallbackDispatcher::notifyQueryLatency(
    const std::string& queryTag, std::chrono::microseconds duration, bool success)
{
    std::shared_ptr<const std::vector<CallbackEntry<QueryLatencyCallback>>> snapshot;
    {
        std::scoped_lock lock(m_mutex);
        snapshot = m_queryLatencyCallbacks;
    }
    for (const auto& entry : *snapshot) {
        if (entry.gate) {
            const CallbackGate::Pass pass { *entry.gate };
            if (pass) {
                invokeCallbackSafely("QueryLatencyCallback", entry.cb, queryTag, duration, success);
            }
        }
    }
}

} // namespace PelcoD
