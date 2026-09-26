/// @file PayloadAutoTrackerBridge.cpp
/// @brief Implementation of PayloadAutoTrackerBridge connecting Tracking::PtzAutoTracker to PayloadHal.

#include "PayloadAutoTrackerBridge.h"
#include <algorithm>
#include <cmath>

namespace PayloadHal {

PayloadAutoTrackerBridge::PayloadAutoTrackerBridge(std::shared_ptr<IPayload> payload) noexcept
{
    if (payload) {
        m_ptu = payload->panTilt();
        m_camera = payload->primaryCamera();
    }
}

PayloadAutoTrackerBridge::PayloadAutoTrackerBridge(std::shared_ptr<IPanTiltUnit> ptu,
                                                   std::shared_ptr<ICameraPayload> camera) noexcept
    : m_ptu(std::move(ptu))
    , m_camera(std::move(camera))
{
}

PayloadAutoTrackerBridge::~PayloadAutoTrackerBridge()
{
    stopTrackingLoop();
    disengage();
}

bool PayloadAutoTrackerBridge::engage()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ptu) {
        return false;
    }
    m_engaged = true;
    m_status.engaged = true;
    return true;
}

void PayloadAutoTrackerBridge::disengage()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_engaged = false;
    m_status.engaged = false;
    stopActuators();
}

bool PayloadAutoTrackerBridge::isEngaged() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_engaged;
}

void PayloadAutoTrackerBridge::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracker.reset();
    m_hasLastBox = false;
    m_status.trackingState = Tracking::PtzAutoTracker::TrackingState::Idle;
    m_status.lastErrorX = 0.0;
    m_status.lastErrorY = 0.0;
    m_status.commandedPanVel = 0.0;
    m_status.commandedTiltVel = 0.0;
    m_status.commandedZoomVel = 0.0;
    m_status.lastLeadOffsetX = 0.0;
    m_status.lastLeadOffsetY = 0.0;
    stopActuators();
}

void PayloadAutoTrackerBridge::setDriveMode(TrackerDriveMode mode) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_driveMode = mode;
}

TrackerDriveMode PayloadAutoTrackerBridge::driveMode() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_driveMode;
}

void PayloadAutoTrackerBridge::setMaxPhysicalRates(double maxPanDegPerSec, double maxTiltDegPerSec) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxPanRateDegPerSec = std::abs(maxPanDegPerSec);
    m_maxTiltRateDegPerSec = std::abs(maxTiltDegPerSec);
}

void PayloadAutoTrackerBridge::setPanGains(double kp, double ki, double kd, double kff) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracker.setPanGains(kp, ki, kd, kff);
}

void PayloadAutoTrackerBridge::setTiltGains(double kp, double ki, double kd, double kff) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracker.setTiltGains(kp, ki, kd, kff);
}

void PayloadAutoTrackerBridge::setDeadbands(double panDeadband, double tiltDeadband) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracker.setDeadbands(panDeadband, tiltDeadband);
}

void PayloadAutoTrackerBridge::setAutoZoomEnabled(bool enabled) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracker.setAutoZoomEnabled(enabled);
}

void PayloadAutoTrackerBridge::setTargetFramingHeight(double targetNormHeight, double deadband) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracker.setTargetFramingHeight(targetNormHeight, deadband);
}

void PayloadAutoTrackerBridge::setPredictiveLeadEnabled(bool enabled, double kLead, double maxLead) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracker.setPredictiveLeadEnabled(enabled);
    m_tracker.setLeadGain(kLead, maxLead);
}

void PayloadAutoTrackerBridge::setAdaptiveLatencyEnabled(bool enabled, double estimatedLatencySeconds) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracker.setAdaptiveLatencyEnabled(enabled);
    m_tracker.setEstimatedLatencySeconds(estimatedLatencySeconds);
}

double PayloadAutoTrackerBridge::queryCurrentZoom() const noexcept
{
    if (!m_camera) {
        return 1.0;
    }
    const auto telem = m_camera->currentTelemetry();
    return (telem.opticalZoomFactor >= 1.0) ? telem.opticalZoomFactor : 1.0;
}

void PayloadAutoTrackerBridge::stopActuators()
{
    if (m_ptu) {
        m_ptu->stopMotion();
    }
    if (m_camera && std::abs(m_lastZoomVel) > 0.001f) {
        m_camera->zoomStop();
        m_lastZoomVel = 0.0f;
    }
}

void PayloadAutoTrackerBridge::dispatchCommand(const Tracking::PtzAutoTracker::TrackingCommand& cmd)
{
    m_status.trackingState = cmd.state;
    m_status.lastLeadOffsetX = m_tracker.getLastLeadOffsetX();
    m_status.lastLeadOffsetY = m_tracker.getLastLeadOffsetY();

    if (!cmd.shouldMove || cmd.state == Tracking::PtzAutoTracker::TrackingState::Lost ||
        cmd.state == Tracking::PtzAutoTracker::TrackingState::Idle) {
        if (m_ptu) {
            m_ptu->stopMotion();
        }
        m_status.commandedPanVel = 0.0;
        m_status.commandedTiltVel = 0.0;
    } else {
        if (m_driveMode == TrackerDriveMode::NormalizedVelocity) {
            const float panVel = static_cast<float>(cmd.panDirection * cmd.panSpeed) / 63.0f;
            const float tiltVel = static_cast<float>(cmd.tiltDirection * cmd.tiltSpeed) / 63.0f;
            if (m_ptu) {
                m_ptu->setNormalizedVelocity(panVel, tiltVel);
            }
            m_status.commandedPanVel = panVel;
            m_status.commandedTiltVel = tiltVel;
        } else {
            const double panRate =
                (static_cast<double>(cmd.panDirection * cmd.panSpeed) / 63.0) * m_maxPanRateDegPerSec;
            const double tiltRate =
                (static_cast<double>(cmd.tiltDirection * cmd.tiltSpeed) / 63.0) * m_maxTiltRateDegPerSec;
            if (m_ptu) {
                m_ptu->setRate(panRate, tiltRate);
            }
            m_status.commandedPanVel = panRate;
            m_status.commandedTiltVel = tiltRate;
        }
    }

    if (cmd.shouldZoom && m_camera) {
        const float zoomVel = static_cast<float>(cmd.zoomDirection * cmd.zoomSpeed) / 63.0f;
        m_camera->zoomContinuous(zoomVel);
        m_lastZoomVel = zoomVel;
        m_status.commandedZoomVel = zoomVel;
    } else {
        if (std::abs(m_lastZoomVel) > 0.001f && m_camera) {
            m_camera->zoomStop();
            m_lastZoomVel = 0.0f;
        }
        m_status.commandedZoomVel = 0.0;
    }
}

bool PayloadAutoTrackerBridge::updateVisual(const VisualTargetDetection& target, double dt)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engaged || !m_ptu) {
        return false;
    }

    const double currentZoom = queryCurrentZoom();
    m_status.currentZoomFactor = currentZoom;
    m_status.lastErrorX = target.errorX;
    m_status.lastErrorY = target.errorY;

    const auto cmd = m_tracker.update(target.errorX, target.errorY, target.velocityX, target.velocityY,
                                      target.isLocked, target.isCoasting, dt, target.targetNormHeight,
                                      currentZoom);

    dispatchCommand(cmd);
    m_status.updateCount++;
    return true;
}

bool PayloadAutoTrackerBridge::updateBoundingBox(double normX, double normY, double normWidth,
                                                 double normHeight, bool isLocked, double dt)
{
    const double centerX = normX + (normWidth * 0.5);
    const double centerY = normY + (normHeight * 0.5);

    // Normalize error coordinates to [-1.0, +1.0] from screen center (0.5, 0.5)
    const double errorX = (centerX - 0.5) * 2.0;
    const double errorY = (centerY - 0.5) * 2.0;

    double vx = 0.0;
    double vy = 0.0;
    if (m_hasLastBox && dt > 0.0001) {
        vx = ((centerX - m_lastBoxCenterX) * 2.0) / dt;
        vy = ((centerY - m_lastBoxCenterY) * 2.0) / dt;
    }
    m_lastBoxCenterX = centerX;
    m_lastBoxCenterY = centerY;
    m_hasLastBox = true;

    VisualTargetDetection detection;
    detection.errorX = errorX;
    detection.errorY = errorY;
    detection.velocityX = vx;
    detection.velocityY = vy;
    detection.isLocked = isLocked;
    detection.isCoasting = false;
    detection.targetNormHeight = normHeight;
    detection.timestamp = std::chrono::steady_clock::now();

    return updateVisual(detection, dt);
}

bool PayloadAutoTrackerBridge::updateAngular(double errorAzimuthDeg, double errorElevationDeg,
                                             double omegaAzDegPerSec, double omegaElDegPerSec,
                                             bool isLocked, bool isCoasting, double dt,
                                             double targetNormHeight)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engaged || !m_ptu) {
        return false;
    }

    const double currentZoom = queryCurrentZoom();
    m_status.currentZoomFactor = currentZoom;
    m_status.lastErrorX = errorAzimuthDeg;
    m_status.lastErrorY = errorElevationDeg;

    const auto cmd = m_tracker.updateAngular(errorAzimuthDeg, errorElevationDeg, omegaAzDegPerSec,
                                             omegaElDegPerSec, isLocked, isCoasting, dt,
                                             targetNormHeight, currentZoom);

    dispatchCommand(cmd);
    m_status.updateCount++;
    return true;
}

void PayloadAutoTrackerBridge::setTargetProvider(TargetProvider provider)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_targetProvider = std::move(provider);
}

bool PayloadAutoTrackerBridge::startTrackingLoop(double rateHz)
{
    if (rateHz <= 0.0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_loopRunning.load()) {
        return false;
    }
    if (!m_targetProvider) {
        return false;
    }

    m_stopRequested.store(false);
    m_loopRunning.store(true);
    m_lastUpdateTime = std::chrono::steady_clock::now();

    const auto intervalMs = std::chrono::milliseconds(static_cast<int64_t>(1000.0 / rateHz));
    m_workerThread = std::thread(&PayloadAutoTrackerBridge::trackingWorker, this, intervalMs);
    return true;
}

void PayloadAutoTrackerBridge::stopTrackingLoop()
{
    if (!m_loopRunning.load()) {
        return;
    }

    m_stopRequested.store(true);
    m_cv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    m_loopRunning.store(false);
}

bool PayloadAutoTrackerBridge::isTrackingLoopRunning() const noexcept
{
    return m_loopRunning.load();
}

AutoTrackerStatus PayloadAutoTrackerBridge::status() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

Tracking::PtzAutoTracker& PayloadAutoTrackerBridge::tracker() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tracker;
}

void PayloadAutoTrackerBridge::trackingWorker(std::chrono::milliseconds interval)
{
    while (!m_stopRequested.load()) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, interval, [this]() {
            return m_stopRequested.load();
        });

        if (m_stopRequested.load()) {
            break;
        }

        if (!m_targetProvider || !m_engaged) {
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - m_lastUpdateTime).count();
        m_lastUpdateTime = now;

        const auto targetOpt = m_targetProvider();
        if (targetOpt) {
            lock.unlock();
            updateVisual(*targetOpt, (dt > 0.0001 && dt < 1.0) ? dt : 0.033);
        }
    }
}

} // namespace PayloadHal
