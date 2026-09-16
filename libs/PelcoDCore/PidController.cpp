/// @file PidController.cpp
/// @brief Implementation of discrete PID controller with deadband and anti-windup.

#include "PidController.h"

namespace PelcoD {

PidController::PidController(
    double kp, double ki, double kd, double kff, double deadband, double minOutput, double maxOutput)
    : m_kp(kp)
    , m_ki(ki)
    , m_kd(kd)
    , m_kff(kff)
    , m_deadband(std::max(0.0, deadband))
    , m_minOutput(std::min(minOutput, maxOutput))
    , m_maxOutput(std::max(minOutput, maxOutput))
{
}

void PidController::reset() noexcept
{
    m_integral = 0.0;
    m_prevError = 0.0;
    m_filteredDerivative = 0.0;
    m_firstUpdate = true;
}

void PidController::setGains(double kp, double ki, double kd, double kff) noexcept
{
    m_kp = kp;
    m_ki = ki;
    m_kd = kd;
    m_kff = kff;
}

void PidController::setDeadband(double deadband) noexcept
{
    m_deadband = std::max(0.0, deadband);
}

void PidController::setOutputLimits(double minOutput, double maxOutput) noexcept
{
    m_minOutput = std::min(minOutput, maxOutput);
    m_maxOutput = std::max(minOutput, maxOutput);
}

void PidController::setDerivativeFilterAlpha(double alpha) noexcept
{
    m_derivativeFilterAlpha = std::clamp(alpha, 0.0, 1.0);
}

double PidController::update(double error, double dt, double feedforwardVelocity)
{
    if (dt <= 0.0) {
        return 0.0;
    }

    // Proportional term with deadband
    double pTerm = 0.0;
    if (std::abs(error) > m_deadband) {
        pTerm = m_kp * error;
    }

    // Derivative term with low-pass filter
    double dTerm = 0.0;
    if (!m_firstUpdate) {
        double dRaw = (error - m_prevError) / dt;
        m_filteredDerivative = m_derivativeFilterAlpha * dRaw + (1.0 - m_derivativeFilterAlpha) * m_filteredDerivative;
        if (std::abs(error) > m_deadband) {
            dTerm = m_kd * m_filteredDerivative;
        }
    } else {
        m_firstUpdate = false;
        m_filteredDerivative = 0.0;
    }
    m_prevError = error;

    // Velocity feedforward term
    double ffTerm = m_kff * feedforwardVelocity;

    // If within deadband and no feedforward velocity, suppress output to zero
    if (std::abs(error) <= m_deadband && std::abs(ffTerm) < 1e-9) {
        return 0.0;
    }

    // Integral term with conditional anti-windup
    double candidateIntegral = m_integral + (std::abs(error) > m_deadband ? error * dt : 0.0);
    double candidateOutput = pTerm + (m_ki * candidateIntegral) + dTerm + ffTerm;

    if (candidateOutput > m_maxOutput) {
        // Only integrate if error is negative (moving back towards linear range)
        if (error < 0.0) {
            m_integral = candidateIntegral;
        }
        return m_maxOutput;
    }

    if (candidateOutput < m_minOutput) {
        // Only integrate if error is positive (moving back towards linear range)
        if (error > 0.0) {
            m_integral = candidateIntegral;
        }
        return m_minOutput;
    }

    m_integral = candidateIntegral;
    return candidateOutput;
}

} // namespace PelcoD
