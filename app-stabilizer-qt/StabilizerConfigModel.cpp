/// @file StabilizerConfigModel.cpp
/// @brief Implementation of QObject ViewModel exposing stabilization parameters to QML.

#include "StabilizerConfigModel.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>

namespace StabilizerApp {

StabilizerConfigModel::StabilizerConfigModel(QObject* parent)
    : QObject(parent)
    , config_ { }
{
}

Stabilizer::VideoStabilizerConfig StabilizerConfigModel::toConfig() const noexcept
{
    return config_;
}

void StabilizerConfigModel::loadFromConfig(const Stabilizer::VideoStabilizerConfig& config)
{
    config_ = config;
    emit configChanged();
}

void StabilizerConfigModel::resetSection(int sectionIndex)
{
    Stabilizer::VideoStabilizerConfig defaults { };
    switch (sectionIndex) {
    case 0: // Core Mode & Supervisory
        config_.mode = defaults.mode;
        config_.fallback = defaults.fallback;
        config_.minTrackedFeatures = defaults.minTrackedFeatures;
        config_.replenishIntervalFrames = defaults.replenishIntervalFrames;
        config_.maxDegradedFrames = defaults.maxDegradedFrames;
        config_.minInlierRatio = defaults.minInlierRatio;
        break;
    case 1: // Trajectory Smoothing & Filter
        config_.filter = defaults.filter;
        break;
    case 2: // Feature Detector
        config_.detector = defaults.detector;
        break;
    case 3: // Optical Flow Tracker
        config_.tracker = defaults.tracker;
        break;
    case 4: // Motion Estimator
        config_.estimator = defaults.estimator;
        break;
    case 5: // Warp Engine & Rolling Shutter
        config_.warp = defaults.warp;
        break;
    default:
        break;
    }
    emit configChanged();
}

void StabilizerConfigModel::resetAll()
{
    config_ = Stabilizer::VideoStabilizerConfig { };
    emit configChanged();
}

// 1. Core Mode & Supervisory Getters
int StabilizerConfigModel::stabilizationMode() const noexcept
{
    return static_cast<int>(config_.mode);
}

int StabilizerConfigModel::fallbackPolicy() const noexcept
{
    return static_cast<int>(config_.fallback);
}

int StabilizerConfigModel::minTrackedFeatures() const noexcept
{
    return static_cast<int>(config_.minTrackedFeatures);
}

int StabilizerConfigModel::replenishInterval() const noexcept
{
    return static_cast<int>(config_.replenishIntervalFrames);
}

int StabilizerConfigModel::maxDegradedFrames() const noexcept
{
    return static_cast<int>(config_.maxDegradedFrames);
}

double StabilizerConfigModel::minInlierRatio() const noexcept
{
    return static_cast<double>(config_.minInlierRatio);
}

// 2. Trajectory Smoothing Getters
int StabilizerConfigModel::smoothingMethod() const noexcept
{
    return static_cast<int>(config_.filter.method);
}

double StabilizerConfigModel::maxCropMargin() const noexcept
{
    return static_cast<double>(config_.filter.maxCropMargin.value());
}

double StabilizerConfigModel::softMarginRatio() const noexcept
{
    return static_cast<double>(config_.filter.softMarginRatio);
}

double StabilizerConfigModel::processNoiseBase() const noexcept
{
    return static_cast<double>(config_.filter.processNoiseBase);
}

double StabilizerConfigModel::measurementNoise() const noexcept
{
    return static_cast<double>(config_.filter.measurementNoise);
}

double StabilizerConfigModel::ptzGain() const noexcept
{
    return static_cast<double>(config_.filter.ptzFeedforwardGain);
}

double StabilizerConfigModel::ptzDeadband() const noexcept
{
    return static_cast<double>(config_.filter.ptzDeadband.degPerSec());
}

double StabilizerConfigModel::focalLength() const noexcept
{
    return static_cast<double>(config_.filter.focalLength.pixels());
}

int StabilizerConfigModel::windowSize() const noexcept
{
    return static_cast<int>(config_.filter.windowSize);
}

// 3. Feature Detector Getters
double StabilizerConfigModel::qualityThreshold() const noexcept
{
    return static_cast<double>(config_.detector.quality.value());
}

int StabilizerConfigModel::borderMargin() const noexcept
{
    return static_cast<int>(config_.detector.borderMargin);
}

int StabilizerConfigModel::maxTotalFeatures() const noexcept
{
    return static_cast<int>(config_.detector.maxTotalFeatures);
}

int StabilizerConfigModel::gridCols() const noexcept
{
    return static_cast<int>(config_.detector.bucketing.gridCols);
}

int StabilizerConfigModel::gridRows() const noexcept
{
    return static_cast<int>(config_.detector.bucketing.gridRows);
}

int StabilizerConfigModel::maxFeaturesPerCell() const noexcept
{
    return static_cast<int>(config_.detector.bucketing.maxFeaturesPerCell);
}

double StabilizerConfigModel::minFeatureDistance() const noexcept
{
    return static_cast<double>(config_.detector.bucketing.minDistance);
}

// 4. Optical Flow Tracker Getters
int StabilizerConfigModel::flowWindowHalfWidth() const noexcept
{
    return static_cast<int>(config_.tracker.windowSize.halfWidth());
}

int StabilizerConfigModel::numPyramidLevels() const noexcept
{
    return static_cast<int>(config_.tracker.numPyramidLevels);
}

int StabilizerConfigModel::maxFlowIterations() const noexcept
{
    return static_cast<int>(config_.tracker.maxIterations);
}

double StabilizerConfigModel::minEpsilon() const noexcept
{
    return static_cast<double>(config_.tracker.minEpsilon);
}

double StabilizerConfigModel::minEigenvalue() const noexcept
{
    return static_cast<double>(config_.tracker.minEigenvalue);
}

double StabilizerConfigModel::maxFbError() const noexcept
{
    return static_cast<double>(config_.tracker.maxForwardBackwardError);
}

bool StabilizerConfigModel::enableBidirectional() const noexcept
{
    return config_.tracker.enableBidirectional;
}

// 5. Motion Estimator Getters
double StabilizerConfigModel::inlierThreshold() const noexcept
{
    return static_cast<double>(config_.estimator.inlierThreshold.value());
}

int StabilizerConfigModel::maxMsacIterations() const noexcept
{
    return static_cast<int>(config_.estimator.maxIterations);
}

int StabilizerConfigModel::minMsacIterations() const noexcept
{
    return static_cast<int>(config_.estimator.minIterations);
}

double StabilizerConfigModel::msacConfidence() const noexcept
{
    return static_cast<double>(config_.estimator.confidence);
}

bool StabilizerConfigModel::enableRefinement() const noexcept
{
    return config_.estimator.enableRefinement;
}

// 6. Warp Engine Getters
int StabilizerConfigModel::warpInterpolation() const noexcept
{
    return static_cast<int>(config_.warp.interpolation);
}

int StabilizerConfigModel::borderMode() const noexcept
{
    return static_cast<int>(config_.warp.borderMode);
}

int StabilizerConfigModel::borderValue() const noexcept
{
    return static_cast<int>(config_.warp.borderValue);
}

double StabilizerConfigModel::warpCropRatio() const noexcept
{
    return static_cast<double>(config_.warp.cropRatio.value());
}

bool StabilizerConfigModel::enableAutoCrop() const noexcept
{
    return config_.warp.enableAutoCrop;
}

bool StabilizerConfigModel::enableRollingShutter() const noexcept
{
    return config_.warp.enableRollingShutter;
}

double StabilizerConfigModel::rsReadoutRatio() const noexcept
{
    return static_cast<double>(config_.warp.rsProfile.readoutRatio.value());
}

bool StabilizerConfigModel::rsScanTopToBottom() const noexcept
{
    return config_.warp.rsProfile.scanTopToBottom;
}

// Setters
void StabilizerConfigModel::setStabilizationMode(int mode)
{
    if ((mode >= 0) && (mode <= static_cast<int>(Stabilizer::StabilizationMode::RollingShutterComp))) {
        const auto newMode = static_cast<Stabilizer::StabilizationMode>(mode);
        if (config_.mode != newMode) {
            config_.mode = newMode;
            emit configChanged();
        }
    }
}

void StabilizerConfigModel::setFallbackPolicy(int policy)
{
    if ((policy >= 0) && (policy <= static_cast<int>(Stabilizer::FallbackPolicy::BypassWarp))) {
        const auto newPolicy = static_cast<Stabilizer::FallbackPolicy>(policy);
        if (config_.fallback != newPolicy) {
            config_.fallback = newPolicy;
            emit configChanged();
        }
    }
}

void StabilizerConfigModel::setMinTrackedFeatures(int count)
{
    const auto val = static_cast<std::uint32_t>(std::clamp(count, 5, 500));
    if (config_.minTrackedFeatures != val) {
        config_.minTrackedFeatures = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setReplenishInterval(int interval)
{
    const auto val = static_cast<std::uint32_t>(std::clamp(interval, 1, 120));
    if (config_.replenishIntervalFrames != val) {
        config_.replenishIntervalFrames = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMaxDegradedFrames(int count)
{
    const auto val = static_cast<std::uint32_t>(std::clamp(count, 1, 240));
    if (config_.maxDegradedFrames != val) {
        config_.maxDegradedFrames = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMinInlierRatio(double ratio)
{
    const auto val = static_cast<float>(std::clamp(ratio, 0.05, 0.95));
    if (config_.minInlierRatio != val) {
        config_.minInlierRatio = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setSmoothingMethod(int method)
{
    if ((method >= 0) && (method <= static_cast<int>(Stabilizer::SmoothingMethod::L1Optimization))) {
        const auto newMethod = static_cast<Stabilizer::SmoothingMethod>(method);
        if (config_.filter.method != newMethod) {
            config_.filter.method = newMethod;
            emit configChanged();
        }
    }
}

void StabilizerConfigModel::setMaxCropMargin(double margin)
{
    const auto val = static_cast<float>(std::clamp(margin, 0.0, 0.30));
    config_.filter.maxCropMargin = Stabilizer::CropRatio(val);
    emit configChanged();
}

void StabilizerConfigModel::setSoftMarginRatio(double ratio)
{
    const auto val = static_cast<float>(std::clamp(ratio, 0.10, 0.99));
    if (config_.filter.softMarginRatio != val) {
        config_.filter.softMarginRatio = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setProcessNoiseBase(double q)
{
    const auto val = static_cast<float>(std::clamp(q, 1e-6, 1e-1));
    if (config_.filter.processNoiseBase != val) {
        config_.filter.processNoiseBase = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMeasurementNoise(double r)
{
    const auto val = static_cast<float>(std::clamp(r, 1e-4, 1.0));
    if (config_.filter.measurementNoise != val) {
        config_.filter.measurementNoise = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setPtzGain(double gain)
{
    const auto val = static_cast<float>(std::clamp(gain, 0.0, 1.50));
    if (config_.filter.ptzFeedforwardGain != val) {
        config_.filter.ptzFeedforwardGain = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setPtzDeadband(double deadband)
{
    const auto val = static_cast<float>(std::clamp(deadband, 0.0, 5.00));
    config_.filter.ptzDeadband = Stabilizer::AngularRate(val);
    emit configChanged();
}

void StabilizerConfigModel::setFocalLength(double fl)
{
    const auto val = static_cast<float>(std::clamp(fl, 100.0, 50000.0));
    config_.filter.focalLength = Stabilizer::FocalLength(val);
    emit configChanged();
}

void StabilizerConfigModel::setWindowSize(int size)
{
    // Force odd window size in range [3 .. 63]
    int clamped = std::clamp(size, 3, 63);
    if ((clamped % 2) == 0) {
        clamped += 1;
    }
    const auto val = static_cast<std::uint32_t>(clamped);
    if (config_.filter.windowSize != val) {
        config_.filter.windowSize = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setQualityThreshold(double val)
{
    const auto th = static_cast<float>(std::clamp(val, 0.0001, 0.5000));
    config_.detector.quality = Stabilizer::QualityThreshold(th);
    emit configChanged();
}

void StabilizerConfigModel::setBorderMargin(int margin)
{
    const auto val = static_cast<std::uint16_t>(std::clamp(margin, 3, 64));
    if (config_.detector.borderMargin != val) {
        config_.detector.borderMargin = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMaxTotalFeatures(int count)
{
    const auto val = static_cast<std::uint32_t>(std::clamp(count, 10, 4096));
    if (config_.detector.maxTotalFeatures != val) {
        config_.detector.maxTotalFeatures = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setGridCols(int cols)
{
    const auto val = static_cast<std::uint16_t>(std::clamp(cols, 1, 32));
    if (config_.detector.bucketing.gridCols != val) {
        config_.detector.bucketing.gridCols = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setGridRows(int rows)
{
    const auto val = static_cast<std::uint16_t>(std::clamp(rows, 1, 32));
    if (config_.detector.bucketing.gridRows != val) {
        config_.detector.bucketing.gridRows = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMaxFeaturesPerCell(int count)
{
    const auto val = static_cast<std::uint16_t>(std::clamp(count, 1, 64));
    if (config_.detector.bucketing.maxFeaturesPerCell != val) {
        config_.detector.bucketing.maxFeaturesPerCell = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMinFeatureDistance(double dist)
{
    const auto val = static_cast<float>(std::clamp(dist, 0.0, 128.0));
    if (config_.detector.bucketing.minDistance != val) {
        config_.detector.bucketing.minDistance = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setFlowWindowHalfWidth(int halfWidth)
{
    const auto hw = static_cast<std::uint16_t>(std::clamp(halfWidth, 2, 16));
    config_.tracker.windowSize = Stabilizer::FlowWindowSize(hw);
    emit configChanged();
}

void StabilizerConfigModel::setNumPyramidLevels(int levels)
{
    const auto val = static_cast<std::uint8_t>(std::clamp(levels, 1, 3));
    if (config_.tracker.numPyramidLevels != val) {
        config_.tracker.numPyramidLevels = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMaxFlowIterations(int iters)
{
    const auto val = static_cast<std::uint16_t>(std::clamp(iters, 1, 100));
    if (config_.tracker.maxIterations != val) {
        config_.tracker.maxIterations = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMinEpsilon(double eps)
{
    const auto val = static_cast<float>(std::clamp(eps, 0.0001, 0.50));
    if (config_.tracker.minEpsilon != val) {
        config_.tracker.minEpsilon = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMinEigenvalue(double ev)
{
    const auto val = static_cast<float>(std::clamp(ev, 0.00001, 10.0));
    if (config_.tracker.minEigenvalue != val) {
        config_.tracker.minEigenvalue = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMaxFbError(double err)
{
    const auto val = static_cast<float>(std::clamp(err, 0.05, 5.00));
    if (config_.tracker.maxForwardBackwardError != val) {
        config_.tracker.maxForwardBackwardError = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setEnableBidirectional(bool enable)
{
    if (config_.tracker.enableBidirectional != enable) {
        config_.tracker.enableBidirectional = enable;
        emit configChanged();
    }
}

void StabilizerConfigModel::setInlierThreshold(double th)
{
    const auto val = static_cast<float>(std::clamp(th, 0.10, 20.0));
    config_.estimator.inlierThreshold = Stabilizer::InlierThreshold(val);
    emit configChanged();
}

void StabilizerConfigModel::setMaxMsacIterations(int iters)
{
    const auto val = static_cast<std::uint32_t>(std::clamp(iters, 10, 1000));
    if (config_.estimator.maxIterations != val) {
        config_.estimator.maxIterations = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMinMsacIterations(int iters)
{
    const auto val = static_cast<std::uint32_t>(std::clamp(iters, 5, 500));
    if (config_.estimator.minIterations != val) {
        config_.estimator.minIterations = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setMsacConfidence(double conf)
{
    const auto val = static_cast<float>(std::clamp(conf, 0.80, 0.999));
    if (config_.estimator.confidence != val) {
        config_.estimator.confidence = val;
        emit configChanged();
    }
}

void StabilizerConfigModel::setEnableRefinement(bool enable)
{
    if (config_.estimator.enableRefinement != enable) {
        config_.estimator.enableRefinement = enable;
        emit configChanged();
    }
}

void StabilizerConfigModel::setWarpInterpolation(int interp)
{
    if ((interp >= 0) && (interp <= static_cast<int>(Stabilizer::WarpInterpolation::Bicubic))) {
        const auto newInterp = static_cast<Stabilizer::WarpInterpolation>(interp);
        if (config_.warp.interpolation != newInterp) {
            config_.warp.interpolation = newInterp;
            emit configChanged();
        }
    }
}

void StabilizerConfigModel::setBorderMode(int mode)
{
    if ((mode >= 0) && (mode <= static_cast<int>(Stabilizer::BorderMode::Reflect))) {
        const auto newMode = static_cast<Stabilizer::BorderMode>(mode);
        if (config_.warp.borderMode != newMode) {
            config_.warp.borderMode = newMode;
            emit configChanged();
        }
    }
}

void StabilizerConfigModel::setBorderValue(int val)
{
    const auto byteVal = static_cast<std::uint8_t>(std::clamp(val, 0, 255));
    if (config_.warp.borderValue != byteVal) {
        config_.warp.borderValue = byteVal;
        emit configChanged();
    }
}

void StabilizerConfigModel::setWarpCropRatio(double ratio)
{
    const auto val = static_cast<float>(std::clamp(ratio, 0.0, 0.30));
    config_.warp.cropRatio = Stabilizer::CropRatio(val);
    emit configChanged();
}

void StabilizerConfigModel::setEnableAutoCrop(bool enable)
{
    if (config_.warp.enableAutoCrop != enable) {
        config_.warp.enableAutoCrop = enable;
        emit configChanged();
    }
}

void StabilizerConfigModel::setEnableRollingShutter(bool enable)
{
    if (config_.warp.enableRollingShutter != enable) {
        config_.warp.enableRollingShutter = enable;
        emit configChanged();
    }
}

void StabilizerConfigModel::setRsReadoutRatio(double ratio)
{
    const auto val = static_cast<float>(std::clamp(ratio, 0.0, 1.0));
    config_.warp.rsProfile.readoutRatio = Stabilizer::ReadoutRatio(val);
    emit configChanged();
}

void StabilizerConfigModel::setRsScanTopToBottom(bool topToBottom)
{
    if (config_.warp.rsProfile.scanTopToBottom != topToBottom) {
        config_.warp.rsProfile.scanTopToBottom = topToBottom;
        emit configChanged();
    }
}

} // namespace StabilizerApp
