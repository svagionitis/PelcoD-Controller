#pragma once

/// @file MotionGatedAfManager.h
/// @brief Platform PTZ slew motion-gating manager for Continuous Auto-Focus (CAF).

#include <cstdint>

#include "AfHeuristicsTypes.h"

namespace LensControl {

/// @struct MotionGatedAfConfig
/// @brief Configuration parameters for motion-gated autofocus.
struct MotionGatedAfConfig {
    AngularVelocityDegPerSec suppressThreshold { 1.5F }; ///< Rate above which CAF suppresses
    AngularVelocityDegPerSec releaseThreshold { 1.0F }; ///< Rate below which settling begins
    AngularVelocityDegPerSec deadband { 0.2F }; ///< Telemetry noise rejection deadband
    std::uint32_t holdoffMs { 150U }; ///< Mechanical settling time in ms
};

/// @class MotionGatedAfManager
/// @brief Inhibits continuous autofocus hunting during platform slew and provides settling holdoff.
class MotionGatedAfManager final {
public:
    using Config = MotionGatedAfConfig;

    explicit MotionGatedAfManager(const MotionGatedAfConfig& config = { }) noexcept;
    ~MotionGatedAfManager() noexcept = default;

    MotionGatedAfManager(const MotionGatedAfManager&) noexcept = default;
    MotionGatedAfManager& operator=(const MotionGatedAfManager&) noexcept = default;
    MotionGatedAfManager(MotionGatedAfManager&&) noexcept = default;
    MotionGatedAfManager& operator=(MotionGatedAfManager&&) noexcept = default;

    /// @brief Ingests platform pan and tilt angular velocities.
    /// @param panRate Current platform pan velocity in deg/s.
    /// @param tiltRate Current platform tilt velocity in deg/s.
    /// @param dtSec Elapsed time in seconds since previous update.
    void updateRates(AngularVelocityDegPerSec panRate, AngularVelocityDegPerSec tiltRate, float dtSec) noexcept;

    /// @brief Resets gating state machine to Active.
    void reset() noexcept;

    /// @brief Returns the current gating state.
    [[nodiscard]] AfGatingState gatingState() const noexcept;

    /// @brief Checks whether continuous autofocus should be inhibited.
    [[nodiscard]] bool isSuppressed() const noexcept;

    /// @brief Returns the most recently computed composite angular velocity.
    [[nodiscard]] AngularVelocityDegPerSec compositeRate() const noexcept;

    /// @brief Reconfigures motion gating thresholds.
    void setConfig(const Config& config) noexcept;

    /// @brief Returns the active configuration.
    [[nodiscard]] Config config() const noexcept;

private:
    Config m_config { };
    AfGatingState m_state { AfGatingState::Active };
    AngularVelocityDegPerSec m_compositeRate { 0.0F };
    float m_holdoffTimerSec { 0.0F };
};

} // namespace LensControl
