/// @file CallbackDispatcher.cpp
/// @brief Implementation of CallbackDispatcher managing observer channels via SignalChannel.

#include "CallbackDispatcher.h"

#include <cstdint>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace PelcoD {

CallbackDispatcher::CallbackDispatcher() = default;

CallbackDispatcher::~CallbackDispatcher()
{
    clearCallbacks();
}

Connection CallbackDispatcher::addStatusCallback(StatusCallback cb)
{
    return m_statusChannel.subscribe(std::move(cb), m_nextId, weak_from_this());
}

Connection CallbackDispatcher::addTrafficCallback(TrafficCallback cb)
{
    return m_trafficChannel.subscribe(std::move(cb), m_nextId, weak_from_this());
}

Connection CallbackDispatcher::addTrafficCallback(TrafficCallback cb, bool notifyTx, bool notifyRx)
{
    if (!cb) {
        return Connection { };
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
        return Connection { };
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
    return m_timeoutChannel.subscribe(std::move(cb), m_nextId, weak_from_this());
}

Connection CallbackDispatcher::addQueryCompletedCallback(QueryCompletedCallback cb)
{
    return m_queryCompletedChannel.subscribe(std::move(cb), m_nextId, weak_from_this());
}

Connection CallbackDispatcher::addRetryCallback(RetryCallback cb)
{
    return m_retryChannel.subscribe(std::move(cb), m_nextId, weak_from_this());
}

Connection CallbackDispatcher::addQueryLatencyCallback(QueryLatencyCallback cb)
{
    return m_queryLatencyChannel.subscribe(std::move(cb), m_nextId, weak_from_this());
}

Connection CallbackDispatcher::addCommandDroppedCallback(CommandDroppedCallback cb)
{
    return m_commandDroppedChannel.subscribe(std::move(cb), m_nextId, weak_from_this());
}

Connection CallbackDispatcher::addMotionSafetyCallback(MotionSafetyCallback cb)
{
    return m_motionSafetyChannel.subscribe(std::move(cb), m_nextId, weak_from_this());
}

bool CallbackDispatcher::removeStatusCallback(CallbackId id)
{
    return m_statusChannel.unsubscribe(id);
}

bool CallbackDispatcher::removeTrafficCallback(CallbackId id)
{
    return m_trafficChannel.unsubscribe(id);
}

bool CallbackDispatcher::removeTimeoutCallback(CallbackId id)
{
    return m_timeoutChannel.unsubscribe(id);
}

bool CallbackDispatcher::removeQueryCompletedCallback(CallbackId id)
{
    return m_queryCompletedChannel.unsubscribe(id);
}

bool CallbackDispatcher::removeRetryCallback(CallbackId id)
{
    return m_retryChannel.unsubscribe(id);
}

bool CallbackDispatcher::removeQueryLatencyCallback(CallbackId id)
{
    return m_queryLatencyChannel.unsubscribe(id);
}

bool CallbackDispatcher::removeCommandDroppedCallback(CallbackId id)
{
    return m_commandDroppedChannel.unsubscribe(id);
}

bool CallbackDispatcher::removeMotionSafetyCallback(CallbackId id)
{
    return m_motionSafetyChannel.unsubscribe(id);
}

void CallbackDispatcher::clearCallbacks()
{
    m_statusChannel.clear();
    m_trafficChannel.clear();
    m_timeoutChannel.clear();
    m_queryCompletedChannel.clear();
    m_retryChannel.clear();
    m_queryLatencyChannel.clear();
    m_commandDroppedChannel.clear();
    m_motionSafetyChannel.clear();
}

void CallbackDispatcher::notifyStatus(const DeviceStatus& status)
{
    m_statusChannel.notify(status);
}

void CallbackDispatcher::notifyTraffic(bool isTx, const std::vector<std::uint8_t>& frame)
{
    m_trafficChannel.notify(isTx, frame);
}

void CallbackDispatcher::notifyTimeout(const std::string& queryTag)
{
    m_timeoutChannel.notify(queryTag);
}

void CallbackDispatcher::notifyQueryCompleted(const std::string& queryTag, bool success, const DeviceStatus& status)
{
    m_queryCompletedChannel.notify(queryTag, success, status);
}

void CallbackDispatcher::notifyRetry(
    const std::string& queryTag, std::uint32_t attempt, std::uint32_t maxRetries, std::chrono::milliseconds delay)
{
    m_retryChannel.notify(queryTag, attempt, maxRetries, delay);
}

void CallbackDispatcher::notifyQueryLatency(
    const std::string& queryTag, std::chrono::microseconds duration, bool success)
{
    m_queryLatencyChannel.notify(queryTag, duration, success);
}

void CallbackDispatcher::notifyCommandDropped(const std::vector<std::uint8_t>& frame, SendResult reason)
{
    m_commandDroppedChannel.notify(frame, reason);
}

void CallbackDispatcher::notifyMotionSafety(MotionSafetyEvent event, MotionAxis axes)
{
    m_motionSafetyChannel.notify(event, axes);
}

} // namespace PelcoD
