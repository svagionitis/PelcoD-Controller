/// @file TacticalSearchEngine.cpp
/// @brief Implementation of the Tactical Search Patterns & Slew-to-Cue Engine.

#include "TacticalSearchEngine.h"
#include "GeoreferenceUtils.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {

constexpr double kPi = 3.14159265358979323846;

double degToRad(double deg)
{
    return deg * (kPi / 180.0);
}

double normalizeAngleDeg(double angle)
{
    while (angle > 180.0) {
        angle -= 360.0;
    }
    while (angle < -180.0) {
        angle += 360.0;
    }
    return angle;
}

double angularDiffDeg(double a, double b)
{
    return std::abs(normalizeAngleDeg(a - b));
}

} // namespace

TacticalSearchEngine::TacticalSearchEngine(std::shared_ptr<IPanTiltUnit> ptu,
                                           std::shared_ptr<ICameraPayload> camera,
                                           std::shared_ptr<ILaserRangeFinder> lrf)
    : m_ptu(std::move(ptu))
    , m_camera(std::move(camera))
    , m_lrf(std::move(lrf))
{
    m_running = true;
    m_workerThread = std::thread(&TacticalSearchEngine::engineLoop, this);
}

TacticalSearchEngine::~TacticalSearchEngine()
{
    stopPattern();
    clearCues();
    m_running = false;
    m_cv.notify_all();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

double TacticalSearchEngine::effectiveHFOV() const
{
    if (m_camera) {
        const auto telem = m_camera->currentTelemetry();
        if (telem.horizontalFovDeg > 0.1) {
            return telem.horizontalFovDeg;
        }
    }
    return 30.0; // fallback standard 30° HFOV
}

double TacticalSearchEngine::effectiveVFOV() const
{
    if (m_camera) {
        const auto telem = m_camera->currentTelemetry();
        if (telem.verticalFovDeg > 0.1) {
            return telem.verticalFovDeg;
        }
    }
    return 20.0; // fallback standard 20° VFOV
}

bool TacticalSearchEngine::startSectorScan(const SectorScanConfig& config)
{
    if (config.minAzimuthDeg >= config.maxAzimuthDeg || config.minElevationDeg > config.maxElevationDeg) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_sectorConfig = config;
    m_status.activePattern = SearchPatternType::SectorScan;
    m_status.state = TacticalEngineState::ExecutingPattern;
    m_status.completedLoops = 0U;
    m_status.statusMessage = "Starting Sector Scan";

    m_patternCurrentPanDeg = config.minAzimuthDeg;
    m_patternCurrentTiltDeg = config.minElevationDeg;
    m_patternSweepForward = true;
    m_patternTimeSec = 0.0;

    m_cv.notify_all();
    return true;
}

bool TacticalSearchEngine::startExpandingSquare(const ExpandingSquareConfig& config)
{
    if (config.maxRadiusDeg <= 0.0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_squareConfig = config;
    m_status.activePattern = SearchPatternType::ExpandingSquare;
    m_status.state = TacticalEngineState::ExecutingPattern;
    m_status.completedLoops = 0U;
    m_status.statusMessage = "Starting Expanding Square";

    m_patternCurrentPanDeg = config.centerAzimuthDeg;
    m_patternCurrentTiltDeg = config.centerElevationDeg;
    m_patternLegIndex = 0U;
    m_patternLegTraversedDeg = 0.0;
    m_patternTimeSec = 0.0;

    m_cv.notify_all();
    return true;
}

bool TacticalSearchEngine::startCreepingLine(const CreepingLineConfig& config)
{
    if (config.sweepWidthDeg <= 0.0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_creepingConfig = config;
    m_status.activePattern = SearchPatternType::CreepingLine;
    m_status.state = TacticalEngineState::ExecutingPattern;
    m_status.completedLoops = 0U;
    m_status.statusMessage = "Starting Creeping Line Scan";

    m_patternCurrentPanDeg = config.baselineHeadingDeg - (config.sweepWidthDeg / 2.0);
    m_patternCurrentTiltDeg = config.elevationDeg;
    m_patternSweepForward = true;
    m_patternLegIndex = 0U;
    m_patternTimeSec = 0.0;

    m_cv.notify_all();
    return true;
}

bool TacticalSearchEngine::startSpiralScan(const SpiralScanConfig& config)
{
    if (config.maxRadiusDeg <= 0.0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_spiralConfig = config;
    m_status.activePattern = SearchPatternType::SpiralScan;
    m_status.state = TacticalEngineState::ExecutingPattern;
    m_status.completedLoops = 0U;
    m_status.statusMessage = "Starting Spiral Scan";

    m_patternCurrentPanDeg = config.centerAzimuthDeg;
    m_patternCurrentTiltDeg = config.centerElevationDeg;
    m_patternTimeSec = 0.0;

    m_cv.notify_all();
    return true;
}

void TacticalSearchEngine::pausePattern()
{
    StatusCallback cb;
    TacticalEngineStatus stCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_status.state == TacticalEngineState::ExecutingPattern || m_status.state == TacticalEngineState::SlewingToCue) {
            m_status.state = TacticalEngineState::Paused;
            m_status.statusMessage = "Pattern paused by operator";
            cb = m_statusCb;
            stCopy = m_status;
        }
    }
    if (cb) {
        cb(stCopy);
    }
}

void TacticalSearchEngine::resumePattern()
{
    StatusCallback cb;
    TacticalEngineStatus stCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_status.state == TacticalEngineState::Paused) {
            if (m_currentCue.has_value()) {
                m_status.state = TacticalEngineState::SlewingToCue;
            } else if (m_status.activePattern != SearchPatternType::None) {
                m_status.state = TacticalEngineState::ExecutingPattern;
            } else {
                m_status.state = TacticalEngineState::Idle;
            }
            m_status.statusMessage = "Resumed";
            cb = m_statusCb;
            stCopy = m_status;
        }
    }
    if (cb) {
        cb(stCopy);
    }
    m_cv.notify_all();
}

void TacticalSearchEngine::stopPattern()
{
    StatusCallback cb;
    TacticalEngineStatus stCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status.activePattern = SearchPatternType::None;
        if (!m_currentCue.has_value()) {
            m_status.state = TacticalEngineState::Idle;
        }
        m_status.statusMessage = "Search pattern stopped";
        cb = m_statusCb;
        stCopy = m_status;
    }
    if (cb) {
        cb(stCopy);
    }
    m_cv.notify_all();
}

bool TacticalSearchEngine::enqueueCue(const TargetCue& cue)
{
    if (cue.cueId.empty()) {
        return false;
    }

    StatusCallback cb;
    TacticalEngineStatus stCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check for duplicate
        for (const auto& item : m_cueQueue) {
            if (item.cueId == cue.cueId) {
                return false;
            }
        }

        m_cueQueue.push_back(cue);

        // Priority sort: lower numeric value is higher priority (Flash = 0)
        std::stable_sort(m_cueQueue.begin(), m_cueQueue.end(), [](const TargetCue& a, const TargetCue& b) {
            if (a.priority != b.priority) {
                return static_cast<std::uint8_t>(a.priority) < static_cast<std::uint8_t>(b.priority);
            }
            return a.timestamp < b.timestamp;
        });

        m_status.pendingCueCount = m_cueQueue.size();

        // Flash or Immediate cues preempt active scan immediately
        if (cue.priority <= CuePriority::Immediate && m_status.state != TacticalEngineState::DwellingAtCue
            && m_status.state != TacticalEngineState::TrackingCue) {
            m_currentCue = m_cueQueue.front();
            m_cueQueue.erase(m_cueQueue.begin());
            m_status.activeCueId = m_currentCue->cueId;
            m_status.pendingCueCount = m_cueQueue.size();
            m_status.state = TacticalEngineState::SlewingToCue;
            m_status.statusMessage = "Preempted by high-priority cue: " + m_currentCue->cueId;
            cb = m_statusCb;
            stCopy = m_status;
        }
    }

    if (cb) {
        cb(stCopy);
    }

    m_cv.notify_all();
    return true;
}

bool TacticalSearchEngine::cancelCue(const std::string& cueId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentCue.has_value() && m_currentCue->cueId == cueId) {
        m_currentCue.reset();
        m_status.activeCueId.clear();
        if (m_status.activePattern != SearchPatternType::None) {
            m_status.state = TacticalEngineState::ExecutingPattern;
        } else {
            m_status.state = TacticalEngineState::Idle;
        }
        return true;
    }

    const auto it = std::remove_if(m_cueQueue.begin(), m_cueQueue.end(), [&](const TargetCue& c) {
        return c.cueId == cueId;
    });
    const bool erased = (it != m_cueQueue.end());
    m_cueQueue.erase(it, m_cueQueue.end());
    m_status.pendingCueCount = m_cueQueue.size();
    return erased;
}

void TacticalSearchEngine::clearCues()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cueQueue.clear();
    m_currentCue.reset();
    m_status.activeCueId.clear();
    m_status.pendingCueCount = 0U;
    if (m_status.state == TacticalEngineState::SlewingToCue || m_status.state == TacticalEngineState::DwellingAtCue) {
        if (m_status.activePattern != SearchPatternType::None) {
            m_status.state = TacticalEngineState::ExecutingPattern;
        } else {
            m_status.state = TacticalEngineState::Idle;
        }
    }
}

std::optional<TargetCue> TacticalSearchEngine::activeCue() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentCue;
}

std::vector<TargetCue> TacticalSearchEngine::pendingCues() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cueQueue;
}

void TacticalSearchEngine::notifyManualIntervention()
{
    StatusCallback cb;
    TacticalEngineStatus stCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_status.state != TacticalEngineState::Idle && m_status.state != TacticalEngineState::Paused) {
            m_status.state = TacticalEngineState::Paused;
            m_status.statusMessage = "Manual intervention: pattern and cueing suspended";
            cb = m_statusCb;
            stCopy = m_status;
        }
    }
    if (cb) {
        cb(stCopy);
    }
}

TacticalEngineStatus TacticalSearchEngine::status() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

void TacticalSearchEngine::registerStatusCallback(StatusCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statusCb = std::move(cb);
}

void TacticalSearchEngine::registerCueAcquiredCallback(CueAcquiredCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cueAcquiredCb = std::move(cb);
}

void TacticalSearchEngine::updateHostPlatform(const Klv::GeoPoint3D& platformPos, double headingDeg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_platformPos = platformPos;
    m_platformHeadingDeg = headingDeg;
}

void TacticalSearchEngine::setArrivalThresholdDeg(double degrees) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_arrivalThresholdDeg = std::max(0.05, degrees);
}

double TacticalSearchEngine::arrivalThresholdDeg() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_arrivalThresholdDeg;
}

bool TacticalSearchEngine::checkArrival(double targetPanDeg, double targetTiltDeg)
{
    if (!m_ptu) {
        return true;
    }
    const auto telem = m_ptu->currentTelemetry();
    const double panErr = angularDiffDeg(telem.panAngleDeg, targetPanDeg);
    const double tiltErr = std::abs(telem.tiltAngleDeg - targetTiltDeg);

    return (panErr <= m_arrivalThresholdDeg && tiltErr <= m_arrivalThresholdDeg);
}

void TacticalSearchEngine::stepPattern(double dtSec)
{
    if (dtSec <= 0.0) {
        return;
    }

    switch (m_status.activePattern) {
    case SearchPatternType::SectorScan: {
        const double hfov = effectiveHFOV();
        const double vfov = effectiveVFOV();
        const double elStep = m_sectorConfig.elevationStepDeg > 0.0
            ? m_sectorConfig.elevationStepDeg
            : std::max(0.5, vfov * (1.0 - m_sectorConfig.overlapRatio));
        const double speed = m_sectorConfig.scanSpeedDegPerSec;

        if (m_patternSweepForward) {
            m_patternCurrentPanDeg += speed * dtSec;
            if (m_patternCurrentPanDeg >= m_sectorConfig.maxAzimuthDeg) {
                m_patternCurrentPanDeg = m_sectorConfig.maxAzimuthDeg;
                m_patternSweepForward = false;
                m_patternCurrentTiltDeg += elStep;
                if (m_patternCurrentTiltDeg > m_sectorConfig.maxElevationDeg) {
                    m_patternCurrentTiltDeg = m_sectorConfig.minElevationDeg;
                    m_status.completedLoops++;
                }
            }
        } else {
            m_patternCurrentPanDeg -= speed * dtSec;
            if (m_patternCurrentPanDeg <= m_sectorConfig.minAzimuthDeg) {
                m_patternCurrentPanDeg = m_sectorConfig.minAzimuthDeg;
                m_patternSweepForward = true;
                m_patternCurrentTiltDeg += elStep;
                if (m_patternCurrentTiltDeg > m_sectorConfig.maxElevationDeg) {
                    m_patternCurrentTiltDeg = m_sectorConfig.minElevationDeg;
                    m_status.completedLoops++;
                }
            }
        }
        (void)hfov;
        break;
    }

    case SearchPatternType::ExpandingSquare: {
        const double hfov = effectiveHFOV();
        const double step = m_squareConfig.stepSizeDeg > 0.0
            ? m_squareConfig.stepSizeDeg
            : std::max(0.5, hfov * (1.0 - m_squareConfig.overlapRatio));
        const double speed = m_squareConfig.scanSpeedDegPerSec;

        const std::size_t multiplier = (m_patternLegIndex / 2U) + 1U;
        const double legTargetLength = static_cast<double>(multiplier) * step;

        const double distThisStep = speed * dtSec;
        m_patternLegTraversedDeg += distThisStep;

        // Direction: 0 = East (+Pan), 1 = South (-Tilt), 2 = West (-Pan), 3 = North (+Tilt)
        const std::size_t dir = m_patternLegIndex % 4U;
        if (dir == 0U) {
            m_patternCurrentPanDeg += distThisStep;
        } else if (dir == 1U) {
            m_patternCurrentTiltDeg -= distThisStep;
        } else if (dir == 2U) {
            m_patternCurrentPanDeg -= distThisStep;
        } else {
            m_patternCurrentTiltDeg += distThisStep;
        }

        if (m_patternLegTraversedDeg >= legTargetLength) {
            m_patternLegTraversedDeg = 0.0;
            m_patternLegIndex++;

            const double curRadius = std::hypot(
                m_patternCurrentPanDeg - m_squareConfig.centerAzimuthDeg,
                m_patternCurrentTiltDeg - m_squareConfig.centerElevationDeg);
            if (curRadius >= m_squareConfig.maxRadiusDeg) {
                m_status.completedLoops++;
                m_patternCurrentPanDeg = m_squareConfig.centerAzimuthDeg;
                m_patternCurrentTiltDeg = m_squareConfig.centerElevationDeg;
                m_patternLegIndex = 0U;
            }
        }
        break;
    }

    case SearchPatternType::CreepingLine: {
        const double speed = m_creepingConfig.scanSpeedDegPerSec;
        const double halfWidth = m_creepingConfig.sweepWidthDeg / 2.0;
        const double leftBound = m_creepingConfig.baselineHeadingDeg - halfWidth;
        const double rightBound = m_creepingConfig.baselineHeadingDeg + halfWidth;

        if (m_patternSweepForward) {
            m_patternCurrentPanDeg += speed * dtSec;
            if (m_patternCurrentPanDeg >= rightBound) {
                m_patternCurrentPanDeg = rightBound;
                m_patternSweepForward = false;
                m_patternLegIndex++;
            }
        } else {
            m_patternCurrentPanDeg -= speed * dtSec;
            if (m_patternCurrentPanDeg <= leftBound) {
                m_patternCurrentPanDeg = leftBound;
                m_patternSweepForward = true;
                m_patternLegIndex++;
            }
        }
        m_patternCurrentTiltDeg = m_creepingConfig.elevationDeg;
        break;
    }

    case SearchPatternType::SpiralScan: {
        const double hfov = effectiveHFOV();
        const double expansionRate = m_spiralConfig.expansionRateDeg > 0.0
            ? m_spiralConfig.expansionRateDeg
            : std::max(0.5, hfov);
        const double omega = m_spiralConfig.angularVelocityDegPerSec;

        m_patternTimeSec += dtSec;
        const double totalRotations = (omega * m_patternTimeSec) / 360.0;
        const double currentRadius = std::min(m_spiralConfig.maxRadiusDeg, totalRotations * expansionRate);

        const double thetaRad = degToRad(omega * m_patternTimeSec);
        m_patternCurrentPanDeg = m_spiralConfig.centerAzimuthDeg + (currentRadius * std::cos(thetaRad));
        m_patternCurrentTiltDeg = m_spiralConfig.centerElevationDeg + (currentRadius * std::sin(thetaRad));

        if (currentRadius >= m_spiralConfig.maxRadiusDeg) {
            m_status.completedLoops++;
            m_patternTimeSec = 0.0;
        }
        break;
    }

    case SearchPatternType::None:
    default:
        break;
    }

    m_status.currentAzimuthDeg = m_patternCurrentPanDeg;
    m_status.currentElevationDeg = m_patternCurrentTiltDeg;
}

void TacticalSearchEngine::engineLoop()
{
    auto lastTick = std::chrono::steady_clock::now();

    while (m_running) {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_cv.wait_for(lock, std::chrono::milliseconds(40), [this] {
            return !m_running || m_status.state != TacticalEngineState::Idle;
        });

        if (!m_running) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        const double dtSec = std::chrono::duration<double>(now - lastTick).count();
        lastTick = now;

        if (m_status.state == TacticalEngineState::Idle || m_status.state == TacticalEngineState::Paused) {
            continue;
        }

        // 1. Process cues if idle or executing pattern
        if (!m_currentCue.has_value() && !m_cueQueue.empty()) {
            m_currentCue = m_cueQueue.front();
            m_cueQueue.erase(m_cueQueue.begin());
            m_status.activeCueId = m_currentCue->cueId;
            m_status.pendingCueCount = m_cueQueue.size();
            m_status.state = TacticalEngineState::SlewingToCue;
            m_status.statusMessage = "Slewing to target cue: " + m_currentCue->cueId;

            const auto cb = m_statusCb;
            const auto stCopy = m_status;
            lock.unlock();
            if (cb) {
                cb(stCopy);
            }
            continue;
        }

        // 2. Slewing to Cue
        if (m_status.state == TacticalEngineState::SlewingToCue && m_currentCue.has_value()) {
            double targetPan = 0.0;
            double targetTilt = 0.0;

            if (m_currentCue->geoTarget.has_value()) {
                const auto look = GeoreferenceUtils::computeLookAnglesToTarget(
                    m_platformPos, m_platformHeadingDeg, *m_currentCue->geoTarget);
                targetPan = look.panAngleDeg;
                targetTilt = look.tiltAngleDeg;
            } else if (m_currentCue->panAngleDeg.has_value() && m_currentCue->tiltAngleDeg.has_value()) {
                targetPan = *m_currentCue->panAngleDeg;
                targetTilt = *m_currentCue->tiltAngleDeg;
            }

            m_status.currentAzimuthDeg = targetPan;
            m_status.currentElevationDeg = targetTilt;

            auto ptu = m_ptu;
            lock.unlock();

            if (ptu) {
                ptu->setAbsoluteAngles(targetPan, targetTilt);
            }

            lock.lock();
            if (checkArrival(targetPan, targetTilt)) {
                m_status.state = TacticalEngineState::DwellingAtCue;
                m_dwellStartTime = std::chrono::steady_clock::now();
                m_status.remainingDwell = m_currentCue->dwellTime;
                m_status.statusMessage = "Cue acquired. Holding dwell observation";

                const auto cueCopy = *m_currentCue;
                const auto acquiredCb = m_cueAcquiredCb;
                const auto statusCb = m_statusCb;
                const auto stCopy = m_status;
                auto lrf = m_lrf;
                lock.unlock();

                // Optional single LRF ping on cue arrival
                if (lrf && lrf->isArmed()) {
                    lrf->triggerSingleMeasurement();
                }

                if (acquiredCb) {
                    acquiredCb(cueCopy);
                }
                if (statusCb) {
                    statusCb(stCopy);
                }
                continue;
            }
            continue;
        }

        // 3. Dwelling at Cue
        if (m_status.state == TacticalEngineState::DwellingAtCue && m_currentCue.has_value()) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_dwellStartTime);
            if (elapsed >= m_currentCue->dwellTime) {
                // Cue dwell finished
                m_currentCue.reset();
                m_status.activeCueId.clear();

                if (!m_cueQueue.empty()) {
                    // Advance to next cue in queue
                    m_currentCue = m_cueQueue.front();
                    m_cueQueue.erase(m_cueQueue.begin());
                    m_status.activeCueId = m_currentCue->cueId;
                    m_status.pendingCueCount = m_cueQueue.size();
                    m_status.state = TacticalEngineState::SlewingToCue;
                    m_status.statusMessage = "Slewing to next target cue: " + m_currentCue->cueId;
                } else if (m_status.activePattern != SearchPatternType::None) {
                    // Seamless auto-resume to search pattern
                    m_status.state = TacticalEngineState::ExecutingPattern;
                    m_status.statusMessage = "Target cue completed. Resuming search pattern";
                } else {
                    m_status.state = TacticalEngineState::Idle;
                    m_status.statusMessage = "Cue completed. Engine idle";
                }

                const auto statusCb = m_statusCb;
                const auto stCopy = m_status;
                lock.unlock();
                if (statusCb) {
                    statusCb(stCopy);
                }
                continue;
            }

            m_status.remainingDwell = m_currentCue->dwellTime - elapsed;
            continue;
        }

        // 4. Executing Pattern
        if (m_status.state == TacticalEngineState::ExecutingPattern) {
            stepPattern(dtSec);

            const double nextPan = m_patternCurrentPanDeg;
            const double nextTilt = m_patternCurrentTiltDeg;
            auto ptu = m_ptu;
            lock.unlock();

            if (ptu) {
                ptu->setAbsoluteAngles(nextPan, nextTilt);
            }
        }
    }
}

} // namespace PayloadHal
