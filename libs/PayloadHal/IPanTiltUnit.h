#pragma once

/// @file IPanTiltUnit.h
/// @brief Polymorphic interface for pan-tilt units, pedestals, and gyro-stabilized gimbals.

#include "IDevice.h"
#include "PayloadTypes.h"

#include <functional>
#include <string>

namespace PayloadHal {

/// @class IPanTiltUnit
/// @brief Unified interface abstracting rate control, absolute positioning, limits,
///        inertial gyro stabilization, and telemetry for both 2-axis and 3-axis mounts.
class IPanTiltUnit : public virtual IDevice {
public:
    ~IPanTiltUnit() override = default;

    // --- Motion & Positioning ---

    /// @brief Drives pan and tilt axes at physical angular velocities.
    /// @param[in] panDegPerSec Azimuth velocity in degrees/second (positive = Right, negative = Left).
    /// @param[in] tiltDegPerSec Elevation velocity in degrees/second (positive = Up, negative = Down).
    /// @return True if rate command was accepted.
    virtual bool setRate(double panDegPerSec, double tiltDegPerSec) = 0;

    /// @brief Drives pan and tilt axes using normalized velocity ratios.
    /// @param[in] panVel Normalized velocity [-1.0 .. +1.0] (-1.0 = Max Left, +1.0 = Max Right).
    /// @param[in] tiltVel Normalized velocity [-1.0 .. +1.0] (-1.0 = Max Down, +1.0 = Max Up).
    /// @return True if velocity command was dispatched.
    virtual bool setNormalizedVelocity(float panVel, float tiltVel) = 0;

    /// @brief Commands the gimbal to slew to absolute azimuth and elevation angles.
    /// @param[in] panDeg Target azimuth angle in degrees (-180.0° .. +180.0° or 0.0° .. 360.0°).
    /// @param[in] tiltDeg Target elevation angle in degrees (-90.0° .. +90.0°).
    /// @return True if target angle was accepted within limits.
    virtual bool setAbsoluteAngles(double panDeg, double tiltDeg) = 0;

    /// @brief Applies a relative angular delta offset to the current orientation.
    /// @param[in] deltaPanDeg Azimuth delta in degrees.
    /// @param[in] deltaTiltDeg Elevation delta in degrees.
    /// @return True if nudge command was accepted.
    virtual bool setRelativeNudge(double deltaPanDeg, double deltaTiltDeg) = 0;

    /// @brief Stops all pan and tilt axis motion immediately with deceleration profile.
    /// @return True if stop command was accepted.
    virtual bool stopMotion() = 0;

    // --- Gyro Stabilization & Modes ---

    /// @brief Indicates whether this unit features active inertial gyro stabilization.
    [[nodiscard]] virtual bool supportsStabilization() const noexcept = 0;

    /// @brief Configures active stabilization mode.
    /// @param[in] mode Desired stabilization profile.
    /// @return True if mode was applied, false if unsupported by hardware.
    virtual bool setStabilizationMode(StabilizationMode mode) = 0;

    /// @brief Retrieves the active stabilization mode.
    [[nodiscard]] virtual StabilizationMode stabilizationMode() const noexcept = 0;

    /// @brief Triggers gyro bias drift zeroing / calibration routine while stationary.
    /// @return True if calibration was initiated.
    virtual bool zeroGyroDrift() = 0;

    // --- Angular Limits & Presets ---

    /// @brief Queries mechanical / software angular travel limits.
    /// @param[out] minPan Minimum azimuth travel angle in degrees.
    /// @param[out] maxPan Maximum azimuth travel angle in degrees.
    /// @param[out] minTilt Minimum elevation travel angle in degrees.
    /// @param[out] maxTilt Maximum elevation travel angle in degrees.
    /// @return True if limits are known and populated.
    virtual bool getLimits(double& minPan, double& maxPan, double& minTilt, double& maxTilt) const = 0;

    /// @brief Stores the current position as a numbered preset.
    /// @param[in] presetId Numbered preset identifier (1-based).
    /// @param[in] name Optional human-readable designation.
    /// @return True if preset was saved to non-volatile memory.
    virtual bool savePreset(std::uint8_t presetId, const std::string& name = "") = 0;

    /// @brief Commands the gimbal to slew to a stored preset position.
    /// @param[in] presetId Numbered preset identifier (1-based).
    /// @return True if preset recall command was dispatched.
    virtual bool recallPreset(std::uint8_t presetId) = 0;

    // --- Telemetry Callback ---

    /// @brief Callback signature for periodic orientation and velocity updates.
    using TelemetryCallback = std::function<void(const GimbalTelemetry& telemetry)>;

    /// @brief Registers an observer for live orientation telemetry updates.
    /// @param[in] cb Callable receiving GimbalTelemetry snapshots.
    virtual void registerTelemetryCallback(TelemetryCallback cb) = 0;

    /// @brief Retrieves the latest cached orientation, rate, and status telemetry synchronously.
    /// @return Current GimbalTelemetry snapshot.
    [[nodiscard]] virtual GimbalTelemetry currentTelemetry() const = 0;
};

} // namespace PayloadHal
