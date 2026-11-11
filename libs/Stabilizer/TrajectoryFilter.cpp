/// @file TrajectoryFilter.cpp
/// @brief Implementation of trajectory smoothing, PTZ feedforward fusion, and jitter attenuation.
#include "TrajectoryFilter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Stabilizer {

// =============================================================================
// KalmanPathSmoother Implementation
// =============================================================================

KalmanPathSmoother::KalmanPathSmoother(float qBase, float rMeas) noexcept
    : qBase_ { (qBase > 0.0F) ? qBase : 1e-4F }
    , rMeas_ { (rMeas > 0.0F) ? rMeas : 1e-2F }
{
}

void KalmanPathSmoother::predict(float dt, float controlVel, float qScale) noexcept
{
    const float effDt = (dt > 0.0001F) ? dt : (1.0F / 60.0F);
    const float dt2 = effDt * effDt;
    const float dt3 = dt2 * effDt;
    const float dt4 = dt3 * effDt;
    const float dt5 = dt4 * effDt;

    // State prediction: x_pred = F * x + B * u
    x_[0] = x_[0] + (effDt * x_[1]) + (0.5F * dt2 * x_[2]);
    x_[1] = x_[1] + (effDt * x_[2]);
    // x_[2] constant acceleration model

    // Feedforward velocity coupling when active
    if (std::abs(controlVel) > 1e-3F) {
        x_[0] += controlVel * effDt;
        x_[1] += 0.35F * (controlVel - x_[1]);
    }

    // Process noise covariance Q
    const float scale = (qScale > 0.01F) ? qScale : 0.01F;
    const float qEff = qBase_ * scale;
    const float q00 = qEff * (dt5 / 20.0F);
    const float q01 = qEff * (dt4 / 8.0F);
    const float q02 = qEff * (dt3 / 6.0F);
    const float q11 = qEff * (dt3 / 3.0F);
    const float q12 = qEff * (dt2 / 2.0F);
    const float q22 = qEff * effDt;

    // Covariance prediction: P_pred = F * P * F^T + Q
    // Step 1: M = F * P
    const float m00 = p_[0][0] + (effDt * p_[1][0]) + (0.5F * dt2 * p_[2][0]);
    const float m01 = p_[0][1] + (effDt * p_[1][1]) + (0.5F * dt2 * p_[2][1]);
    const float m02 = p_[0][2] + (effDt * p_[1][2]) + (0.5F * dt2 * p_[2][2]);

    const float m11 = p_[1][1] + (effDt * p_[2][1]);
    const float m12 = p_[1][2] + (effDt * p_[2][2]);

    const float m22 = p_[2][2];

    // Step 2: P_next = M * F^T + Q
    p_[0][0] = m00 + (effDt * m01) + (0.5F * dt2 * m02) + q00;
    p_[0][1] = m01 + (effDt * m02) + q01;
    p_[0][2] = m02 + q02;

    p_[1][0] = p_[0][1];
    p_[1][1] = m11 + (effDt * m12) + q11;
    p_[1][2] = m12 + q12;

    p_[2][0] = p_[0][2];
    p_[2][1] = p_[1][2];
    p_[2][2] = m22 + q22;
}

void KalmanPathSmoother::update(float measurement) noexcept
{
    // Innovation: y = z - H * x (H = [1, 0, 0])
    const float y = measurement - x_[0];

    // Innovation covariance: S = H * P * H^T + R
    const float s = p_[0][0] + rMeas_;
    const float invS = (std::abs(s) > 1e-12F) ? (1.0F / s) : 1e12F;

    // Kalman gain: K = P * H^T / S
    const float k0 = p_[0][0] * invS;
    const float k1 = p_[1][0] * invS;
    const float k2 = p_[2][0] * invS;

    // State update: x = x + K * y
    x_[0] += k0 * y;
    x_[1] += k1 * y;
    x_[2] += k2 * y;

    // Covariance update: P = (I - K * H) * P
    const float p00 = p_[0][0];
    const float p01 = p_[0][1];
    const float p02 = p_[0][2];

    p_[0][0] -= k0 * p00;
    p_[0][1] -= k0 * p01;
    p_[0][2] -= k0 * p02;

    p_[1][0] -= k1 * p00;
    p_[1][1] -= k1 * p01;
    p_[1][2] -= k1 * p02;

    p_[2][0] -= k2 * p00;
    p_[2][1] -= k2 * p01;
    p_[2][2] -= k2 * p02;

    // Enforce numerical symmetry
    p_[1][0] = p_[0][1];
    p_[2][0] = p_[0][2];
    p_[2][1] = p_[1][2];
}

void KalmanPathSmoother::reset(float initialPosition) noexcept
{
    x_[0] = initialPosition;
    x_[1] = 0.0F;
    x_[2] = 0.0F;

    p_[0][0] = 1.0F;
    p_[0][1] = 0.0F;
    p_[0][2] = 0.0F;
    p_[1][0] = 0.0F;
    p_[1][1] = 1.0F;
    p_[1][2] = 0.0F;
    p_[2][0] = 0.0F;
    p_[2][1] = 0.0F;
    p_[2][2] = 1.0F;
}

// =============================================================================
// GaussianPathSmoother Implementation
// =============================================================================

GaussianPathSmoother::GaussianPathSmoother(std::uint32_t window) noexcept
    : windowSize_ { (window > 63U) ? 63U : ((window < 3U) ? 3U : window) }
{
    buffer_.fill(0.0F);
}

void GaussianPathSmoother::push(float measurement) noexcept
{
    buffer_[head_] = measurement;
    head_ = (head_ + 1U) % windowSize_;
    if (count_ < windowSize_) {
        ++count_;
    }
}

float GaussianPathSmoother::smooth(float sigma) const noexcept
{
    if (count_ == 0U) {
        return 0.0F;
    }
    if (count_ == 1U) {
        const std::uint32_t latestIdx = (head_ + windowSize_ - 1U) % windowSize_;
        return buffer_[latestIdx];
    }

    const float effSigma = (sigma < 0.5F) ? 0.5F : sigma;
    const float twoSigmaSq = 2.0F * effSigma * effSigma;

    float sumWeighted = 0.0F;
    float sumWeights = 0.0F;

    for (std::uint32_t i = 0U; i < count_; ++i) {
        const std::uint32_t idx = (head_ + windowSize_ - 1U - i) % windowSize_;
        const float dist = static_cast<float>(i);
        const float weight = std::exp(-(dist * dist) / twoSigmaSq);
        sumWeighted += weight * buffer_[idx];
        sumWeights += weight;
    }

    return (sumWeights > 1e-6F) ? (sumWeighted / sumWeights) : buffer_[(head_ + windowSize_ - 1U) % windowSize_];
}

void GaussianPathSmoother::reset(float initialPosition) noexcept
{
    buffer_.fill(initialPosition);
    count_ = 1U;
    head_ = 1U % windowSize_;
}

// =============================================================================
// L1PathSmoother Implementation
// =============================================================================

L1PathSmoother::L1PathSmoother(std::uint32_t window) noexcept
    : windowSize_ { (window > 63U) ? 63U : ((window < 3U) ? 3U : window) }
{
    path_.fill(0.0F);
    smooth_.fill(0.0F);
}

void L1PathSmoother::push(float measurement) noexcept
{
    path_[head_] = measurement;
    smooth_[head_] = measurement;
    head_ = (head_ + 1U) % windowSize_;
    if (count_ < windowSize_) {
        ++count_;
    }
}

void L1PathSmoother::solveAdmm(float maxMargin, float lambda1, float lambda2) noexcept
{
    if (count_ < 3U) {
        return;
    }

    // Unroll ring buffer into chronological temporary arrays
    std::array<float, 64> pChron { };
    std::array<float, 64> sChron { };

    const std::uint32_t startIdx = (count_ < windowSize_) ? 0U : head_;
    for (std::uint32_t i = 0U; i < count_; ++i) {
        const std::uint32_t ringIdx = (startIdx + i) % windowSize_;
        pChron[i] = path_[ringIdx];
        sChron[i] = smooth_[ringIdx];
    }

    // Projected gradient descent with Huberized total variation penalty
    constexpr float kHuberDelta = 0.05F;
    constexpr float kStepSize = 0.08F;
    constexpr std::uint32_t kMaxIters = 20U;

    auto huberDeriv = [](float val, float delta) noexcept -> float {
        if (val > delta) {
            return 1.0F;
        }
        if (val < -delta) {
            return -1.0F;
        }
        return val / delta;
    };

    for (std::uint32_t iter = 0U; iter < kMaxIters; ++iter) {
        std::array<float, 64> grad { };

        // Data fidelity gradient: (S_i - P_i)
        for (std::uint32_t i = 0U; i < count_; ++i) {
            grad[i] = sChron[i] - pChron[i];
        }

        // First difference total variation penalty: lambda1 * |S_{i+1} - S_i|
        if (lambda1 > 0.0F) {
            for (std::uint32_t i = 0U; i + 1U < count_; ++i) {
                const float diff = sChron[i + 1U] - sChron[i];
                const float d = lambda1 * huberDeriv(diff, kHuberDelta);
                grad[i + 1U] += d;
                grad[i] -= d;
            }
        }

        // Second difference total variation penalty: lambda2 * |S_{i+2} - 2S_{i+1} + S_i|
        if (lambda2 > 0.0F) {
            for (std::uint32_t i = 0U; i + 2U < count_; ++i) {
                const float diff2 = sChron[i + 2U] - (2.0F * sChron[i + 1U]) + sChron[i];
                const float d2 = lambda2 * huberDeriv(diff2, kHuberDelta);
                grad[i + 2U] += d2;
                grad[i + 1U] -= 2.0F * d2;
                grad[i] += d2;
            }
        }

        // Gradient descent step and box constraint projection [-maxMargin, +maxMargin]
        for (std::uint32_t i = 0U; i < count_; ++i) {
            sChron[i] -= kStepSize * grad[i];
            const float low = pChron[i] - maxMargin;
            const float high = pChron[i] + maxMargin;
            sChron[i] = std::clamp(sChron[i], low, high);
        }
    }

    // Write back chronological results into smooth_ ring buffer
    for (std::uint32_t i = 0U; i < count_; ++i) {
        const std::uint32_t ringIdx = (startIdx + i) % windowSize_;
        smooth_[ringIdx] = sChron[i];
    }
}

float L1PathSmoother::getSmoothed(std::uint32_t indexFromEnd) const noexcept
{
    if (count_ == 0U) {
        return 0.0F;
    }
    const std::uint32_t offset = (indexFromEnd >= count_) ? (count_ - 1U) : indexFromEnd;
    const std::uint32_t idx = (head_ + windowSize_ - 1U - offset) % windowSize_;
    return smooth_[idx];
}

void L1PathSmoother::reset(float initialPosition) noexcept
{
    path_.fill(initialPosition);
    smooth_.fill(initialPosition);
    count_ = 1U;
    head_ = 1U % windowSize_;
}

// =============================================================================
// TrajectoryFilter Implementation
// =============================================================================

TrajectoryFilter::TrajectoryFilter(const TrajectoryConfig& config) noexcept
    : config_ { config }
    , kfX_ { config.processNoiseBase, config.measurementNoise }
    , kfY_ { config.processNoiseBase, config.measurementNoise }
    , kfTheta_ { config.processNoiseBase * 0.1F, config.measurementNoise }
    , kfScale_ { config.processNoiseBase * 0.01F, config.measurementNoise }
    , gmaX_ { config.windowSize }
    , gmaY_ { config.windowSize }
    , gmaTheta_ { config.windowSize }
    , l1X_ { config.windowSize }
    , l1Y_ { config.windowSize }
    , l1Theta_ { config.windowSize }
{
}

bool TrajectoryFilter::setConfig(const TrajectoryConfig& config) noexcept
{
    if (!config.isValid()) {
        return false;
    }
    config_ = config;
    return true;
}

void TrajectoryFilter::reset() noexcept
{
    cumX_ = 0.0F;
    cumY_ = 0.0F;
    cumTheta_ = 0.0F;
    cumScale_ = 1.0F;
    prevRawTheta_ = 0.0F;
    jitterVariance_ = 0.0F;
    isFirstFrame_ = true;

    kfX_.reset(0.0F);
    kfY_.reset(0.0F);
    kfTheta_.reset(0.0F);
    kfScale_.reset(1.0F);

    gmaX_.reset(0.0F);
    gmaY_.reset(0.0F);
    gmaTheta_.reset(0.0F);

    l1X_.reset(0.0F);
    l1Y_.reset(0.0F);
    l1Theta_.reset(0.0F);
}

float TrajectoryFilter::unwrapAngle(float rawTheta) noexcept
{
    const float delta = rawTheta - prevRawTheta_;
    prevRawTheta_ = rawTheta;
    constexpr float kTwoPi = 6.28318530717958647692F;
    const float turns = std::round(delta / kTwoPi);
    return delta - (turns * kTwoPi);
}

float TrajectoryFilter::mapPtzToPixelVel(AngularRate rate, float focalLengthPx, float dt) noexcept
{
    (void)dt;
    // Commanded angular rate produces pixel velocity: v = -f * omega (rad/s)
    const float omegaRad = rate.radPerSec();
    return -focalLengthPx * omegaRad;
}

float TrajectoryFilter::applyMarginDamping(float rawP, float smoothS, float maxLimit, bool& outDamped) noexcept
{
    outDamped = false;
    if (maxLimit <= 1e-3F) {
        return rawP;
    }

    float delta = smoothS - rawP;
    const float absDelta = std::abs(delta);
    const float softBound = maxLimit * config_.softMarginRatio;

    if (absDelta > softBound) {
        outDamped = true;
        const float excess = absDelta - softBound;
        const float range = maxLimit - softBound;
        const float normExcess = (range > 1e-4F) ? (excess / range) : 1.0F;
        const float dampedExcess = range * std::tanh(normExcess);
        const float sign = (delta >= 0.0F) ? 1.0F : -1.0F;
        delta = sign * (softBound + dampedExcess);
    }

    // Hard boundary clamp
    if (delta > maxLimit) {
        delta = maxLimit;
    } else if (delta < -maxLimit) {
        delta = -maxLimit;
    }

    return rawP + delta;
}

bool TrajectoryFilter::update(const AffineTransform2D& interFrameTransform, const PtzTelemetrySample& ptz, float dt,
    TrajectoryState& outState) noexcept
{
    const float effDt = (dt > 0.0001F) ? dt : (1.0F / 60.0F);

    // 1. Decompose incremental affine transform
    const float dx = interFrameTransform.tx;
    const float dy = interFrameTransform.ty;
    const float a11 = interFrameTransform.a11;
    const float a21 = interFrameTransform.a21;
    const float dTheta = std::atan2(a21, a11);
    float dScale = std::sqrt((a11 * a11) + (a21 * a21));
    dScale = std::clamp(dScale, 0.1F, 10.0F);

    // 2. Accumulate raw continuous path
    if (isFirstFrame_) {
        isFirstFrame_ = false;
        cumX_ = dx;
        cumY_ = dy;
        cumTheta_ = dTheta;
        cumScale_ = dScale;
        prevRawTheta_ = dTheta;

        kfX_.reset(cumX_);
        kfY_.reset(cumY_);
        kfTheta_.reset(cumTheta_);
        kfScale_.reset(cumScale_);

        gmaX_.reset(cumX_);
        gmaY_.reset(cumY_);
        gmaTheta_.reset(cumTheta_);

        l1X_.reset(cumX_);
        l1Y_.reset(cumY_);
        l1Theta_.reset(cumTheta_);
    } else {
        cumX_ += dx;
        cumY_ += dy;
        cumScale_ *= dScale;
        const float unwrappedDeltaTheta = unwrapAngle(dTheta);
        cumTheta_ += unwrappedDeltaTheta;
    }

    // 3. PTZ Feedforward mapping and adaptive Q modulation
    const float focalLen = config_.focalLength.pixels();
    const float vxPtz = mapPtzToPixelVel(ptz.panRate, focalLen, effDt);
    const float vyPtz = mapPtzToPixelVel(ptz.tiltRate, focalLen, effDt);

    const float vxCtrl = vxPtz * config_.ptzFeedforwardGain;
    const float vyCtrl = vyPtz * config_.ptzFeedforwardGain;

    const float panDeg = ptz.panRate.degPerSec();
    const float tiltDeg = ptz.tiltRate.degPerSec();
    const float omegaMag = std::sqrt((panDeg * panDeg) + (tiltDeg * tiltDeg));
    const float deadband = config_.ptzDeadband.degPerSec();

    float qScale = 1.0F;
    if (omegaMag > deadband) {
        const float excess = omegaMag - deadband;
        qScale = 1.0F + (15.0F * excess * excess);
        if (qScale > 2000.0F) {
            qScale = 2000.0F;
        }
    }

    // 4. Filtering execution
    float smoothX = cumX_;
    float smoothY = cumY_;
    float smoothTheta = cumTheta_;
    float smoothScale = cumScale_;

    switch (config_.method) {
    case SmoothingMethod::KalmanFilter: {
        kfX_.predict(effDt, vxCtrl, qScale);
        kfX_.update(cumX_);

        kfY_.predict(effDt, vyCtrl, qScale);
        kfY_.update(cumY_);

        kfTheta_.predict(effDt, 0.0F, qScale);
        kfTheta_.update(cumTheta_);

        kfScale_.predict(effDt, 0.0F, 1.0F);
        kfScale_.update(cumScale_);

        smoothX = kfX_.state();
        smoothY = kfY_.state();
        smoothTheta = kfTheta_.state();
        smoothScale = kfScale_.state();
        break;
    }
    case SmoothingMethod::GaussianFIR: {
        gmaX_.push(cumX_);
        gmaY_.push(cumY_);
        gmaTheta_.push(cumTheta_);

        constexpr float kSigma = 4.0F;
        smoothX = gmaX_.smooth(kSigma);
        smoothY = gmaY_.smooth(kSigma);
        smoothTheta = gmaTheta_.smooth(kSigma);
        smoothScale = cumScale_;
        break;
    }
    case SmoothingMethod::L1Optimization: {
        l1X_.push(cumX_);
        l1Y_.push(cumY_);
        l1Theta_.push(cumTheta_);

        const float maxMx = static_cast<float>(config_.frameDimensions.width()) * config_.maxCropMargin.value();
        const float maxMy = static_cast<float>(config_.frameDimensions.height()) * config_.maxCropMargin.value();

        l1X_.solveAdmm(maxMx, 4.0F, 8.0F);
        l1Y_.solveAdmm(maxMy, 4.0F, 8.0F);
        l1Theta_.solveAdmm(0.1F, 4.0F, 8.0F);

        smoothX = l1X_.getSmoothed(0U);
        smoothY = l1Y_.getSmoothed(0U);
        smoothTheta = l1Theta_.getSmoothed(0U);
        smoothScale = cumScale_;
        break;
    }
    default:
        break;
    }

    // 5. Margin damping and boundary clamping
    const float limitX = static_cast<float>(config_.frameDimensions.width()) * config_.maxCropMargin.value();
    const float limitY = static_cast<float>(config_.frameDimensions.height()) * config_.maxCropMargin.value();
    constexpr float limitTheta = 0.20F; // ~11.5 degrees

    bool dampedX = false;
    bool dampedY = false;
    bool dampedTheta = false;

    const float dampedSmoothX = applyMarginDamping(cumX_, smoothX, limitX, dampedX);
    const float dampedSmoothY = applyMarginDamping(cumY_, smoothY, limitY, dampedY);
    const float dampedSmoothTheta = applyMarginDamping(cumTheta_, smoothTheta, limitTheta, dampedTheta);

    // 6. Synthesize stabilizing correction transform C_t = P_t^{-1} * S_t
    const float corrX = dampedSmoothX - cumX_;
    const float corrY = dampedSmoothY - cumY_;
    const float corrTheta = dampedSmoothTheta - cumTheta_;
    const float corrScale = (cumScale_ > 1e-4F) ? (smoothScale / cumScale_) : 1.0F;

    const float cosT = std::cos(corrTheta);
    const float sinT = std::sin(corrTheta);

    outState.correctionTransform.a11 = corrScale * cosT;
    outState.correctionTransform.a12 = -corrScale * sinT;
    outState.correctionTransform.tx = corrX;
    outState.correctionTransform.a21 = corrScale * sinT;
    outState.correctionTransform.a22 = corrScale * cosT;
    outState.correctionTransform.ty = corrY;

    // 7. Compute telemetry metrics
    const float residualSq = (corrX * corrX) + (corrY * corrY);
    jitterVariance_ = (0.9F * jitterVariance_) + (0.1F * residualSq);

    const float utilX = (limitX > 1e-3F) ? (std::abs(corrX) / limitX) : 0.0F;
    const float utilY = (limitY > 1e-3F) ? (std::abs(corrY) / limitY) : 0.0F;

    outState.marginUtilization = std::max(utilX, utilY);
    outState.jitterVariance = jitterVariance_;
    outState.estimatedVelocityX = kfX_.velocity();
    outState.estimatedVelocityY = kfY_.velocity();
    outState.isDampingActive = dampedX || dampedY || dampedTheta;

    outState.rawPose = AffineTransform2D { 1.0F, 0.0F, cumX_, 0.0F, 1.0F, cumY_ };
    outState.smoothedPose = AffineTransform2D { 1.0F, 0.0F, dampedSmoothX, 0.0F, 1.0F, dampedSmoothY };

    return true;
}

} // namespace Stabilizer
