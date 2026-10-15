/// @file MotionSafetyGuard.cpp
/// @brief Implementation of motion safety guard and dead-man timer watchdog.

#include "MotionSafetyGuard.h"
#include "PelcoDFrame.h"
#include "PelcoDSession.h"
#include "ProtocolBuilder.h"

#include <glog/logging.h>

#include <algorithm>

namespace PelcoD {

MotionSafetyGuard::MotionSafetyGuard(std::shared_ptr<PelcoDSession> session, MotionSafetyPolicy policy)
    : m_session { session }
    , m_policy { policy }
{
    m_worker = std::thread(&MotionSafetyGuard::watchdogLoop, this);
}

MotionSafetyGuard::~MotionSafetyGuard()
{
    shutdown();
}

void MotionSafetyGuard::setPolicy(const MotionSafetyPolicy& policy) noexcept
{
    std::scoped_lock lock { m_mutex };
    m_policy = policy;
    if (m_moving) {
        updateMotionStateLocked(m_activeAxes, false);
    }
}

MotionSafetyPolicy MotionSafetyGuard::getPolicy() const noexcept
{
    std::scoped_lock lock { m_mutex };
    return m_policy;
}

void MotionSafetyGuard::setDeadManTimeout(std::chrono::milliseconds timeout) noexcept
{
    std::scoped_lock lock { m_mutex };
    m_policy.deadManTimeout = (timeout.count() > 0) ? timeout : std::chrono::milliseconds { 0 };
    if (m_moving) {
        updateMotionStateLocked(m_activeAxes, false);
    } else {
        m_armed = false;
        m_cv.notify_one();
    }
}

std::chrono::milliseconds MotionSafetyGuard::getDeadManTimeout() const noexcept
{
    std::scoped_lock lock { m_mutex };
    return m_policy.deadManTimeout;
}

void MotionSafetyGuard::setSafetyEventCallback(SafetyEventCallback cb) noexcept
{
    std::scoped_lock lock { m_mutex };
    m_eventCb = std::move(cb);
}

bool MotionSafetyGuard::isArmed() const noexcept
{
    std::scoped_lock lock { m_mutex };
    return m_armed;
}

bool MotionSafetyGuard::isMoving() const noexcept
{
    std::scoped_lock lock { m_mutex };
    return m_moving;
}

bool MotionSafetyGuard::isAxisMoving(MotionAxis axis) const noexcept
{
    std::scoped_lock lock { m_mutex };
    return (m_activeAxes & axis) != MotionAxis::None;
}

MotionAxis MotionSafetyGuard::getActiveAxes() const noexcept
{
    std::scoped_lock lock { m_mutex };
    return m_activeAxes;
}

bool MotionSafetyGuard::isInterlocked() const noexcept
{
    std::scoped_lock lock { m_mutex };
    return m_interlocked;
}

void MotionSafetyGuard::resetInterlock() noexcept
{
    std::scoped_lock lock { m_mutex };
    m_interlocked = false;
}

SendResult MotionSafetyGuard::dispatch(const StandardFrame& frame, QueryKind queryKind, CommandPriority priority)
{
    auto session = m_session.lock();
    if (!session) {
        return SendResult::TransportClosed;
    }

    const MotionAxis axes = PelcoDFrame::extractMotionAxes(frame);
    const bool isStop
        = (axes == MotionAxis::None) && (PelcoDFrame::isStandardStop(frame) || priority == CommandPriority::Urgent);

    {
        std::scoped_lock lock { m_mutex };
        if (m_interlocked) {
            if (!isStop) {
                return SendResult::MotionPurged;
            }
            m_interlocked = false;
        }
    }

    const SendResult result = session->enqueueCommand(frame, queryKind, priority);
    if (result == SendResult::Success) {
        std::scoped_lock lock { m_mutex };
        updateMotionStateLocked(axes, isStop);
    }
    return result;
}

SendResult MotionSafetyGuard::dispatch(
    const StandardFrame& frame, const std::string& queryTag, CommandPriority priority)
{
    auto session = m_session.lock();
    if (!session) {
        return SendResult::TransportClosed;
    }

    const MotionAxis axes = PelcoDFrame::extractMotionAxes(frame);
    const bool isStop
        = (axes == MotionAxis::None) && (PelcoDFrame::isStandardStop(frame) || priority == CommandPriority::Urgent);

    {
        std::scoped_lock lock { m_mutex };
        if (m_interlocked) {
            if (!isStop) {
                return SendResult::MotionPurged;
            }
            m_interlocked = false;
        }
    }

    const SendResult result = session->enqueueCommand(frame, queryTag, priority);
    if (result == SendResult::Success) {
        std::scoped_lock lock { m_mutex };
        updateMotionStateLocked(axes, isStop);
    }
    return result;
}

SendResult MotionSafetyGuard::dispatch(
    const std::vector<std::uint8_t>& frame, QueryKind queryKind, CommandPriority priority)
{
    auto session = m_session.lock();
    if (!session) {
        return SendResult::TransportClosed;
    }

    const MotionAxis axes = PelcoDFrame::extractMotionAxes(frame);
    const bool isStop
        = (axes == MotionAxis::None) && (PelcoDFrame::isStandardStop(frame) || priority == CommandPriority::Urgent);

    {
        std::scoped_lock lock { m_mutex };
        if (m_interlocked) {
            if (!isStop) {
                return SendResult::MotionPurged;
            }
            m_interlocked = false;
        }
    }

    const SendResult result = session->enqueueCommand(frame, queryKind, priority);
    if (result == SendResult::Success) {
        std::scoped_lock lock { m_mutex };
        updateMotionStateLocked(axes, isStop);
    }
    return result;
}

SendResult MotionSafetyGuard::dispatch(
    const std::vector<std::uint8_t>& frame, const std::string& queryTag, CommandPriority priority)
{
    auto session = m_session.lock();
    if (!session) {
        return SendResult::TransportClosed;
    }

    const MotionAxis axes = PelcoDFrame::extractMotionAxes(frame);
    const bool isStop
        = (axes == MotionAxis::None) && (PelcoDFrame::isStandardStop(frame) || priority == CommandPriority::Urgent);

    {
        std::scoped_lock lock { m_mutex };
        if (m_interlocked) {
            if (!isStop) {
                return SendResult::MotionPurged;
            }
            m_interlocked = false;
        }
    }

    const SendResult result = session->enqueueCommand(frame, queryTag, priority);
    if (result == SendResult::Success) {
        std::scoped_lock lock { m_mutex };
        updateMotionStateLocked(axes, isStop);
    }
    return result;
}

SendResult MotionSafetyGuard::triggerEmergencyStop(MotionSafetyEvent reason) noexcept
{
    std::shared_ptr<PelcoDSession> session {};
    SafetyEventCallback eventCb {};
    MotionAxis previousAxes { MotionAxis::None };

    {
        std::scoped_lock lock { m_mutex };
        if (reason == MotionSafetyEvent::ContinuousMotionLimit) {
            m_interlocked = true;
        }
        previousAxes = m_activeAxes;
        resetMotionLocked();
        session = m_session.lock();
        eventCb = m_eventCb;
    }

    if (eventCb) {
        eventCb(reason, previousAxes);
    }

    if (!session) {
        return SendResult::TransportClosed;
    }
    static_cast<void>(session->purgeMotionCommands());
    const auto stopFrame = ProtocolBuilder::buildStopFrame(session->getAddress());
    return session->enqueueCommand(stopFrame, QueryKind::None, CommandPriority::Urgent);
}

void MotionSafetyGuard::handleDisconnect() noexcept
{
    bool stopOnDisconnect { false };
    bool purgeQueue { false };
    std::shared_ptr<PelcoDSession> session {};
    {
        std::scoped_lock lock { m_mutex };
        stopOnDisconnect = m_policy.stopOnDisconnect;
        purgeQueue = m_policy.purgeQueueOnDisconnect;
        if (!stopOnDisconnect) {
            resetMotionLocked();
            session = m_session.lock();
        }
    }

    if (stopOnDisconnect) {
        static_cast<void>(triggerEmergencyStop(MotionSafetyEvent::DisconnectStop));
    } else if (session && purgeQueue) {
        static_cast<void>(session->purgeMotionCommands());
    }
}

void MotionSafetyGuard::shutdown() noexcept
{
    bool needStop { false };
    {
        std::scoped_lock lock { m_mutex };
        if (m_isShuttingDown) {
            return;
        }
        m_isShuttingDown = true;
        m_stopRequested = true;
        needStop = m_moving && m_policy.stopOnShutdown;
        resetMotionLocked();
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }

    if (needStop) {
        static_cast<void>(triggerEmergencyStop(MotionSafetyEvent::ShutdownStop));
    }
}

void MotionSafetyGuard::resetMotionLocked() noexcept
{
    m_activeAxes = MotionAxis::None;
    m_moving = false;
    m_armed = false;
    m_continuousStart = {};
    m_cv.notify_all();
}

void MotionSafetyGuard::updateMotionStateLocked(const MotionAxis axes, bool isStop)
{
    if (isStop || axes == MotionAxis::None) {
        m_interlocked = false;
        resetMotionLocked();
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    m_activeAxes = axes;
    m_moving = true;

    m_armed = (m_policy.deadManTimeout.count() > 0);
    if (m_armed) {
        m_deadline = now + m_policy.deadManTimeout;
    }

    if ((m_policy.maxContinuousDuration.count() > 0)
        && (m_continuousStart == std::chrono::steady_clock::time_point {})) {
        m_continuousStart = now;
    }

    m_cv.notify_all();
}

void MotionSafetyGuard::watchdogLoop()
{
    while (true) {
        MotionSafetyEvent triggerEvent { MotionSafetyEvent::DeadManTimeout };
        bool shouldTrigger { false };

        {
            std::unique_lock<std::mutex> lock { m_mutex };
            if (m_stopRequested) {
                break;
            }

            if (!m_armed && m_continuousStart == std::chrono::steady_clock::time_point {}) {
                m_cv.wait(lock, [this] {
                    return m_stopRequested || m_armed || m_continuousStart != std::chrono::steady_clock::time_point {};
                });
                continue;
            }

            const auto now = std::chrono::steady_clock::now();

            // Runaway limit check
            if (m_policy.maxContinuousDuration.count() > 0
                && m_continuousStart != std::chrono::steady_clock::time_point {}) {
                if (now >= m_continuousStart + m_policy.maxContinuousDuration) {
                    triggerEvent = MotionSafetyEvent::ContinuousMotionLimit;
                    shouldTrigger = true;
                }
            }

            // Dead-man watchdog check
            if (!shouldTrigger && m_armed && m_policy.deadManTimeout.count() > 0) {
                if (now >= m_deadline) {
                    triggerEvent = MotionSafetyEvent::DeadManTimeout;
                    shouldTrigger = true;
                }
            }

            if (!shouldTrigger) {
                auto waitDuration = std::chrono::steady_clock::duration::max();
                if (m_armed && m_policy.deadManTimeout.count() > 0 && m_deadline > now) {
                    waitDuration = std::min(waitDuration, m_deadline - now);
                }
                if (m_policy.maxContinuousDuration.count() > 0
                    && m_continuousStart != std::chrono::steady_clock::time_point {}) {
                    const auto runawayDeadline = m_continuousStart + m_policy.maxContinuousDuration;
                    if (runawayDeadline > now) {
                        waitDuration = std::min(waitDuration, runawayDeadline - now);
                    }
                }
                m_cv.wait_for(lock, waitDuration, [this] { return m_stopRequested; });
                continue;
            }
        }

        if (shouldTrigger) {
            LOG(WARNING) << ((triggerEvent == MotionSafetyEvent::ContinuousMotionLimit)
                    ? "MotionSafetyGuard: continuous motion duration limit exceeded, triggering fail-safe stop"
                    : "MotionSafetyGuard: dead-man watchdog timeout expired, triggering fail-safe stop");
            static_cast<void>(triggerEmergencyStop(triggerEvent));
        }
    }
}

} // namespace PelcoD
