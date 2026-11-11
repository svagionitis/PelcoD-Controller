#pragma once

/// @file FocusEvaluator.h
/// @brief Zero-heap optical sharpness and focus score evaluator for video frames.

#include <cstddef>
#include <cstdint>

#include "LensTypes.h"

namespace LensControl {

/// @struct PixelRoi
/// @brief Frame-space region of interest rectangle in absolute integer pixels.
struct PixelRoi {
    std::size_t x { 0U };
    std::size_t y { 0U };
    std::size_t width { 0U };
    std::size_t height { 0U };

    /// @brief Checks whether the pixel ROI has sufficient area for kernel evaluation.
    /// @return True if width and height are at least 3 pixels.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return width >= 3U && height >= 3U;
    }
};

/// @class FocusEvaluator
/// @brief High-performance, zero-heap optical focus and sharpness evaluator.
/// @details Evaluates spatial gradients, Laplacian curvature, Tenengrad noise gating,
///          Vollath autocorrelations, and 2D DCT high-frequency AC energy on 8-bit luma buffers.
class FocusEvaluator final {
public:
    /// @brief Constructs a FocusEvaluator with given metric type and default center ROI.
    /// @param metric Initial focus evaluation algorithm.
    explicit FocusEvaluator(FocusMetricType metric = FocusMetricType::SobelGradientSquared) noexcept;

    ~FocusEvaluator() noexcept = default;
    FocusEvaluator(const FocusEvaluator&) noexcept = default;
    FocusEvaluator& operator=(const FocusEvaluator&) noexcept = default;
    FocusEvaluator(FocusEvaluator&&) noexcept = default;
    FocusEvaluator& operator=(FocusEvaluator&&) noexcept = default;

    /// @brief Evaluates the focus sharpness score of an 8-bit grayscale image frame.
    /// @details Executes with strictly zero dynamic heap allocations.
    /// @param grayPixels Pointer to contiguous 8-bit grayscale pixel data.
    /// @param width Frame width in pixels.
    /// @param height Frame height in pixels.
    /// @param stride Row stride in bytes (if 0, assumes stride == width).
    /// @param timestampNs Frame acquisition timestamp in nanoseconds.
    /// @param frameId Monotonically increasing frame sequence identifier.
    /// @return FocusMetricScore containing sharpness metric, mean, variance, and pixel count.
    [[nodiscard]] FocusMetricScore evaluate(const std::uint8_t* grayPixels, std::size_t width, std::size_t height,
        std::size_t stride = 0U, std::uint64_t timestampNs = 0U, std::uint32_t frameId = 0U) const noexcept;

    /// @brief Sets the active focus evaluation algorithm.
    /// @param metric FocusMetricType enumeration value.
    void setMetricType(FocusMetricType metric) noexcept;

    /// @brief Returns the active focus evaluation algorithm.
    /// @return Currently configured FocusMetricType.
    [[nodiscard]] FocusMetricType metricType() const noexcept;

    /// @brief Sets the fractional Region of Interest.
    /// @param roi Normalized fractional coordinates in [0..255].
    void setRoi(NormalizedRoi roi) noexcept;

    /// @brief Returns the active fractional Region of Interest.
    /// @return Active NormalizedRoi structure.
    [[nodiscard]] NormalizedRoi roi() const noexcept;

    /// @brief Sets the noise threshold gate for Tenengrad metric calculation.
    /// @param threshold Minimum gradient magnitude squared to accumulate.
    void setNoiseThreshold(std::uint32_t threshold) noexcept;

    /// @brief Returns the Tenengrad noise gate threshold.
    /// @return Active noise threshold value.
    [[nodiscard]] std::uint32_t noiseThreshold() const noexcept;

    /// @brief Converts normalized fractional ROI to frame-bounded pixel coordinates.
    /// @param roi Normalized fractional coordinates [0..255].
    /// @param frameWidth Frame width in pixels.
    /// @param frameHeight Frame height in pixels.
    /// @return PixelRoi clamped to frame bounds.
    [[nodiscard]] static PixelRoi calculatePixelRoi(
        NormalizedRoi roi, std::size_t frameWidth, std::size_t frameHeight) noexcept;

private:
    [[nodiscard]] std::uint32_t evalSobel(
        const std::uint8_t* pixels, std::size_t stride, const PixelRoi& roi) const noexcept;

    [[nodiscard]] std::uint32_t evalLaplacian(
        const std::uint8_t* pixels, std::size_t stride, const PixelRoi& roi) const noexcept;

    [[nodiscard]] std::uint32_t evalTenengrad(
        const std::uint8_t* pixels, std::size_t stride, const PixelRoi& roi) const noexcept;

    [[nodiscard]] std::uint32_t evalVollathF4(
        const std::uint8_t* pixels, std::size_t stride, const PixelRoi& roi) const noexcept;

    [[nodiscard]] std::uint32_t evalVollathF5(
        const std::uint8_t* pixels, std::size_t stride, const PixelRoi& roi, std::uint32_t meanLuma) const noexcept;

    [[nodiscard]] std::uint32_t evalDct(
        const std::uint8_t* pixels, std::size_t width, std::size_t height, std::size_t stride) const noexcept;

    FocusMetricType m_metricType { FocusMetricType::SobelGradientSquared };
    NormalizedRoi m_roi { };
    std::uint32_t m_noiseThreshold { 0U };
};

} // namespace LensControl
