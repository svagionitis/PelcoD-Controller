#pragma once

/// @file DctSharpnessEvaluator.h
/// @brief 2D 8x8 DCT-based optical sharpness and focus score evaluator.

#include "Dct.h"
#include <cstddef>
#include <cstdint>

namespace PelcoD {

/// @enum DctMetricMode
/// @brief Frequency accumulation mode for DCT focus score calculation.
enum class DctMetricMode {
    HighFrequencyAcSum, ///< Sum of absolute AC coefficients where (u + v) >= minFrequencySum.
    HighFrequencyEnergy, ///< Sum of squared AC coefficients where (u + v) >= minFrequencySum.
    ModifiedDct ///< Absolute difference sum measuring local high-frequency curvature.
};

/// @struct DctSharpnessConfig
/// @brief Configuration settings for DCT sharpness evaluation.
struct DctSharpnessConfig {
    DctMetricMode mode { DctMetricMode::HighFrequencyAcSum };
    int minFrequencySum { 3 }; ///< Diagonal high-pass threshold (u + v >= minFrequencySum).
    int blockStride { 8 }; ///< Horizontal and vertical step size in pixels (default 8 = non-overlapping).
    double roiNormX { 0.25 }; ///< Normalized ROI left boundary [0.0, 1.0].
    double roiNormY { 0.25 }; ///< Normalized ROI top boundary [0.0, 1.0].
    double roiNormWidth { 0.50 }; ///< Normalized ROI width [0.0, 1.0] (default center 50%).
    double roiNormHeight { 0.50 }; ///< Normalized ROI height [0.0, 1.0] (default center 50%).
    bool enableNormalization { true }; ///< Normalize AC sharpness by DC luminance for contrast/illumination invariance.
};

/// @struct SharpnessResult
/// @brief Evaluation results containing raw and normalized optical focus scores.
struct SharpnessResult {
    double rawScore { 0.0 }; ///< Total unnormalized high-frequency AC energy.
    double normalizedScore { 0.0 }; ///< Contrast- and illumination-invariant sharpness metric.
    double meanLuminance { 0.0 }; ///< Average DC brightness across evaluated blocks.
    std::size_t blocksEvaluated { 0U }; ///< Total number of 8x8 blocks evaluated.
};

/// @class DctSharpnessEvaluator
/// @brief Evaluates optical sharpness and focus metrics across video frames using 2D 8x8 DCT.
class DctSharpnessEvaluator {
public:
    /// @brief Constructs a DCT sharpness evaluator with given configuration.
    explicit DctSharpnessEvaluator(DctSharpnessConfig config = {});

    /// @brief Evaluates the sharpness score of a grayscale image buffer.
    /// @param[in] grayPixels Pointer to contiguous 8-bit grayscale pixel data (Y channel).
    /// @param[in] width Frame width in pixels.
    /// @param[in] height Frame height in pixels.
    /// @param[in] stride Row stride in bytes (if 0, assumes stride == width).
    /// @return SharpnessResult containing raw and normalized focus metrics.
    [[nodiscard]] SharpnessResult evaluate(
        const uint8_t* grayPixels, int width, int height, int stride = 0) const noexcept;

    /// @brief Evaluates a single isolated 8x8 pixel block.
    /// @param[in] block Pointer to top-left pixel of 8x8 block.
    /// @param[in] stride Row stride in bytes.
    /// @return Unnormalized high-frequency AC score for the block.
    [[nodiscard]] double evaluateBlock(const uint8_t* block, int stride) const noexcept;

    /// @brief Reconfigures the sharpness evaluator.
    void setConfig(const DctSharpnessConfig& config) noexcept;

    /// @brief Returns the active configuration.
    [[nodiscard]] const DctSharpnessConfig& getConfig() const noexcept;

private:
    DctSharpnessConfig m_config {};
};

} // namespace PelcoD
