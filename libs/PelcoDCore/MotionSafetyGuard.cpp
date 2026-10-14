/// @file MotionSafetyGuard.cpp
/// @brief Implementation of motion safety guard and dead-man timer watchdog.

#include "MotionSafetyGuard.h"
#include "PelcoDFrame.h"
#include "PelcoDSession.h"
#include "ProtocolBuilder.h"

#include <glog/logging.h>

#include <algorithm>

namespace PelcoD {

MotionSafetyGuard::MotionSafetyGuard(
    std::shared_ptr<PelcoDSession> session, std::shared_ptr<TaskScheduler> scheduler, MotionSafetyPolicy policy)
    : m_session { session }
    , m_scheduler { std::move(scheduler) }
    , m_policy { policy }
{
    if (!m_scheduler) {
        m_worker = std::thread(&MotionSafetyGuard::watchdogLoop, this);
    }
}

MotionSafetyGuard::MotionSafetyGuard(StopTriggerCallback stopCb, std::shared_ptr<TaskScheduler> scheduler)
    : m_stopCb { std::move(stopCb) }
    , m_scheduler { std::move(scheduler) }
{
    if (!m_scheduler) {
        m_worker = std::thread(&MotionSafetyGuard::watchdogLoop, this);
    }
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
        cancelDeadManLocked();
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
    std::shared_ptr<PelcoDSession> session;
    StopTriggerCallback stopCb;
    SafetyEventCallback eventCb;
    MotionAxis previousAxes { MotionAxis::None };

    {
        std::scoped_lock lock { m_mutex };
        if (reason == MotionSafetyEvent::ContinuousMotionLimit) {
            m_interlocked = true;
        }
        previousAxes = m_activeAxes;
        m_activeAxes = MotionAxis::None;
        m_moving = false;
        m_armed = false;
        m_continuousStart = {};
        cancelDeadManLocked();
        cancelRunawayLocked();
        m_cv.notify_all();

        session = m_session.lock();
        stopCb = m_stopCb;
        eventCb = m_eventCb;
    }

    if (eventCb) {
        eventCb(reason, previousAxes);
    }

    if (session) {
        static_cast<void>(session->purgeMotionCommands());
        const auto stopFrame = ProtocolBuilder::buildStopFrame(session->getAddress());
        return session->enqueueCommand(stopFrame, QueryKind::None, CommandPriority::Urgent);
    }

    if (stopCb) {
        stopCb();
        return SendResult::Success;
    }

    return SendResult::TransportClosed;
}

void MotionSafetyGuard::onMotionCommand(const StandardFrame& frame)
{
    const MotionAxis axes = PelcoDFrame::extractMotionAxes(frame);
    if (axes == MotionAxis::None && !PelcoDFrame::isStandardMotion(frame)) {
        return;
    }

    std::scoped_lock lock { m_mutex };
    updateMotionStateLocked(axes != MotionAxis::None ? axes : MotionAxis::All, false);
}

void MotionSafetyGuard::onMotionCommand(const std::vector<std::uint8_t>& frame)
{
    const MotionAxis axes = PelcoDFrame::extractMotionAxes(frame);
    if (axes == MotionAxis::None && !PelcoDFrame::isStandardMotion(frame)) {
        return;
    }

    std::scoped_lock lock { m_mutex };
    updateMotionStateLocked(axes != MotionAxis::None ? axes : MotionAxis::All, false);
}

void MotionSafetyGuard::onStopCommand() noexcept
{
    std::scoped_lock lock { m_mutex };
    updateMotionStateLocked(MotionAxis::None, true);
}

void MotionSafetyGuard::handleDisconnect() noexcept
{
    if (m_policy.stopOnDisconnect) {
        static_cast<void>(triggerEmergencyStop(MotionSafetyEvent::DisconnectStop));
    } else {
        std::shared_ptr<PelcoDSession> session;
        {
            std::scoped_lock lock { m_mutex };
            m_activeAxes = MotionAxis::None;
            m_moving = false;
            m_armed = false;
            m_continuousStart = {};
            cancelDeadManLocked();
            cancelRunawayLocked();
            m_cv.notify_all();
            session = m_session.lock();
        }
        if (session && m_policy.purgeQueueOnDisconnect) {
            static_cast<void>(session->purgeMotionCommands());
        }
    }
}

void MotionSafetyGuard::onDisconnect() noexcept
{
    handleDisconnect();
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
        m_activeAxes = MotionAxis::None;
        m_moving = false;
        m_armed = false;
        m_continuousStart = {};
        cancelDeadManLocked();
        cancelRunawayLocked();
        m_cv.notify_all();
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }

    if (needStop) {
        static_cast<void>(triggerEmergencyStop(MotionSafetyEvent::ShutdownStop));
    }
}

void MotionSafetyGuard::rescheduleDeadManLocked()
{
    if (!m_scheduler) {
        return;
    }
    if (m_scheduledTaskId != 0U) {
        static_cast<void>(m_scheduler->cancel(m_scheduledTaskId));
    }
    m_scheduledTaskId = m_scheduler->schedule(m_policy.deadManTimeout, [this] { onDeadManTimeout(); });
}

void MotionSafetyGuard::cancelDeadManLocked()
{
    if (m_scheduler && m_scheduledTaskId != 0U) {
        static_cast<void>(m_scheduler->cancel(m_scheduledTaskId));
        m_scheduledTaskId = 0U;
    }
}

void MotionSafetyGuard::rescheduleRunawayLocked()
{
    if (!m_scheduler) {
        return;
    }
    if (m_runawayTaskId != 0U) {
        return; // Monotonic timer active from continuous motion start
    }
    m_runawayTaskId = m_scheduler->schedule(m_policy.maxContinuousDuration, [this] { onRunawayTimeout(); });
}

void MotionSafetyGuard::cancelRunawayLocked()
{
    if (m_scheduler && m_runawayTaskId != 0U) {
        static_cast<void>(m_scheduler->cancel(m_runawayTaskId));
        m_runawayTaskId = 0U;
    }
}

void MotionSafetyGuard::updateMotionStateLocked(const MotionAxis axes, bool isStop)
{
    const auto now = std::chrono::steady_clock::now();
    if (isStop || axes == MotionAxis::None) {
        m_activeAxes = MotionAxis::None;
        m_moving = false;
        m_armed = false;
        m_continuousStart = {};
        m_interlocked = false;
        cancelDeadManLocked();
        cancelRunawayLocked();
        m_cv.notify_all();
        return;
    }

    m_activeAxes = axes;
    m_moving = true;

    if (m_policy.deadManTimeout.count() > 0) {
        m_deadline = now + m_policy.deadManTimeout;
        m_armed = true;
        rescheduleDeadManLocked();
    } else {
        m_armed = false;
        cancelDeadManLocked();
    }

    if (m_policy.maxContinuousDuration.count() > 0) {
        if (m_continuousStart == std::chrono::steady_clock::time_point {}) {
            m_continuousStart = now;
        }
        rescheduleRunawayLocked();
    } else {
        cancelRunawayLocked();
    }

    m_cv.notify_all();
}

void MotionSafetyGuard::onDeadManTimeout()
{
    LOG(WARNING) << "MotionSafetyGuard: dead-man watchdog timeout expired, triggering fail-safe stop";
    static_cast<void>(triggerEmergencyStop(MotionSafetyEvent::DeadManTimeout));
}

void MotionSafetyGuard::onRunawayTimeout()
{
    LOG(WARNING) << "MotionSafetyGuard: continuous motion duration limit exceeded, triggering fail-safe stop";
    static_cast<void>(triggerEmergencyStop(MotionSafetyEvent::ContinuousMotionLimit));
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
            static_cast<void>(triggerEmergencyStop(triggerEvent));
        }
    }
}

} // namespace PelcoD
