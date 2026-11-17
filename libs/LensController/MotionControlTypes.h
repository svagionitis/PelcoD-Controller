#pragma once

/// @file MotionControlTypes.h
/// @brief Strongly-typed value objects, units, and limits for motion control and actuator dynamics.

#include <cstddef>
#include <cstdint>

#include <algorithm>

namespace LensControl {

/// @class VelocityTicksPerSec
/// @brief Encoder velocity in steps/second.
class VelocityTicksPerSec {
public:
    constexpr VelocityTicksPerSec() noexcept = default;

    explicit constexpr VelocityTicksPerSec(float vel) noexcept
        : m_vel { vel }
    {
    }

    [[nodiscard]] constexpr float value() const noexcept
    {
        return m_vel;
    }

    [[nodiscard]] constexpr bool isZero() const noexcept
    {
        return m_vel == 0.0F;
    }

    [[nodiscard]] constexpr bool operator==(VelocityTicksPerSec other) const noexcept
    {
        return m_vel == other.m_vel;
    }

    [[nodiscard]] constexpr bool operator!=(VelocityTicksPerSec other) const noexcept
    {
        return m_vel != other.m_vel;
    }

    [[nodiscard]] constexpr bool operator<(VelocityTicksPerSec other) const noexcept
    {
        return m_vel < other.m_vel;
    }

    [[nodiscard]] constexpr bool operator<=(VelocityTicksPerSec other) const noexcept
    {
        return m_vel <= other.m_vel;
    }

    [[nodiscard]] constexpr bool operator>(VelocityTicksPerSec other) const noexcept
    {
        return m_vel > other.m_vel;
    }

    [[nodiscard]] constexpr bool operator>=(VelocityTicksPerSec other) const noexcept
    {
        return m_vel >= other.m_vel;
    }

private:
    float m_vel { 0.0F };
};

/// @class AccelerationTicksPerSec2
/// @brief Encoder acceleration in steps/second^2.
class AccelerationTicksPerSec2 {
public:
    constexpr AccelerationTicksPerSec2() noexcept = default;

    explicit constexpr AccelerationTicksPerSec2(float acc) noexcept
        : m_acc { acc }
    {
    }

    [[nodiscard]] constexpr float value() const noexcept
    {
        return m_acc;
    }

    [[nodiscard]] constexpr bool isZero() const noexcept
    {
        return m_acc == 0.0F;
    }

    [[nodiscard]] constexpr bool operator==(AccelerationTicksPerSec2 other) const noexcept
    {
        return m_acc == other.m_acc;
    }

    [[nodiscard]] constexpr bool operator!=(AccelerationTicksPerSec2 other) const noexcept
    {
        return m_acc != other.m_acc;
    }

    [[nodiscard]] constexpr bool operator<(AccelerationTicksPerSec2 other) const noexcept
    {
        return m_acc < other.m_acc;
    }

    [[nodiscard]] constexpr bool operator<=(AccelerationTicksPerSec2 other) const noexcept
    {
        return m_acc <= other.m_acc;
    }

    [[nodiscard]] constexpr bool operator>(AccelerationTicksPerSec2 other) const noexcept
    {
        return m_acc > other.m_acc;
    }

    [[nodiscard]] constexpr bool operator>=(AccelerationTicksPerSec2 other) const noexcept
    {
        return m_acc >= other.m_acc;
    }

private:
    float m_acc { 0.0F };
};

/// @class JerkTicksPerSec3
/// @brief Rate of change of acceleration in steps/second^3.
class JerkTicksPerSec3 {
public:
    constexpr JerkTicksPerSec3() noexcept = default;

    explicit constexpr JerkTicksPerSec3(float jerk) noexcept
        : m_jerk { jerk }
    {
    }

    [[nodiscard]] constexpr float value() const noexcept
    {
        return m_jerk;
    }

    [[nodiscard]] constexpr bool isZero() const noexcept
    {
        return m_jerk == 0.0F;
    }

    [[nodiscard]] constexpr bool operator==(JerkTicksPerSec3 other) const noexcept
    {
        return m_jerk == other.m_jerk;
    }

    [[nodiscard]] constexpr bool operator!=(JerkTicksPerSec3 other) const noexcept
    {
        return m_jerk != other.m_jerk;
    }

    [[nodiscard]] constexpr bool operator<(JerkTicksPerSec3 other) const noexcept
    {
        return m_jerk < other.m_jerk;
    }

    [[nodiscard]] constexpr bool operator<=(JerkTicksPerSec3 other) const noexcept
    {
        return m_jerk <= other.m_jerk;
    }

    [[nodiscard]] constexpr bool operator>(JerkTicksPerSec3 other) const noexcept
    {
        return m_jerk > other.m_jerk;
    }

    [[nodiscard]] constexpr bool operator>=(JerkTicksPerSec3 other) const noexcept
    {
        return m_jerk >= other.m_jerk;
    }

private:
    float m_jerk { 0.0F };
};

/// @class DeltaTimeSec
/// @brief Strictly positive sample interval in seconds.
class DeltaTimeSec {
public:
    static constexpr float kMinDeltaTime = 0.0001F;

    explicit constexpr DeltaTimeSec(float dt = 0.02F) noexcept
        : m_dt { (dt > kMinDeltaTime) ? dt : kMinDeltaTime }
    {
    }

    [[nodiscard]] constexpr float value() const noexcept
    {
        return m_dt;
    }

    [[nodiscard]] constexpr bool operator==(DeltaTimeSec other) const noexcept
    {
        return m_dt == other.m_dt;
    }

    [[nodiscard]] constexpr bool operator!=(DeltaTimeSec other) const noexcept
    {
        return m_dt != other.m_dt;
    }

    [[nodiscard]] constexpr bool operator<(DeltaTimeSec other) const noexcept
    {
        return m_dt < other.m_dt;
    }

private:
    float m_dt { 0.02F };
};

/// @enum ProfilePhase
/// @brief Active phase of the 7-phase S-curve trajectory.
enum class ProfilePhase : std::uint8_t {
    Idle = 0U,
    JerkAccRampUp = 1U, ///< Phase I: Jerk = +J_max
    ConstantAcc = 2U, ///< Phase II: Acc = +A_max
    JerkAccRampDown = 3U, ///< Phase III: Jerk = -J_max
    CruiseVelocity = 4U, ///< Phase IV: Acc = 0, Vel = V_max
    JerkDecRampUp = 5U, ///< Phase V: Jerk = -J_max
    ConstantDec = 6U, ///< Phase VI: Acc = -D_max
    JerkDecRampDown = 7U, ///< Phase VII: Jerk = +J_max
    TargetReached = 8U
};

/// @struct TrajectorySample
/// @brief Instantaneous kinematics state of an S-curve profile.
struct TrajectorySample {
    float position { 0.0F };
    float velocity { 0.0F };
    float acceleration { 0.0F };
    float jerk { 0.0F };
    float timeElapsed { 0.0F };
    ProfilePhase phase { ProfilePhase::Idle };
};

/// @struct SCurveConstraints
/// @brief Physical kinematic limits for S-curve trajectory generation.
struct SCurveConstraints {
    VelocityTicksPerSec maxVelocity { 10000.0F };
    AccelerationTicksPerSec2 maxAcceleration { 50000.0F };
    AccelerationTicksPerSec2 maxDeceleration { 50000.0F };
    JerkTicksPerSec3 maxJerk { 200000.0F };

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (maxVelocity.value() > 0.0F) && (maxAcceleration.value() > 0.0F) && (maxDeceleration.value() > 0.0F)
            && (maxJerk.value() > 0.0F);
    }
};

/// @struct PidGains
/// @brief Actuator closed-loop PID gains and non-linear thresholds.
struct PidGains {
    float kp { 1.0F };
    float ki { 0.0F };
    float kd { 0.0F };
    float integralMax { 5000.0F };
    float outputMin { -255.0F };
    float outputMax { 255.0F };
    float stictionBreakaway { 15.0F };
    float deadband { 1.0F };
    float derivativeFilterAlpha { 0.2F };
    float movingVelocityThreshold { 0.5F };
    std::uint32_t settlingCyclesRequired { 3U };
};

/// @struct FeedforwardConfig
/// @brief Configuration for parfocal zoom-velocity derivative feedforward.
struct FeedforwardConfig {
    float kp { 0.5F }; ///< Position error closing gain
    float filterAlpha { 0.3F }; ///< Zoom differentiator LPF alpha [0..1]
    float deadbandZoomVelocity { 5.0F }; ///< Velocity threshold below which zoom is stationary
    float maxFocusVelocity { 255.0F }; ///< Actuator saturation clamp
};

} // namespace LensControl
