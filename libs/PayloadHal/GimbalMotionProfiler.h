#pragma once

#include <chrono>
#include <cmath>
#include <string>
#include <vector>

namespace PayloadHal {

/// @enum ProfileAlgorithm
/// @brief Mathematical profile calculation algorithm.
enum class ProfileAlgorithm {
    LinearTrapezoidal,  ///< 3-segment trapezoidal velocity profile (piecewise constant acceleration).
    SCurve7Segment      ///< 7-segment jerk-limited S-curve profile (smooth continuous acceleration).
};

/// @struct AxisMotionConstraints
/// @brief Physical kinematic velocity, acceleration, and jerk constraints for a rotational axis.
struct AxisMotionConstraints {
    double maxVelocityDegPerSec{60.0};       ///< Maximum slewing speed (|v| <= v_max) in deg/s.
    double maxAccelerationDegPerSec2{120.0};  ///< Maximum acceleration (|a| <= a_max) in deg/s^2.
    double maxJerkDegPerSec3{300.0};          ///< Maximum jerk (|j| <= j_max) in deg/s^3.

    /// @brief Checks if constraints are physically positive and valid.
    /// @return True if all constraints > 0.
    [[nodiscard]] bool isValid() const noexcept {
        return maxVelocityDegPerSec > 0.0 &&
               maxAccelerationDegPerSec2 > 0.0 &&
               maxJerkDegPerSec3 > 0.0;
    }
};

/// @struct GimbalMotionConstraints
/// @brief Physical kinematic limits configured across all 3 rotational gimbal axes.
struct GimbalMotionConstraints {
    AxisMotionConstraints pan{60.0, 120.0, 300.0};   ///< Azimuth / pan axis constraints.
    AxisMotionConstraints tilt{45.0, 90.0, 250.0};   ///< Elevation / tilt axis constraints.
    AxisMotionConstraints roll{30.0, 60.0, 150.0};   ///< Horizon / roll axis constraints.

    /// @brief Validates all axis constraints.
    /// @return True if pan, tilt, and roll constraints are all valid.
    [[nodiscard]] bool isValid() const noexcept {
        return pan.isValid() && tilt.isValid() && roll.isValid();
    }
};

/// @struct KinematicState1D
/// @brief Instantaneous 1-dimensional kinematic state.
struct KinematicState1D {
    double positionDeg{0.0};          ///< Angular position in degrees.
    double velocityDegPerSec{0.0};    ///< Angular velocity in deg/s.
    double accelerationDegPerSec2{0.0};///< Angular acceleration in deg/s^2.
    double jerkDegPerSec3{0.0};       ///< Angular jerk in deg/s^3.
};

/// @struct GimbalKinematicState
/// @brief Instantaneous 3-axis kinematic state with capture timestamp.
struct GimbalKinematicState {
    KinematicState1D pan;  ///< Pan kinematic state.
    KinematicState1D tilt; ///< Tilt kinematic state.
    KinematicState1D roll; ///< Roll kinematic state.
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
};

/// @class AxisTrajectory
/// @brief Point-to-point motion trajectory for a single rotational axis.
/// @details Evaluates analytical continuous position, velocity, and acceleration
///          at any arbitrary continuous time offset.
class AxisTrajectory {
public:
    /// @brief Default constructor for an empty trajectory.
    AxisTrajectory() = default;

    /// @brief Constructs an evaluated axis trajectory.
    /// @param startPos Initial position in degrees.
    /// @param targetPos Target position in degrees.
    /// @param durations Array of segment durations in seconds.
    /// @param jMax Maximum jerk magnitude in deg/s^3.
    /// @param aLim Peak acceleration reached in deg/s^2.
    /// @param vLim Peak cruise velocity reached in deg/s.
    /// @param algorithm Profile algorithm used.
    AxisTrajectory(double startPos, double targetPos,
                   const std::vector<double>& durations,
                   double jMax, double aLim, double vLim,
                   ProfileAlgorithm algorithm) noexcept;

    /// @brief Returns the total duration of the trajectory in seconds.
    /// @return Total transit time in seconds.
    [[nodiscard]] double totalDuration() const noexcept { return m_totalDuration; }

    /// @brief Samples the trajectory at the given time offset.
    /// @param timeOffsetSec Elapsed time from trajectory start in seconds.
    /// @return Instantaneous 1D kinematic state.
    [[nodiscard]] KinematicState1D sample(double timeOffsetSec) const noexcept;

    /// @brief Checks whether the trajectory has finished at the given time offset.
    /// @param timeOffsetSec Elapsed time in seconds.
    /// @return True if timeOffsetSec >= totalDuration().
    [[nodiscard]] bool isFinished(double timeOffsetSec) const noexcept {
        return timeOffsetSec >= m_totalDuration;
    }

    /// @brief Returns the initial start position in degrees.
    [[nodiscard]] double startPosition() const noexcept { return m_startPos; }

    /// @brief Returns the final target position in degrees.
    [[nodiscard]] double targetPosition() const noexcept { return m_targetPos; }

    /// @brief Returns the profile algorithm used to plan this trajectory.
    [[nodiscard]] ProfileAlgorithm algorithm() const noexcept { return m_algorithm; }

private:
    double m_startPos{0.0};
    double m_targetPos{0.0};
    double m_direction{1.0};
    double m_totalDuration{0.0};
    double m_jMax{0.0};
    double m_aLim{0.0};
    double m_vLim{0.0};
    ProfileAlgorithm m_algorithm{ProfileAlgorithm::SCurve7Segment};
    std::vector<double> m_durations;       ///< Durations of individual segments.
    std::vector<double> m_switchTimes;     ///< Cumulative switch timestamps.
    std::vector<double> m_boundaryPos;     ///< Position at start of each segment.
    std::vector<double> m_boundaryVel;     ///< Velocity at start of each segment.
    std::vector<double> m_boundaryAcc;     ///< Acceleration at start of each segment.
};

/// @struct CoordinatedGimbalTrajectory
/// @brief Coordinated 3-axis trajectory bundle.
struct CoordinatedGimbalTrajectory {
    AxisTrajectory panTrajectory;   ///< Pan axis trajectory.
    AxisTrajectory tiltTrajectory;  ///< Tilt axis trajectory.
    AxisTrajectory rollTrajectory;  ///< Roll axis trajectory.
    double totalDurationSec{0.0};   ///< Master coordinated duration in seconds.

    /// @brief Samples the coordinated 3-axis state at the given time offset.
    /// @param timeOffsetSec Elapsed time from start in seconds.
    /// @return 3-axis instantaneous kinematic state.
    [[nodiscard]] GimbalKinematicState sample(double timeOffsetSec) const noexcept;

    /// @brief Checks if all axes have reached their final positions.
    /// @param timeOffsetSec Elapsed time in seconds.
    /// @return True if timeOffsetSec >= totalDurationSec.
    [[nodiscard]] bool isFinished(double timeOffsetSec) const noexcept {
        return timeOffsetSec >= totalDurationSec;
    }
};

/// @class GimbalMotionProfiler
/// @brief Gimbal S-Curve Motion Profiler & Real-Time Kinematics Filter.
/// @details Generates analytical jerk-limited 7-segment S-curve and trapezoidal point-to-point
///          trajectories with multi-axis duration synchronization, and filters continuous
///          streaming rate inputs for jerk-free gimbal movement.
class GimbalMotionProfiler {
public:
    /// @brief Constructs a motion profiler with specified physical constraints.
    /// @param constraints 3-axis kinematic velocity, acceleration, and jerk limits.
    explicit GimbalMotionProfiler(GimbalMotionConstraints constraints = {});

    /// @brief Destructor.
    ~GimbalMotionProfiler() = default;

    /// @brief Updates kinematic constraints across all axes.
    /// @param constraints New constraints.
    void setConstraints(const GimbalMotionConstraints& constraints);

    /// @brief Returns the current gimbal kinematic constraints.
    [[nodiscard]] GimbalMotionConstraints constraints() const;

    /// @brief Sets the profile calculation algorithm.
    /// @param algorithm Algorithm selection (SCurve7Segment or LinearTrapezoidal).
    void setAlgorithm(ProfileAlgorithm algorithm) noexcept;

    /// @brief Returns the current profile calculation algorithm.
    [[nodiscard]] ProfileAlgorithm algorithm() const noexcept;

    /// @brief Plans a coordinated 3-axis point-to-point gimbal slew.
    /// @details When synchronizeDuration is true, subordinate axes automatically scale
    ///          velocities and accelerations so all 3 axes arrive at the exact same timestamp.
    /// @param startState Current 3-axis gimbal kinematic state.
    /// @param targetPanDeg Target pan angle in degrees.
    /// @param targetTiltDeg Target tilt angle in degrees.
    /// @param targetRollDeg Target roll angle in degrees (default 0.0).
    /// @param synchronizeDuration Whether to synchronize total durations across all axes.
    /// @return Coordinated 3-axis trajectory bundle.
    [[nodiscard]] CoordinatedGimbalTrajectory planCoordinatedMove(
        const GimbalKinematicState& startState,
        double targetPanDeg,
        double targetTiltDeg,
        double targetRollDeg = 0.0,
        bool synchronizeDuration = true) const;

    /// @brief Plans a single-axis point-to-point trajectory.
    /// @param startPosDeg Initial angle in degrees.
    /// @param targetPosDeg Target angle in degrees.
    /// @param constraints Kinematic limits to enforce.
    /// @return Evaluated single-axis trajectory.
    [[nodiscard]] AxisTrajectory planAxisMove(
        double startPosDeg,
        double targetPosDeg,
        const AxisMotionConstraints& constraints) const;

    /// @brief Filters a discrete streaming velocity command for a single axis.
    /// @details Computes bounded acceleration and jerk transitions to approach
    ///          targetVelocityDegPerSec without overshooting or exceeding limits.
    /// @param currentState Current 1D kinematic state.
    /// @param targetVelocityDegPerSec Desired velocity command in deg/s.
    /// @param dtSec Discrete time step in seconds.
    /// @param constraints Kinematic constraints for the axis.
    /// @return Updated 1D kinematic state for the next time step.
    [[nodiscard]] KinematicState1D filterVelocityStep(
        const KinematicState1D& currentState,
        double targetVelocityDegPerSec,
        double dtSec,
        const AxisMotionConstraints& constraints) const noexcept;

    /// @brief Filters streaming 3-axis velocity commands simultaneously.
    /// @param currentState Current 3-axis kinematic state.
    /// @param targetPanRate Desired pan velocity command in deg/s.
    /// @param targetTiltRate Desired tilt velocity command in deg/s.
    /// @param targetRollRate Desired roll velocity command in deg/s.
    /// @param dtSec Discrete time step in seconds.
    /// @return Updated 3-axis kinematic state.
    [[nodiscard]] GimbalKinematicState filterGimbalVelocityStep(
        const GimbalKinematicState& currentState,
        double targetPanRate,
        double targetTiltRate,
        double targetRollRate,
        double dtSec) const noexcept;

private:
    GimbalMotionConstraints m_constraints;
    ProfileAlgorithm m_algorithm{ProfileAlgorithm::SCurve7Segment};

    [[nodiscard]] AxisTrajectory planSCurve(
        double startPos, double targetPos,
        const AxisMotionConstraints& constraints) const;

    [[nodiscard]] AxisTrajectory planTrapezoidal(
        double startPos, double targetPos,
        const AxisMotionConstraints& constraints) const;
};

} // namespace PayloadHal
