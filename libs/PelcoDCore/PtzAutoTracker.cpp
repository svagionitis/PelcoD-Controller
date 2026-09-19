/// @file PtzAutoTracker.cpp
/// @brief Implementation of 3-axis closed-loop automated PTZ tracking orchestrator.

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
    m_lastZoomDir = 0;
    m_lastLeadOffsetX = 0.0;
    m_lastLeadOffsetY = 0.0;
}

void PtzAutoTracker::setPanGains(double kp, double ki, double kd, double kff) noexcept
{
    m_basePanKp = kp;
    m_basePanKi = ki;
    m_basePanKd = kd;
    m_basePanKff = kff;
    m_panPid.setGains(kp, ki, kd, kff);
}

void PtzAutoTracker::setTiltGains(double kp, double ki, double kd, double kff) noexcept
{
    m_baseTiltKp = kp;
    m_baseTiltKi = ki;
    m_baseTiltKd = kd;
    m_baseTiltKff = kff;
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

void PtzAutoTracker::setAutoZoomEnabled(bool enabled) noexcept
{
    m_autoZoomEnabled = enabled;
}

void PtzAutoTracker::setTargetFramingHeight(double targetNormHeight, double deadband) noexcept
{
    m_targetFramingHeight = std::clamp(targetNormHeight, 0.05, 0.95);
    m_framingDeadband = std::clamp(deadband, 0.005, 0.20);
}

void PtzAutoTracker::setZoomCenteringThreshold(double threshold) noexcept
{
    m_zoomCenteringThreshold = std::clamp(threshold, 0.05, 0.90);
}

void PtzAutoTracker::setPredictiveLeadEnabled(bool enabled) noexcept
{
    m_predictiveLeadEnabled = enabled;
}

void PtzAutoTracker::setLeadGain(double kLead, double maxLead) noexcept
{
    m_leadGain = std::max(0.0, kLead);
    m_maxLead = std::clamp(maxLead, 0.01, 0.80);
}

void PtzAutoTracker::setAdaptiveLatencyEnabled(bool enabled) noexcept
{
    m_adaptiveLatencyEnabled = enabled;
}

void PtzAutoTracker::setEstimatedLatencySeconds(double latencySeconds) noexcept
{
    m_estimatedLatencySeconds = std::clamp(latencySeconds, 0.0, 1.0);
}

void PtzAutoTracker::setZoomGainSchedulingEnabled(bool enabled) noexcept
{
    m_zoomGainSchedulingEnabled = enabled;
}

PtzAutoTracker::TrackingCommand PtzAutoTracker::update(double errorX, double errorY, double vx, double vy,
    bool isLocked, bool isCoasting, double dt, double targetNormHeight, double currentZoom)
{
    TrackingCommand cmd;

    if (!isLocked) {
        if (m_state == TrackingState::Tracking || m_state == TrackingState::Coasting) {
            m_lostDuration += dt;
            if (m_lostDuration < MAX_COAST_DECEL_TIME) {
                // Graceful deceleration ramp for pan/tilt
                double decelFactor = 1.0 - (m_lostDuration / MAX_COAST_DECEL_TIME);
                cmd.panDirection = m_lastPanDir;
                cmd.panSpeed = static_cast<int>(std::round(static_cast<double>(m_lastPanSpeed) * decelFactor));
                cmd.tiltDirection = m_lastTiltDir;
                cmd.tiltSpeed = static_cast<int>(std::round(static_cast<double>(m_lastTiltSpeed) * decelFactor));
                cmd.zoomDirection = 0;
                cmd.zoomSpeed = 0;
                cmd.shouldZoom = false;
                cmd.state = TrackingState::Lost;
                cmd.shouldMove = (cmd.panSpeed > 0 || cmd.tiltSpeed > 0);
                return cmd;
            }
        }
        reset();
        cmd.state = TrackingState::Lost;
        cmd.shouldMove = false;
        cmd.shouldZoom = false;
        return cmd;
    }

    m_lostDuration = 0.0;
    m_state = isCoasting ? TrackingState::Coasting : TrackingState::Tracking;

    // 1. Predictive Lead Angle Boresight Deflection
    double effectiveErrorX = errorX;
    double effectiveErrorY = errorY;
    if (m_predictiveLeadEnabled) {
        const double effectiveLeadGain = m_adaptiveLatencyEnabled ? m_estimatedLatencySeconds : m_leadGain;
        m_lastLeadOffsetX = std::clamp(effectiveLeadGain * vx, -m_maxLead, m_maxLead);
        m_lastLeadOffsetY = std::clamp(effectiveLeadGain * vy, -m_maxLead, m_maxLead);
        effectiveErrorX += m_lastLeadOffsetX;
        effectiveErrorY += m_lastLeadOffsetY;
    } else {
        m_lastLeadOffsetX = 0.0;
        m_lastLeadOffsetY = 0.0;
    }

    // 2. Zoom-Aware Adaptive Gain Scheduling
    if (m_zoomGainSchedulingEnabled && currentZoom > 1.0) {
        const double zoomFactor = std::sqrt(currentZoom);
        m_panPid.setGains(
            m_basePanKp / zoomFactor, m_basePanKi / zoomFactor, m_basePanKd / zoomFactor, m_basePanKff / zoomFactor);
        m_tiltPid.setGains(m_baseTiltKp / zoomFactor, m_baseTiltKi / zoomFactor, m_baseTiltKd / zoomFactor,
            m_baseTiltKff / zoomFactor);
    } else {
        m_panPid.setGains(m_basePanKp, m_basePanKi, m_basePanKd, m_basePanKff);
        m_tiltPid.setGains(m_baseTiltKp, m_baseTiltKi, m_baseTiltKd, m_baseTiltKff);
    }

    // 3. Pan Axis: Positive errorX means target is to the right -> Pan Right (+1)
    double panOutput = m_panPid.update(effectiveErrorX, dt, vx);
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

    // 4. Tilt Axis: Screen Y increases downward.
    // Positive errorY means target is below center -> Tilt Down (-1)
    // Negative errorY means target is above center -> Tilt Up (+1)
    double tiltOutput = m_tiltPid.update(effectiveErrorY, dt, vy);
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

    // 5. Closed-Loop Auto-Zoom Framing
    if (m_autoZoomEnabled && !isCoasting && targetNormHeight > 0.0) {
        const bool isCentered
            = (std::abs(errorX) <= m_zoomCenteringThreshold) && (std::abs(errorY) <= m_zoomCenteringThreshold);

        if (targetNormHeight < (m_targetFramingHeight - m_framingDeadband) && isCentered) {
            cmd.zoomDirection = 1; // Tele (zoom in)
            cmd.zoomSpeed = 32;
            cmd.shouldZoom = true;
        } else if (targetNormHeight > (m_targetFramingHeight + m_framingDeadband)) {
            cmd.zoomDirection = -1; // Wide (zoom out)
            cmd.zoomSpeed = 32;
            cmd.shouldZoom = true;
        } else {
            cmd.zoomDirection = 0;
            cmd.zoomSpeed = 0;
            cmd.shouldZoom = false;
        }
    } else {
        cmd.zoomDirection = 0;
        cmd.zoomSpeed = 0;
        cmd.shouldZoom = false;
    }

    cmd.state = m_state;
    cmd.shouldMove = (cmd.panSpeed > 0 || cmd.tiltSpeed > 0);

    m_lastPanDir = cmd.panDirection;
    m_lastPanSpeed = cmd.panSpeed;
    m_lastTiltDir = cmd.tiltDirection;
    m_lastTiltSpeed = cmd.tiltSpeed;
    m_lastZoomDir = cmd.zoomDirection;

    return cmd;
}

PtzAutoTracker::TrackingCommand PtzAutoTracker::updateAngular(double errorAzimuthDeg, double errorElevationDeg,
    double omegaAzimuthDegPerSec, double omegaElevationDegPerSec, bool isLocked, bool isCoasting,
    double dt, double targetNormHeight, double currentZoom)
{
    TrackingCommand cmd;

    if (!isLocked) {
        if (m_state == TrackingState::Tracking || m_state == TrackingState::Coasting) {
            m_lostDuration += dt;
            if (m_lostDuration < MAX_COAST_DECEL_TIME) {
                // Graceful deceleration ramp for pan/tilt
                double decelFactor = 1.0 - (m_lostDuration / MAX_COAST_DECEL_TIME);
                cmd.panDirection = m_lastPanDir;
                cmd.panSpeed = static_cast<int>(std::round(static_cast<double>(m_lastPanSpeed) * decelFactor));
                cmd.tiltDirection = m_lastTiltDir;
                cmd.tiltSpeed = static_cast<int>(std::round(static_cast<double>(m_lastTiltSpeed) * decelFactor));
                cmd.zoomDirection = 0;
                cmd.zoomSpeed = 0;
                cmd.shouldZoom = false;
                cmd.state = TrackingState::Lost;
                cmd.shouldMove = (cmd.panSpeed > 0 || cmd.tiltSpeed > 0);
                return cmd;
            }
        }
        reset();
        cmd.state = TrackingState::Lost;
        cmd.shouldMove = false;
        cmd.shouldZoom = false;
        return cmd;
    }

    m_lostDuration = 0.0;
    m_state = isCoasting ? TrackingState::Coasting : TrackingState::Tracking;

    // 1. Predictive Lead Angle Deflection in physical angular domain
    double effectiveAz = errorAzimuthDeg;
    double effectiveEl = errorElevationDeg;
    if (m_predictiveLeadEnabled) {
        const double effectiveLeadGain = m_adaptiveLatencyEnabled ? m_estimatedLatencySeconds : m_leadGain;
        // Clamp lead to reasonable angular bounds (e.g. maxLead scaled by field of view / 10 degrees)
        const double maxAngleLead = m_maxLead * 20.0; // max angular deflection lead
        m_lastLeadOffsetX = std::clamp(effectiveLeadGain * omegaAzimuthDegPerSec, -maxAngleLead, maxAngleLead);
        m_lastLeadOffsetY = std::clamp(effectiveLeadGain * omegaElevationDegPerSec, -maxAngleLead, maxAngleLead);
        effectiveAz += m_lastLeadOffsetX;
        effectiveEl += m_lastLeadOffsetY;
    } else {
        m_lastLeadOffsetX = 0.0;
        m_lastLeadOffsetY = 0.0;
    }

    // 2. Zoom-Aware Adaptive Gain Scheduling
    if (m_zoomGainSchedulingEnabled && currentZoom > 1.0) {
        const double zoomFactor = std::sqrt(currentZoom);
        m_panPid.setGains(
            m_basePanKp / zoomFactor, m_basePanKi / zoomFactor, m_basePanKd / zoomFactor, m_basePanKff / zoomFactor);
        m_tiltPid.setGains(m_baseTiltKp / zoomFactor, m_baseTiltKi / zoomFactor, m_baseTiltKd / zoomFactor,
            m_baseTiltKff / zoomFactor);
    } else {
        m_panPid.setGains(m_basePanKp, m_basePanKi, m_basePanKd, m_basePanKff);
        m_tiltPid.setGains(m_baseTiltKp, m_baseTiltKi, m_baseTiltKd, m_baseTiltKff);
    }

    // 3. Pan Axis: Positive errorAzimuthDeg means target is to the right -> Pan Right (+1)
    double panOutput = m_panPid.update(effectiveAz, dt, omegaAzimuthDegPerSec);
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

    // 4. Tilt Axis: Positive errorElevationDeg means target is above boresight -> Tilt Up (+1)
    // Negative errorElevationDeg means target is below boresight -> Tilt Down (-1)
    double tiltOutput = m_tiltPid.update(effectiveEl, dt, omegaElevationDegPerSec);
    if (tiltOutput > 0.0) {
        cmd.tiltDirection = 1; // Up
        cmd.tiltSpeed = std::clamp(static_cast<int>(std::round(tiltOutput)), 1, m_maxTiltSpeed);
    } else if (tiltOutput < 0.0) {
        cmd.tiltDirection = -1; // Down
        cmd.tiltSpeed = std::clamp(static_cast<int>(std::round(-tiltOutput)), 1, m_maxTiltSpeed);
    } else {
        cmd.tiltDirection = 0;
        cmd.tiltSpeed = 0;
    }

    // 5. Closed-Loop Auto-Zoom Framing
    if (m_autoZoomEnabled && !isCoasting && targetNormHeight > 0.0) {
        // In angular mode, consider centered if within centering threshold in degrees (e.g. 2.0 deg)
        const bool isCentered = (std::abs(errorAzimuthDeg) <= 3.0) && (std::abs(errorElevationDeg) <= 3.0);

        if (targetNormHeight < (m_targetFramingHeight - m_framingDeadband) && isCentered) {
            cmd.zoomDirection = 1; // Tele (zoom in)
            cmd.zoomSpeed = 32;
            cmd.shouldZoom = true;
        } else if (targetNormHeight > (m_targetFramingHeight + m_framingDeadband)) {
            cmd.zoomDirection = -1; // Wide (zoom out)
            cmd.zoomSpeed = 32;
            cmd.shouldZoom = true;
        } else {
            cmd.zoomDirection = 0;
            cmd.zoomSpeed = 0;
            cmd.shouldZoom = false;
        }
    } else {
        cmd.zoomDirection = 0;
        cmd.zoomSpeed = 0;
        cmd.shouldZoom = false;
    }

    cmd.state = m_state;
    cmd.shouldMove = (cmd.panSpeed > 0 || cmd.tiltSpeed > 0);

    m_lastPanDir = cmd.panDirection;
    m_lastPanSpeed = cmd.panSpeed;
    m_lastTiltDir = cmd.tiltDirection;
    m_lastTiltSpeed = cmd.tiltSpeed;
    m_lastZoomDir = cmd.zoomDirection;

    return cmd;
}

} // namespace PelcoD
