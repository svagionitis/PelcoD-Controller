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

    // --- 3-Axis & Roll Control ---

    /// @brief Checks whether the gimbal possesses a controllable physical roll axis.
    /// @return True if 3-axis gimbal with roll axis, false for traditional 2-axis PTU.
    [[nodiscard]] virtual bool hasRollAxis() const noexcept { return false; }

    /// @brief Checks whether the gimbal supports automatic horizon leveling stabilization.
    [[nodiscard]] virtual bool supportsHorizonLeveling() const noexcept { return false; }

    /// @brief Commands the roll axis to slew to an absolute angle.
    /// @param[in] rollDeg Target roll angle in degrees.
    /// @return True if roll command was accepted, false if unsupported or out of limits.
    virtual bool setRollAngle(double rollDeg) { (void)rollDeg; return false; }

    /// @brief Drives the roll axis at physical angular velocity.
    /// @param[in] rollDegPerSec Roll velocity in degrees/second.
    /// @return True if rate command was accepted, false if unsupported.
    virtual bool setRollRate(double rollDegPerSec) { (void)rollDegPerSec; return false; }

    /// @brief Commands 3-axis gimbal to slew to absolute azimuth, elevation, and roll angles.
    /// @param[in] panDeg Target azimuth angle in degrees.
    /// @param[in] tiltDeg Target elevation angle in degrees.
    /// @param[in] rollDeg Target roll angle in degrees.
    /// @return True if target orientation was accepted.
    virtual bool setAbsoluteAngles3Axis(double panDeg, double tiltDeg, double rollDeg)
    {
        if (!hasRollAxis()) {
            return (rollDeg == 0.0) && setAbsoluteAngles(panDeg, tiltDeg);
        }
        return false;
    }

    /// @brief Drives all 3 axes at physical angular velocities simultaneously.
    /// @param[in] panDegPerSec Azimuth velocity in degrees/second.
    /// @param[in] tiltDegPerSec Elevation velocity in degrees/second.
    /// @param[in] rollDegPerSec Roll velocity in degrees/second.
    /// @return True if rate commands were accepted.
    virtual bool setRate3Axis(double panDegPerSec, double tiltDegPerSec, double rollDegPerSec)
    {
        if (!hasRollAxis()) {
            return (rollDegPerSec == 0.0) && setRate(panDegPerSec, tiltDegPerSec);
        }
        return false;
    }

    /// @brief Queries mechanical / software angular travel limits for the roll axis.
    /// @param[out] minRoll Minimum roll travel angle in degrees.
    /// @param[out] maxRoll Maximum roll travel angle in degrees.
    /// @return True if roll limits are available, false otherwise.
    virtual bool getRollLimits(double& minRoll, double& maxRoll) const
    {
        (void)minRoll;
        (void)maxRoll;
        return false;
    }

    /// @brief Enables or disables automatic optical horizon leveling.
    /// @param[in] enable True to enable horizon leveling mode, false to disable.
    /// @return True if the request was accepted.
    virtual bool setHorizonLeveling(bool enable) { (void)enable; return false; }

    /// @brief Checks whether automatic horizon leveling is currently enabled.
    [[nodiscard]] virtual bool isHorizonLevelingEnabled() const noexcept { return false; }

    /// @brief Ingests host platform attitude to compute and apply horizon counter-roll.
    /// @param[in] platformRollDeg Platform roll / bank angle in degrees.
    /// @param[in] platformPitchDeg Platform pitch angle in degrees.
    /// @param[in] headingDeg Platform heading angle in degrees (default 0.0).
    /// @return True if compensation was updated, false if horizon leveling is inactive or unsupported.
    virtual bool updateHorizonLeveling(double platformRollDeg, double platformPitchDeg, double headingDeg = 0.0)
    {
        (void)platformRollDeg;
        (void)platformPitchDeg;
        (void)headingDeg;
        return false;
    }

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
