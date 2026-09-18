#pragma once

/// @file PtzSphericalEstimator.h
/// @brief High-level domain-specific spherical kinematic state estimator for PTZ auto-tracking.

#include "ExtendedKalmanFilter.h"
#include "PtzCameraModel.h"
#include "UnscentedKalmanFilter.h"

#include <cstdint>
#include <memory>
#include <mutex>

namespace PelcoD {

/// @enum EstimatorType
/// @brief Non-linear estimation algorithm type.
enum class EstimatorType : std::uint8_t {
    EKF, ///< Extended Kalman Filter (analytical Jacobian linearization, lowest CPU).
    UKF  ///< Unscented Kalman Filter (deterministic sigma points, higher-order non-linear accuracy).
};

/// @struct SphericalTargetState
/// @brief Telemetry snapshot representing target spherical kinematics and relative boresight errors.
struct SphericalTargetState {
    double azimuthRad { 0.0 }; ///< Target true azimuth angle in radians [0, 2*pi).
    double elevationRad { 0.0 }; ///< Target true elevation angle in radians [-pi/2, +pi/2].
    double azimuthVelocityRadPerSec { 0.0 }; ///< Target true azimuth angular rate (rad/s).
    double elevationVelocityRadPerSec { 0.0 }; ///< Target true elevation angular rate (rad/s).

    double errorAzimuthDeg { 0.0 }; ///< Horizontal boresight error relative to camera pan (degrees).
    double errorElevationDeg { 0.0 }; ///< Vertical boresight error relative to camera tilt (degrees).
    double omegaAzimuthDegPerSec { 0.0 }; ///< Azimuth angular rate in degrees/sec.
    double omegaElevationDegPerSec { 0.0 }; ///< Elevation angular rate in degrees/sec.

    double predictedErrorAzimuthDeg { 0.0 }; ///< Latency-compensated lookahead azimuth error (degrees).
    double predictedErrorElevationDeg { 0.0 }; ///< Latency-compensated lookahead elevation error (degrees).

    double mahalanobisDistance { 0.0 }; ///< Statistical distance of innovation residual.
    bool locked { false }; ///< True if target is acquired.
    bool isOutlierGated { false }; ///< True if latest visual detection exceeded Mahalanobis threshold.
};

/// @struct SphericalEstimatorConfig
/// @brief Tuning parameters for spherical kinematic estimation.
struct SphericalEstimatorConfig {
    EstimatorType type { EstimatorType::EKF }; ///< Non-linear filter algorithm (EKF or UKF).
    double mahalanobisGateThreshold { 5.99 }; ///< Chi-square 95% rejection gate for 2-DOF measurements.
    double pixelNoiseStd { 2.0 }; ///< Image sensor measurement noise standard deviation in pixels.
    double qAngularPos { 1e-4 }; ///< Process noise covariance for angular position (rad^2/s).
    double qAngularVel { 1e-3 }; ///< Process noise covariance for angular velocity ((rad/s)^2/s).
    double qAngularAcc { 1e-2 }; ///< Process noise covariance for angular acceleration ((rad/s^2)^2/s).
    CameraIntrinsics intrinsics {}; ///< Optical and sensor projection parameters.
};

/// @class PtzSphericalEstimator
/// @brief Fuses 2D visual centroid detections with PTZ camera telemetry via non-linear EKF/UKF.
/// @details Converts 2D pixel coordinates directly into true 3D spherical angles and angular velocities,
///          accounting for gimbal elevation non-linearities, optical zoom scaling, and lens distortion.
class PtzSphericalEstimator {
public:
    /// @brief Constructs estimator with given configuration.
    explicit PtzSphericalEstimator(SphericalEstimatorConfig config = {});

    /// @brief Initializes target state at specified true spherical angles.
    /// @param[in] targetAzimuthRad Target azimuth in radians.
    /// @param[in] targetElevationRad Target elevation in radians.
    void init(double targetAzimuthRad, double targetElevationRad);

    /// @brief Initializes target state from an initial pixel detection and camera telemetry.
    /// @param[in] u Pixel horizontal coordinate.
    /// @param[in] v Pixel vertical coordinate.
    /// @param[in] camPanRad Camera physical pan angle in radians.
    /// @param[in] camTiltRad Camera physical tilt angle in radians.
    /// @param[in] zoom Optical zoom multiplier factor (>= 1.0).
    void initFromPixel(double u, double v, double camPanRad, double camTiltRad, double zoom = 1.0);

    /// @brief Ingests a visual pixel measurement and updates non-linear filter state.
    /// @param[in] u Pixel horizontal coordinate.
    /// @param[in] v Pixel vertical coordinate.
    /// @param[in] camPanRad Camera physical pan angle in radians.
    /// @param[in] camTiltRad Camera physical tilt angle in radians.
    /// @param[in] zoom Optical zoom multiplier factor (>= 1.0).
    /// @param[in] dt Elapsed time in seconds since previous update.
    void update(double u, double v, double camPanRad, double camTiltRad, double zoom, double dt);

    /// @brief Executes prediction coasting step when target is temporarily occluded.
    /// @param[in] dt Elapsed time in seconds.
    void predict(double dt);

    /// @brief Retrieves estimated spherical target kinematics and boresight errors.
    /// @param[in] lookaheadLatencySeconds Forward projection lookahead time in seconds.
    /// @param[in] camPanRad Camera current pan angle in radians.
    /// @param[in] camTiltRad Camera current tilt angle in radians.
    [[nodiscard]] SphericalTargetState getState(double lookaheadLatencySeconds = 0.0,
        double camPanRad = 0.0, double camTiltRad = 0.0) const noexcept;

    /// @brief Sets active estimation algorithm (EKF or UKF).
    void setType(EstimatorType type);

    /// @brief Reconfigures estimator tuning and camera model.
    void setConfig(const SphericalEstimatorConfig& config);

    /// @brief Retrieves active configuration.
    [[nodiscard]] const SphericalEstimatorConfig& getConfig() const noexcept;

    /// @brief Resets estimator state.
    void reset() noexcept;

    /// @brief Query if estimator has acquired lock.
    [[nodiscard]] bool isLocked() const noexcept;

private:
    mutable std::mutex m_mutex;
    SphericalEstimatorConfig m_config {};
    PtzCameraModel m_cameraModel;

    ExtendedKalmanFilter m_ekf;
    UnscentedKalmanFilter m_ukf;

    bool m_locked { false };
    bool m_lastOutlierGated { false };
    double m_lastCamPanRad { 0.0 };
    double m_lastCamTiltRad { 0.0 };
};

} // namespace PelcoD
