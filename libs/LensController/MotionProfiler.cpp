/// @file MotionProfiler.cpp
#include "MotionProfiler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "LensTypes.h"
#include "MotionControlTypes.h"

namespace LensControl {

MotionProfiler::MotionProfiler(const SCurveConstraints& constraints) noexcept
    : m_limits { constraints }
{
}

bool MotionProfiler::setConstraints(const SCurveConstraints& constraints) noexcept
{
    if (!constraints.isValid()) {
        return false;
    }
    m_limits = constraints;
    return true;
}

const SCurveConstraints& MotionProfiler::constraints() const noexcept
{
    return m_limits;
}

bool MotionProfiler::planTrajectory(float startPos, float targetPos) noexcept
{
    if (!m_limits.isValid()) {
        return false;
    }

    m_startPos = startPos;
    m_targetPos = targetPos;

    const float delta = targetPos - startPos;
    if (std::abs(delta) < 1e-6F) {
        m_totalDistance = 0.0F;
        m_direction = 1.0F;
        m_totalDuration = 0.0F;
        m_currentTime = 0.0F;
        m_isComplete = true;
        for (std::size_t i = 0U; i < 7U; ++i) {
            m_phaseDuration[i] = 0.0F;
            m_phaseStartTime[i] = 0.0F;
            m_phaseStartPos[i] = 0.0F;
            m_phaseStartVel[i] = 0.0F;
            m_phaseStartAcc[i] = 0.0F;
            m_phaseJerk[i] = 0.0F;
        }
        return true;
    }

    m_direction = (delta >= 0.0F) ? 1.0F : -1.0F;
    m_totalDistance = std::abs(delta);
    m_currentTime = 0.0F;
    m_isComplete = false;

    calculateTiming();
    return true;
}

float MotionProfiler::computeDistanceForV(float v) const noexcept
{
    if (v <= 0.0F) {
        return 0.0F;
    }

    const float aMax = m_limits.maxAcceleration.value();
    const float dMax = m_limits.maxDeceleration.value();
    const float jMax = m_limits.maxJerk.value();

    float tj1 = 0.0F;
    float ta = 0.0F;
    const float deltaVj1 = (aMax * aMax) / jMax;
    if (v < deltaVj1) {
        tj1 = std::sqrt(v / jMax);
        ta = 0.0F;
    } else {
        tj1 = aMax / jMax;
        ta = (v - deltaVj1) / aMax;
    }
    const float sAcc = v * (tj1 + (0.5F * ta));

    float tj2 = 0.0F;
    float td = 0.0F;
    const float deltaVj2 = (dMax * dMax) / jMax;
    if (v < deltaVj2) {
        tj2 = std::sqrt(v / jMax);
        td = 0.0F;
    } else {
        tj2 = dMax / jMax;
        td = (v - deltaVj2) / dMax;
    }
    const float sDec = v * (tj2 + (0.5F * td));

    return sAcc + sDec;
}

void MotionProfiler::calculateTiming() noexcept
{
    const float vMax = m_limits.maxVelocity.value();
    const float aMax = m_limits.maxAcceleration.value();
    const float dMax = m_limits.maxDeceleration.value();
    const float jMax = m_limits.maxJerk.value();

    const float distAtVmax = computeDistanceForV(vMax);

    float vPeak = vMax;
    float tv = 0.0F;

    if (m_totalDistance >= distAtVmax) {
        // Full 7-phase profile with cruise phase
        tv = (m_totalDistance - distAtVmax) / vMax;
        vPeak = vMax;
    } else {
        // Short stroke: no cruise phase
        tv = 0.0F;
        float lowV = 0.0F;
        float highV = vMax;
        for (std::size_t iter = 0U; iter < 32U; ++iter) {
            const float midV = 0.5F * (lowV + highV);
            if (computeDistanceForV(midV) < m_totalDistance) {
                lowV = midV;
            } else {
                highV = midV;
            }
        }
        vPeak = 0.5F * (lowV + highV);
    }

    float tj1 = 0.0F;
    float ta = 0.0F;
    const float deltaVj1 = (aMax * aMax) / jMax;
    if (vPeak < deltaVj1) {
        tj1 = std::sqrt(vPeak / jMax);
        ta = 0.0F;
    } else {
        tj1 = aMax / jMax;
        ta = (vPeak - deltaVj1) / aMax;
    }

    float tj2 = 0.0F;
    float td = 0.0F;
    const float deltaVj2 = (dMax * dMax) / jMax;
    if (vPeak < deltaVj2) {
        tj2 = std::sqrt(vPeak / jMax);
        td = 0.0F;
    } else {
        tj2 = dMax / jMax;
        td = (vPeak - deltaVj2) / dMax;
    }

    m_phaseDuration[0] = tj1;
    m_phaseDuration[1] = ta;
    m_phaseDuration[2] = tj1;
    m_phaseDuration[3] = tv;
    m_phaseDuration[4] = tj2;
    m_phaseDuration[5] = td;
    m_phaseDuration[6] = tj2;

    m_phaseJerk[0] = jMax;
    m_phaseJerk[1] = 0.0F;
    m_phaseJerk[2] = -jMax;
    m_phaseJerk[3] = 0.0F;
    m_phaseJerk[4] = -jMax;
    m_phaseJerk[5] = 0.0F;
    m_phaseJerk[6] = jMax;

    m_phaseStartTime[0] = 0.0F;
    m_phaseStartPos[0] = 0.0F;
    m_phaseStartVel[0] = 0.0F;
    m_phaseStartAcc[0] = 0.0F;

    for (std::size_t i = 0U; i < 6U; ++i) {
        const float tau = m_phaseDuration[i];
        const float j = m_phaseJerk[i];
        m_phaseStartTime[i + 1U] = m_phaseStartTime[i] + tau;
        m_phaseStartAcc[i + 1U] = m_phaseStartAcc[i] + (j * tau);
        m_phaseStartVel[i + 1U] = m_phaseStartVel[i] + (m_phaseStartAcc[i] * tau) + (0.5F * j * tau * tau);
        m_phaseStartPos[i + 1U] = m_phaseStartPos[i] + (m_phaseStartVel[i] * tau)
            + (0.5F * m_phaseStartAcc[i] * tau * tau) + ((1.0F / 6.0F) * j * tau * tau * tau);
    }

    m_totalDuration = m_phaseStartTime[6] + m_phaseDuration[6];
}

TrajectorySample MotionProfiler::samplePhase(float tau, std::size_t phaseIdx) const noexcept
{
    const float j = m_phaseJerk[phaseIdx];
    const float a = m_phaseStartAcc[phaseIdx] + (j * tau);
    const float v = m_phaseStartVel[phaseIdx] + (m_phaseStartAcc[phaseIdx] * tau) + (0.5F * j * tau * tau);
    const float s = m_phaseStartPos[phaseIdx] + (m_phaseStartVel[phaseIdx] * tau)
        + (0.5F * m_phaseStartAcc[phaseIdx] * tau * tau) + ((1.0F / 6.0F) * j * tau * tau * tau);

    TrajectorySample sample;
    sample.position = (m_direction >= 0.0F) ? (m_startPos + s) : (m_startPos - s);
    sample.velocity = (m_direction >= 0.0F) ? v : -v;
    sample.acceleration = (m_direction >= 0.0F) ? a : -a;
    sample.jerk = (m_direction >= 0.0F) ? j : -j;
    sample.timeElapsed = m_currentTime;
    sample.phase = static_cast<ProfilePhase>(phaseIdx + 1U);

    return sample;
}

TrajectorySample MotionProfiler::evaluateAt(float t) const noexcept
{
    TrajectorySample sample;

    if (t <= 0.0F || m_totalDuration <= 0.0F) {
        sample.position = m_startPos;
        sample.velocity = 0.0F;
        sample.acceleration = 0.0F;
        sample.jerk = 0.0F;
        sample.timeElapsed = 0.0F;
        sample.phase = ProfilePhase::Idle;
        return sample;
    }

    if (t >= m_totalDuration) {
        sample.position = m_targetPos;
        sample.velocity = 0.0F;
        sample.acceleration = 0.0F;
        sample.jerk = 0.0F;
        sample.timeElapsed = m_totalDuration;
        sample.phase = ProfilePhase::TargetReached;
        return sample;
    }

    // Determine active phase
    for (std::size_t i = 0U; i < 7U; ++i) {
        const float pStart = m_phaseStartTime[i];
        const float pEnd = pStart + m_phaseDuration[i];
        if (t <= pEnd || i == 6U) {
            const float tau = std::clamp(t - pStart, 0.0F, m_phaseDuration[i]);
            sample = samplePhase(tau, i);
            sample.timeElapsed = t;
            return sample;
        }
    }

    sample.position = m_targetPos;
    sample.phase = ProfilePhase::TargetReached;
    return sample;
}

TrajectorySample MotionProfiler::advance(DeltaTimeSec dt) noexcept
{
    m_currentTime += dt.value();
    if (m_currentTime >= m_totalDuration) {
        m_currentTime = m_totalDuration;
        m_isComplete = true;
    }
    return evaluateAt(m_currentTime);
}

bool MotionProfiler::isComplete() const noexcept
{
    return m_isComplete;
}

float MotionProfiler::totalDuration() const noexcept
{
    return m_totalDuration;
}

float MotionProfiler::targetPosition() const noexcept
{
    return m_targetPos;
}

void MotionProfiler::cancel() noexcept
{
    m_isComplete = true;
    m_currentTime = m_totalDuration;
}

} // namespace LensControl
