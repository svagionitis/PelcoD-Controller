#pragma once

/// @file ActuatorPidController.h
/// @brief Closed-loop actuator position controller with anti-windup and stiction inversion.

#include <cstdint>

#include "MotionControlTypes.h"

namespace LensControl {

/// @class ActuatorPidController
/// @brief Discrete PID controller with conditional integration and static friction boost.
class ActuatorPidController final {
public:
    /// @brief Constructs an actuator PID controller with specified or default gains.
    /// @param gains PID parameters and thresholds.
    explicit ActuatorPidController(const PidGains& gains = PidGains { }) noexcept;

    /// @brief Default destructor.
    ~ActuatorPidController() noexcept = default;

    /// @brief Resets integrator, filter state, and settling counters.
    void reset() noexcept;

    /// @brief Configures PID parameters and limits.
    /// @param gains Updated PID gains.
    void setGains(const PidGains& gains) noexcept;

    /// @brief Returns current controller gains.
    /// @return Active gains reference.
    [[nodiscard]] const PidGains& gains() const noexcept;

    /// @brief Computes control effort for one discrete time step.
    /// @param setpoint Desired target position.
    /// @param actual Current measured encoder feedback.
    /// @param dt Time elapsed since previous cycle.
    /// @return Clamped motor control effort.
    [[nodiscard]] float compute(float setpoint, float actual, DeltaTimeSec dt) noexcept;

    /// @brief Tests if actuator is within position tolerance of setpoint.
    /// @return True if settled in position.
    [[nodiscard]] bool isInPosition() const noexcept;

    /// @brief Returns last proportional contribution.
    /// @return Proportional term magnitude.
    [[nodiscard]] float lastPTerm() const noexcept;

    /// @brief Returns last integral contribution.
    /// @return Integral term magnitude.
    [[nodiscard]] float lastITerm() const noexcept;

    /// @brief Returns last derivative contribution.
    /// @return Derivative term magnitude.
    [[nodiscard]] float lastDTerm() const noexcept;

    /// @brief Returns last stiction breakaway boost contribution.
    /// @return Stiction term magnitude.
    [[nodiscard]] float lastStictionTerm() const noexcept;

private:
    PidGains m_gains { };
    float m_prevActual { 0.0F };
    float m_filteredDerivative { 0.0F };
    float m_integrator { 0.0F };
    float m_lastP { 0.0F };
    float m_lastI { 0.0F };
    float m_lastD { 0.0F };
    float m_lastStiction { 0.0F };
    std::uint32_t m_settledCycles { 0U };
    bool m_isInPosition { false };
    bool m_isInitialized { false };
};

} // namespace LensControl
