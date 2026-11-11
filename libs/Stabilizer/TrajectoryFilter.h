/// @file TrajectoryFilter.h
/// @brief Trajectory smoothing, PTZ feedforward sensor fusion, and jitter attenuation.
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "StabilizerTypes.h"

namespace Stabilizer {

/// @brief Strongly-typed angular velocity entity.
class AngularRate {
public:
    constexpr AngularRate() noexcept = default;

    /// @brief Construct angular velocity from degrees per second.
    /// @param degPerSec Angular rate in degrees/second.
    explicit constexpr AngularRate(float degPerSec) noexcept
        : degPerSec_ { degPerSec }
    {
    }

    /// @brief Angular rate in degrees per second.
    [[nodiscard]] constexpr float degPerSec() const noexcept
    {
        return degPerSec_;
    }

    /// @brief Angular rate in radians per second.
    [[nodiscard]] constexpr float radPerSec() const noexcept
    {
        return degPerSec_ * (3.14159265358979323846F / 180.0F);
    }

private:
    float degPerSec_ { 0.0F };
};

/// @brief Strongly-typed optical focal length in pixels.
class FocalLength {
public:
    constexpr FocalLength() noexcept = default;

    /// @brief Construct focal length in pixels.
    /// @param pixels Effective focal length in pixels.
    explicit constexpr FocalLength(float pixels) noexcept
        : pixels_ { (pixels < 1.0F) ? 1.0F : pixels }
    {
    }

    /// @brief Focal length in pixels.
    [[nodiscard]] constexpr float pixels() const noexcept
    {
        return pixels_;
    }

    /// @brief Check whether focal length is positive and valid.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return pixels_ >= 1.0F;
    }

private:
    float pixels_ { 1000.0F };
};

/// @brief Selectable trajectory smoothing algorithm.
enum class SmoothingMethod : std::uint8_t {
    KalmanFilter = 0U, ///< Constant-acceleration EKF with PTZ feedforward
    GaussianFIR, ///< Sliding-window Gaussian moving average
    L1Optimization ///< Constrained L1 total-variation path smoothing
};

/// @brief Gimbal PTZ telemetry snapshot for feedforward velocity gating.
struct PtzTelemetrySample {
    AngularRate panRate { 0.0F };
    AngularRate tiltRate { 0.0F };
    AngularRate rollRate { 0.0F };
    float zoomMagnification { 1.0F };
    std::uint64_t timestampUs { 0U };

    /// @brief Check if all angular rates are within threshold.
    /// @param threshold Maximum allowable rate to consider stationary.
    [[nodiscard]] bool isStationary(AngularRate threshold) const noexcept
    {
        const float th = threshold.degPerSec();
        return (std::abs(panRate.degPerSec()) <= th) && (std::abs(tiltRate.degPerSec()) <= th)
            && (std::abs(rollRate.degPerSec()) <= th);
    }
};

/// @brief Trajectory filter configuration parameters.
struct TrajectoryConfig {
    SmoothingMethod method { SmoothingMethod::KalmanFilter };
    CropRatio maxCropMargin { 0.08F }; ///< 8% default border margin
    float processNoiseBase { 1e-4F }; ///< Base continuous acceleration variance q_a
    float measurementNoise { 1e-2F }; ///< Measurement noise variance R
    float ptzFeedforwardGain { 0.95F }; ///< PTZ velocity coupling gain [0.0 .. 1.5]
    AngularRate ptzDeadband { 0.15F }; ///< Slew velocity deadband (deg/s)
    float softMarginRatio { 0.80F }; ///< Soft damping engagement ratio
    std::uint32_t windowSize { 31U }; ///< FIR / L1 sliding window depth
    FrameDimensions frameDimensions { 1920U, 1080U };
    FocalLength focalLength { 1000.0F }; ///< Optical focal length in pixels

    /// @brief Verify configuration parameter invariants.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (processNoiseBase > 0.0F) && (measurementNoise > 0.0F) && (ptzFeedforwardGain >= 0.0F)
            && (ptzFeedforwardGain <= 1.5F) && (softMarginRatio >= 0.1F) && (softMarginRatio <= 0.99F)
            && (windowSize >= 3U) && (windowSize <= 63U) && frameDimensions.isValid() && focalLength.isValid();
    }
};

/// @brief Rich telemetry state produced by TrajectoryFilter.
struct TrajectoryState {
    AffineTransform2D rawPose { AffineTransform2D::identity() };
    AffineTransform2D smoothedPose { AffineTransform2D::identity() };
    AffineTransform2D correctionTransform { AffineTransform2D::identity() };
    float estimatedVelocityX { 0.0F };
    float estimatedVelocityY { 0.0F };
    float jitterVariance { 0.0F };
    float marginUtilization { 0.0F };
    bool isDampingActive { false };
};

/// @brief 1D Constant-Acceleration Discrete Kalman Filter.
class KalmanPathSmoother {
public:
    constexpr KalmanPathSmoother() noexcept = default;

    /// @brief Construct smoother with base process and measurement noise.
    /// @param qBase Continuous acceleration noise spectral density.
    /// @param rMeas Measurement noise variance.
    explicit KalmanPathSmoother(float qBase, float rMeas) noexcept;

    /// @brief Predict state forward across time step dt.
    /// @param dt Time delta in seconds.
    /// @param controlVel Feedforward velocity input.
    /// @param qScale Dynamic process noise multiplier.
    void predict(float dt, float controlVel, float qScale) noexcept;

    /// @brief Measurement update with observed cumulative position.
    /// @param measurement Observed scalar position.
    void update(float measurement) noexcept;

    /// @brief Estimated position.
    [[nodiscard]] float state() const noexcept
    {
        return x_[0];
    }

    /// @brief Estimated velocity.
    [[nodiscard]] float velocity() const noexcept
    {
        return x_[1];
    }

    /// @brief Estimated acceleration.
    [[nodiscard]] float acceleration() const noexcept
    {
        return x_[2];
    }

    /// @brief Estimated position variance.
    [[nodiscard]] float variance() const noexcept
    {
        return p_[0][0];
    }

    /// @brief Reset state and covariance to initial position.
    /// @param initialPosition Initial scalar position coordinate.
    void reset(float initialPosition) noexcept;

private:
    std::array<float, 3> x_ { 0.0F, 0.0F, 0.0F };
    std::array<std::array<float, 3>, 3> p_ { { { 1.0F, 0.0F, 0.0F }, { 0.0F, 1.0F, 0.0F }, { 0.0F, 0.0F, 1.0F } } };
    float qBase_ { 1e-4F };
    float rMeas_ { 1e-2F };
};

/// @brief Sliding-window Gaussian moving average smoother.
class GaussianPathSmoother {
public:
    constexpr GaussianPathSmoother() noexcept = default;

    /// @brief Construct Gaussian smoother with window capacity.
    /// @param window Window size (clamped to max 63).
    explicit GaussianPathSmoother(std::uint32_t window) noexcept;

    /// @brief Push a new measurement into the ring buffer.
    /// @param measurement Scalar measurement.
    void push(float measurement) noexcept;

    /// @brief Compute Gaussian-weighted smoothed value at current head.
    /// @param sigma Gaussian standard deviation in samples.
    /// @return Filtered scalar value.
    [[nodiscard]] float smooth(float sigma) const noexcept;

    /// @brief Reset ring buffer with initial position.
    /// @param initialPosition Initial scalar position.
    void reset(float initialPosition) noexcept;

private:
    std::array<float, 64> buffer_ { };
    std::uint32_t windowSize_ { 31U };
    std::uint32_t count_ { 0U };
    std::uint32_t head_ { 0U };
};

/// @brief Constrained L1 total-variation path smoother using zero-heap ADMM.
class L1PathSmoother {
public:
    constexpr L1PathSmoother() noexcept = default;

    /// @brief Construct L1 smoother with window capacity.
    /// @param window Window depth (clamped to max 63).
    explicit L1PathSmoother(std::uint32_t window) noexcept;

    /// @brief Push a new measurement into the path buffer.
    /// @param measurement Scalar measurement.
    void push(float measurement) noexcept;

    /// @brief Solve constrained L1 optimization over window.
    /// @param maxMargin Maximum crop boundary excursion.
    /// @param lambda1 First-difference penalty weight.
    /// @param lambda2 Second-difference penalty weight.
    void solveAdmm(float maxMargin, float lambda1, float lambda2) noexcept;

    /// @brief Retrieve smoothed value at given index from end (0 = latest).
    /// @param indexFromEnd Offset from latest sample.
    [[nodiscard]] float getSmoothed(std::uint32_t indexFromEnd) const noexcept;

    /// @brief Reset path buffer with initial position.
    /// @param initialPosition Initial scalar position.
    void reset(float initialPosition) noexcept;

private:
    std::array<float, 64> path_ { };
    std::array<float, 64> smooth_ { };
    std::uint32_t windowSize_ { 31U };
    std::uint32_t count_ { 0U };
    std::uint32_t head_ { 0U };
};

/// @brief Master trajectory filtering and PTZ sensor fusion engine.
class TrajectoryFilter {
public:
    /// @brief Construct filter with configuration parameters.
    /// @param config Filter configuration settings.
    explicit TrajectoryFilter(const TrajectoryConfig& config) noexcept;

    /// @brief Update trajectory with inter-frame motion and PTZ telemetry.
    /// @param[in] interFrameTransform Affine transform from frame t-1 to frame t.
    /// @param[in] ptz Telemetry sample from gimbal (pan/tilt rate).
    /// @param[in] dt Frame interval in seconds.
    /// @param[out] outState Output trajectory state and stabilizing warp.
    /// @return true if filtering succeeded; false if parameters invalid.
    [[nodiscard]] bool update(const AffineTransform2D& interFrameTransform, const PtzTelemetrySample& ptz, float dt,
        TrajectoryState& outState) noexcept;

    /// @brief Reset accumulated paths and internal filter states.
    void reset() noexcept;

    /// @brief Current configuration parameters.
    [[nodiscard]] const TrajectoryConfig& config() const noexcept
    {
        return config_;
    }

    /// @brief Update configuration dynamically.
    /// @param config New configuration parameters.
    /// @return true if valid; false if rejected.
    [[nodiscard]] bool setConfig(const TrajectoryConfig& config) noexcept;

private:
    [[nodiscard]] float unwrapAngle(float rawTheta) noexcept;
    [[nodiscard]] float applyMarginDamping(float rawP, float smoothS, float maxLimit, bool& outDamped) noexcept;
    [[nodiscard]] float mapPtzToPixelVel(AngularRate rate, float focalLengthPx, float dt) noexcept;

    TrajectoryConfig config_ { };
    KalmanPathSmoother kfX_ { };
    KalmanPathSmoother kfY_ { };
    KalmanPathSmoother kfTheta_ { };
    KalmanPathSmoother kfScale_ { };

    GaussianPathSmoother gmaX_ { };
    GaussianPathSmoother gmaY_ { };
    GaussianPathSmoother gmaTheta_ { };

    L1PathSmoother l1X_ { };
    L1PathSmoother l1Y_ { };
    L1PathSmoother l1Theta_ { };

    float cumX_ { 0.0F };
    float cumY_ { 0.0F };
    float cumTheta_ { 0.0F };
    float cumScale_ { 1.0F };
    float prevRawTheta_ { 0.0F };
    float jitterVariance_ { 0.0F };
    bool isFirstFrame_ { true };
};

} // namespace Stabilizer
