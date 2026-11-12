#pragma once

/// @file StabilizerConfigModel.h
/// @brief QObject ViewModel exposing strongly-typed stabilization parameters to QML.

#include <cstdint>

#include <QObject>

#include "FeatureDetector.h"
#include "MotionModelEstimator.h"
#include "OpticalFlowTracker.h"
#include "StabilizerTypes.h"
#include "TrajectoryFilter.h"
#include "VideoStabilizerEngine.h"
#include "WarpEngine.h"

namespace StabilizerApp {

/// @class StabilizerConfigModel
/// @brief Comprehensive property bridge exposing stabilization parameters with two-way QML bindings.
class StabilizerConfigModel : public QObject {
    Q_OBJECT

    // 1. Core Mode & Supervisory
    Q_PROPERTY(int stabilizationMode READ stabilizationMode WRITE setStabilizationMode NOTIFY configChanged)
    Q_PROPERTY(int fallbackPolicy READ fallbackPolicy WRITE setFallbackPolicy NOTIFY configChanged)
    Q_PROPERTY(int minTrackedFeatures READ minTrackedFeatures WRITE setMinTrackedFeatures NOTIFY configChanged)
    Q_PROPERTY(int replenishInterval READ replenishInterval WRITE setReplenishInterval NOTIFY configChanged)
    Q_PROPERTY(int maxDegradedFrames READ maxDegradedFrames WRITE setMaxDegradedFrames NOTIFY configChanged)
    Q_PROPERTY(double minInlierRatio READ minInlierRatio WRITE setMinInlierRatio NOTIFY configChanged)

    // 2. Trajectory Smoothing & Filter
    Q_PROPERTY(int smoothingMethod READ smoothingMethod WRITE setSmoothingMethod NOTIFY configChanged)
    Q_PROPERTY(double maxCropMargin READ maxCropMargin WRITE setMaxCropMargin NOTIFY configChanged)
    Q_PROPERTY(double softMarginRatio READ softMarginRatio WRITE setSoftMarginRatio NOTIFY configChanged)
    Q_PROPERTY(double processNoiseBase READ processNoiseBase WRITE setProcessNoiseBase NOTIFY configChanged)
    Q_PROPERTY(double measurementNoise READ measurementNoise WRITE setMeasurementNoise NOTIFY configChanged)
    Q_PROPERTY(double ptzGain READ ptzGain WRITE setPtzGain NOTIFY configChanged)
    Q_PROPERTY(double ptzDeadband READ ptzDeadband WRITE setPtzDeadband NOTIFY configChanged)
    Q_PROPERTY(double focalLength READ focalLength WRITE setFocalLength NOTIFY configChanged)
    Q_PROPERTY(int windowSize READ windowSize WRITE setWindowSize NOTIFY configChanged)

    // 3. Feature Detector
    Q_PROPERTY(double qualityThreshold READ qualityThreshold WRITE setQualityThreshold NOTIFY configChanged)
    Q_PROPERTY(int borderMargin READ borderMargin WRITE setBorderMargin NOTIFY configChanged)
    Q_PROPERTY(int maxTotalFeatures READ maxTotalFeatures WRITE setMaxTotalFeatures NOTIFY configChanged)
    Q_PROPERTY(int gridCols READ gridCols WRITE setGridCols NOTIFY configChanged)
    Q_PROPERTY(int gridRows READ gridRows WRITE setGridRows NOTIFY configChanged)
    Q_PROPERTY(int maxFeaturesPerCell READ maxFeaturesPerCell WRITE setMaxFeaturesPerCell NOTIFY configChanged)
    Q_PROPERTY(double minFeatureDistance READ minFeatureDistance WRITE setMinFeatureDistance NOTIFY configChanged)

    // 4. Optical Flow Tracker
    Q_PROPERTY(int flowWindowHalfWidth READ flowWindowHalfWidth WRITE setFlowWindowHalfWidth NOTIFY configChanged)
    Q_PROPERTY(int numPyramidLevels READ numPyramidLevels WRITE setNumPyramidLevels NOTIFY configChanged)
    Q_PROPERTY(int maxFlowIterations READ maxFlowIterations WRITE setMaxFlowIterations NOTIFY configChanged)
    Q_PROPERTY(double minEpsilon READ minEpsilon WRITE setMinEpsilon NOTIFY configChanged)
    Q_PROPERTY(double minEigenvalue READ minEigenvalue WRITE setMinEigenvalue NOTIFY configChanged)
    Q_PROPERTY(double maxFbError READ maxFbError WRITE setMaxFbError NOTIFY configChanged)
    Q_PROPERTY(bool enableBidirectional READ enableBidirectional WRITE setEnableBidirectional NOTIFY configChanged)

    // 5. Motion Estimator
    Q_PROPERTY(double inlierThreshold READ inlierThreshold WRITE setInlierThreshold NOTIFY configChanged)
    Q_PROPERTY(int maxMsacIterations READ maxMsacIterations WRITE setMaxMsacIterations NOTIFY configChanged)
    Q_PROPERTY(int minMsacIterations READ minMsacIterations WRITE setMinMsacIterations NOTIFY configChanged)
    Q_PROPERTY(double msacConfidence READ msacConfidence WRITE setMsacConfidence NOTIFY configChanged)
    Q_PROPERTY(bool enableRefinement READ enableRefinement WRITE setEnableRefinement NOTIFY configChanged)

    // 6. Warp Engine & Rolling Shutter
    Q_PROPERTY(int warpInterpolation READ warpInterpolation WRITE setWarpInterpolation NOTIFY configChanged)
    Q_PROPERTY(int borderMode READ borderMode WRITE setBorderMode NOTIFY configChanged)
    Q_PROPERTY(int borderValue READ borderValue WRITE setBorderValue NOTIFY configChanged)
    Q_PROPERTY(double warpCropRatio READ warpCropRatio WRITE setWarpCropRatio NOTIFY configChanged)
    Q_PROPERTY(bool enableAutoCrop READ enableAutoCrop WRITE setEnableAutoCrop NOTIFY configChanged)
    Q_PROPERTY(bool enableRollingShutter READ enableRollingShutter WRITE setEnableRollingShutter NOTIFY configChanged)
    Q_PROPERTY(double rsReadoutRatio READ rsReadoutRatio WRITE setRsReadoutRatio NOTIFY configChanged)
    Q_PROPERTY(bool rsScanTopToBottom READ rsScanTopToBottom WRITE setRsScanTopToBottom NOTIFY configChanged)

public:
    explicit StabilizerConfigModel(QObject* parent = nullptr);
    ~StabilizerConfigModel() override = default;

    StabilizerConfigModel(const StabilizerConfigModel&) = delete;
    StabilizerConfigModel& operator=(const StabilizerConfigModel&) = delete;
    StabilizerConfigModel(StabilizerConfigModel&&) = delete;
    StabilizerConfigModel& operator=(StabilizerConfigModel&&) = delete;

    /// @brief Retrieve underlying strongly-typed C++ configuration.
    [[nodiscard]] Stabilizer::VideoStabilizerConfig toConfig() const noexcept;

    /// @brief Overwrite state with an external configuration.
    void loadFromConfig(const Stabilizer::VideoStabilizerConfig& config);

    /// @brief Reset specific section to factory default settings.
    Q_INVOKABLE void resetSection(int sectionIndex);

    /// @brief Reset all parameters to factory default settings.
    Q_INVOKABLE void resetAll();

    // Getters
    [[nodiscard]] int stabilizationMode() const noexcept;
    [[nodiscard]] int fallbackPolicy() const noexcept;
    [[nodiscard]] int minTrackedFeatures() const noexcept;
    [[nodiscard]] int replenishInterval() const noexcept;
    [[nodiscard]] int maxDegradedFrames() const noexcept;
    [[nodiscard]] double minInlierRatio() const noexcept;

    [[nodiscard]] int smoothingMethod() const noexcept;
    [[nodiscard]] double maxCropMargin() const noexcept;
    [[nodiscard]] double softMarginRatio() const noexcept;
    [[nodiscard]] double processNoiseBase() const noexcept;
    [[nodiscard]] double measurementNoise() const noexcept;
    [[nodiscard]] double ptzGain() const noexcept;
    [[nodiscard]] double ptzDeadband() const noexcept;
    [[nodiscard]] double focalLength() const noexcept;
    [[nodiscard]] int windowSize() const noexcept;

    [[nodiscard]] double qualityThreshold() const noexcept;
    [[nodiscard]] int borderMargin() const noexcept;
    [[nodiscard]] int maxTotalFeatures() const noexcept;
    [[nodiscard]] int gridCols() const noexcept;
    [[nodiscard]] int gridRows() const noexcept;
    [[nodiscard]] int maxFeaturesPerCell() const noexcept;
    [[nodiscard]] double minFeatureDistance() const noexcept;

    [[nodiscard]] int flowWindowHalfWidth() const noexcept;
    [[nodiscard]] int numPyramidLevels() const noexcept;
    [[nodiscard]] int maxFlowIterations() const noexcept;
    [[nodiscard]] double minEpsilon() const noexcept;
    [[nodiscard]] double minEigenvalue() const noexcept;
    [[nodiscard]] double maxFbError() const noexcept;
    [[nodiscard]] bool enableBidirectional() const noexcept;

    [[nodiscard]] double inlierThreshold() const noexcept;
    [[nodiscard]] int maxMsacIterations() const noexcept;
    [[nodiscard]] int minMsacIterations() const noexcept;
    [[nodiscard]] double msacConfidence() const noexcept;
    [[nodiscard]] bool enableRefinement() const noexcept;

    [[nodiscard]] int warpInterpolation() const noexcept;
    [[nodiscard]] int borderMode() const noexcept;
    [[nodiscard]] int borderValue() const noexcept;
    [[nodiscard]] double warpCropRatio() const noexcept;
    [[nodiscard]] bool enableAutoCrop() const noexcept;
    [[nodiscard]] bool enableRollingShutter() const noexcept;
    [[nodiscard]] double rsReadoutRatio() const noexcept;
    [[nodiscard]] bool rsScanTopToBottom() const noexcept;

    // Setters
    void setStabilizationMode(int mode);
    void setFallbackPolicy(int policy);
    void setMinTrackedFeatures(int count);
    void setReplenishInterval(int interval);
    void setMaxDegradedFrames(int count);
    void setMinInlierRatio(double ratio);

    void setSmoothingMethod(int method);
    void setMaxCropMargin(double margin);
    void setSoftMarginRatio(double ratio);
    void setProcessNoiseBase(double q);
    void setMeasurementNoise(double r);
    void setPtzGain(double gain);
    void setPtzDeadband(double deadband);
    void setFocalLength(double fl);
    void setWindowSize(int size);

    void setQualityThreshold(double val);
    void setBorderMargin(int margin);
    void setMaxTotalFeatures(int count);
    void setGridCols(int cols);
    void setGridRows(int rows);
    void setMaxFeaturesPerCell(int count);
    void setMinFeatureDistance(double dist);

    void setFlowWindowHalfWidth(int halfWidth);
    void setNumPyramidLevels(int levels);
    void setMaxFlowIterations(int iters);
    void setMinEpsilon(double eps);
    void setMinEigenvalue(double ev);
    void setMaxFbError(double err);
    void setEnableBidirectional(bool enable);

    void setInlierThreshold(double th);
    void setMaxMsacIterations(int iters);
    void setMinMsacIterations(int iters);
    void setMsacConfidence(double conf);
    void setEnableRefinement(bool enable);

    void setWarpInterpolation(int interp);
    void setBorderMode(int mode);
    void setBorderValue(int val);
    void setWarpCropRatio(double ratio);
    void setEnableAutoCrop(bool enable);
    void setEnableRollingShutter(bool enable);
    void setRsReadoutRatio(double ratio);
    void setRsScanTopToBottom(bool topToBottom);

signals:
    /// @brief Emitted whenever any parameter changes.
    void configChanged();

private:
    Stabilizer::VideoStabilizerConfig config_ { };
};

} // namespace StabilizerApp
