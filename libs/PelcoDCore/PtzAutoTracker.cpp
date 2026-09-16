/// @file PtzAutoTracker.cpp
/// @brief Implementation of dual-axis closed-loop automated PTZ tracking orchestrator.

#include "PtzAutoTracker.h"
#include <cmath>

namespace PelcoD {

PtzAutoTracker::PtzAutoTracker()
    : m_panPid(45.0, 1.5, 4.0, 8.0, 0.03, -63.0, 63.0)
    , m_tiltPid(35.0, 1.0, 3.0, 6.0, 0.03, -63.0, 63.0)
{
}

void PtzAutoTracker::reset() noexcept
{
    m_panPid.reset();
    m_tiltPid.reset();
    m_state = TrackingState::Idle;
    m_lostDuration = 0.0;
    m_lastPanSpeed = 0;
    m_lastTiltSpeed = 0;
    m_lastPanDir = 0;
    m_lastTiltDir = 0;
}

void PtzAutoTracker::setPanGains(double kp, double ki, double kd, double kff) noexcept
{
    m_panPid.setGains(kp, ki, kd, kff);
}

void PtzAutoTracker::setTiltGains(double kp, double ki, double kd, double kff) noexcept
{
    m_tiltPid.setGains(kp, ki, kd, kff);
}

void PtzAutoTracker::setDeadbands(double panDeadband, double tiltDeadband) noexcept
{
    m_panPid.setDeadband(panDeadband);
    m_tiltPid.setDeadband(tiltDeadband);
}

void PtzAutoTracker::setMaxSpeeds(int maxPan, int maxTilt) noexcept
{
    m_maxPanSpeed = std::clamp(maxPan, 1, 63);
    m_maxTiltSpeed = std::clamp(maxTilt, 1, 63);
    m_panPid.setOutputLimits(-static_cast<double>(m_maxPanSpeed), static_cast<double>(m_maxPanSpeed));
    m_tiltPid.setOutputLimits(-static_cast<double>(m_maxTiltSpeed), static_cast<double>(m_maxTiltSpeed));
}

PtzAutoTracker::TrackingCommand PtzAutoTracker::update(
    double errorX, double errorY, double vx, double vy, bool isLocked, bool isCoasting, double dt)
{
    TrackingCommand cmd;

    if (!isLocked) {
        if (m_state == TrackingState::Tracking || m_state == TrackingState::Coasting) {
            m_lostDuration += dt;
            if (m_lostDuration < MAX_COAST_DECEL_TIME) {
                // Graceful deceleration ramp
                double decelFactor = 1.0 - (m_lostDuration / MAX_COAST_DECEL_TIME);
                cmd.panDirection = m_lastPanDir;
                cmd.panSpeed = static_cast<int>(std::round(static_cast<double>(m_lastPanSpeed) * decelFactor));
                cmd.tiltDirection = m_lastTiltDir;
                cmd.tiltSpeed = static_cast<int>(std::round(static_cast<double>(m_lastTiltSpeed) * decelFactor));
                cmd.state = TrackingState::Lost;
                cmd.shouldMove = (cmd.panSpeed > 0 || cmd.tiltSpeed > 0);
                return cmd;
            }
        }
        reset();
        cmd.state = TrackingState::Lost;
        cmd.shouldMove = false;
        return cmd;
    }

    m_lostDuration = 0.0;
    m_state = isCoasting ? TrackingState::Coasting : TrackingState::Tracking;

    // Pan Axis: Positive errorX means target is to the right -> Pan Right (+1)
    double panOutput = m_panPid.update(errorX, dt, vx);
    if (panOutput > 0.0) {
        cmd.panDirection = 1;
        cmd.panSpeed = std::clamp(static_cast<int>(std::round(panOutput)), 1, m_maxPanSpeed);
    } else if (panOutput < 0.0) {
        cmd.panDirection = -1;
        cmd.panSpeed = std::clamp(static_cast<int>(std::round(-panOutput)), 1, m_maxPanSpeed);
    } else {
        cmd.panDirection = 0;
        cmd.panSpeed = 0;
    }

    // Tilt Axis: Screen Y increases downward.
    // Positive errorY means target is below center -> Tilt Down (-1)
    // Negative errorY means target is above center -> Tilt Up (+1)
    double tiltOutput = m_tiltPid.update(errorY, dt, vy);
    if (tiltOutput > 0.0) {
        cmd.tiltDirection = -1; // Down
        cmd.tiltSpeed = std::clamp(static_cast<int>(std::round(tiltOutput)), 1, m_maxTiltSpeed);
    } else if (tiltOutput < 0.0) {
        cmd.tiltDirection = 1; // Up
        cmd.tiltSpeed = std::clamp(static_cast<int>(std::round(-tiltOutput)), 1, m_maxTiltSpeed);
    } else {
        cmd.tiltDirection = 0;
        cmd.tiltSpeed = 0;
    }

    cmd.state = m_state;
    cmd.shouldMove = (cmd.panSpeed > 0 || cmd.tiltSpeed > 0);

    m_lastPanDir = cmd.panDirection;
    m_lastPanSpeed = cmd.panSpeed;
    m_lastTiltDir = cmd.tiltDirection;
    m_lastTiltSpeed = cmd.tiltSpeed;

    return cmd;
}

} // namespace PelcoD
