#pragma once

/// @file CallbackDispatcher.h
/// @brief Thread-safe observer dispatcher managing copy-on-write subscription lists with exception barriers.

#include "CallbackGate.h"
#include "Connection.h"
#include "DeviceStatus.h"
#include "MotionSafetyGuard.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <glog/logging.h>

namespace PelcoD {

/// @class SignalChannel
/// @brief Generic thread-safe copy-on-write subscription channel with CallbackGate drainage.
/// @tparam Args Argument types emitted to subscriber callbacks.
template <typename... Args> class SignalChannel final {
public:
    using CallbackType = std::function<void(Args...)>;

    struct Entry {
        CallbackId id { 0U };
        CallbackType cb {};
        std::shared_ptr<CallbackGate> gate {};
    };

    /// @brief Constructs a named signal channel.
    /// @param[in] name Channel diagnostic name used in log messages.
    explicit SignalChannel(const char* name = "SignalChannel") noexcept
        : m_name { name }
        , m_list { std::make_shared<const std::vector<Entry>>() }
    {
    }

    ~SignalChannel() = default;

    SignalChannel(const SignalChannel&) = delete;
    SignalChannel& operator=(const SignalChannel&) = delete;
    SignalChannel(SignalChannel&&) = delete;
    SignalChannel& operator=(SignalChannel&&) = delete;

    /// @brief Registers an observer callback, returning a scoped RAII Connection.
    /// @param[in] cb Callback to register.
    /// @param[in,out] idGen Shared atomic counter generating unique callback IDs.
    /// @param[in] weakDispatcher Weak pointer to parent dispatcher to coordinate unsubscription.
    /// @return Connection object whose destruction or disconnect() unregisters the callback.
    [[nodiscard]] Connection subscribe(
        CallbackType cb, std::atomic<CallbackId>& idGen, std::weak_ptr<void> weakDispatcher)
    {
        if (!cb) {
            return Connection {};
        }
        const auto id = idGen.fetch_add(1U, std::memory_order_relaxed);
        auto gate = std::make_shared<CallbackGate>();

        {
            std::scoped_lock lock { m_mutex };
            const auto& current = *m_list;
            auto nextList = std::make_shared<std::vector<Entry>>();
            nextList->reserve(current.size() + 1U);
            for (const auto& entry : current) {
                nextList->push_back(entry);
            }
            nextList->push_back(Entry { id, std::move(cb), gate });
            m_list = std::move(nextList);
        }

        return Connection([this, weakDispatcher, id, gate]() {
            if (auto pinned = weakDispatcher.lock()) {
                static_cast<void>(unsubscribe(id));
            } else if (gate) {
                gate->close();
            }
        });
    }

    /// @brief Unregisters a callback by ID, closing its gate and draining in-flight invocations.
    /// @param[in] id Unique callback identifier.
    /// @return True if found and removed; false if not found.
    bool unsubscribe(CallbackId id)
    {
        std::shared_ptr<CallbackGate> gateToClose;
        {
            std::scoped_lock lock { m_mutex };
            const auto& current = *m_list;
            auto it = std::find_if(current.begin(), current.end(), [id](const auto& entry) { return entry.id == id; });
            if (it == current.end()) {
                return false;
            }
            gateToClose = it->gate;
            auto nextList = std::make_shared<std::vector<Entry>>();
            nextList->reserve(current.size() - 1U);
            for (const auto& entry : current) {
                if (entry.id != id) {
                    nextList->push_back(entry);
                }
            }
            m_list = std::move(nextList);
        }
        if (gateToClose) {
            gateToClose->close();
        }
        return true;
    }

    /// @brief Purges all registered callbacks, draining in-flight invocations across all gates.
    void clear()
    {
        std::vector<std::shared_ptr<CallbackGate>> gatesToClose;
        {
            std::scoped_lock lock { m_mutex };
            for (const auto& entry : *m_list) {
                if (entry.gate) {
                    gatesToClose.push_back(entry.gate);
                }
            }
            m_list = std::make_shared<const std::vector<Entry>>();
        }
        for (auto& gate : gatesToClose) {
            if (gate) {
                gate->close();
            }
        }
    }

    /// @brief Dispatches notification to all registered observers under an exception barrier.
    /// @param[in] args Arguments forwarded to subscriber callbacks.
    void notify(const Args&... args)
    {
        std::shared_ptr<const std::vector<Entry>> snapshot;
        {
            std::scoped_lock lock { m_mutex };
            snapshot = m_list;
        }
        for (const auto& entry : *snapshot) {
            if (entry.gate) {
                const CallbackGate::Pass pass { *entry.gate };
                if (pass) {
                    try {
                        entry.cb(args...);
                    } catch (const std::exception& e) {
                        LOG(WARNING) << "CallbackDispatcher: exception in " << m_name << ": " << e.what();
                    } catch (...) {
                        LOG(WARNING) << "CallbackDispatcher: unknown exception in " << m_name;
                    }
                }
            }
        }
    }

private:
    const char* m_name { "SignalChannel" };
    mutable std::mutex m_mutex {};
    std::shared_ptr<const std::vector<Entry>> m_list {};
};

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
    using CommandDroppedCallback = std::function<void(const std::vector<std::uint8_t>& frame, SendResult reason)>;
    using MotionSafetyCallback = std::function<void(MotionSafetyEvent event, MotionAxis axes)>;

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
    [[nodiscard]] Connection addCommandDroppedCallback(CommandDroppedCallback cb);
    [[nodiscard]] Connection addMotionSafetyCallback(MotionSafetyCallback cb);

    [[nodiscard]] bool removeStatusCallback(CallbackId id);
    [[nodiscard]] bool removeTrafficCallback(CallbackId id);
    [[nodiscard]] bool removeTimeoutCallback(CallbackId id);
    [[nodiscard]] bool removeQueryCompletedCallback(CallbackId id);
    [[nodiscard]] bool removeRetryCallback(CallbackId id);
    [[nodiscard]] bool removeQueryLatencyCallback(CallbackId id);
    [[nodiscard]] bool removeCommandDroppedCallback(CallbackId id);
    [[nodiscard]] bool removeMotionSafetyCallback(CallbackId id);

    void clearCallbacks();

    void notifyStatus(const DeviceStatus& status);
    void notifyTraffic(bool isTx, const std::vector<std::uint8_t>& frame);
    void notifyTimeout(const std::string& queryTag);
    void notifyQueryCompleted(const std::string& queryTag, bool success, const DeviceStatus& status);
    void notifyRetry(
        const std::string& queryTag, std::uint32_t attempt, std::uint32_t maxRetries, std::chrono::milliseconds delay);
    void notifyQueryLatency(const std::string& queryTag, std::chrono::microseconds duration, bool success);
    void notifyCommandDropped(const std::vector<std::uint8_t>& frame, SendResult reason);
    void notifyMotionSafety(MotionSafetyEvent event, MotionAxis axes);

private:
    std::atomic<CallbackId> m_nextId { 1U };

    SignalChannel<const DeviceStatus&> m_statusChannel { "StatusCallback" };
    SignalChannel<bool, const std::vector<std::uint8_t>&> m_trafficChannel { "TrafficCallback" };
    SignalChannel<const std::string&> m_timeoutChannel { "TimeoutCallback" };
    SignalChannel<const std::string&, bool, const DeviceStatus&> m_queryCompletedChannel { "QueryCompletedCallback" };
    SignalChannel<const std::string&, std::uint32_t, std::uint32_t, std::chrono::milliseconds> m_retryChannel {
        "RetryCallback"
    };
    SignalChannel<const std::string&, std::chrono::microseconds, bool> m_queryLatencyChannel { "QueryLatencyCallback" };
    SignalChannel<const std::vector<std::uint8_t>&, SendResult> m_commandDroppedChannel { "CommandDroppedCallback" };
    SignalChannel<MotionSafetyEvent, MotionAxis> m_motionSafetyChannel { "MotionSafetyCallback" };
};

} // namespace PelcoD
