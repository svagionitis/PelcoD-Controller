/// @file MotionSafetyGuard.cpp
/// @brief Implementation of motion safety guard and dead-man timer watchdog.

#include "MotionSafetyGuard.h"
#include "PelcoDFrame.h"

#include <glog/logging.h>

namespace PelcoD {

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

void MotionSafetyGuard::setDeadManTimeout(std::chrono::milliseconds timeout) noexcept
{
    std::scoped_lock lock { m_mutex };
    m_timeout = (timeout.count() > 0) ? timeout : std::chrono::milliseconds { 0 };
    if (m_timeout.count() <= 0) {
        m_armed = false;
        if (m_scheduler && m_scheduledTaskId != 0U) {
            static_cast<void>(m_scheduler->cancel(m_scheduledTaskId));
            m_scheduledTaskId = 0U;
        }
    } else if (m_moving) {
        m_deadline = std::chrono::steady_clock::now() + m_timeout;
        m_armed = true;
        if (m_scheduler) {
            if (m_scheduledTaskId != 0U) {
                static_cast<void>(m_scheduler->cancel(m_scheduledTaskId));
            }
            m_scheduledTaskId = m_scheduler->schedule(m_timeout, [this] { onDeadManTimeout(); });
        }
    }
    m_cv.notify_one();
}

std::chrono::milliseconds MotionSafetyGuard::getDeadManTimeout() const noexcept
{
    std::scoped_lock lock { m_mutex };
    return m_timeout;
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

void MotionSafetyGuard::onMotionCommand(const StandardFrame& frame)
{
    if (!PelcoDFrame::isStandardMotion(frame)) {
        return;
    }

    std::scoped_lock lock { m_mutex };
    m_moving = true;
    if (m_timeout.count() > 0) {
        m_deadline = std::chrono::steady_clock::now() + m_timeout;
        m_armed = true;
        if (m_scheduler) {
            if (m_scheduledTaskId != 0U) {
                static_cast<void>(m_scheduler->cancel(m_scheduledTaskId));
            }
            m_scheduledTaskId = m_scheduler->schedule(m_timeout, [this] { onDeadManTimeout(); });
        } else {
            m_cv.notify_one();
        }
    }
}

void MotionSafetyGuard::onMotionCommand(const std::vector<std::uint8_t>& frame)
{
    if (!PelcoDFrame::isStandardMotion(frame)) {
        return;
    }

    std::scoped_lock lock { m_mutex };
    m_moving = true;
    if (m_timeout.count() > 0) {
        m_deadline = std::chrono::steady_clock::now() + m_timeout;
        m_armed = true;
        if (m_scheduler) {
            if (m_scheduledTaskId != 0U) {
                static_cast<void>(m_scheduler->cancel(m_scheduledTaskId));
            }
            m_scheduledTaskId = m_scheduler->schedule(m_timeout, [this] { onDeadManTimeout(); });
        } else {
            m_cv.notify_one();
        }
    }
}

void MotionSafetyGuard::onStopCommand() noexcept
{
    std::scoped_lock lock { m_mutex };
    m_moving = false;
    m_armed = false;
    if (m_scheduler && m_scheduledTaskId != 0U) {
        static_cast<void>(m_scheduler->cancel(m_scheduledTaskId));
        m_scheduledTaskId = 0U;
    }
    m_cv.notify_one();
}

void MotionSafetyGuard::onDisconnect() noexcept
{
    std::scoped_lock lock { m_mutex };
    m_moving = false;
    m_armed = false;
    if (m_scheduler && m_scheduledTaskId != 0U) {
        static_cast<void>(m_scheduler->cancel(m_scheduledTaskId));
        m_scheduledTaskId = 0U;
    }
    m_cv.notify_one();
}

void MotionSafetyGuard::shutdown() noexcept
{
    {
        std::scoped_lock lock { m_mutex };
        if (m_stopRequested) {
            return;
        }
        m_stopRequested = true;
        m_armed = false;
        if (m_scheduler && m_scheduledTaskId != 0U) {
            static_cast<void>(m_scheduler->cancel(m_scheduledTaskId));
            m_scheduledTaskId = 0U;
        }
        m_cv.notify_all();
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void MotionSafetyGuard::onDeadManTimeout()
{
    StopTriggerCallback triggerCb {};
    {
        std::scoped_lock lock { m_mutex };
        if (!m_armed || !m_moving) {
            return;
        }
        m_armed = false;
        m_moving = false;
        m_scheduledTaskId = 0U;
        triggerCb = m_stopCb;
    }

    if (triggerCb) {
        LOG(WARNING) << "MotionSafetyGuard: dead-man watchdog timeout expired, triggering fail-safe stop";
        triggerCb();
    }
}

void MotionSafetyGuard::watchdogLoop()
{
    while (true) {
        StopTriggerCallback triggerCb {};
        {
            std::unique_lock<std::mutex> lock { m_mutex };
            if (m_stopRequested) {
                break;
            }

            if (!m_armed || m_timeout.count() <= 0) {
                m_cv.wait(lock, [this] { return m_stopRequested || (m_armed && m_timeout.count() > 0); });
                continue;
            }

            const auto now = std::chrono::steady_clock::now();
            if (now >= m_deadline) {
                m_armed = false;
                m_moving = false;
                triggerCb = m_stopCb;
            } else {
                const auto waitDuration = m_deadline - now;
                m_cv.wait_for(lock, waitDuration, [this] { return m_stopRequested || !m_armed; });
                continue;
            }
        }

        if (triggerCb) {
            LOG(WARNING) << "MotionSafetyGuard: dead-man watchdog timeout expired, triggering fail-safe stop";
            triggerCb();
        }
    }
}

} // namespace PelcoD
