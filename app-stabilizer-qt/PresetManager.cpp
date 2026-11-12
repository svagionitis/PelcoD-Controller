/// @file PresetManager.cpp
/// @brief Implementation of factory stabilization presets and JSON persistence.

#include "PresetManager.h"

#include <cstdint>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace StabilizerApp {

PresetManager::PresetManager(QObject* parent)
    : QObject(parent)
{
}

QStringList PresetManager::presetNames() const
{
    return { QStringLiteral("Drone Gimbal"), QStringLiteral("Pole Vibration"), QStringLiteral("Handheld Action Cam"),
        QStringLiteral("Rolling Shutter"), QStringLiteral("Bypass") };
}

bool PresetManager::getPreset(const QString& name, Stabilizer::VideoStabilizerConfig& outConfig) const
{
    outConfig = Stabilizer::VideoStabilizerConfig { };

    if (name == QStringLiteral("Drone Gimbal")) {
        outConfig.mode = Stabilizer::StabilizationMode::Affine;
        outConfig.fallback = Stabilizer::FallbackPolicy::VelocityExtrapolate;
        outConfig.minTrackedFeatures = 25U;
        outConfig.replenishIntervalFrames = 15U;
        outConfig.minInlierRatio = 0.30F;

        outConfig.filter.method = Stabilizer::SmoothingMethod::KalmanFilter;
        outConfig.filter.processNoiseBase = 1e-4F;
        outConfig.filter.measurementNoise = 1e-2F;
        outConfig.filter.ptzFeedforwardGain = 0.95F;
        outConfig.filter.ptzDeadband = Stabilizer::AngularRate(0.15F);
        outConfig.filter.maxCropMargin = Stabilizer::CropRatio(0.08F);
        outConfig.filter.softMarginRatio = 0.80F;

        outConfig.detector.quality = Stabilizer::QualityThreshold(0.01F);
        outConfig.detector.maxTotalFeatures = 300U;
        outConfig.detector.bucketing.gridCols = 8U;
        outConfig.detector.bucketing.gridRows = 6U;
        outConfig.detector.bucketing.maxFeaturesPerCell = 8U;

        outConfig.tracker.windowSize = Stabilizer::FlowWindowSize(5U);
        outConfig.tracker.enableBidirectional = true;
        outConfig.tracker.maxForwardBackwardError = 0.50F;

        outConfig.estimator.mode = Stabilizer::StabilizationMode::Affine;
        outConfig.estimator.inlierThreshold = Stabilizer::InlierThreshold(2.0F);
        outConfig.estimator.enableRefinement = true;

        outConfig.warp.interpolation = Stabilizer::WarpInterpolation::Bilinear;
        outConfig.warp.cropRatio = Stabilizer::CropRatio(0.06F);
        outConfig.warp.enableAutoCrop = true;
        return true;
    }

    if (name == QStringLiteral("Pole Vibration")) {
        outConfig.mode = Stabilizer::StabilizationMode::TranslationRotation;
        outConfig.fallback = Stabilizer::FallbackPolicy::IdentityHold;
        outConfig.minTrackedFeatures = 30U;
        outConfig.replenishIntervalFrames = 10U;

        outConfig.filter.method = Stabilizer::SmoothingMethod::GaussianFIR;
        outConfig.filter.windowSize = 31U;
        outConfig.filter.maxCropMargin = Stabilizer::CropRatio(0.10F);
        outConfig.filter.softMarginRatio = 0.85F;

        outConfig.detector.quality = Stabilizer::QualityThreshold(0.005F);
        outConfig.detector.maxTotalFeatures = 400U;
        outConfig.detector.bucketing.gridCols = 10U;
        outConfig.detector.bucketing.gridRows = 8U;
        outConfig.detector.bucketing.maxFeaturesPerCell = 10U;

        outConfig.tracker.windowSize = Stabilizer::FlowWindowSize(7U);
        outConfig.tracker.enableBidirectional = true;

        outConfig.estimator.mode = Stabilizer::StabilizationMode::TranslationRotation;
        outConfig.estimator.inlierThreshold = Stabilizer::InlierThreshold(1.5F);

        outConfig.warp.cropRatio = Stabilizer::CropRatio(0.08F);
        return true;
    }

    if (name == QStringLiteral("Handheld Action Cam")) {
        outConfig.mode = Stabilizer::StabilizationMode::Affine;
        outConfig.fallback = Stabilizer::FallbackPolicy::VelocityExtrapolate;
        outConfig.minTrackedFeatures = 20U;
        outConfig.replenishIntervalFrames = 10U;

        outConfig.filter.method = Stabilizer::SmoothingMethod::L1Optimization;
        outConfig.filter.windowSize = 45U;
        outConfig.filter.maxCropMargin = Stabilizer::CropRatio(0.12F);
        outConfig.filter.softMarginRatio = 0.85F;

        outConfig.detector.maxTotalFeatures = 350U;
        outConfig.tracker.windowSize = Stabilizer::FlowWindowSize(6U);
        outConfig.tracker.enableBidirectional = true;
        outConfig.tracker.maxForwardBackwardError = 0.40F;

        outConfig.warp.cropRatio = Stabilizer::CropRatio(0.10F);
        outConfig.warp.enableAutoCrop = true;
        return true;
    }

    if (name == QStringLiteral("Rolling Shutter")) {
        outConfig.mode = Stabilizer::StabilizationMode::RollingShutterComp;
        outConfig.fallback = Stabilizer::FallbackPolicy::VelocityExtrapolate;

        outConfig.warp.interpolation = Stabilizer::WarpInterpolation::Bilinear;
        outConfig.warp.enableRollingShutter = true;
        outConfig.warp.rsProfile.readoutRatio = Stabilizer::ReadoutRatio(0.85F);
        outConfig.warp.rsProfile.scanTopToBottom = true;
        outConfig.warp.cropRatio = Stabilizer::CropRatio(0.06F);
        outConfig.warp.enableAutoCrop = true;
        return true;
    }

    if (name == QStringLiteral("Bypass")) {
        outConfig.mode = Stabilizer::StabilizationMode::Disabled;
        outConfig.fallback = Stabilizer::FallbackPolicy::BypassWarp;
        outConfig.warp.enableAutoCrop = false;
        outConfig.warp.cropRatio = Stabilizer::CropRatio(0.0F);
        return true;
    }

    return false;
}

bool PresetManager::exportJson(const QString& filePath, const Stabilizer::VideoStabilizerConfig& config)
{
    QJsonObject root;
    root[QStringLiteral("mode")] = static_cast<int>(config.mode);
    root[QStringLiteral("fallback")] = static_cast<int>(config.fallback);
    root[QStringLiteral("minTrackedFeatures")] = static_cast<int>(config.minTrackedFeatures);
    root[QStringLiteral("replenishInterval")] = static_cast<int>(config.replenishIntervalFrames);
    root[QStringLiteral("maxDegradedFrames")] = static_cast<int>(config.maxDegradedFrames);
    root[QStringLiteral("minInlierRatio")] = static_cast<double>(config.minInlierRatio);

    QJsonObject filterObj;
    filterObj[QStringLiteral("method")] = static_cast<int>(config.filter.method);
    filterObj[QStringLiteral("windowSize")] = static_cast<int>(config.filter.windowSize);
    filterObj[QStringLiteral("maxCropMargin")] = static_cast<double>(config.filter.maxCropMargin.value());
    filterObj[QStringLiteral("softMarginRatio")] = static_cast<double>(config.filter.softMarginRatio);
    filterObj[QStringLiteral("processNoiseBase")] = static_cast<double>(config.filter.processNoiseBase);
    filterObj[QStringLiteral("measurementNoise")] = static_cast<double>(config.filter.measurementNoise);
    filterObj[QStringLiteral("ptzGain")] = static_cast<double>(config.filter.ptzFeedforwardGain);
    filterObj[QStringLiteral("ptzDeadband")] = static_cast<double>(config.filter.ptzDeadband.degPerSec());
    filterObj[QStringLiteral("focalLength")] = static_cast<double>(config.filter.focalLength.pixels());
    root[QStringLiteral("filter")] = filterObj;

    QJsonObject detectorObj;
    detectorObj[QStringLiteral("quality")] = static_cast<double>(config.detector.quality.value());
    detectorObj[QStringLiteral("borderMargin")] = static_cast<int>(config.detector.borderMargin);
    detectorObj[QStringLiteral("maxFeatures")] = static_cast<int>(config.detector.maxTotalFeatures);
    detectorObj[QStringLiteral("gridCols")] = static_cast<int>(config.detector.bucketing.gridCols);
    detectorObj[QStringLiteral("gridRows")] = static_cast<int>(config.detector.bucketing.gridRows);
    detectorObj[QStringLiteral("maxPerCell")] = static_cast<int>(config.detector.bucketing.maxFeaturesPerCell);
    detectorObj[QStringLiteral("minDistance")] = static_cast<double>(config.detector.bucketing.minDistance);
    root[QStringLiteral("detector")] = detectorObj;

    QJsonObject trackerObj;
    trackerObj[QStringLiteral("halfWidth")] = static_cast<int>(config.tracker.windowSize.halfWidth());
    trackerObj[QStringLiteral("levels")] = static_cast<int>(config.tracker.numPyramidLevels);
    trackerObj[QStringLiteral("maxIters")] = static_cast<int>(config.tracker.maxIterations);
    trackerObj[QStringLiteral("minEpsilon")] = static_cast<double>(config.tracker.minEpsilon);
    trackerObj[QStringLiteral("minEigenvalue")] = static_cast<double>(config.tracker.minEigenvalue);
    trackerObj[QStringLiteral("maxFbError")] = static_cast<double>(config.tracker.maxForwardBackwardError);
    trackerObj[QStringLiteral("bidirectional")] = config.tracker.enableBidirectional;
    root[QStringLiteral("tracker")] = trackerObj;

    QJsonObject warpObj;
    warpObj[QStringLiteral("interpolation")] = static_cast<int>(config.warp.interpolation);
    warpObj[QStringLiteral("cropRatio")] = static_cast<double>(config.warp.cropRatio.value());
    warpObj[QStringLiteral("autoCrop")] = config.warp.enableAutoCrop;
    warpObj[QStringLiteral("rollingShutter")] = config.warp.enableRollingShutter;
    warpObj[QStringLiteral("readoutRatio")] = static_cast<double>(config.warp.rsProfile.readoutRatio.value());
    warpObj[QStringLiteral("scanTopToBottom")] = config.warp.rsProfile.scanTopToBottom;
    root[QStringLiteral("warp")] = warpObj;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool PresetManager::importJson(const QString& filePath, Stabilizer::VideoStabilizerConfig& outConfig)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = file.readAll();
    QJsonParseError parseError { };
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    outConfig = Stabilizer::VideoStabilizerConfig { };

    if (root.contains(QStringLiteral("mode"))) {
        outConfig.mode = static_cast<Stabilizer::StabilizationMode>(root[QStringLiteral("mode")].toInt());
    }
    if (root.contains(QStringLiteral("fallback"))) {
        outConfig.fallback = static_cast<Stabilizer::FallbackPolicy>(root[QStringLiteral("fallback")].toInt());
    }
    if (root.contains(QStringLiteral("minTrackedFeatures"))) {
        outConfig.minTrackedFeatures = static_cast<std::uint32_t>(root[QStringLiteral("minTrackedFeatures")].toInt());
    }
    if (root.contains(QStringLiteral("replenishInterval"))) {
        outConfig.replenishIntervalFrames
            = static_cast<std::uint32_t>(root[QStringLiteral("replenishInterval")].toInt());
    }
    if (root.contains(QStringLiteral("minInlierRatio"))) {
        outConfig.minInlierRatio = static_cast<float>(root[QStringLiteral("minInlierRatio")].toDouble());
    }

    if (root.contains(QStringLiteral("filter")) && root[QStringLiteral("filter")].isObject()) {
        const QJsonObject f = root[QStringLiteral("filter")].toObject();
        outConfig.filter.method = static_cast<Stabilizer::SmoothingMethod>(f[QStringLiteral("method")].toInt());
        outConfig.filter.windowSize = static_cast<std::uint32_t>(f[QStringLiteral("windowSize")].toInt(31));
        outConfig.filter.maxCropMargin
            = Stabilizer::CropRatio(static_cast<float>(f[QStringLiteral("maxCropMargin")].toDouble(0.08)));
        outConfig.filter.softMarginRatio = static_cast<float>(f[QStringLiteral("softMarginRatio")].toDouble(0.80));
        outConfig.filter.processNoiseBase = static_cast<float>(f[QStringLiteral("processNoiseBase")].toDouble(1e-4));
        outConfig.filter.measurementNoise = static_cast<float>(f[QStringLiteral("measurementNoise")].toDouble(1e-2));
    }

    return true;
}

} // namespace StabilizerApp
