#pragma once

/// @file IntegralImage.h
/// @brief Header declaring Integral Images (Summed-Area Tables) and fast local window statistics.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Math {

/// @struct Rect
/// @brief 2D rectangle specification for bounding box queries.
struct Rect {
    int x { 0 }; ///< Horizontal coordinate of top-left corner.
    int y { 0 }; ///< Vertical coordinate of top-left corner.
    int width { 0 }; ///< Rectangle width in pixels.
    int height { 0 }; ///< Rectangle height in pixels.
};

/// @class IntegralImage
/// @brief Summed-Area Table (SAT) for O(1) rectangular box sums, means, variances, and adaptive filtering.
/// @details Precomputes cumulative distributions in O(W * H) single pass using 64-bit accumulators.
///          Supports arbitrary rectangular box queries in strict O(1) time without boundary branching.
class IntegralImage {
public:
    IntegralImage() = default;

    /// @brief Constructs integral tables from an 8-bit grayscale image buffer.
    /// @param[in] pixels Grayscale frame buffer.
    /// @param[in] width Frame width in pixels.
    /// @param[in] height Frame height in pixels.
    /// @param[in] stride Row pitch in bytes (if 0 or negative, stride == width).
    /// @param[in] computeSquared If true, also calculates squared integral table for variance queries.
    void compute(const std::uint8_t* pixels, int width, int height, int stride = 0, bool computeSquared = true);

    /// @brief Checks if the integral image contains valid computed data.
    [[nodiscard]] bool isValid() const noexcept;

    /// @brief Returns the width of the source image in pixels.
    [[nodiscard]] int getWidth() const noexcept;

    /// @brief Returns the height of the source image in pixels.
    [[nodiscard]] int getHeight() const noexcept;

    /// @brief Computes the sum of pixel values within a rectangular region in O(1) time.
    /// @param[in] x Top-left column index.
    /// @param[in] y Top-left row index.
    /// @param[in] w Rectangle width in pixels.
    /// @param[in] h Rectangle height in pixels.
    /// @return Sum of pixel values (clipped to image boundaries).
    [[nodiscard]] std::uint64_t computeSum(int x, int y, int w, int h) const noexcept;

    /// @brief Computes sum using Rect structure.
    /// @param[in] rect Rectangle bounding box.
    /// @return Sum of pixel values.
    [[nodiscard]] std::uint64_t computeSum(const Rect& rect) const noexcept;

    /// @brief Computes sum of squared pixel values within a rectangular region in O(1) time.
    /// @param[in] x Top-left column index.
    /// @param[in] y Top-left row index.
    /// @param[in] w Rectangle width in pixels.
    /// @param[in] h Rectangle height in pixels.
    /// @return Sum of squared pixel values (clipped to image boundaries).
    [[nodiscard]] std::uint64_t computeSquaredSum(int x, int y, int w, int h) const noexcept;

    /// @brief Computes squared sum using Rect structure.
    /// @param[in] rect Rectangle bounding box.
    /// @return Sum of squared pixel values.
    [[nodiscard]] std::uint64_t computeSquaredSum(const Rect& rect) const noexcept;

    /// @brief Computes the mean (average) pixel intensity within a rectangular region in O(1) time.
    /// @param[in] x Top-left column index.
    /// @param[in] y Top-left row index.
    /// @param[in] w Rectangle width in pixels.
    /// @param[in] h Rectangle height in pixels.
    /// @return Average pixel intensity in range [0.0, 255.0].
    [[nodiscard]] double computeMean(int x, int y, int w, int h) const noexcept;

    /// @brief Computes mean intensity using Rect structure.
    /// @param[in] rect Rectangle bounding box.
    /// @return Average pixel intensity.
    [[nodiscard]] double computeMean(const Rect& rect) const noexcept;

    /// @brief Computes the variance of pixel values within a rectangular region in O(1) time.
    /// @param[in] x Top-left column index.
    /// @param[in] y Top-left row index.
    /// @param[in] w Rectangle width in pixels.
    /// @param[in] h Rectangle height in pixels.
    /// @return Pixel variance (sigma^2).
    [[nodiscard]] double computeVariance(int x, int y, int w, int h) const noexcept;

    /// @brief Computes variance using Rect structure.
    /// @param[in] rect Rectangle bounding box.
    /// @return Pixel variance.
    [[nodiscard]] double computeVariance(const Rect& rect) const noexcept;

    /// @brief Computes the standard deviation within a rectangular region in O(1) time.
    /// @param[in] x Top-left column index.
    /// @param[in] y Top-left row index.
    /// @param[in] w Rectangle width in pixels.
    /// @param[in] h Rectangle height in pixels.
    /// @return Pixel standard deviation (sigma).
    [[nodiscard]] double computeStdDev(int x, int y, int w, int h) const noexcept;

    /// @brief Computes standard deviation using Rect structure.
    /// @param[in] rect Rectangle bounding box.
    /// @return Pixel standard deviation.
    [[nodiscard]] double computeStdDev(const Rect& rect) const noexcept;

    /// @brief Fast O(1)-per-pixel box blur filter independent of kernel radius.
    /// @param[out] dst Output grayscale buffer (must be at least width * height bytes).
    /// @param[in] radius Kernel radius in pixels (filter diameter = 2 * radius + 1).
    void boxBlur(std::uint8_t* dst, int radius) const;

    /// @brief Bradley-Roth adaptive thresholding for illumination-invariant binarization.
    /// @param[out] dst Output binary mask buffer (0 or 255).
    /// @param[in] windowSize Size of sliding window in pixels (default 16).
    /// @param[in] thresholdFraction Percentage drop below local average to binarize (default 0.15 = 15%).
    void adaptiveThreshold(std::uint8_t* dst, int windowSize = 16, double thresholdFraction = 0.15) const;

private:
    int m_width { 0 };
    int m_height { 0 };
    bool m_hasSquared { false };
    std::vector<std::uint64_t> m_sumTable;
    std::vector<std::uint64_t> m_sqSumTable;
    std::vector<std::uint8_t> m_sourcePixels;
};

} // namespace Math

namespace PelcoD {
namespace Math = ::Math;
using Rect = ::Math::Rect;
using IntegralImage = ::Math::IntegralImage;
} // namespace PelcoD
