#pragma once

/// @file PidController.h
/// @brief Discrete PID controller with deadband, velocity feedforward, and anti-windup clamping.

#include <algorithm>
#include <cmath>

namespace Tracking {

/// @class PidController
/// @brief Proportional-Integral-Derivative (PID) controller optimized for closed-loop motion control.
/// @details Supports configurable deadband to prevent motor jitter, derivative low-pass filtering,
///          conditional integration anti-windup, and velocity feedforward injection.
class PidController {
public:
    /// @brief Construct a PID controller with specified gains and limits.
    /// @param[in] kp Proportional gain.
    /// @param[in] ki Integral gain.
    /// @param[in] kd Derivative gain.
    /// @param[in] kff Velocity feedforward gain.
    /// @param[in] deadband Threshold below which error is treated as zero.
    /// @param[in] minOutput Minimum allowable controller output.
    /// @param[in] maxOutput Maximum allowable controller output.
    PidController(double kp = 1.0, double ki = 0.0, double kd = 0.0, double kff = 0.0, double deadband = 0.0,
        double minOutput = -63.0, double maxOutput = 63.0);

    /// @brief Compute the controller output for a single time step.
    /// @param[in] error Difference between setpoint and process variable (setpoint - current).
    /// @param[in] dt Elapsed time in seconds since previous update.
    /// @param[in] feedforwardVelocity Estimated target velocity for feedforward injection.
    /// @return Clamped controller command output.
    double update(double error, double dt, double feedforwardVelocity = 0.0);

    /// @brief Reset integrator accumulation and derivative history to zero.
    void reset() noexcept;

    /// @brief Configure PID and feedforward gains.
    /// @param[in] kp Proportional gain.
    /// @param[in] ki Integral gain.
    /// @param[in] kd Derivative gain.
    /// @param[in] kff Velocity feedforward gain.
    void setGains(double kp, double ki, double kd, double kff = 0.0) noexcept;

    /// @brief Configure the deadband threshold.
    /// @param[in] deadband Error threshold below which output is suppressed to zero.
    void setDeadband(double deadband) noexcept;

    /// @brief Configure output saturation limits.
    /// @param[in] minOutput Lower output clamp boundary.
    /// @param[in] maxOutput Upper output clamp boundary.
    void setOutputLimits(double minOutput, double maxOutput) noexcept;

    /// @brief Configure the derivative low-pass filter smoothing factor (0.0 to 1.0).
    /// @param[in] alpha Smoothing factor (1.0 = unfiltered, lower = smoother).
    void setDerivativeFilterAlpha(double alpha) noexcept;

    [[nodiscard]] double getKp() const noexcept
    {
        return m_kp;
    }
    [[nodiscard]] double getKi() const noexcept
    {
        return m_ki;
    }
    [[nodiscard]] double getKd() const noexcept
    {
        return m_kd;
    }
    [[nodiscard]] double getKff() const noexcept
    {
        return m_kff;
    }
    [[nodiscard]] double getDeadband() const noexcept
    {
        return m_deadband;
    }
    [[nodiscard]] double getMinOutput() const noexcept
    {
        return m_minOutput;
    }
    [[nodiscard]] double getMaxOutput() const noexcept
    {
        return m_maxOutput;
    }
    [[nodiscard]] double getIntegral() const noexcept
    {
        return m_integral;
    }

private:
    double m_kp { 1.0 };
    double m_ki { 0.0 };
    double m_kd { 0.0 };
    double m_kff { 0.0 };
    double m_deadband { 0.0 };
    double m_minOutput { -63.0 };
    double m_maxOutput { 63.0 };
    double m_derivativeFilterAlpha { 0.8 };

    double m_integral { 0.0 };
    double m_prevError { 0.0 };
    double m_filteredDerivative { 0.0 };
    bool m_firstUpdate { true };
};

} // namespace Tracking
