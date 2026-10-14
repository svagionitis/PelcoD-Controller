#pragma once

/// @file CallbackDispatcher.h
/// @brief Thread-safe observer dispatcher managing copy-on-write subscription lists with exception barriers.

#include "CallbackGate.h"
#include "Connection.h"
#include "DeviceStatus.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace PelcoD {

/// @class CallbackDispatcher
/// @brief Hub managing subscriber channels, thread-safe registration, and isolated callback invocation.
/// @details Implements copy-on-write snapshot vectors for lock-free invocation, CallbackGate drainage
///          upon unregistration, and CERT ERR50-CPP/ERR51-CPP exception boundaries.
class CallbackDispatcher final : public std::enable_shared_from_this<CallbackDispatcher> {
public:
    using StatusCallback = std::function<void(const DeviceStatus& status)>;
    using TrafficCallback = std::function<void(bool isTx, const std::vector<std::uint8_t>& frame)>;
    using TimeoutCallback = std::function<void(const std::string& queryTag)>;
    using QueryCompletedCallback
        = std::function<void(const std::string& queryTag, bool success, const DeviceStatus& status)>;
    using RetryCallback = std::function<void(
        const std::string& queryTag, std::uint32_t attempt, std::uint32_t maxRetries, std::chrono::milliseconds delay)>;
    using QueryLatencyCallback
        = std::function<void(const std::string& queryTag, std::chrono::microseconds duration, bool success)>;

    CallbackDispatcher();
    ~CallbackDispatcher();

    CallbackDispatcher(const CallbackDispatcher&) = delete;
    CallbackDispatcher& operator=(const CallbackDispatcher&) = delete;
    CallbackDispatcher(CallbackDispatcher&&) = delete;
    CallbackDispatcher& operator=(CallbackDispatcher&&) = delete;

    [[nodiscard]] Connection addStatusCallback(StatusCallback cb);
    [[nodiscard]] Connection addTrafficCallback(TrafficCallback cb);
    [[nodiscard]] Connection addTrafficCallback(TrafficCallback cb, bool notifyTx, bool notifyRx);
    [[nodiscard]] Connection addTrafficCallback(
        std::uint8_t addressFilter, TrafficCallback cb, bool notifyTx = true, bool notifyRx = true);
    [[nodiscard]] Connection addTimeoutCallback(TimeoutCallback cb);
    [[nodiscard]] Connection addQueryCompletedCallback(QueryCompletedCallback cb);
    [[nodiscard]] Connection addRetryCallback(RetryCallback cb);
    [[nodiscard]] Connection addQueryLatencyCallback(QueryLatencyCallback cb);

    [[nodiscard]] bool removeStatusCallback(CallbackId id);
    [[nodiscard]] bool removeTrafficCallback(CallbackId id);
    [[nodiscard]] bool removeTimeoutCallback(CallbackId id);
    [[nodiscard]] bool removeQueryCompletedCallback(CallbackId id);
    [[nodiscard]] bool removeRetryCallback(CallbackId id);
    [[nodiscard]] bool removeQueryLatencyCallback(CallbackId id);

    void clearCallbacks();

    void notifyStatus(const DeviceStatus& status);
    void notifyTraffic(bool isTx, const std::vector<std::uint8_t>& frame);
    void notifyTimeout(const std::string& queryTag);
    void notifyQueryCompleted(const std::string& queryTag, bool success, const DeviceStatus& status);
    void notifyRetry(
        const std::string& queryTag, std::uint32_t attempt, std::uint32_t maxRetries, std::chrono::milliseconds delay);
    void notifyQueryLatency(const std::string& queryTag, std::chrono::microseconds duration, bool success);

private:
    template <typename CallbackT> struct CallbackEntry {
        CallbackId id { 0U };
        CallbackT cb {};
        std::shared_ptr<CallbackGate> gate {};
    };

    template <typename CallbackT>
    static bool removeEntry(std::shared_ptr<const std::vector<CallbackEntry<CallbackT>>>& list, CallbackId id,
        std::mutex& mtx, std::shared_ptr<CallbackGate>& outGate)
    {
        std::scoped_lock lock(mtx);
        const auto& current = *list;
        auto it = std::find_if(current.begin(), current.end(), [id](const auto& entry) { return entry.id == id; });
        if (it == current.end()) {
            return false;
        }
        outGate = it->gate;
        auto nextList = std::make_shared<std::vector<CallbackEntry<CallbackT>>>();
        nextList->reserve(current.size() - 1U);
        for (const auto& entry : current) {
            if (entry.id != id) {
                nextList->push_back(entry);
            }
        }
        list = std::move(nextList);
        return true;
    }

    template <typename CallbackT, typename RemoveMemFn>
    Connection registerHelper(CallbackT cb,
        std::shared_ptr<const std::vector<CallbackEntry<CallbackT>>> CallbackDispatcher::*listMember,
        RemoveMemFn removeFn);

    mutable std::mutex m_mutex;
    std::atomic<CallbackId> m_nextId { 1U };

    std::shared_ptr<const std::vector<CallbackEntry<StatusCallback>>> m_statusCallbacks {
        std::make_shared<const std::vector<CallbackEntry<StatusCallback>>>()
    };
    std::shared_ptr<const std::vector<CallbackEntry<TrafficCallback>>> m_trafficCallbacks {
        std::make_shared<const std::vector<CallbackEntry<TrafficCallback>>>()
    };
    std::shared_ptr<const std::vector<CallbackEntry<TimeoutCallback>>> m_timeoutCallbacks {
        std::make_shared<const std::vector<CallbackEntry<TimeoutCallback>>>()
    };
    std::shared_ptr<const std::vector<CallbackEntry<QueryCompletedCallback>>> m_queryCompletedCallbacks {
        std::make_shared<const std::vector<CallbackEntry<QueryCompletedCallback>>>()
    };
    std::shared_ptr<const std::vector<CallbackEntry<RetryCallback>>> m_retryCallbacks {
        std::make_shared<const std::vector<CallbackEntry<RetryCallback>>>()
    };
    std::shared_ptr<const std::vector<CallbackEntry<QueryLatencyCallback>>> m_queryLatencyCallbacks {
        std::make_shared<const std::vector<CallbackEntry<QueryLatencyCallback>>>()
    };
};

} // namespace PelcoD
