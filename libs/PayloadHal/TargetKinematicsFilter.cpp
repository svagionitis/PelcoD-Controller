#include "TargetKinematicsFilter.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kEpsilon = 1e-6;

double normalizeAzimuth(double deg) {
    deg = std::fmod(deg, 360.0);
    if (deg < 0.0) {
        deg += 360.0;
    }
    return deg;
}

double angleDifference(double targetDeg, double sourceDeg) {
    double diff = std::fmod(targetDeg - sourceDeg + 180.0, 360.0);
    if (diff < 0.0) {
        diff += 360.0;
    }
    return diff - 180.0;
}

} // namespace

// ============================================================================
// Axis1DFilter Implementation
// ============================================================================

void TargetKinematicsFilter::Axis1DFilter::reset(double initPos, double initVel) {
    x[0] = initPos;
    x[1] = initVel;
    x[2] = 0.0;

    P[0][0] = 50.0;  P[0][1] = 0.0;   P[0][2] = 0.0;
    P[1][0] = 0.0;   P[1][1] = 25.0;  P[1][2] = 0.0;
    P[2][0] = 0.0;   P[2][1] = 0.0;   P[2][2] = 5.0;
}

void TargetKinematicsFilter::Axis1DFilter::predict(double dt, double qAcc) {
    if (dt <= kEpsilon) {
        return;
    }

    const double dt2 = dt * dt;
    const double dt3 = dt2 * dt;
    const double dt4 = dt3 * dt;
    const double dt5 = dt4 * dt;

    // State propagation: x' = F * x
    const double x0_new = x[0] + x[1] * dt + 0.5 * x[2] * dt2;
    const double x1_new = x[1] + x[2] * dt;
    const double x2_new = x[2];

    x[0] = x0_new;
    x[1] = x1_new;
    x[2] = x2_new;

    // Intermediate M = F * P
    double M[3][3];
    M[0][0] = P[0][0] + dt * P[1][0] + 0.5 * dt2 * P[2][0];
    M[0][1] = P[0][1] + dt * P[1][1] + 0.5 * dt2 * P[2][1];
    M[0][2] = P[0][2] + dt * P[1][2] + 0.5 * dt2 * P[2][2];

    M[1][0] = P[1][0] + dt * P[2][0];
    M[1][1] = P[1][1] + dt * P[2][1];
    M[1][2] = P[1][2] + dt * P[2][2];

    M[2][0] = P[2][0];
    M[2][1] = P[2][1];
    M[2][2] = P[2][2];

    // P' = M * F^T + Q
    P[0][0] = M[0][0] + dt * M[0][1] + 0.5 * dt2 * M[0][2] + qAcc * (dt5 / 20.0);
    P[0][1] = M[0][1] + dt * M[0][2] + qAcc * (dt4 / 8.0);
    P[0][2] = M[0][2] + qAcc * (dt3 / 6.0);

    P[1][0] = P[0][1];
    P[1][1] = M[1][1] + dt * M[1][2] + qAcc * (dt3 / 3.0);
    P[1][2] = M[1][2] + qAcc * (dt2 / 2.0);

    P[2][0] = P[0][2];
    P[2][1] = P[1][2];
    P[2][2] = M[2][2] + qAcc * dt;

    // Ensure non-negative diagonal variance
    P[0][0] = std::max(P[0][0], 1e-6);
    P[1][1] = std::max(P[1][1], 1e-6);
    P[2][2] = std::max(P[2][2], 1e-6);
}

void TargetKinematicsFilter::Axis1DFilter::update(double z, double r) {
    const double y = z - x[0];
    const double S = P[0][0] + r;

    if (S <= kEpsilon) {
        return;
    }

    const double K0 = P[0][0] / S;
    const double K1 = P[1][0] / S;
    const double K2 = P[2][0] / S;

    x[0] += K0 * y;
    x[1] += K1 * y;
    x[2] += K2 * y;

    // P = (I - K*H) * P
    const double p00 = P[0][0];
    const double p01 = P[0][1];
    const double p02 = P[0][2];

    P[0][0] -= K0 * p00;
    P[0][1] -= K0 * p01;
    P[0][2] -= K0 * p02;

    P[1][0] -= K1 * p00;
    P[1][1] -= K1 * p01;
    P[1][2] -= K1 * p02;

    P[2][0] -= K2 * p00;
    P[2][1] -= K2 * p01;
    P[2][2] -= K2 * p02;

    // Symmetrize
    P[1][0] = P[0][1] = 0.5 * (P[0][1] + P[1][0]);
    P[2][0] = P[0][2] = 0.5 * (P[0][2] + P[2][0]);
    P[2][1] = P[1][2] = 0.5 * (P[1][2] + P[2][1]);

    P[0][0] = std::max(P[0][0], 1e-6);
    P[1][1] = std::max(P[1][1], 1e-6);
    P[2][2] = std::max(P[2][2], 1e-6);
}

// ============================================================================
// TargetKinematicsFilter Implementation
// ============================================================================

TargetKinematicsFilter::TargetKinematicsFilter(TargetKinematicsConfig config)
    : m_config(config),
      m_lastMeasurementTime(std::chrono::steady_clock::now()),
      m_lastPredictTime(std::chrono::steady_clock::now()) {
    reset();
}

void TargetKinematicsFilter::setConfig(const TargetKinematicsConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

TargetKinematicsConfig TargetKinematicsFilter::config() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void TargetKinematicsFilter::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = TargetTrackState::Unacquired;
    m_hitCount = 0;
    m_filterN.reset(0.0);
    m_filterE.reset(0.0);
    m_filterD.reset(0.0);
    m_lastEstimatedRange = 1000.0;
    m_lastMeasurementTime = std::chrono::steady_clock::now();
    m_lastPredictTime = std::chrono::steady_clock::now();
}

void TargetKinematicsFilter::updateFullMeasurement(
    double azimuthDeg, double elevationDeg, double slantRangeMeters,
    const Vector3D& platformPositionNed,
    std::chrono::steady_clock::time_point timestamp) {

    std::lock_guard<std::mutex> lock(m_mutex);

    if (slantRangeMeters <= 0.0) {
        return;
    }

    const double azRad = azimuthDeg * kDegToRad;
    const double elRad = elevationDeg * kDegToRad;
    const double cosEl = std::cos(elRad);
    const double sinEl = std::sin(elRad);
    const double cosAz = std::cos(azRad);
    const double sinAz = std::sin(azRad);

    const double zN = platformPositionNed.x + slantRangeMeters * cosEl * cosAz;
    const double zE = platformPositionNed.y + slantRangeMeters * cosEl * sinAz;
    const double zD = platformPositionNed.z - slantRangeMeters * sinEl;

    m_lastPlatformPos = platformPositionNed;
    m_lastEstimatedRange = slantRangeMeters;

    const double dt = std::chrono::duration<double>(timestamp - m_lastPredictTime).count();
    const double sigmaR = m_config.defaultRangeUncertaintyMeters;
    const double sigmaAng = m_config.angleUncertaintyDeg * kDegToRad;
    const double sigmaCross = slantRangeMeters * sigmaAng;

    const double varR = sigmaR * sigmaR;
    const double varCross = sigmaCross * sigmaCross;
    const double rN = varR * cosEl * cosEl * cosAz * cosAz + varCross;
    const double rE = varR * cosEl * cosEl * sinAz * sinAz + varCross;
    const double rD = varR * sinEl * sinEl + varCross;

    if (m_state == TargetTrackState::Unacquired || m_state == TargetTrackState::Lost) {
        m_filterN.reset(zN);
        m_filterE.reset(zE);
        m_filterD.reset(zD);
        m_state = TargetTrackState::Acquiring;
        m_hitCount = 1;
    } else {
        // Gating test during coasting
        if (m_state == TargetTrackState::Coasting) {
            const double dx = zN - m_filterN.x[0];
            const double dy = zE - m_filterE.x[0];
            const double dz = zD - m_filterD.x[0];
            const double distErr = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distErr > m_config.gateThresholdMeters) {
                // Out of gate, re-initialize
                m_filterN.reset(zN);
                m_filterE.reset(zE);
                m_filterD.reset(zD);
                m_state = TargetTrackState::Acquiring;
                m_hitCount = 1;
                m_lastMeasurementTime = timestamp;
                m_lastPredictTime = timestamp;
                return;
            }
        }

        if (dt > kEpsilon && dt < 5.0) {
            m_filterN.predict(dt, m_config.processNoiseAcc);
            m_filterE.predict(dt, m_config.processNoiseAcc);
            m_filterD.predict(dt, m_config.processNoiseAcc);
        }

        m_filterN.update(zN, rN);
        m_filterE.update(zE, rE);
        m_filterD.update(zD, rD);

        m_hitCount++;
        if (m_hitCount >= m_config.confirmHitsRequired) {
            m_state = TargetTrackState::Tracking;
        }
    }

    m_lastMeasurementTime = timestamp;
    m_lastPredictTime = timestamp;
}

void TargetKinematicsFilter::updateBearingMeasurement(
    double azimuthDeg, double elevationDeg,
    const Vector3D& platformPositionNed,
    std::chrono::steady_clock::time_point timestamp) {

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state == TargetTrackState::Unacquired || m_state == TargetTrackState::Lost) {
        return; // Bearing-only cannot initialize 3D range
    }

    const double azRad = azimuthDeg * kDegToRad;
    const double elRad = elevationDeg * kDegToRad;
    const double cosEl = std::cos(elRad);
    const double sinEl = std::sin(elRad);
    const double cosAz = std::cos(azRad);
    const double sinAz = std::sin(azRad);

    const double dt = std::chrono::duration<double>(timestamp - m_lastPredictTime).count();
    if (dt > kEpsilon && dt < 5.0) {
        m_filterN.predict(dt, m_config.processNoiseAcc);
        m_filterE.predict(dt, m_config.processNoiseAcc);
        m_filterD.predict(dt, m_config.processNoiseAcc);
    }

    // Use current estimated range
    const double dx = m_filterN.x[0] - platformPositionNed.x;
    const double dy = m_filterE.x[0] - platformPositionNed.y;
    const double dz = m_filterD.x[0] - platformPositionNed.z;
    const double estRange = std::max(10.0, std::sqrt(dx * dx + dy * dy + dz * dz));
    m_lastEstimatedRange = estRange;

    const double zN = platformPositionNed.x + estRange * cosEl * cosAz;
    const double zE = platformPositionNed.y + estRange * cosEl * sinAz;
    const double zD = platformPositionNed.z - estRange * sinEl;

    // Cross-track uncertainty
    const double sigmaAng = m_config.angleUncertaintyDeg * kDegToRad;
    const double sigmaCross = estRange * sigmaAng;
    const double varCross = sigmaCross * sigmaCross;
    const double varAlong = (estRange * 0.1) * (estRange * 0.1); // high along-track uncertainty

    const double rN = varAlong * cosEl * cosEl * cosAz * cosAz + varCross;
    const double rE = varAlong * cosEl * cosEl * sinAz * sinAz + varCross;
    const double rD = varAlong * sinEl * sinEl + varCross;

    m_filterN.update(zN, rN);
    m_filterE.update(zE, rE);
    m_filterD.update(zD, rD);

    m_lastMeasurementTime = timestamp;
    m_lastPredictTime = timestamp;
}

void TargetKinematicsFilter::predict(std::chrono::steady_clock::time_point currentTime) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state == TargetTrackState::Unacquired || m_state == TargetTrackState::Lost) {
        return;
    }

    const double dt = std::chrono::duration<double>(currentTime - m_lastPredictTime).count();
    if (dt > kEpsilon && dt < 5.0) {
        m_filterN.predict(dt, m_config.processNoiseAcc);
        m_filterE.predict(dt, m_config.processNoiseAcc);
        m_filterD.predict(dt, m_config.processNoiseAcc);
        m_lastPredictTime = currentTime;
    }

    checkCoastingTimeout(currentTime);
}

void TargetKinematicsFilter::checkCoastingTimeout(std::chrono::steady_clock::time_point now) {
    if (m_state == TargetTrackState::Unacquired || m_state == TargetTrackState::Lost) {
        return;
    }

    const double noMeasDuration = std::chrono::duration<double>(now - m_lastMeasurementTime).count();

    if (noMeasDuration > 0.25) { // No measurement for 250ms -> transition to Coasting
        if (noMeasDuration <= m_config.maxCoastDurationSec) {
            m_state = TargetTrackState::Coasting;
        } else {
            m_state = TargetTrackState::Lost;
        }
    }
}

TargetTrackState TargetKinematicsFilter::trackState() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

bool TargetKinematicsFilter::isTracking() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == TargetTrackState::Tracking || m_state == TargetTrackState::Coasting;
}

TargetKinematics3D TargetKinematicsFilter::currentKinematics(
    const Vector3D& platformPositionNed) const {

    std::lock_guard<std::mutex> lock(m_mutex);
    TargetKinematics3D state;
    state.trackState = m_state;
    state.timestamp = m_lastPredictTime;

    state.positionNedMeters = {m_filterN.x[0], m_filterE.x[0], m_filterD.x[0]};
    state.velocityNedMps = {m_filterN.x[1], m_filterE.x[1], m_filterD.x[1]};
    state.accelerationNedMps2 = {m_filterN.x[2], m_filterE.x[2], m_filterD.x[2]};

    const double vx = state.velocityNedMps.x;
    const double vy = state.velocityNedMps.y;
    const double vz = state.velocityNedMps.z;

    state.groundSpeedMps = std::sqrt(vx * vx + vy * vy);
    state.speedMps = std::sqrt(vx * vx + vy * vy + vz * vz);
    state.climbRateMps = -vz; // Down is positive, so climb is -vz

    if (state.groundSpeedMps > 0.1) {
        state.courseDeg = normalizeAzimuth(std::atan2(vy, vx) * kRadToDeg);
    } else {
        state.courseDeg = 0.0;
    }

    // Relative to platform
    const double dN = state.positionNedMeters.x - platformPositionNed.x;
    const double dE = state.positionNedMeters.y - platformPositionNed.y;
    const double dD = state.positionNedMeters.z - platformPositionNed.z;
    const double rHoriz = std::sqrt(dN * dN + dE * dE);

    state.slantRangeMeters = std::sqrt(dN * dN + dE * dE + dD * dD);
    state.azimuthDeg = normalizeAzimuth(std::atan2(dE, dN) * kRadToDeg);
    state.elevationDeg = std::atan2(-dD, rHoriz) * kRadToDeg;

    // Uncertainties
    state.positionUncertaintyMeters = std::sqrt(
        (m_filterN.P[0][0] + m_filterE.P[0][0] + m_filterD.P[0][0]) / 3.0);
    state.velocityUncertaintyMps = std::sqrt(
        (m_filterN.P[1][1] + m_filterE.P[1][1] + m_filterD.P[1][1]) / 3.0);

    return state;
}

PredictiveLeadSolution TargetKinematicsFilter::computeLeadAngles(
    double latencySec,
    double projectileVelocityMps,
    const Vector3D& platformPositionNed) const {

    std::lock_guard<std::mutex> lock(m_mutex);

    PredictiveLeadSolution sol;
    if (m_state != TargetTrackState::Tracking && m_state != TargetTrackState::Coasting) {
        sol.valid = false;
        return sol;
    }

    const Vector3D pTarget{m_filterN.x[0], m_filterE.x[0], m_filterD.x[0]};
    const Vector3D vTarget{m_filterN.x[1], m_filterE.x[1], m_filterD.x[1]};
    const Vector3D aTarget{m_filterN.x[2], m_filterE.x[2], m_filterD.x[2]};

    // Current LOS
    const double curDN = pTarget.x - platformPositionNed.x;
    const double curDE = pTarget.y - platformPositionNed.y;
    const double curDD = pTarget.z - platformPositionNed.z;
    const double curRHoriz = std::sqrt(curDN * curDN + curDE * curDE);

    sol.currentAzimuthDeg = normalizeAzimuth(std::atan2(curDE, curDN) * kRadToDeg);
    sol.currentElevationDeg = std::atan2(-curDD, curRHoriz) * kRadToDeg;

    // Advance for latency
    const double lat = std::max(0.0, latencySec);
    Vector3D r0{
        pTarget.x + vTarget.x * lat - platformPositionNed.x,
        pTarget.y + vTarget.y * lat - platformPositionNed.y,
        pTarget.z + vTarget.z * lat - platformPositionNed.z
    };

    double tauTof = 0.0;
    if (projectileVelocityMps > 1.0) {
        // Solve ||r0 + vTarget * tau||^2 = (vProj * tau)^2
        const double vSq = vTarget.x * vTarget.x + vTarget.y * vTarget.y + vTarget.z * vTarget.z;
        const double projSq = projectileVelocityMps * projectileVelocityMps;
        const double r0Sq = r0.x * r0.x + r0.y * r0.y + r0.z * r0.z;
        const double r0DotV = r0.x * vTarget.x + r0.y * vTarget.y + r0.z * vTarget.z;

        const double A = vSq - projSq;
        const double B = 2.0 * r0DotV;
        const double C = r0Sq;

        const double discriminant = B * B - 4.0 * A * C;
        if (discriminant >= 0.0 && std::abs(A) > kEpsilon) {
            const double sqrtDisc = std::sqrt(discriminant);
            const double t1 = (-B - sqrtDisc) / (2.0 * A);
            const double t2 = (-B + sqrtDisc) / (2.0 * A);

            if (t1 > 0.0 && t2 > 0.0) {
                tauTof = std::min(t1, t2);
            } else if (t1 > 0.0) {
                tauTof = t1;
            } else if (t2 > 0.0) {
                tauTof = t2;
            }
        }

        if (tauTof <= 0.0) {
            tauTof = std::sqrt(r0Sq) / projectileVelocityMps;
        }
    }

    const double totalLead = lat + tauTof;
    sol.totalLeadTimeSec = totalLead;
    sol.timeOfFlightSec = tauTof;

    // Intercept position
    sol.predictedPositionNed.x = pTarget.x + vTarget.x * totalLead + 0.5 * aTarget.x * totalLead * totalLead;
    sol.predictedPositionNed.y = pTarget.y + vTarget.y * totalLead + 0.5 * aTarget.y * totalLead * totalLead;
    sol.predictedPositionNed.z = pTarget.z + vTarget.z * totalLead + 0.5 * aTarget.z * totalLead * totalLead;

    // Lead LOS
    const double leadDN = sol.predictedPositionNed.x - platformPositionNed.x;
    const double leadDE = sol.predictedPositionNed.y - platformPositionNed.y;
    const double leadDD = sol.predictedPositionNed.z - platformPositionNed.z;
    const double leadRHoriz = std::sqrt(leadDN * leadDN + leadDE * leadDE);

    sol.leadAzimuthDeg = normalizeAzimuth(std::atan2(leadDE, leadDN) * kRadToDeg);
    sol.leadElevationDeg = std::atan2(-leadDD, leadRHoriz) * kRadToDeg;

    sol.deltaAzimuthDeg = angleDifference(sol.leadAzimuthDeg, sol.currentAzimuthDeg);
    sol.deltaElevationDeg = sol.leadElevationDeg - sol.currentElevationDeg;
    sol.valid = true;

    return sol;
}

} // namespace PayloadHal
