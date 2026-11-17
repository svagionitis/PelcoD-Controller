#pragma once

/// @file MotionProfiler.h
/// @brief 7-phase jerk-limited (S-curve) trajectory planner for optical actuators.

#include <cstddef>
#include <cstdint>

#include "LensTypes.h"
#include "MotionControlTypes.h"

namespace LensControl {

/// @class MotionProfiler
/// @brief Generates jerk-bounded, resonance-free trajectories for motor positioning.
class MotionProfiler final {
public:
    /// @brief Constructs a motion profiler with default or custom kinematic limits.
    /// @param constraints Velocity, acceleration, deceleration, and jerk limits.
    explicit MotionProfiler(const SCurveConstraints& constraints = SCurveConstraints { }) noexcept;

    /// @brief Default destructor.
    ~MotionProfiler() noexcept = default;

    /// @brief Configures kinematic constraints.
    /// @param constraints Limits for velocity, acceleration, deceleration, and jerk.
    /// @return True if constraints are valid and positive.
    [[nodiscard]] bool setConstraints(const SCurveConstraints& constraints) noexcept;

    /// @brief Returns current kinematic constraints.
    /// @return Active constraints reference.
    [[nodiscard]] const SCurveConstraints& constraints() const noexcept;

    /// @brief Plans a new 7-phase trajectory from start to target.
    /// @param startPos Initial encoder position.
    /// @param targetPos Desired encoder position.
    /// @return True if trajectory was successfully computed.
    [[nodiscard]] bool planTrajectory(float startPos, float targetPos) noexcept;

    /// @brief Advances profile by time dt and returns the updated state.
    /// @param dt Sample interval.
    /// @return Kinematics sample at the new timestamp.
    [[nodiscard]] TrajectorySample advance(DeltaTimeSec dt) noexcept;

    /// @brief Evaluates trajectory state at an exact analytical time offset.
    /// @param t Time in seconds from profile start.
    /// @return Kinematics sample at time t.
    [[nodiscard]] TrajectorySample evaluateAt(float t) const noexcept;

    /// @brief Tests whether the current profile has arrived at the target position.
    /// @return True if complete.
    [[nodiscard]] bool isComplete() const noexcept;

    /// @brief Returns total planned duration in seconds.
    /// @return Profile duration.
    [[nodiscard]] float totalDuration() const noexcept;

    /// @brief Returns the planned target position.
    /// @return Target position.
    [[nodiscard]] float targetPosition() const noexcept;

    /// @brief Aborts active trajectory immediately.
    void cancel() noexcept;

private:
    void calculateTiming() noexcept;
    [[nodiscard]] float computeDistanceForV(float v) const noexcept;
    [[nodiscard]] TrajectorySample samplePhase(float tau, std::size_t phaseIdx) const noexcept;

    SCurveConstraints m_limits { };
    float m_startPos { 0.0F };
    float m_targetPos { 0.0F };
    float m_direction { 1.0F };
    float m_totalDistance { 0.0F };
    float m_currentTime { 0.0F };
    float m_totalDuration { 0.0F };
    bool m_isComplete { true };

    // Phase intervals: indices [0..6] for phases 1..7
    float m_phaseDuration[7] { 0.0F };
    float m_phaseStartTime[7] { 0.0F };
    float m_phaseStartPos[7] { 0.0F };
    float m_phaseStartVel[7] { 0.0F };
    float m_phaseStartAcc[7] { 0.0F };
    float m_phaseJerk[7] { 0.0F };
};

} // namespace LensControl
