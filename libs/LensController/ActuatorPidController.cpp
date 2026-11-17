/// @file ActuatorPidController.cpp
#include "ActuatorPidController.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "MotionControlTypes.h"

namespace LensControl {

ActuatorPidController::ActuatorPidController(const PidGains& gains) noexcept
    : m_gains { gains }
{
}

void ActuatorPidController::reset() noexcept
{
    m_prevActual = 0.0F;
    m_filteredDerivative = 0.0F;
    m_integrator = 0.0F;
    m_lastP = 0.0F;
    m_lastI = 0.0F;
    m_lastD = 0.0F;
    m_lastStiction = 0.0F;
    m_settledCycles = 0U;
    m_isInPosition = false;
    m_isInitialized = false;
}

void ActuatorPidController::setGains(const PidGains& gains) noexcept
{
    m_gains = gains;
}

const PidGains& ActuatorPidController::gains() const noexcept
{
    return m_gains;
}

float ActuatorPidController::compute(float setpoint, float actual, DeltaTimeSec dt) noexcept
{
    const float error = setpoint - actual;
    const float dtVal = dt.value();

    if (!m_isInitialized) {
        m_prevActual = actual;
        m_filteredDerivative = 0.0F;
        m_integrator = 0.0F;
        m_isInitialized = true;
    }

    // Velocity estimation from actual feedback
    const float deltaActual = actual - m_prevActual;
    const float actualVelocity = deltaActual / dtVal;
    m_prevActual = actual;

    // 1. Proportional term
    m_lastP = m_gains.kp * error;

    // 2. Derivative on measurement (with low-pass filter)
    const float rawD = -m_gains.kd * actualVelocity;
    const float beta = std::clamp(m_gains.derivativeFilterAlpha, 0.0F, 1.0F);
    m_filteredDerivative = (beta * rawD) + ((1.0F - beta) * m_filteredDerivative);
    m_lastD = m_filteredDerivative;

    // In-position check
    const bool withinDeadband = std::abs(error) <= m_gains.deadband;
    const bool velocityLow = std::abs(actualVelocity) <= m_gains.movingVelocityThreshold;

    if (withinDeadband && velocityLow) {
        ++m_settledCycles;
        if (m_settledCycles >= m_gains.settlingCyclesRequired) {
            m_isInPosition = true;
            m_lastStiction = 0.0F;
            return 0.0F;
        }
    } else {
        m_settledCycles = 0U;
        m_isInPosition = false;
    }

    // 3. Stiction Breakaway Boost
    if (!withinDeadband && velocityLow) {
        m_lastStiction = (error > 0.0F) ? m_gains.stictionBreakaway : -m_gains.stictionBreakaway;
    } else {
        m_lastStiction = 0.0F;
    }

    // 4. Integral term with conditional anti-windup clamping
    const float candIntegrator = m_integrator + (m_gains.ki * error * dtVal);
    const float unconstrainedEffort = m_lastP + candIntegrator + m_lastD + m_lastStiction;

    const bool saturatedHigh = (unconstrainedEffort > m_gains.outputMax) && (error > 0.0F);
    const bool saturatedLow = (unconstrainedEffort < m_gains.outputMin) && (error < 0.0F);

    if (!saturatedHigh && !saturatedLow) {
        m_integrator = std::clamp(candIntegrator, -m_gains.integralMax, m_gains.integralMax);
    }
    m_lastI = m_integrator;

    // Total effort
    const float totalEffort = m_lastP + m_lastI + m_lastD + m_lastStiction;
    return std::clamp(totalEffort, m_gains.outputMin, m_gains.outputMax);
}

bool ActuatorPidController::isInPosition() const noexcept
{
    return m_isInPosition;
}

float ActuatorPidController::lastPTerm() const noexcept
{
    return m_lastP;
}

float ActuatorPidController::lastITerm() const noexcept
{
    return m_lastI;
}

float ActuatorPidController::lastDTerm() const noexcept
{
    return m_lastD;
}

float ActuatorPidController::lastStictionTerm() const noexcept
{
    return m_lastStiction;
}

} // namespace LensControl
