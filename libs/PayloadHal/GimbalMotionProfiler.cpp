#include "GimbalMotionProfiler.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {

constexpr double kEpsilon = 1e-6;

template <typename T>
T clampVal(T val, T minVal, T maxVal) {
    return std::max(minVal, std::min(val, maxVal));
}

} // namespace

// ============================================================================
// AxisTrajectory Implementation
// ============================================================================

AxisTrajectory::AxisTrajectory(double startPos, double targetPos,
                               const std::vector<double>& durations,
                               double jMax, double aLim, double vLim,
                               ProfileAlgorithm algorithm) noexcept
    : m_startPos(startPos),
      m_targetPos(targetPos),
      m_direction((targetPos >= startPos) ? 1.0 : -1.0),
      m_jMax(jMax),
      m_aLim(aLim),
      m_vLim(vLim),
      m_algorithm(algorithm),
      m_durations(durations) {

    m_totalDuration = 0.0;
    for (double d : m_durations) {
        m_totalDuration += d;
    }

    if (m_totalDuration <= kEpsilon || std::abs(targetPos - startPos) <= kEpsilon) {
        m_totalDuration = 0.0;
        return;
    }

    const size_t numSegments = m_durations.size();
    m_switchTimes.resize(numSegments + 1, 0.0);
    m_boundaryPos.resize(numSegments + 1, 0.0);
    m_boundaryVel.resize(numSegments + 1, 0.0);
    m_boundaryAcc.resize(numSegments + 1, 0.0);

    // Jerk for each segment
    std::vector<double> jerks(numSegments, 0.0);
    if (m_algorithm == ProfileAlgorithm::SCurve7Segment && numSegments == 7) {
        jerks[0] = +m_jMax;  // Segment 1: Jerk up
        jerks[1] = 0.0;      // Segment 2: Const acc
        jerks[2] = -m_jMax;  // Segment 3: Jerk down
        jerks[3] = 0.0;      // Segment 4: Const cruise
        jerks[4] = -m_jMax;  // Segment 5: Jerk down
        jerks[5] = 0.0;      // Segment 6: Const dec
        jerks[6] = +m_jMax;  // Segment 7: Jerk up
    } else if (m_algorithm == ProfileAlgorithm::LinearTrapezoidal && numSegments == 3) {
        // Trapezoidal: jerk is zero, acceleration is constant piecewise
        jerks[0] = 0.0;
        jerks[1] = 0.0;
        jerks[2] = 0.0;
    }

    // Initialize boundary conditions at t = 0
    m_switchTimes[0] = 0.0;
    m_boundaryPos[0] = 0.0; // Distance traveled from 0
    m_boundaryVel[0] = 0.0;
    if (m_algorithm == ProfileAlgorithm::LinearTrapezoidal && numSegments == 3) {
        m_boundaryAcc[0] = m_aLim;
    } else {
        m_boundaryAcc[0] = 0.0;
    }

    // Integrate boundaries forward
    for (size_t i = 0; i < numSegments; ++i) {
        const double dt = m_durations[i];
        m_switchTimes[i + 1] = m_switchTimes[i] + dt;

        double j = jerks[i];
        double a0 = m_boundaryAcc[i];
        double v0 = m_boundaryVel[i];
        double s0 = m_boundaryPos[i];

        if (m_algorithm == ProfileAlgorithm::LinearTrapezoidal && numSegments == 3) {
            if (i == 1) {
                a0 = 0.0; // Cruise segment
            } else if (i == 2) {
                a0 = -m_aLim; // Deceleration segment
            }
        }

        m_boundaryAcc[i + 1] = a0 + j * dt;
        m_boundaryVel[i + 1] = v0 + a0 * dt + 0.5 * j * dt * dt;
        m_boundaryPos[i + 1] = s0 + v0 * dt + 0.5 * a0 * dt * dt + (1.0 / 6.0) * j * dt * dt * dt;
    }
}

KinematicState1D AxisTrajectory::sample(double timeOffsetSec) const noexcept {
    KinematicState1D state;

    if (m_totalDuration <= kEpsilon || std::abs(m_targetPos - m_startPos) <= kEpsilon) {
        state.positionDeg = m_targetPos;
        state.velocityDegPerSec = 0.0;
        state.accelerationDegPerSec2 = 0.0;
        state.jerkDegPerSec3 = 0.0;
        return state;
    }

    if (timeOffsetSec <= 0.0) {
        state.positionDeg = m_startPos;
        state.velocityDegPerSec = 0.0;
        state.accelerationDegPerSec2 = 0.0;
        state.jerkDegPerSec3 = 0.0;
        return state;
    }

    if (timeOffsetSec >= m_totalDuration) {
        state.positionDeg = m_targetPos;
        state.velocityDegPerSec = 0.0;
        state.accelerationDegPerSec2 = 0.0;
        state.jerkDegPerSec3 = 0.0;
        return state;
    }

    const size_t numSegments = m_durations.size();
    size_t seg = numSegments - 1;
    for (size_t i = 0; i < numSegments; ++i) {
        if (timeOffsetSec < m_switchTimes[i + 1]) {
            seg = i;
            break;
        }
    }

    const double tau = timeOffsetSec - m_switchTimes[seg];
    double j = 0.0;
    double a0 = m_boundaryAcc[seg];
    double v0 = m_boundaryVel[seg];
    double s0 = m_boundaryPos[seg];

    if (m_algorithm == ProfileAlgorithm::SCurve7Segment && numSegments == 7) {
        switch (seg) {
            case 0: j = +m_jMax; break;
            case 1: j = 0.0; break;
            case 2: j = -m_jMax; break;
            case 3: j = 0.0; break;
            case 4: j = -m_jMax; break;
            case 5: j = 0.0; break;
            case 6: j = +m_jMax; break;
            default: j = 0.0; break;
        }
    } else if (m_algorithm == ProfileAlgorithm::LinearTrapezoidal && numSegments == 3) {
        j = 0.0;
        if (seg == 0) a0 = m_aLim;
        else if (seg == 1) a0 = 0.0;
        else if (seg == 2) a0 = -m_aLim;
    }

    const double a = a0 + j * tau;
    const double v = v0 + a0 * tau + 0.5 * j * tau * tau;
    const double s = s0 + v0 * tau + 0.5 * a0 * tau * tau + (1.0 / 6.0) * j * tau * tau * tau;

    state.positionDeg = m_startPos + m_direction * s;
    state.velocityDegPerSec = m_direction * v;
    state.accelerationDegPerSec2 = m_direction * a;
    state.jerkDegPerSec3 = m_direction * j;

    return state;
}

// ============================================================================
// CoordinatedGimbalTrajectory Implementation
// ============================================================================

GimbalKinematicState CoordinatedGimbalTrajectory::sample(double timeOffsetSec) const noexcept {
    GimbalKinematicState state;
    state.pan = panTrajectory.sample(timeOffsetSec);
    state.tilt = tiltTrajectory.sample(timeOffsetSec);
    state.roll = rollTrajectory.sample(timeOffsetSec);
    state.timestamp = std::chrono::steady_clock::now();
    return state;
}

// ============================================================================
// GimbalMotionProfiler Implementation
// ============================================================================

GimbalMotionProfiler::GimbalMotionProfiler(GimbalMotionConstraints constraints)
    : m_constraints(constraints) {}

void GimbalMotionProfiler::setConstraints(const GimbalMotionConstraints& constraints) {
    if (constraints.isValid()) {
        m_constraints = constraints;
    }
}

GimbalMotionConstraints GimbalMotionProfiler::constraints() const {
    return m_constraints;
}

void GimbalMotionProfiler::setAlgorithm(ProfileAlgorithm algorithm) noexcept {
    m_algorithm = algorithm;
}

ProfileAlgorithm GimbalMotionProfiler::algorithm() const noexcept {
    return m_algorithm;
}

AxisTrajectory GimbalMotionProfiler::planAxisMove(
    double startPosDeg,
    double targetPosDeg,
    const AxisMotionConstraints& constraints) const {

    if (!constraints.isValid() || std::abs(targetPosDeg - startPosDeg) <= kEpsilon) {
        return AxisTrajectory(startPosDeg, targetPosDeg, {0.0}, 0.0, 0.0, 0.0, m_algorithm);
    }

    if (m_algorithm == ProfileAlgorithm::LinearTrapezoidal) {
        return planTrapezoidal(startPosDeg, targetPosDeg, constraints);
    }
    return planSCurve(startPosDeg, targetPosDeg, constraints);
}

AxisTrajectory GimbalMotionProfiler::planTrapezoidal(
    double startPos, double targetPos,
    const AxisMotionConstraints& constraints) const {

    const double L = std::abs(targetPos - startPos);
    const double vMax = constraints.maxVelocityDegPerSec;
    const double aMax = constraints.maxAccelerationDegPerSec2;

    const double sAcc = (vMax * vMax) / (2.0 * aMax);

    double Ta = 0.0;
    double Tv = 0.0;
    double vLim = 0.0;

    if (L >= 2.0 * sAcc) {
        // Full cruise profile
        vLim = vMax;
        Ta = vMax / aMax;
        Tv = (L - 2.0 * sAcc) / vMax;
    } else {
        // Triangular profile (no cruise)
        vLim = std::sqrt(aMax * L);
        Ta = vLim / aMax;
        Tv = 0.0;
    }

    std::vector<double> durations = {Ta, Tv, Ta};
    return AxisTrajectory(startPos, targetPos, durations, 0.0, aMax, vLim, ProfileAlgorithm::LinearTrapezoidal);
}

AxisTrajectory GimbalMotionProfiler::planSCurve(
    double startPos, double targetPos,
    const AxisMotionConstraints& constraints) const {

    const double L = std::abs(targetPos - startPos);
    const double vMax = constraints.maxVelocityDegPerSec;
    const double aMax = constraints.maxAccelerationDegPerSec2;
    const double jMax = constraints.maxJerkDegPerSec3;

    double Tj = 0.0;
    double Ta = 0.0;
    double Tv = 0.0;
    double aLim = 0.0;
    double vLim = 0.0;

    // Check if maximum acceleration can be reached before maximum velocity
    if (vMax >= (aMax * aMax) / jMax) {
        // Case A: aMax can theoretically be reached
        Tj = aMax / jMax;
        const double vj = (aMax * aMax) / jMax;
        const double TaCandidate = (vMax - vj) / aMax;
        const double sAccCandidate = 0.5 * vMax * (2.0 * Tj + TaCandidate);
        const double Lcruise = 2.0 * sAccCandidate;

        if (L >= Lcruise) {
            // Reaches both aMax and vMax
            aLim = aMax;
            vLim = vMax;
            Ta = TaCandidate;
            Tv = (L - Lcruise) / vMax;
        } else {
            // Does not reach vMax
            Tv = 0.0;
            const double La = 2.0 * (aMax * aMax * aMax) / (jMax * jMax);
            if (L >= La) {
                // Reaches aMax, but not vMax
                aLim = aMax;
                // Solve quadratic: Ta^2 + 3*Tj*Ta + 2*Tj^2 - L/aMax = 0
                const double discriminant = Tj * Tj + 4.0 * L / aMax;
                Ta = (-3.0 * Tj + std::sqrt(discriminant)) / 2.0;
                if (Ta < 0.0) Ta = 0.0;
                vLim = aMax * (Tj + Ta);
            } else {
                // Short move: does not even reach aMax (triangular jerk)
                Ta = 0.0;
                Tj = std::cbrt(L / (2.0 * jMax));
                aLim = jMax * Tj;
                vLim = jMax * Tj * Tj;
            }
        }
    } else {
        // Case B: vMax is small, so vMax is reached before aMax could be reached
        Tj = std::sqrt(vMax / jMax);
        aLim = jMax * Tj;
        const double Lcruise = 2.0 * vMax * Tj;

        if (L >= Lcruise) {
            // Reaches vMax with aLim < aMax
            Ta = 0.0;
            vLim = vMax;
            Tv = (L - Lcruise) / vMax;
        } else {
            // Short move: does not reach vMax or aMax
            Ta = 0.0;
            Tv = 0.0;
            Tj = std::cbrt(L / (2.0 * jMax));
            aLim = jMax * Tj;
            vLim = jMax * Tj * Tj;
        }
    }

    std::vector<double> durations = {Tj, Ta, Tj, Tv, Tj, Ta, Tj};
    return AxisTrajectory(startPos, targetPos, durations, jMax, aLim, vLim, ProfileAlgorithm::SCurve7Segment);
}

CoordinatedGimbalTrajectory GimbalMotionProfiler::planCoordinatedMove(
    const GimbalKinematicState& startState,
    double targetPanDeg,
    double targetTiltDeg,
    double targetRollDeg,
    bool synchronizeDuration) const {

    CoordinatedGimbalTrajectory coordinated;

    AxisTrajectory trajPan = planAxisMove(startState.pan.positionDeg, targetPanDeg, m_constraints.pan);
    AxisTrajectory trajTilt = planAxisMove(startState.tilt.positionDeg, targetTiltDeg, m_constraints.tilt);
    AxisTrajectory trajRoll = planAxisMove(startState.roll.positionDeg, targetRollDeg, m_constraints.roll);

    const double tPan = trajPan.totalDuration();
    const double tTilt = trajTilt.totalDuration();
    const double tRoll = trajRoll.totalDuration();
    const double tMaster = std::max({tPan, tTilt, tRoll});

    coordinated.totalDurationSec = tMaster;

    if (!synchronizeDuration || tMaster <= kEpsilon) {
        coordinated.panTrajectory = trajPan;
        coordinated.tiltTrajectory = trajTilt;
        coordinated.rollTrajectory = trajRoll;
        return coordinated;
    }

    // Synchronize each moving axis to tMaster by scaling constraints
    auto syncAxis = [this, tMaster](double startPos, double targetPos,
                                   double tAxis, const AxisMotionConstraints& origConstraints) -> AxisTrajectory {
        if (std::abs(targetPos - startPos) <= kEpsilon || tAxis <= kEpsilon) {
            return AxisTrajectory(startPos, targetPos, {tMaster}, 0.0, 0.0, 0.0, m_algorithm);
        }

        const double lambda = tMaster / tAxis;
        if (lambda <= 1.0 + kEpsilon) {
            return planAxisMove(startPos, targetPos, origConstraints);
        }

        // Scale constraints by powers of lambda:
        // v' = v / lambda, a' = a / (lambda^2), j' = j / (lambda^3)
        AxisMotionConstraints scaled;
        scaled.maxVelocityDegPerSec = origConstraints.maxVelocityDegPerSec / lambda;
        scaled.maxAccelerationDegPerSec2 = origConstraints.maxAccelerationDegPerSec2 / (lambda * lambda);
        scaled.maxJerkDegPerSec3 = origConstraints.maxJerkDegPerSec3 / (lambda * lambda * lambda);

        return planAxisMove(startPos, targetPos, scaled);
    };

    coordinated.panTrajectory = syncAxis(startState.pan.positionDeg, targetPanDeg, tPan, m_constraints.pan);
    coordinated.tiltTrajectory = syncAxis(startState.tilt.positionDeg, targetTiltDeg, tTilt, m_constraints.tilt);
    coordinated.rollTrajectory = syncAxis(startState.roll.positionDeg, targetRollDeg, tRoll, m_constraints.roll);

    return coordinated;
}

KinematicState1D GimbalMotionProfiler::filterVelocityStep(
    const KinematicState1D& currentState,
    double targetVelocityDegPerSec,
    double dtSec,
    const AxisMotionConstraints& constraints) const noexcept {

    KinematicState1D nextState = currentState;

    if (dtSec <= 0.0 || !constraints.isValid()) {
        return nextState;
    }

    // Clamp dt to a reasonable maximum to maintain stability
    const double dt = clampVal(dtSec, 0.0001, 0.2);

    const double vMax = constraints.maxVelocityDegPerSec;
    const double aMax = constraints.maxAccelerationDegPerSec2;
    const double jMax = constraints.maxJerkDegPerSec3;

    const double vTarget = clampVal(targetVelocityDegPerSec, -vMax, vMax);
    const double vCur = clampVal(currentState.velocityDegPerSec, -vMax, vMax);
    const double aCur = clampVal(currentState.accelerationDegPerSec2, -aMax, aMax);

    const double vErr = vTarget - vCur;

    if (std::abs(vErr) <= kEpsilon && std::abs(aCur) <= kEpsilon) {
        nextState.velocityDegPerSec = vTarget;
        nextState.accelerationDegPerSec2 = 0.0;
        nextState.jerkDegPerSec3 = 0.0;
        nextState.positionDeg += vTarget * dt;
        return nextState;
    }

    // Velocity change required to ramp acceleration aCur to 0 at jerk jMax
    const double vBrake = (aCur * std::abs(aCur)) / (2.0 * jMax);

    double desiredJerk = 0.0;
    if (vErr > vBrake + kEpsilon) {
        // Need to accelerate toward +aMax
        if (aCur < aMax - kEpsilon) {
            desiredJerk = +jMax;
        } else {
            desiredJerk = 0.0;
        }
    } else if (vErr < vBrake - kEpsilon) {
        // Need to decelerate toward -aMax
        if (aCur > -aMax + kEpsilon) {
            desiredJerk = -jMax;
        } else {
            desiredJerk = 0.0;
        }
    } else {
        // Ramp acceleration smoothly toward 0
        if (aCur > kEpsilon) {
            desiredJerk = -jMax;
        } else if (aCur < -kEpsilon) {
            desiredJerk = +jMax;
        } else {
            desiredJerk = 0.0;
        }
    }

    double aNext = clampVal(aCur + desiredJerk * dt, -aMax, aMax);
    double vNext = vCur + aNext * dt;

    // Check if target velocity is reached or crossed
    if ((vTarget - vCur) * (vTarget - vNext) <= 0.0) {
        vNext = vTarget;
        aNext = 0.0;
        desiredJerk = 0.0;
    } else {
        vNext = clampVal(vNext, -vMax, vMax);
    }

    nextState.jerkDegPerSec3 = desiredJerk;
    nextState.accelerationDegPerSec2 = aNext;
    nextState.velocityDegPerSec = vNext;
    nextState.positionDeg += vNext * dt;

    return nextState;
}

GimbalKinematicState GimbalMotionProfiler::filterGimbalVelocityStep(
    const GimbalKinematicState& currentState,
    double targetPanRate,
    double targetTiltRate,
    double targetRollRate,
    double dtSec) const noexcept {

    GimbalKinematicState nextState;
    nextState.pan = filterVelocityStep(currentState.pan, targetPanRate, dtSec, m_constraints.pan);
    nextState.tilt = filterVelocityStep(currentState.tilt, targetTiltRate, dtSec, m_constraints.tilt);
    nextState.roll = filterVelocityStep(currentState.roll, targetRollRate, dtSec, m_constraints.roll);
    nextState.timestamp = std::chrono::steady_clock::now();
    return nextState;
}

} // namespace PayloadHal
