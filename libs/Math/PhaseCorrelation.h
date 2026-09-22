#pragma once

/// @file PhaseCorrelation.h
/// @brief Header declaring 2D Phase Correlation Global Motion Estimator for camera translation estimation.

#include "Fft.h"

#include <cstddef>
#include <cstdint>

namespace Math {

/// @struct PhaseCorrelationConfig
/// @brief Configuration parameters for PhaseCorrelationEstimator.
struct PhaseCorrelationConfig {
    std::size_t gridWidth { 128U }; ///< Internal processing grid width (power of 2: 64, 128, 256).
    std::size_t gridHeight { 128U }; ///< Internal processing grid height (power of 2: 64, 128, 256).
    WindowType windowType { WindowType::Hann }; ///< 2D window to eliminate periodic boundary wraparound.
    double confidenceThreshold { 0.15 }; ///< Minimum normalized peak height to consider motion valid [0.0, 1.0].
    double maxTranslationFraction { 0.35 }; ///< Maximum allowable shift as fraction of frame dimensions.
};

/// @struct MotionResult
/// @brief Result of Phase Correlation global motion estimation.
struct MotionResult {
    double deltaX { 0.0 }; ///< Horizontal translation in source image pixels (positive = shifted right).
    double deltaY { 0.0 }; ///< Vertical translation in source image pixels (positive = shifted down).
    double peakCorrelation { 0.0 }; ///< Normalized peak correlation impulse height [0.0, 1.0].
    bool isConfident { false }; ///< True if peak meets confidence threshold and displacement bounds.
};

/// @class PhaseCorrelationEstimator
/// @brief Computes sub-pixel 2D translational motion between consecutive video frames using Phase Correlation.
/// @details Operates in frequency domain via separable 2D FFT and normalized cross-power spectrum.
///          Invariant to illumination, contrast, and brightness variations. Thread-safe.
class PhaseCorrelationEstimator {
public:
    /// @brief Constructs estimator with given configuration.
    /// @param[in] config Configuration settings.
    explicit PhaseCorrelationEstimator(PhaseCorrelationConfig config = {});

    /// @brief Estimates global translation from reference frame to current frame.
    /// @param[in] refPixels Grayscale pixel buffer of reference frame.
    /// @param[in] curPixels Grayscale pixel buffer of current frame.
    /// @param[in] width Frame width in pixels.
    /// @param[in] height Frame height in pixels.
    /// @param[in] stride Row pitch in bytes (if 0 or negative, assumes stride == width).
    /// @return MotionResult with sub-pixel translation (deltaX, deltaY) and confidence.
    [[nodiscard]] MotionResult estimateMotion(
        const std::uint8_t* refPixels, const std::uint8_t* curPixels, int width, int height, int stride = 0) const;

    /// @brief Updates estimator configuration.
    /// @param[in] config New configuration settings.
    void setConfig(const PhaseCorrelationConfig& config);

    /// @brief Gets active estimator configuration.
    /// @return Reference to active config.
    [[nodiscard]] const PhaseCorrelationConfig& getConfig() const noexcept;

private:
    PhaseCorrelationConfig m_config;
};

} // namespace Math

namespace PelcoD {
namespace Math = ::Math;
using PhaseCorrelationConfig = ::Math::PhaseCorrelationConfig;
using MotionResult = ::Math::MotionResult;
using PhaseCorrelationEstimator = ::Math::PhaseCorrelationEstimator;
} // namespace PelcoD
