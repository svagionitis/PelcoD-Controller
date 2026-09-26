#pragma once

#include "GeoreferenceUtils.h"
#include "PlatformLeverArmCompensator.h"
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace PayloadHal {

/// @enum TargetTrackState
/// @brief Operational tracking state of the target kinematics estimator.
enum class TargetTrackState {
    Unacquired,  ///< No target track active.
    Acquiring,   ///< Initial observations pending track confirmation.
    Tracking,    ///< Steady-state tracking with active measurement updates.
    Coasting,    ///< Measurement occluded; propagating on estimated kinematics.
    Lost         ///< Target lost (coasting duration exceeded timeout).
};

/// @struct TargetKinematics3D
/// @brief Filtered 3D kinematic target state in local North-East-Down (NED) frame.
struct TargetKinematics3D {
    Vector3D positionNedMeters{0.0, 0.0, 0.0};      ///< [North, East, Down] position relative to platform in meters.
    Vector3D velocityNedMps{0.0, 0.0, 0.0};         ///< [North, East, Down] velocity in m/s.
    Vector3D accelerationNedMps2{0.0, 0.0, 0.0};    ///< [North, East, Down] acceleration in m/s^2.
    double speedMps{0.0};                            ///< Scalar 3D speed in m/s.
    double groundSpeedMps{0.0};                      ///< Horizontal ground speed in m/s.
    double courseDeg{0.0};                           ///< Ground track heading in degrees [0, 360).
    double climbRateMps{0.0};                        ///< Vertical rate (-Down) in m/s.
    double slantRangeMeters{0.0};                    ///< Instantaneous slant range from platform in meters.
    double azimuthDeg{0.0};                          ///< Current line-of-sight azimuth in degrees [0, 360).
    double elevationDeg{0.0};                        ///< Current line-of-sight elevation in degrees [-90, +90].
    double positionUncertaintyMeters{0.0};          ///< 1-sigma position standard deviation in meters.
    double velocityUncertaintyMps{0.0};             ///< 1-sigma velocity standard deviation in m/s.
    TargetTrackState trackState{TargetTrackState::Unacquired}; ///< Current track state.
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()}; ///< State timestamp.
};

/// @struct PredictiveLeadSolution
/// @brief Predictive lead-angle and time-of-flight firing/pointing solution.
struct PredictiveLeadSolution {
    double currentAzimuthDeg{0.0};    ///< Line-of-sight azimuth to target now in degrees.
    double currentElevationDeg{0.0};  ///< Line-of-sight elevation to target now in degrees.
    double leadAzimuthDeg{0.0};       ///< Predictive lead azimuth angle in degrees.
    double leadElevationDeg{0.0};     ///< Predictive lead elevation angle in degrees.
    double deltaAzimuthDeg{0.0};      ///< Angular lead offset in azimuth (leadAz - curAz) in degrees [-180, 180].
    double deltaElevationDeg{0.0};    ///< Angular lead offset in elevation (leadEl - curEl) in degrees.
    double totalLeadTimeSec{0.0};     ///< Total lead time applied (latency + TOF) in seconds.
    double timeOfFlightSec{0.0};      ///< Projectile/pulse time-of-flight in seconds.
    Vector3D predictedPositionNed{0.0, 0.0, 0.0}; ///< 3D predicted target coordinates at intercept in meters.
    bool valid{false};                ///< True if solution converged and track is active.
};

/// @struct TargetKinematicsConfig
/// @brief Filter tuning parameters and kinematic thresholds.
struct TargetKinematicsConfig {
    double processNoiseAcc{2.5};               ///< Acceleration noise spectral density (m/s^2)^2/s.
    double defaultRangeUncertaintyMeters{2.0}; ///< 1-sigma LRF range measurement error in meters.
    double angleUncertaintyDeg{0.05};          ///< 1-sigma angular tracking error in degrees.
    double maxCoastDurationSec{3.0};           ///< Maximum duration to coast without observations before Lost.
    double systemLatencySec{0.060};            ///< Composite video + tracking + gimbal latency (default 60ms).
    int confirmHitsRequired{3};                ///< Consecutive hits to transition from Acquiring to Tracking.
    double gateThresholdMeters{50.0};          ///< Maximum innovation distance to validate reacquisition during coasting.
};

/// @class TargetKinematicsFilter
/// @brief 9-state 3D Cartesian target kinematic Kalman filter and lead-angle predictor.
/// @details Estimates 3D target position, velocity, and acceleration vectors in local NED frame
///          from spherical sensor observations (Azimuth, Elevation, Slant Range), computes
///          latency and ballistic lead angles, and coasts tracks across optical occlusions.
class TargetKinematicsFilter {
public:
    /// @brief Constructs a target kinematics filter with the specified configuration.
    /// @param config Filter tuning parameters.
    explicit TargetKinematicsFilter(TargetKinematicsConfig config = {});

    /// @brief Destructor.
    ~TargetKinematicsFilter() = default;

    /// @brief Updates filter configuration parameters.
    /// @param config New configuration.
    void setConfig(const TargetKinematicsConfig& config);

    /// @brief Returns the current filter configuration.
    [[nodiscard]] TargetKinematicsConfig config() const;

    /// @brief Resets the filter to unacquired state.
    void reset();

    /// @brief Ingests a full 3D measurement (Azimuth, Elevation, Slant Range).
    /// @param azimuthDeg Measured azimuth angle in degrees [0, 360).
    /// @param elevationDeg Measured elevation angle in degrees [-90, +90].
    /// @param slantRangeMeters Measured slant range in meters (> 0).
    /// @param platformPositionNed Platform position in NED meters (default origin).
    /// @param timestamp Measurement capture timestamp.
    void updateFullMeasurement(
        double azimuthDeg, double elevationDeg, double slantRangeMeters,
        const Vector3D& platformPositionNed = {0.0, 0.0, 0.0},
        std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now());

    /// @brief Ingests a bearing-only measurement (optical tracker centroid without LRF).
    /// @param azimuthDeg Measured azimuth angle in degrees [0, 360).
    /// @param elevationDeg Measured elevation angle in degrees [-90, +90].
    /// @param platformPositionNed Platform position in NED meters.
    /// @param timestamp Measurement capture timestamp.
    void updateBearingMeasurement(
        double azimuthDeg, double elevationDeg,
        const Vector3D& platformPositionNed = {0.0, 0.0, 0.0},
        std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now());

    /// @brief Propagates filter state forward in time (handles periodic prediction and occlusion coasting).
    /// @param currentTime Current timestamp.
    void predict(std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now());

    /// @brief Returns the current operational tracking state.
    [[nodiscard]] TargetTrackState trackState() const noexcept;

    /// @brief Checks if a valid track is currently maintained (Tracking or Coasting).
    [[nodiscard]] bool isTracking() const noexcept;

    /// @brief Retrieves the instantaneous estimated 3D kinematic target state.
    /// @param platformPositionNed Platform position in NED frame for relative range/look-angle calculation.
    [[nodiscard]] TargetKinematics3D currentKinematics(
        const Vector3D& platformPositionNed = {0.0, 0.0, 0.0}) const;

    /// @brief Computes predictive lead-angle and time-of-flight pointing solution.
    /// @param latencySec Forward lead time for system latency compensation in seconds.
    /// @param projectileVelocityMps Projectile / interceptor velocity in m/s (0.0 for latency-only lead).
    /// @param platformPositionNed Platform position in NED frame.
    /// @return Predictive lead solution with target look angles and intercept coordinates.
    [[nodiscard]] PredictiveLeadSolution computeLeadAngles(
        double latencySec,
        double projectileVelocityMps = 0.0,
        const Vector3D& platformPositionNed = {0.0, 0.0, 0.0}) const;

private:
    struct Axis1DFilter {
        double x[3]{0.0, 0.0, 0.0};     // [pos, vel, acc]
        double P[3][3]{                 // Covariance matrix
            {100.0, 0.0, 0.0},
            {0.0, 25.0, 0.0},
            {0.0, 0.0, 4.0}
        };

        void predict(double dt, double qAcc);
        void update(double z, double r);
        void reset(double initPos, double initVel = 0.0);
    };

    mutable std::mutex m_mutex;
    TargetKinematicsConfig m_config;
    TargetTrackState m_state{TargetTrackState::Unacquired};
    int m_hitCount{0};

    Axis1DFilter m_filterN;
    Axis1DFilter m_filterE;
    Axis1DFilter m_filterD;

    std::chrono::steady_clock::time_point m_lastMeasurementTime;
    std::chrono::steady_clock::time_point m_lastPredictTime;
    Vector3D m_lastPlatformPos{0.0, 0.0, 0.0};
    double m_lastEstimatedRange{1000.0};

    void checkCoastingTimeout(std::chrono::steady_clock::time_point now);
};

} // namespace PayloadHal
