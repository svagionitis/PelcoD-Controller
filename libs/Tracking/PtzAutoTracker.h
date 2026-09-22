#pragma once

/// @file PtzAutoTracker.h
/// @brief 3-Axis closed-loop automated PTZ tracking orchestrator.

#include "PidController.h"
#include <algorithm>
#include <cstdint>

namespace Tracking {

/// @class PtzAutoTracker
/// @brief Coordinates 3-axis closed-loop PID auto-tracking for Pelco-D PTZ heads.
/// @details Consumes visual target tracking telemetry (normalized boresight error, velocity, and size)
///          and produces discrete pan/tilt/zoom velocity commands with deadbands, predictive lead,
///          zoom-aware adaptive gain scheduling, and coasting deceleration.
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
    /// @brief Output commands for the Pelco-D pan/tilt/zoom head.
    struct TrackingCommand {
        int panDirection { 0 }; ///< -1 for Pan Left, 0 for Stop, +1 for Pan Right
        int panSpeed { 0 }; ///< Pelco-D discrete speed 0 to 63
        int tiltDirection { 0 }; ///< -1 for Tilt Down, 0 for Stop, +1 for Tilt Up
        int tiltSpeed { 0 }; ///< Pelco-D discrete speed 0 to 63
        int zoomDirection { 0 }; ///< -1 for Zoom Wide, 0 for Stop, +1 for Zoom Tele
        int zoomSpeed { 0 }; ///< Pelco-D discrete zoom speed (0 to 63)
        TrackingState state { TrackingState::Idle };
        bool shouldMove { false };
        bool shouldZoom { false };
    };

    /// @brief Construct the PTZ auto-tracker with default tuning for surveillance heads.
    PtzAutoTracker();

    /// @brief Compute pan/tilt/zoom motor commands from current tracking state.
    /// @param[in] errorX Normalized horizontal boresight error [-1.0 left to +1.0 right].
    /// @param[in] errorY Normalized vertical boresight error [-1.0 up to +1.0 down].
    /// @param[in] vx Estimated target horizontal velocity (normalized units / sec).
    /// @param[in] vy Estimated target vertical velocity (normalized units / sec).
    /// @param[in] isLocked True if target is actively acquired.
    /// @param[in] isCoasting True if target is temporarily occluded and coasting on prediction.
    /// @param[in] dt Elapsed time in seconds since previous update.
    /// @param[in] targetNormHeight Optional normalized target height [0.0 to 1.0] for auto-framing zoom.
    /// @param[in] currentZoom Current optical/sensor magnification factor (>= 1.0) for gain scheduling.
    /// @return Actionable TrackingCommand with discrete pan/tilt/zoom directions and speeds.
    TrackingCommand update(double errorX, double errorY, double vx, double vy, bool isLocked, bool isCoasting,
        double dt, double targetNormHeight = 0.0, double currentZoom = 1.0);

    /// @brief Compute pan/tilt/zoom motor commands from physical spherical angular errors.
    /// @param[in] errorAzimuthDeg Boresight azimuth error in degrees (positive = target right of boresight).
    /// @param[in] errorElevationDeg Boresight elevation error in degrees (positive = target above boresight).
    /// @param[in] omegaAzimuthDegPerSec Target azimuth angular velocity in degrees/second.
    /// @param[in] omegaElevationDegPerSec Target elevation angular velocity in degrees/second.
    /// @param[in] isLocked True if target is actively acquired.
    /// @param[in] isCoasting True if target is temporarily occluded and coasting on prediction.
    /// @param[in] dt Elapsed time in seconds since previous update.
    /// @param[in] targetNormHeight Optional normalized target height [0.0 to 1.0] for auto-framing zoom.
    /// @param[in] currentZoom Current optical/sensor magnification factor (>= 1.0) for gain scheduling.
    /// @return Actionable TrackingCommand with discrete pan/tilt/zoom directions and speeds.
    TrackingCommand updateAngular(double errorAzimuthDeg, double errorElevationDeg, double omegaAzimuthDegPerSec,
        double omegaElevationDegPerSec, bool isLocked, bool isCoasting, double dt, double targetNormHeight = 0.0,
        double currentZoom = 1.0);

    /// @brief Reset axis controllers, framing state, and set state to Idle.
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

    /// @brief Enable or disable 3-axis closed-loop automated optical zoom framing.
    void setAutoZoomEnabled(bool enabled) noexcept;

    /// @brief Configure target framing height and hysteresis deadband.
    /// @param[in] targetNormHeight Desired normalized target height (e.g. 0.20 for 20% viewport).
    /// @param[in] deadband Hysteresis deadband around target height (default 0.04).
    void setTargetFramingHeight(double targetNormHeight, double deadband = 0.04) noexcept;

    /// @brief Configure centering threshold to inhibit zoom-in when target is near frame edge.
    /// @param[in] threshold Max absolute errorX/errorY allowed for zoom-in (default 0.25).
    void setZoomCenteringThreshold(double threshold) noexcept;

    /// @brief Enable or disable predictive lead angle boresight deflection.
    void setPredictiveLeadEnabled(bool enabled) noexcept;

    /// @brief Configure predictive lead gain and clamp limit.
    /// @param[in] kLead Predictive lead gain in seconds (e.g. 0.15s forward projection).
    /// @param[in] maxLead Maximum allowable normalized boresight deflection offset (default 0.25).
    void setLeadGain(double kLead, double maxLead = 0.25) noexcept;

    /// @brief Enable or disable zoom-aware adaptive gain scheduling.
    void setZoomGainSchedulingEnabled(bool enabled) noexcept;

    /// @brief Enable or disable adaptive empirical latency compensation.
    void setAdaptiveLatencyEnabled(bool enabled) noexcept;

    /// @brief Set the empirically estimated plant latency in seconds.
    /// @param[in] latencySeconds Measured physical delay from LatencyEstimator or LatencyCalibrator.
    void setEstimatedLatencySeconds(double latencySeconds) noexcept;

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
    [[nodiscard]] bool isAutoZoomEnabled() const noexcept
    {
        return m_autoZoomEnabled;
    }
    [[nodiscard]] double getTargetFramingHeight() const noexcept
    {
        return m_targetFramingHeight;
    }
    [[nodiscard]] double getFramingDeadband() const noexcept
    {
        return m_framingDeadband;
    }
    [[nodiscard]] double getZoomCenteringThreshold() const noexcept
    {
        return m_zoomCenteringThreshold;
    }
    [[nodiscard]] bool isPredictiveLeadEnabled() const noexcept
    {
        return m_predictiveLeadEnabled;
    }
    [[nodiscard]] double getLeadGain() const noexcept
    {
        return m_leadGain;
    }
    [[nodiscard]] double getMaxLead() const noexcept
    {
        return m_maxLead;
    }
    [[nodiscard]] bool isZoomGainSchedulingEnabled() const noexcept
    {
        return m_zoomGainSchedulingEnabled;
    }
    [[nodiscard]] bool isAdaptiveLatencyEnabled() const noexcept
    {
        return m_adaptiveLatencyEnabled;
    }
    [[nodiscard]] double getEstimatedLatencySeconds() const noexcept
    {
        return m_estimatedLatencySeconds;
    }
    [[nodiscard]] double getLastLeadOffsetX() const noexcept
    {
        return m_lastLeadOffsetX;
    }
    [[nodiscard]] double getLastLeadOffsetY() const noexcept
    {
        return m_lastLeadOffsetY;
    }

private:
    PidController m_panPid;
    PidController m_tiltPid;

    double m_basePanKp { 45.0 };
    double m_basePanKi { 1.5 };
    double m_basePanKd { 4.0 };
    double m_basePanKff { 8.0 };

    double m_baseTiltKp { 35.0 };
    double m_baseTiltKi { 1.0 };
    double m_baseTiltKd { 3.0 };
    double m_baseTiltKff { 6.0 };

    int m_maxPanSpeed { 63 };
    int m_maxTiltSpeed { 63 };
    TrackingState m_state { TrackingState::Idle };

    // Closed-Loop Auto-Zoom (Target Framing)
    bool m_autoZoomEnabled { false };
    double m_targetFramingHeight { 0.20 };
    double m_framingDeadband { 0.04 };
    double m_zoomCenteringThreshold { 0.25 };
    int m_lastZoomDir { 0 };

    // Predictive Lead Angle Deflection & Adaptive Latency
    bool m_predictiveLeadEnabled { false };
    double m_leadGain { 0.15 };
    double m_maxLead { 0.25 };
    bool m_adaptiveLatencyEnabled { false };
    double m_estimatedLatencySeconds { 0.10 };
    double m_lastLeadOffsetX { 0.0 };
    double m_lastLeadOffsetY { 0.0 };

    // Zoom-Aware Gain Scheduling
    bool m_zoomGainSchedulingEnabled { true };

    double m_lostDuration { 0.0 };
    static constexpr double MAX_COAST_DECEL_TIME { 0.6 }; // Seconds to decelerate after lock lost
    int m_lastPanSpeed { 0 };
    int m_lastTiltSpeed { 0 };
    int m_lastPanDir { 0 };
    int m_lastTiltDir { 0 };
};

} // namespace Tracking
