#pragma once

/// @file KinematicsSimulator.h
/// @brief Physical PTZ motion dynamics, velocity profiling, and angular slewing simulation.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace PelcoD {

/// @struct KinematicsConfig
/// @brief Configuration settings controlling motion dynamics, limits, and rates.
struct KinematicsConfig {
    bool enabled { false };
    double maxPanSpeedDegPerSec { 60.0 };
    double maxTiltSpeedDegPerSec { 30.0 };
    double panAccelerationDegPerSec2 { 180.0 };
    double tiltAccelerationDegPerSec2 { 90.0 };
    double zoomTransitTimeSeconds { 2.0 };
    double minTiltDeg { 0.0 };
    double maxTiltDeg { 90.0 };
};

/// @enum MotionMode
/// @brief Current kinematics profile mode.
enum class MotionMode {
    Stopped,
    ManualVelocity,
    SlewingToTarget
};

/// @class KinematicsSimulator
/// @brief Real-time physics engine simulating realistic PTZ pan/tilt/zoom kinematics.
class KinematicsSimulator {
public:
    KinematicsSimulator();
    ~KinematicsSimulator() = default;

    // Non-copyable, non-movable
    KinematicsSimulator(const KinematicsSimulator&) = delete;
    KinematicsSimulator& operator=(const KinematicsSimulator&) = delete;
    KinematicsSimulator(KinematicsSimulator&&) = delete;
    KinematicsSimulator& operator=(KinematicsSimulator&&) = delete;

    /// @brief Update simulation configuration.
    /// @param[in] config Kinematics settings.
    void setConfig(const KinematicsConfig& config);

    /// @brief Get current simulation configuration.
    /// @return Active KinematicsConfig.
    [[nodiscard]] KinematicsConfig getConfig() const;

    /// @brief Immediately set absolute coordinates bypassing kinematics smoothing.
    /// @param[in] panDeg Pan angle in degrees [0, 360).
    /// @param[in] tiltDeg Tilt angle in degrees.
    /// @param[in] zoom Raw zoom position [1000, 65535].
    void setPositionImmediate(double panDeg, double tiltDeg, double zoom);

    /// @brief Command directional manual motion.
    /// @param[in] panFraction Normalized pan speed [-1.0..+1.0] (negative = left, positive = right).
    /// @param[in] tiltFraction Normalized tilt speed [-1.0..+1.0] (negative = down, positive = up).
    /// @param[in] zoomFraction Normalized zoom speed [-1.0..+1.0] (negative = wide, positive = tele).
    void setDirectionalMotion(double panFraction, double tiltFraction, double zoomFraction = 0.0);

    /// @brief Command camera head to slew toward an absolute target coordinate.
    /// @param[in] targetPanDeg Desired target pan in degrees [0, 360).
    /// @param[in] targetTiltDeg Desired target tilt in degrees.
    void slewTo(double targetPanDeg, double targetTiltDeg);

    /// @brief Command motorized lens to slew to a target zoom position.
    /// @param[in] targetZoom Target optical zoom value [1000, 65535].
    void slewZoomTo(double targetZoom);

    /// @brief Decelerate all motion axes to a complete stop.
    void stop();

    /// @brief Advance physics equations up to the specified timestamp.
    /// @param[in] now Current monotonic clock time (defaults to now).
    void update(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    /// @brief Check if camera head or lens is actively moving.
    /// @return true if non-zero velocity or slewing in progress.
    [[nodiscard]] bool isMoving() const;

    /// @brief Get current pan angle in degrees [0.0, 360.0).
    /// @return Pan position in degrees.
    [[nodiscard]] double currentPanDeg() const;

    /// @brief Get current tilt angle in degrees.
    /// @return Tilt position in degrees.
    [[nodiscard]] double currentTiltDeg() const;

    /// @brief Get current optical zoom position.
    /// @return Zoom value [1000, 65535].
    [[nodiscard]] double currentZoom() const;

    /// @brief Get current pan position converted to integer centidegrees (0..35999).
    /// @return Pan in centidegrees.
    [[nodiscard]] std::uint16_t currentPanCentidegrees() const;

    /// @brief Get current tilt position converted to integer centidegrees.
    /// @return Tilt in centidegrees.
    [[nodiscard]] std::uint16_t currentTiltCentidegrees() const;

    /// @brief Get current zoom position rounded to integer [1000..65535].
    /// @return Zoom position integer.
    [[nodiscard]] std::uint16_t currentZoomInt() const;

    /// @brief Wrap an arbitrary pan angle into standard [0.0, 360.0) degrees.
    [[nodiscard]] static double normalizePanDeg(double deg) noexcept;

    /// @brief Compute shortest signed angular difference between two azimuth angles in range [-180, +180).
    [[nodiscard]] static double shortestAngularDelta(double fromDeg, double toDeg) noexcept;

private:
    mutable std::mutex m_mutex;
    KinematicsConfig m_config {};

    double m_currentPanDeg { 0.0 };
    double m_currentTiltDeg { 0.0 };
    double m_currentZoom { 1000.0 };

    double m_panVelocity { 0.0 };
    double m_tiltVelocity { 0.0 };
    double m_zoomVelocity { 0.0 };

    MotionMode m_mode { MotionMode::Stopped };

    double m_targetPanVelocity { 0.0 };
    double m_targetTiltVelocity { 0.0 };
    double m_targetZoomVelocity { 0.0 };

    double m_targetPanDeg { 0.0 };
    double m_targetTiltDeg { 0.0 };
    double m_targetZoom { 1000.0 };

    std::chrono::steady_clock::time_point m_lastUpdateTime {};
    bool m_hasTimestamp { false };
};

} // namespace PelcoD
