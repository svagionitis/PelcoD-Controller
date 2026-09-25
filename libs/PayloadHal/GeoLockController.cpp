/// @file GeoLockController.cpp
/// @brief Implementation of GeoLockController closed-loop tracking.

#include "GeoLockController.h"
#include "GeoreferenceUtils.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

GeoLockController::GeoLockController(std::shared_ptr<IPayload> payload) noexcept
    : m_payload(std::move(payload))
{
}

GeoLockController::~GeoLockController()
{
    stopTrackingLoop();
}

void GeoLockController::setDeadbandDeg(double deadbandDeg) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deadbandDeg = std::max(0.0, deadbandDeg);
}

double GeoLockController::deadbandDeg() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_deadbandDeg;
}

void GeoLockController::setPlatformNavProvider(PlatformNavProvider provider)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_navProvider = std::move(provider);
}

bool GeoLockController::engage(const Klv::GeoPoint3D& targetPos)
{
    if (!m_payload) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_target = targetPos;
    m_engaged = true;
    m_hasCommanded = false;
    m_updateCount = 0;
    m_payload->engageGeoLock(targetPos);
    return true;
}

void GeoLockController::disengage()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_engaged = false;
    m_target.reset();
    m_hasCommanded = false;
    if (m_payload) {
        m_payload->disengageGeoLock();
    }
}

bool GeoLockController::isEngaged() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_engaged;
}

std::optional<Klv::GeoPoint3D> GeoLockController::currentTarget() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_target;
}

bool GeoLockController::updatePlatform(const Klv::GeoPoint3D& platformPos, double platformHeadingDeg)
{
    Klv::GeoPoint3D target;
    double deadband = 0.0;
    std::shared_ptr<IPayload> payload;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_engaged || !m_target || !m_payload) {
            return false;
        }
        target = *m_target;
        deadband = m_deadbandDeg;
        payload = m_payload;
    }

    const auto ptu = payload->panTilt();
    if (!ptu) {
        return false;
    }

    const auto look = GeoreferenceUtils::computeLookAnglesToTarget(platformPos, platformHeadingDeg, target);

    bool shouldCommand = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_hasCommanded) {
            shouldCommand = true;
        } else {
            double panDiff = std::abs(look.panAngleDeg - m_lastCommandedPan);
            if (panDiff > 180.0) {
                panDiff = 360.0 - panDiff;
            }
            double tiltDiff = std::abs(look.tiltAngleDeg - m_lastCommandedTilt);
            if (panDiff >= deadband || tiltDiff >= deadband) {
                shouldCommand = true;
            }
        }
    }

    if (shouldCommand) {
        if (!ptu->setAbsoluteAngles(look.panAngleDeg, look.tiltAngleDeg)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastCommandedPan = look.panAngleDeg;
        m_lastCommandedTilt = look.tiltAngleDeg;
        m_hasCommanded = true;
    }

    const auto telem = ptu->currentTelemetry();
    const double currentPan = telem.panAngleDeg;
    const double currentTilt = telem.tiltAngleDeg;

    double errPan = std::abs(look.panAngleDeg - currentPan);
    if (errPan > 180.0) {
        errPan = 360.0 - errPan;
    }
    const double errTilt = std::abs(look.tiltAngleDeg - currentTilt);
    const double totalErr = std::sqrt(errPan * errPan + errTilt * errTilt);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentPanDeg = currentPan;
        m_currentTiltDeg = currentTilt;
        m_trackingErrorDeg = totalErr;
        m_slantRangeMeters = look.slantRangeMeters;
        m_updateCount++;
    }

    return true;
}

bool GeoLockController::startTrackingLoop(double rateHz)
{
    if (rateHz <= 0.0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_loopRunning.load() || !m_navProvider) {
        return false;
    }
    m_stopRequested.store(false);
    m_loopRunning.store(true);
    const auto intervalMs = std::chrono::milliseconds(static_cast<int64_t>(1000.0 / rateHz));
    m_workerThread = std::thread(&GeoLockController::trackingWorker, this, intervalMs);
    return true;
}

void GeoLockController::stopTrackingLoop()
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

bool GeoLockController::isTrackingLoopRunning() const noexcept
{
    return m_loopRunning.load();
}

GeoLockStatus GeoLockController::status() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    GeoLockStatus s {};
    s.engaged = m_engaged;
    s.target = m_target;
    s.commandedPanDeg = m_lastCommandedPan;
    s.commandedTiltDeg = m_lastCommandedTilt;
    s.currentPanDeg = m_currentPanDeg;
    s.currentTiltDeg = m_currentTiltDeg;
    s.trackingErrorDeg = m_trackingErrorDeg;
    s.slantRangeMeters = m_slantRangeMeters;
    s.updateCount = m_updateCount;
    return s;
}

void GeoLockController::trackingWorker(std::chrono::milliseconds interval)
{
    while (!m_stopRequested.load()) {
        PlatformNavProvider nav;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            nav = m_navProvider;
        }
        if (nav) {
            const auto navState = nav();
            if (navState.has_value()) {
                updatePlatform(navState->first, navState->second);
            }
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, interval, [this] { return m_stopRequested.load(); });
    }
}

} // namespace PayloadHal
