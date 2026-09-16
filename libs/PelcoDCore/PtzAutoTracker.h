#pragma once

/// @file PtzAutoTracker.h
/// @brief Dual-axis closed-loop automated PTZ tracking orchestrator.

#include "PidController.h"
#include <algorithm>
#include <cstdint>

namespace PelcoD {

/// @class PtzAutoTracker
/// @brief Coordinates dual-axis closed-loop PID auto-tracking for Pelco-D PTZ heads.
/// @details Consumes visual target tracking telemetry (normalized boresight error and velocity)
///          and produces discrete pan/tilt velocity commands with deadbands and coasting deceleration.
class PtzAutoTracker {
public:
    /// @enum TrackingState
    /// @brief Operating state of the auto-tracking controller.
    enum class TrackingState {
        Idle, ///< No target assigned or tracking disabled
        Tracking, ///< Actively tracking target with positive optical lock
        Coasting, ///< Target visually occluded; moving on predictive Kalman trajectory
        Lost ///< Target lost; camera brought to a graceful stop
    };

    /// @struct TrackingCommand
    /// @brief Output commands for the Pelco-D pan/tilt head.
    struct TrackingCommand {
        int panDirection { 0 }; ///< -1 for Pan Left, 0 for Stop, +1 for Pan Right
        int panSpeed { 0 }; ///< Pelco-D discrete speed 0 to 63
        int tiltDirection { 0 }; ///< -1 for Tilt Down, 0 for Stop, +1 for Tilt Up
        int tiltSpeed { 0 }; ///< Pelco-D discrete speed 0 to 63
        TrackingState state { TrackingState::Idle };
        bool shouldMove { false };
    };

    /// @brief Construct the PTZ auto-tracker with default tuning for surveillance heads.
    PtzAutoTracker();

    /// @brief Compute pan/tilt motor commands from current tracking state.
    /// @param[in] errorX Normalized horizontal boresight error [-1.0 left to +1.0 right].
    /// @param[in] errorY Normalized vertical boresight error [-1.0 up to +1.0 down].
    /// @param[in] vx Estimated target horizontal velocity (normalized units / sec).
    /// @param[in] vy Estimated target vertical velocity (normalized units / sec).
    /// @param[in] isLocked True if target is actively acquired.
    /// @param[in] isCoasting True if target is temporarily occluded and coasting on prediction.
    /// @param[in] dt Elapsed time in seconds since previous update.
    /// @return Actionable TrackingCommand with discrete pan/tilt directions and speeds.
    TrackingCommand update(
        double errorX, double errorY, double vx, double vy, bool isLocked, bool isCoasting, double dt);

    /// @brief Reset both axis controllers and set state to Idle.
    void reset() noexcept;

    /// @brief Configure PID and feedforward gains for the Pan (Azimuth) axis.
    void setPanGains(double kp, double ki, double kd, double kff = 0.0) noexcept;

    /// @brief Configure PID and feedforward gains for the Tilt (Elevation) axis.
    void setTiltGains(double kp, double ki, double kd, double kff = 0.0) noexcept;

    /// @brief Configure deadband thresholds for Pan and Tilt axes.
    /// @param[in] panDeadband Normalized threshold below which pan commands are zeroed.
    /// @param[in] tiltDeadband Normalized threshold below which tilt commands are zeroed.
    void setDeadbands(double panDeadband, double tiltDeadband) noexcept;

    /// @brief Configure maximum allowable speed limits for Pan and Tilt.
    /// @param[in] maxPan Maximum Pelco-D pan speed (1 to 63).
    /// @param[in] maxTilt Maximum Pelco-D tilt speed (1 to 63).
    void setMaxSpeeds(int maxPan, int maxTilt) noexcept;

    [[nodiscard]] TrackingState getState() const noexcept
    {
        return m_state;
    }
    [[nodiscard]] const PidController& getPanPid() const noexcept
    {
        return m_panPid;
    }
    [[nodiscard]] const PidController& getTiltPid() const noexcept
    {
        return m_tiltPid;
    }

private:
    PidController m_panPid;
    PidController m_tiltPid;
    int m_maxPanSpeed { 63 };
    int m_maxTiltSpeed { 63 };
    TrackingState m_state { TrackingState::Idle };

    double m_lostDuration { 0.0 };
    static constexpr double MAX_COAST_DECEL_TIME { 0.6 }; // Seconds to decelerate after lock lost
    int m_lastPanSpeed { 0 };
    int m_lastTiltSpeed { 0 };
    int m_lastPanDir { 0 };
    int m_lastTiltDir { 0 };
};

} // namespace PelcoD
