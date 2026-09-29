#include "SlewToCueDirector.h"

#include <algorithm>

namespace PayloadHal {

SlewToCueDirector::SlewToCueDirector(std::shared_ptr<TargetThreatEvaluator> threatEvaluator,
    std::shared_ptr<NmeaSlavingBridge> slavingBridge, std::shared_ptr<AutoFramingController> autoFraming,
    std::shared_ptr<PayloadAutoTrackerBridge> autoTracker, std::shared_ptr<ICameraPayload> camera,
    std::shared_ptr<GeoLockController> geoLock, const SlewToCueConfig& config)
    : m_threatEvaluator(std::move(threatEvaluator))
    , m_slavingBridge(std::move(slavingBridge))
    , m_autoFraming(std::move(autoFraming))
    , m_autoTracker(std::move(autoTracker))
    , m_camera(std::move(camera))
    , m_geoLock(std::move(geoLock))
    , m_config(config)
    , m_stateEntryTime(std::chrono::steady_clock::now())
{
}

SlewToCueDirector::~SlewToCueDirector()
{
    disengage();
}

void SlewToCueDirector::setConfig(const SlewToCueConfig& config)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_config = config;
}

SlewToCueConfig SlewToCueDirector::config() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_config;
}

void SlewToCueDirector::setStateChangeCallback(StateChangeCallback cb)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_stateChangeCb = std::move(cb);
}

void SlewToCueDirector::setStatusCallback(StatusCallback cb)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_statusCb = std::move(cb);
}

void SlewToCueDirector::transitionTo(CueingState newState)
{
    CueingState oldState = m_state;
    m_state = newState;
    m_stateEntryTime = std::chrono::steady_clock::now();

    if (newState == CueingState::DwellInspection) {
        m_dwellStartTime = m_stateEntryTime;
    }

    StateChangeCallback cbCopy {};
    {
        cbCopy = m_stateChangeCb;
    }

    if (cbCopy && oldState != newState) {
        cbCopy(oldState, newState);
    }
}

bool SlewToCueDirector::cueTarget(const EvaluatedTarget& target)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    m_activeTargetId = target.targetId;
    m_activeTargetSource = target.source;
    m_activeTargetName = target.targetName;
    m_activeThreatScore = target.threatScore;
    m_isEmergencyTarget = target.isEmergency;
    m_activeTargetLengthMeters = 20.0; // Standard default
    m_activeTargetBeamMeters = 6.0;
    m_activeTargetHeightMeters = 4.0;
    m_activeTargetCogDeg = target.cogDegrees;

    if (m_autoFraming) {
        Nmea::AisDimensions dims {};
        const auto env = m_autoFraming->estimateTargetEnvelope(dims);
        m_activeTargetLengthMeters = env.lengthMeters;
        m_activeTargetBeamMeters = env.beamMeters;
        m_activeTargetHeightMeters = env.heightMeters;
    }

    if (!m_slavingBridge) {
        return false;
    }

    bool slavedOk { false };
    if (target.isEmergency) {
        const std::uint32_t mmsi = target.associatedAisMmsi.value_or(target.targetId);
        slavedOk = m_slavingBridge->slaveToEmergencyBeacon(mmsi);
    } else if (target.source == TargetTrackSource::RadarArpa) {
        const std::uint32_t radarId = target.associatedRadarId.value_or(target.targetId);
        slavedOk = m_slavingBridge->slaveToRadarTarget(radarId);
    } else if (target.source == TargetTrackSource::Ais || target.source == TargetTrackSource::FusedRadarAis) {
        const std::uint32_t mmsi = target.associatedAisMmsi.value_or(target.targetId);
        slavedOk = m_slavingBridge->slaveToAisVessel(mmsi);
    }

    if (slavedOk) {
        transitionTo(CueingState::SlewingToTarget);
    }

    return slavedOk;
}

bool SlewToCueDirector::cueRadarTarget(std::uint32_t targetId)
{
    if (m_threatEvaluator) {
        const auto targetOpt = m_threatEvaluator->getTarget(targetId, TargetTrackSource::RadarArpa);
        if (targetOpt.has_value()) {
            return cueTarget(*targetOpt);
        }
    }

    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_activeTargetId = targetId;
    m_activeTargetSource = TargetTrackSource::RadarArpa;
    m_activeTargetName = "RADAR_" + std::to_string(targetId);
    m_activeThreatScore = 50.0;
    m_isEmergencyTarget = false;

    if (m_slavingBridge && m_slavingBridge->slaveToRadarTarget(targetId)) {
        transitionTo(CueingState::SlewingToTarget);
        return true;
    }
    return false;
}

bool SlewToCueDirector::cueAisTarget(std::uint32_t mmsi)
{
    if (m_threatEvaluator) {
        const auto targetOpt = m_threatEvaluator->getTarget(mmsi, TargetTrackSource::Ais);
        if (targetOpt.has_value()) {
            return cueTarget(*targetOpt);
        }
    }

    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_activeTargetId = mmsi;
    m_activeTargetSource = TargetTrackSource::Ais;
    m_activeTargetName = "MMSI_" + std::to_string(mmsi);
    m_activeThreatScore = 50.0;
    m_isEmergencyTarget = false;

    if (m_slavingBridge && m_slavingBridge->slaveToAisVessel(mmsi)) {
        transitionTo(CueingState::SlewingToTarget);
        return true;
    }
    return false;
}

void SlewToCueDirector::dismissActiveTarget()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_autoTracker && m_autoTracker->isEngaged()) {
        m_autoTracker->disengage();
    }
    transitionTo(CueingState::TargetCompleted);
}

void SlewToCueDirector::disengage()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_autoTracker && m_autoTracker->isEngaged()) {
        m_autoTracker->disengage();
    }
    if (m_slavingBridge) {
        m_slavingBridge->disengage();
    }
    m_activeTargetId = 0U;
    m_activeTargetSource = TargetTrackSource::None;
    m_activeTargetName.clear();
    m_activeThreatScore = 0.0;
    m_isEmergencyTarget = false;
    transitionTo(CueingState::Idle);
}

bool SlewToCueDirector::isCueingActive() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_state != CueingState::Idle && m_state != CueingState::TargetCompleted;
}

bool SlewToCueDirector::checkGimbalConvergence() const noexcept
{
    if (m_geoLock) {
        const auto geoStatus = m_geoLock->currentTarget();
        if (geoStatus.has_value()) {
            // Check tracking error
            const double errorDeg = m_geoLock->deadbandDeg();
            return errorDeg <= m_config.lockToleranceDeg;
        }
    }
    return true;
}

void SlewToCueDirector::executeAutoFraming(double rangeMeters, double targetLengthMeters)
{
    if (m_config.autoFraming && m_autoFraming && m_camera) {
        TargetPhysicalEnvelope env {};
        env.lengthMeters = targetLengthMeters;
        env.beamMeters = m_activeTargetBeamMeters;
        env.heightMeters = m_activeTargetHeightMeters;

        double bearingDeg { 0.0 };
        if (m_slavingBridge) {
            bearingDeg = m_slavingBridge->status().trueBearingDeg;
        }

        if (m_config.waitForZoomConvergence) {
            (void)m_autoFraming->scheduleFraming(*m_camera, rangeMeters, env, m_activeTargetCogDeg, bearingDeg);
        } else {
            (void)m_autoFraming->frameTarget(*m_camera, rangeMeters, targetLengthMeters);
        }
    }
}

void SlewToCueDirector::update()
{
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    const auto now = std::chrono::steady_clock::now();

    // 1. Check for autonomous threat pre-emption
    if (m_threatEvaluator && m_config.allowPreemptionByHigherThreat) {
        const auto topCandidateOpt = m_threatEvaluator->getHighestThreatTarget();
        if (topCandidateOpt.has_value()) {
            const auto& cand = *topCandidateOpt;
            const bool isNewTarget = (cand.targetId != m_activeTargetId || cand.source != m_activeTargetSource);

            if (isNewTarget) {
                // Immediate preemption on Emergency beacon
                if (cand.isEmergency && !m_isEmergencyTarget) {
                    lock.unlock();
                    (void)cueTarget(cand);
                    return;
                }

                // Preemption if score significantly exceeds active target
                if (cand.threatScore > (m_activeThreatScore + m_config.preemptionScoreDelta)) {
                    lock.unlock();
                    (void)cueTarget(cand);
                    return;
                }
            }
        }
    }

    // 2. State Machine Transitions
    switch (m_state) {
    case CueingState::Idle: {
        if (m_config.autonomousEngagement && m_threatEvaluator) {
            const auto candidateOpt = m_threatEvaluator->getHighestThreatTarget();
            if (candidateOpt.has_value()) {
                const auto& cand = *candidateOpt;
                if (cand.threatScore >= m_config.minThreatScoreToCue || cand.threatLevel == ThreatLevel::Critical
                    || cand.threatLevel == ThreatLevel::Emergency) {
                    lock.unlock();
                    (void)cueTarget(cand);
                    return;
                }
            }
        }
        break;
    }

    case CueingState::SlewingToTarget: {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_stateEntryTime);
        const bool converged = checkGimbalConvergence();

        if (converged || elapsed >= m_config.maxSlewWaitTimeout) {
            transitionTo(CueingState::FramingTarget);
            if (m_config.waitForZoomConvergence) {
                double rangeMeters { 1000.0 };
                if (m_slavingBridge) {
                    const auto slavingStatus = m_slavingBridge->status();
                    if (slavingStatus.slantRangeMeters > 0.0) {
                        rangeMeters = slavingStatus.slantRangeMeters;
                    }
                }
                executeAutoFraming(rangeMeters, m_activeTargetLengthMeters);
            }
        }
        break;
    }

    case CueingState::FramingTarget: {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_stateEntryTime);

        if (m_config.waitForZoomConvergence && m_config.autoFraming && m_autoFraming && m_camera) {
            const auto dt = (m_lastUpdateTick.time_since_epoch().count() > 0)
                ? std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUpdateTick)
                : std::chrono::milliseconds(50);
            m_autoFraming->update(*m_camera, dt);

            const bool converged = m_autoFraming->isZoomConverged();
            const bool timedOut = (elapsed >= m_config.maxFramingDuration);

            if (!converged && !timedOut) {
                break; // Still converging; remain in FramingTarget
            }
        } else {
            // Immediate framing mode
            double rangeMeters { 1000.0 };
            if (m_slavingBridge) {
                const auto slavingStatus = m_slavingBridge->status();
                if (slavingStatus.slantRangeMeters > 0.0) {
                    rangeMeters = slavingStatus.slantRangeMeters;
                }
            }
            executeAutoFraming(rangeMeters, m_activeTargetLengthMeters);
        }

        if (m_config.autoOpticalHandover && m_autoTracker) {
            transitionTo(CueingState::AcquiringOpticalLock);
            (void)m_autoTracker->engage();
        } else {
            transitionTo(CueingState::DwellInspection);
        }
        break;
    }

    case CueingState::AcquiringOpticalLock: {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_stateEntryTime);

        if (m_autoTracker && m_autoTracker->isEngaged()) {
            const auto trkStatus = m_autoTracker->status();
            if (trkStatus.trackingState == Tracking::PtzAutoTracker::TrackingState::Tracking) {
                transitionTo(CueingState::DwellInspection);
                break;
            }
        }

        if (elapsed >= m_config.opticalAcquisitionTimeout) {
            // Visual tracker failed to acquire; fallback to geodetic slaving and proceed with dwell
            transitionTo(CueingState::DwellInspection);
        }
        break;
    }

    case CueingState::OpticalTracking:
    case CueingState::GeodeticTrackingFallback: {
        transitionTo(CueingState::DwellInspection);
        break;
    }

    case CueingState::DwellInspection: {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_dwellStartTime);
        if (elapsed >= m_config.inspectionDwellDuration) {
            dismissActiveTarget();
        }
        break;
    }

    case CueingState::TargetCompleted: {
        // Reset active target and return to Idle
        m_activeTargetId = 0U;
        m_activeTargetSource = TargetTrackSource::None;
        m_activeTargetName.clear();
        m_activeThreatScore = 0.0;
        m_isEmergencyTarget = false;
        transitionTo(CueingState::Idle);
        break;
    }
    }

    m_lastUpdateTick = now;

    // 3. Emit status callback
    StatusCallback cbCopy = m_statusCb;
    if (cbCopy) {
        cbCopy(status());
    }
}

SlewToCueStatus SlewToCueDirector::status() const
{
    SlewToCueStatus st;
    st.state = m_state;
    st.isEngaged = (m_state != CueingState::Idle && m_state != CueingState::TargetCompleted);
    st.activeTargetId = m_activeTargetId;
    st.activeTargetSource = m_activeTargetSource;
    st.activeTargetName = m_activeTargetName;
    st.activeThreatScore = m_activeThreatScore;
    st.isEmergencyTarget = m_isEmergencyTarget;

    if (m_slavingBridge) {
        const auto slavingSt = m_slavingBridge->status();
        st.slantRangeMeters = slavingSt.slantRangeMeters;
        st.trueBearingDeg = slavingSt.trueBearingDeg;
    }

    if (m_camera) {
        st.currentHfovDeg = m_camera->currentTelemetry().horizontalFovDeg;
    }

    if (m_autoTracker) {
        st.isOpticalTrackerLocked = m_autoTracker->isEngaged()
            && (m_autoTracker->status().trackingState == Tracking::PtzAutoTracker::TrackingState::Tracking);
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_state == CueingState::DwellInspection) {
        st.dwellElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_dwellStartTime);
        if (st.dwellElapsed < m_config.inspectionDwellDuration) {
            st.dwellRemaining = m_config.inspectionDwellDuration - st.dwellElapsed;
        } else {
            st.dwellRemaining = std::chrono::milliseconds(0);
        }
    }

    return st;
}

} // namespace PayloadHal
