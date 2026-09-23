/// @file KinematicsSimulator.cpp
/// @brief Physical PTZ motion dynamics, velocity profiling, and angular slewing simulation.

#include "KinematicsSimulator.h"

#include <algorithm>
#include <cmath>

namespace PelcoD {

namespace {
    constexpr double kFloatEpsilon = 1e-6;
} // namespace

KinematicsSimulator::KinematicsSimulator()
    : m_lastUpdateTime(std::chrono::steady_clock::now())
    , m_hasTimestamp(true)
{
}

void KinematicsSimulator::setConfig(const KinematicsConfig& config)
{
    std::scoped_lock lock(m_mutex);
    m_config = config;
}

KinematicsConfig KinematicsSimulator::getConfig() const
{
    std::scoped_lock lock(m_mutex);
    return m_config;
}

void KinematicsSimulator::setPositionImmediate(double panDeg, double tiltDeg, double zoom)
{
    std::scoped_lock lock(m_mutex);
    m_currentPanDeg = normalizePanDeg(panDeg);
    m_currentTiltDeg = std::clamp(tiltDeg, m_config.minTiltDeg, m_config.maxTiltDeg);
    m_currentZoom = std::clamp(zoom, 1000.0, 65535.0);
    m_targetPanDeg = m_currentPanDeg;
    m_targetTiltDeg = m_currentTiltDeg;
    m_targetZoom = m_currentZoom;
    m_panVelocity = 0.0;
    m_tiltVelocity = 0.0;
    m_zoomVelocity = 0.0;
    m_mode = MotionMode::Stopped;
    m_lastUpdateTime = std::chrono::steady_clock::now();
}

void KinematicsSimulator::setDirectionalMotion(double panFraction, double tiltFraction, double zoomFraction)
{
    std::scoped_lock lock(m_mutex);
    const auto now = std::chrono::steady_clock::now();

    m_targetPanVelocity = std::clamp(panFraction, -1.0, 1.0) * m_config.maxPanSpeedDegPerSec;
    m_targetTiltVelocity = std::clamp(tiltFraction, -1.0, 1.0) * m_config.maxTiltSpeedDegPerSec;

    const double fullZoomSpan = 65535.0 - 1000.0;
    const double maxZoomSpeed = fullZoomSpan / std::max(0.1, m_config.zoomTransitTimeSeconds);
    m_targetZoomVelocity = std::clamp(zoomFraction, -1.0, 1.0) * maxZoomSpeed;

    if (m_mode == MotionMode::Stopped
        && (std::abs(panFraction) > kFloatEpsilon || std::abs(tiltFraction) > kFloatEpsilon
            || std::abs(zoomFraction) > kFloatEpsilon)) {
        m_mode = MotionMode::ManualVelocity;
    }

    if (!m_config.enabled) {
        // Without kinematics enabled, immediately adopt target velocity
        m_panVelocity = m_targetPanVelocity;
        m_tiltVelocity = m_targetTiltVelocity;
        m_zoomVelocity = m_targetZoomVelocity;
    }

    if (std::abs(panFraction) <= kFloatEpsilon && std::abs(tiltFraction) <= kFloatEpsilon
        && std::abs(zoomFraction) <= kFloatEpsilon && !m_config.enabled) {
        m_mode = MotionMode::Stopped;
    }

    m_lastUpdateTime = now;
    m_hasTimestamp = true;
}

void KinematicsSimulator::slewTo(double targetPanDeg, double targetTiltDeg)
{
    std::scoped_lock lock(m_mutex);
    m_targetPanDeg = normalizePanDeg(targetPanDeg);
    m_targetTiltDeg = std::clamp(targetTiltDeg, m_config.minTiltDeg, m_config.maxTiltDeg);

    if (!m_config.enabled) {
        m_currentPanDeg = m_targetPanDeg;
        m_currentTiltDeg = m_targetTiltDeg;
        m_panVelocity = 0.0;
        m_tiltVelocity = 0.0;
        m_mode = MotionMode::Stopped;
    } else {
        m_mode = MotionMode::SlewingToTarget;
    }

    m_lastUpdateTime = std::chrono::steady_clock::now();
    m_hasTimestamp = true;
}

void KinematicsSimulator::slewZoomTo(double targetZoom)
{
    std::scoped_lock lock(m_mutex);
    m_targetZoom = std::clamp(targetZoom, 1000.0, 65535.0);

    if (!m_config.enabled) {
        m_currentZoom = m_targetZoom;
        m_zoomVelocity = 0.0;
    } else {
        m_mode = MotionMode::SlewingToTarget;
    }

    m_lastUpdateTime = std::chrono::steady_clock::now();
    m_hasTimestamp = true;
}

void KinematicsSimulator::stop()
{
    std::scoped_lock lock(m_mutex);
    m_targetPanVelocity = 0.0;
    m_targetTiltVelocity = 0.0;
    m_targetZoomVelocity = 0.0;

    if (!m_config.enabled) {
        m_panVelocity = 0.0;
        m_tiltVelocity = 0.0;
        m_zoomVelocity = 0.0;
        m_mode = MotionMode::Stopped;
    } else if (m_mode == MotionMode::SlewingToTarget) {
        // Cancel slew immediately
        m_mode = MotionMode::ManualVelocity;
    }

    m_lastUpdateTime = std::chrono::steady_clock::now();
}

void KinematicsSimulator::update(std::chrono::steady_clock::time_point now)
{
    std::scoped_lock lock(m_mutex);
    if (!m_hasTimestamp) {
        m_lastUpdateTime = now;
        m_hasTimestamp = true;
        return;
    }

    double dt = std::chrono::duration<double>(now - m_lastUpdateTime).count();
    m_lastUpdateTime = now;

    if (dt <= 0.0) {
        return;
    }
    // Cap delta-time to avoid huge positional steps after debugger pauses
    if (dt > 0.2) {
        dt = 0.2;
    }

    if (m_mode == MotionMode::Stopped) {
        m_panVelocity = 0.0;
        m_tiltVelocity = 0.0;
        m_zoomVelocity = 0.0;
        return;
    }

    if (!m_config.enabled) {
        if (m_mode == MotionMode::ManualVelocity) {
            m_currentPanDeg = normalizePanDeg(m_currentPanDeg + m_targetPanVelocity * dt);
            m_currentTiltDeg
                = std::clamp(m_currentTiltDeg + m_targetTiltVelocity * dt, m_config.minTiltDeg, m_config.maxTiltDeg);
            m_currentZoom = std::clamp(m_currentZoom + m_targetZoomVelocity * dt, 1000.0, 65535.0);
        } else if (m_mode == MotionMode::SlewingToTarget) {
            m_currentPanDeg = m_targetPanDeg;
            m_currentTiltDeg = m_targetTiltDeg;
            m_currentZoom = m_targetZoom;
            m_mode = MotionMode::Stopped;
        }
        return;
    }

    // Kinematics physics simulation
    if (m_mode == MotionMode::ManualVelocity) {
        // Accelerate or decelerate toward target velocity
        const auto stepVelocity = [](double currentVel, double targetVel, double accel, double stepDt) noexcept {
            const double diff = targetVel - currentVel;
            const double maxChange = accel * stepDt;
            if (std::abs(diff) <= maxChange) {
                return targetVel;
            }
            return currentVel + std::copysign(maxChange, diff);
        };

        m_panVelocity = stepVelocity(m_panVelocity, m_targetPanVelocity, m_config.panAccelerationDegPerSec2, dt);
        m_tiltVelocity = stepVelocity(m_tiltVelocity, m_targetTiltVelocity, m_config.tiltAccelerationDegPerSec2, dt);

        const double fullZoomSpan = 65535.0 - 1000.0;
        const double zoomAccel = fullZoomSpan / std::max(0.1, m_config.zoomTransitTimeSeconds);
        m_zoomVelocity = stepVelocity(m_zoomVelocity, m_targetZoomVelocity, zoomAccel, dt);

        m_currentPanDeg = normalizePanDeg(m_currentPanDeg + m_panVelocity * dt);
        m_currentTiltDeg = std::clamp(m_currentTiltDeg + m_tiltVelocity * dt, m_config.minTiltDeg, m_config.maxTiltDeg);
        m_currentZoom = std::clamp(m_currentZoom + m_zoomVelocity * dt, 1000.0, 65535.0);

        if (std::abs(m_targetPanVelocity) <= kFloatEpsilon && std::abs(m_targetTiltVelocity) <= kFloatEpsilon
            && std::abs(m_targetZoomVelocity) <= kFloatEpsilon && std::abs(m_panVelocity) < 0.01
            && std::abs(m_tiltVelocity) < 0.01 && std::abs(m_zoomVelocity) < 0.01) {
            m_panVelocity = 0.0;
            m_tiltVelocity = 0.0;
            m_zoomVelocity = 0.0;
            m_mode = MotionMode::Stopped;
        }
    } else if (m_mode == MotionMode::SlewingToTarget) {
        bool panArrived { false };
        bool tiltArrived { false };
        bool zoomArrived { false };

        // 1. Pan shortest-arc profile
        const double panDiff = shortestAngularDelta(m_currentPanDeg, m_targetPanDeg);
        if (std::abs(panDiff) < 0.2) {
            m_currentPanDeg = m_targetPanDeg;
            m_panVelocity = 0.0;
            panArrived = true;
        } else {
            const double accel = m_config.panAccelerationDegPerSec2;
            const double maxSpeed = m_config.maxPanSpeedDegPerSec;
            // Stopping distance: d = v^2 / (2a)
            const double decelDist = (m_panVelocity * m_panVelocity) / (2.0 * accel);
            double desiredSpeed = maxSpeed;
            if (std::abs(panDiff) <= decelDist + 0.1) {
                desiredSpeed = std::max(0.5, std::sqrt(2.0 * accel * std::abs(panDiff)));
            }
            desiredSpeed = std::min(desiredSpeed, std::max(0.5, std::abs(panDiff) / std::max(0.001, dt)));

            const double targetVel = std::copysign(desiredSpeed, panDiff);
            const double diffVel = targetVel - m_panVelocity;
            const double maxDv = accel * dt;
            if (std::abs(diffVel) <= maxDv) {
                m_panVelocity = targetVel;
            } else {
                m_panVelocity += std::copysign(maxDv, diffVel);
            }
            const double step = m_panVelocity * dt;
            if ((panDiff > 0.0 && step >= panDiff) || (panDiff < 0.0 && step <= panDiff)) {
                m_currentPanDeg = m_targetPanDeg;
                m_panVelocity = 0.0;
                panArrived = true;
            } else {
                m_currentPanDeg = normalizePanDeg(m_currentPanDeg + step);
            }
        }

        // 2. Tilt profile
        const double tiltDiff = m_targetTiltDeg - m_currentTiltDeg;
        if (std::abs(tiltDiff) < 0.2) {
            m_currentTiltDeg = m_targetTiltDeg;
            m_tiltVelocity = 0.0;
            tiltArrived = true;
        } else {
            const double accel = m_config.tiltAccelerationDegPerSec2;
            const double maxSpeed = m_config.maxTiltSpeedDegPerSec;
            const double decelDist = (m_tiltVelocity * m_tiltVelocity) / (2.0 * accel);
            double desiredSpeed = maxSpeed;
            if (std::abs(tiltDiff) <= decelDist + 0.1) {
                desiredSpeed = std::max(0.5, std::sqrt(2.0 * accel * std::abs(tiltDiff)));
            }
            desiredSpeed = std::min(desiredSpeed, std::max(0.5, std::abs(tiltDiff) / std::max(0.001, dt)));

            const double targetVel = std::copysign(desiredSpeed, tiltDiff);
            const double diffVel = targetVel - m_tiltVelocity;
            const double maxDv = accel * dt;
            if (std::abs(diffVel) <= maxDv) {
                m_tiltVelocity = targetVel;
            } else {
                m_tiltVelocity += std::copysign(maxDv, diffVel);
            }
            const double step = m_tiltVelocity * dt;
            if ((tiltDiff > 0.0 && step >= tiltDiff) || (tiltDiff < 0.0 && step <= tiltDiff)) {
                m_currentTiltDeg = m_targetTiltDeg;
                m_tiltVelocity = 0.0;
                tiltArrived = true;
            } else {
                m_currentTiltDeg = std::clamp(m_currentTiltDeg + step, m_config.minTiltDeg, m_config.maxTiltDeg);
            }
        }

        // 3. Zoom profile
        const double zoomDiff = m_targetZoom - m_currentZoom;
        const double fullZoomSpan = 65535.0 - 1000.0;
        const double zoomSpeed = fullZoomSpan / std::max(0.1, m_config.zoomTransitTimeSeconds);
        const double step = zoomSpeed * dt;
        if (std::abs(zoomDiff) <= step || std::abs(zoomDiff) < 10.0) {
            m_currentZoom = m_targetZoom;
            m_zoomVelocity = 0.0;
            zoomArrived = true;
        } else {
            m_currentZoom = std::clamp(m_currentZoom + std::copysign(step, zoomDiff), 1000.0, 65535.0);
        }

        if (panArrived && tiltArrived && zoomArrived) {
            m_mode = MotionMode::Stopped;
        }
    }
}

bool KinematicsSimulator::isMoving() const
{
    std::scoped_lock lock(m_mutex);
    return m_mode != MotionMode::Stopped || std::abs(m_panVelocity) > 0.01 || std::abs(m_tiltVelocity) > 0.01;
}

double KinematicsSimulator::currentPanDeg() const
{
    std::scoped_lock lock(m_mutex);
    return m_currentPanDeg;
}

double KinematicsSimulator::currentTiltDeg() const
{
    std::scoped_lock lock(m_mutex);
    return m_currentTiltDeg;
}

double KinematicsSimulator::currentZoom() const
{
    std::scoped_lock lock(m_mutex);
    return m_currentZoom;
}

std::uint16_t KinematicsSimulator::currentPanCentidegrees() const
{
    std::scoped_lock lock(m_mutex);
    const double rounded = std::round(m_currentPanDeg * 100.0);
    const auto intVal = static_cast<long long>(rounded);
    return static_cast<std::uint16_t>((intVal % 36000 + 36000) % 36000);
}

std::uint16_t KinematicsSimulator::currentTiltCentidegrees() const
{
    std::scoped_lock lock(m_mutex);
    const double rounded = std::round(m_currentTiltDeg * 100.0);
    const auto intVal = static_cast<long long>(rounded);
    return static_cast<std::uint16_t>((intVal % 36000 + 36000) % 36000);
}

std::uint16_t KinematicsSimulator::currentZoomInt() const
{
    std::scoped_lock lock(m_mutex);
    return static_cast<std::uint16_t>(std::clamp(std::round(m_currentZoom), 1000.0, 65535.0));
}

double KinematicsSimulator::normalizePanDeg(double deg) noexcept
{
    deg = std::fmod(deg, 360.0);
    if (deg < 0.0) {
        deg += 360.0;
    }
    return deg;
}

double KinematicsSimulator::shortestAngularDelta(double fromDeg, double toDeg) noexcept
{
    double diff = toDeg - fromDeg;
    diff = std::fmod(diff, 360.0);
    if (diff > 180.0) {
        diff -= 360.0;
    } else if (diff < -180.0) {
        diff += 360.0;
    }
    return diff;
}

} // namespace PelcoD
