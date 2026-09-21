/// @file TestPhaseCorrelation.cpp
/// @brief Automated unit test suite for 2D FFT and Phase Correlation Global Motion Estimator.

#include "Fft.h"
#include "PhaseCorrelation.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <cmath>
#include <iostream>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace PelcoD;
using namespace PelcoD::Math;

namespace {

TEST(PhaseCorrelationTest, Fft2DIdentity)
{
    std::cout << "[Test] testFft2DIdentity...\n";
    const std::size_t rows = 32U;
    const std::size_t cols = 64U;
    const std::size_t total = rows * cols;

    std::vector<Complex> original(total);
    for (std::size_t r = 0U; r < rows; ++r) {
        for (std::size_t c = 0U; c < cols; ++c) {
            const double v1 = std::sin(2.0 * M_PI * 3.0 * static_cast<double>(r) / static_cast<double>(rows));
            const double v2 = std::cos(2.0 * M_PI * 5.0 * static_cast<double>(c) / static_cast<double>(cols));
            original[r * cols + c] = Complex { v1 + v2, v1 * v2 };
        }
    }

    std::vector<Complex> transformed = original;
    fft2D(transformed, rows, cols, false);

    // Inverse transform must perfectly reconstruct original complex matrix
    fft2D(transformed, rows, cols, true);

    double maxError = 0.0;
    for (std::size_t i = 0U; i < total; ++i) {
        const double err = std::abs(transformed[i] - original[i]);
        if (err > maxError) {
            maxError = err;
        }
    }

    std::cout << "  -> Max reconstruction error: " << maxError << "\n";
    EXPECT_TRUE(maxError < 1e-9);
    std::cout << "  -> PASSED\n";
}

TEST(PhaseCorrelationTest, Window2D)
{
    std::cout << "[Test] testWindow2D...\n";
    const std::size_t rows = 32U;
    const std::size_t cols = 32U;
    std::vector<double> image(rows * cols, 100.0);

    applyWindow2D(image, rows, cols, WindowType::Hann);

    // Hann window is 0.0 at borders (index 0 and N-1)
    EXPECT_TRUE(std::abs(image[0]) < 1e-6);
    EXPECT_TRUE(std::abs(image[rows * cols - 1U]) < 1e-6);

    // Center value should remain close to 100.0
    const double centerVal = image[(rows / 2U) * cols + (cols / 2U)];
    EXPECT_TRUE(centerVal > 90.0);

    std::cout << "  -> PASSED\n";
}

TEST(PhaseCorrelationTest, ExactIntegerTranslation)
{
    std::cout << "[Test] testExactIntegerTranslation...\n";
    const int width = 128;
    const int height = 128;
    std::vector<std::uint8_t> ref(static_cast<std::size_t>(width * height), 0U);
    std::vector<std::uint8_t> cur(static_cast<std::size_t>(width * height), 0U);

    // Generate multi-frequency 2D texture pattern with broad spatial frequencies (no short repetition)
    auto textureFunc = [](double x, double y) -> double {
        const double v = std::sin(x * 0.06) * std::cos(y * 0.07) + std::sin(x * 0.13 + y * 0.11)
            + std::cos(x * 0.23 - y * 0.17) + 0.5 * std::sin(x * 0.41 + y * 0.37);
        return (v + 3.5) * (255.0 / 7.0);
    };

    const int shiftX = 7;
    const int shiftY = -4;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto uIdx = static_cast<std::size_t>(y * width + x);
            const double refVal = textureFunc(static_cast<double>(x), static_cast<double>(y));
            ref[uIdx] = static_cast<std::uint8_t>(std::clamp(refVal, 0.0, 255.0));

            // Shifted image: cur(x, y) = ref(x - shiftX, y - shiftY)
            const double curVal = textureFunc(static_cast<double>(x - shiftX), static_cast<double>(y - shiftY));
            cur[uIdx] = static_cast<std::uint8_t>(std::clamp(curVal, 0.0, 255.0));
        }
    }

    PhaseCorrelationConfig config {};
    config.gridWidth = 128U;
    config.gridHeight = 128U;
    PhaseCorrelationEstimator estimator(config);

    const MotionResult result = estimator.estimateMotion(ref.data(), cur.data(), width, height);

    std::cout << "  -> Detected shift: (" << result.deltaX << ", " << result.deltaY
              << "), Peak: " << result.peakCorrelation << ", Confident: " << result.isConfident << "\n";

    EXPECT_TRUE(result.isConfident);
    EXPECT_TRUE(std::abs(result.deltaX - static_cast<double>(shiftX)) < 0.15);
    EXPECT_TRUE(std::abs(result.deltaY - static_cast<double>(shiftY)) < 0.15);
    EXPECT_TRUE(result.peakCorrelation > 0.2);

    std::cout << "  -> PASSED\n";
}

TEST(PhaseCorrelationTest, SubPixelFractionalTranslation)
{
    std::cout << "[Test] testSubPixelFractionalTranslation...\n";
    const int width = 128;
    const int height = 128;
    const auto uWidth = static_cast<std::size_t>(width);
    const auto uHeight = static_cast<std::size_t>(height);
    std::vector<double> raw(uWidth * uHeight, 0.0);

    // Deterministic pseudo-random noise
    std::uint32_t seed = 12345U;
    auto prng = [&seed]() -> double {
        seed = seed * 1664525U + 1013904223U;
        return static_cast<double>(seed & 0xFFFFU) / 65535.0;
    };

    for (auto& val : raw) {
        val = prng() * 255.0;
    }

    // Two passes of 5x5 box filter to simulate realistic camera continuous textures
    auto boxFilter5 = [uWidth, uHeight](const std::vector<double>& in) {
        std::vector<double> out(uWidth * uHeight, 0.0);
        for (std::size_t y = 0U; y < uHeight; ++y) {
            for (std::size_t x = 0U; x < uWidth; ++x) {
                double sum = 0.0;
                int count = 0;
                for (int dy = -2; dy <= 2; ++dy) {
                    const int ny = static_cast<int>(y) + dy;
                    if (ny < 0 || ny >= static_cast<int>(uHeight)) {
                        continue;
                    }
                    for (int dx = -2; dx <= 2; ++dx) {
                        const int nx = static_cast<int>(x) + dx;
                        if (nx < 0 || nx >= static_cast<int>(uWidth)) {
                            continue;
                        }
                        sum += in[static_cast<std::size_t>(ny) * uWidth + static_cast<std::size_t>(nx)];
                        ++count;
                    }
                }
                out[y * uWidth + x] = sum / static_cast<double>(count);
            }
        }
        return out;
    };

    const auto filtered = boxFilter5(boxFilter5(raw));
    std::vector<std::uint8_t> ref(uWidth * uHeight, 0U);
    for (std::size_t i = 0U; i < uWidth * uHeight; ++i) {
        ref[i] = static_cast<std::uint8_t>(std::clamp(filtered[i], 0.0, 255.0));
    }

    const double trueShiftX = 3.4;
    const double trueShiftY = 2.6;

    std::vector<std::uint8_t> cur(uWidth * uHeight, 0U);
    for (std::size_t y = 0U; y < uHeight; ++y) {
        const double srcY = static_cast<double>(y) - trueShiftY;
        const double clampedY = std::clamp(srcY, 0.0, static_cast<double>(uHeight - 1U));
        const auto y0 = static_cast<std::size_t>(clampedY);
        const std::size_t y1 = std::min(y0 + 1U, uHeight - 1U);
        const double fy = clampedY - static_cast<double>(y0);

        for (std::size_t x = 0U; x < uWidth; ++x) {
            const double srcX = static_cast<double>(x) - trueShiftX;
            const double clampedX = std::clamp(srcX, 0.0, static_cast<double>(uWidth - 1U));
            const auto x0 = static_cast<std::size_t>(clampedX);
            const std::size_t x1 = std::min(x0 + 1U, uWidth - 1U);
            const double fx = clampedX - static_cast<double>(x0);

            const double val = (1.0 - fx) * (1.0 - fy) * static_cast<double>(ref[y0 * uWidth + x0])
                + fx * (1.0 - fy) * static_cast<double>(ref[y0 * uWidth + x1])
                + (1.0 - fx) * fy * static_cast<double>(ref[y1 * uWidth + x0])
                + fx * fy * static_cast<double>(ref[y1 * uWidth + x1]);

            cur[y * uWidth + x] = static_cast<std::uint8_t>(std::clamp(val, 0.0, 255.0));
        }
    }

    PhaseCorrelationConfig config {};
    config.gridWidth = 128U;
    config.gridHeight = 128U;
    PhaseCorrelationEstimator estimator(config);

    const MotionResult result = estimator.estimateMotion(ref.data(), cur.data(), width, height);

    std::cout << "  -> Expected: (" << trueShiftX << ", " << trueShiftY << "), Detected: (" << result.deltaX << ", "
              << result.deltaY << "), Peak: " << result.peakCorrelation << "\n";

    EXPECT_TRUE(result.isConfident);
    EXPECT_TRUE(std::abs(result.deltaX - trueShiftX) < 0.35);
    EXPECT_TRUE(std::abs(result.deltaY - trueShiftY) < 0.35);
    EXPECT_TRUE(result.peakCorrelation > 0.3);

    std::cout << "  -> PASSED\n";
}

TEST(PhaseCorrelationTest, IlluminationInvariance)
{
    std::cout << "[Test] testIlluminationInvariance...\n";
    const int width = 128;
    const int height = 128;
    std::vector<std::uint8_t> ref(static_cast<std::size_t>(width * height), 0U);
    std::vector<std::uint8_t> cur(static_cast<std::size_t>(width * height), 0U);

    auto textureFunc = [](double x, double y) -> double {
        const double v = std::sin(x * 0.06) * std::cos(y * 0.07) + std::sin(x * 0.13 + y * 0.11)
            + std::cos(x * 0.23 - y * 0.17) + 0.5 * std::sin(x * 0.41 + y * 0.37);
        return (v + 3.5) * (255.0 / 7.0);
    };

    const int shiftX = 5;
    const int shiftY = 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto uIdx = static_cast<std::size_t>(y * width + x);
            const double refVal = textureFunc(static_cast<double>(x), static_cast<double>(y));
            ref[uIdx] = static_cast<std::uint8_t>(std::clamp(refVal, 0.0, 255.0));

            // Drastic illumination change: halve contrast and add DC offset +60
            const double curVal
                = 0.5 * textureFunc(static_cast<double>(x - shiftX), static_cast<double>(y - shiftY)) + 60.0;
            cur[uIdx] = static_cast<std::uint8_t>(std::clamp(curVal, 0.0, 255.0));
        }
    }

    PhaseCorrelationEstimator estimator;
    const MotionResult result = estimator.estimateMotion(ref.data(), cur.data(), width, height);

    std::cout << "  -> Detected with severe illumination change: (" << result.deltaX << ", " << result.deltaY
              << "), Peak: " << result.peakCorrelation << "\n";

    EXPECT_TRUE(result.isConfident);
    EXPECT_TRUE(std::abs(result.deltaX - static_cast<double>(shiftX)) < 0.2);
    EXPECT_TRUE(std::abs(result.deltaY - static_cast<double>(shiftY)) < 0.2);
    EXPECT_TRUE(result.peakCorrelation > 0.4);

    std::cout << "  -> PASSED\n";
}

TEST(PhaseCorrelationTest, UncorrelatedSceneRejection)
{
    std::cout << "[Test] testUncorrelatedSceneRejection...\n";
    const int width = 128;
    const int height = 128;
    std::vector<std::uint8_t> ref(static_cast<std::size_t>(width * height), 0U);
    std::vector<std::uint8_t> cur(static_cast<std::size_t>(width * height), 0U);

    // Two orthogonal frequency patterns with zero cross-correlation
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto uIdx = static_cast<std::size_t>(y * width + x);
            ref[uIdx] = static_cast<std::uint8_t>(128.0 + 100.0 * std::sin(2.0 * M_PI * static_cast<double>(x) / 4.0));
            cur[uIdx] = static_cast<std::uint8_t>(128.0 + 100.0 * std::sin(2.0 * M_PI * static_cast<double>(y) / 19.0));
        }
    }

    PhaseCorrelationConfig config {};
    config.confidenceThreshold = 0.15;
    PhaseCorrelationEstimator estimator(config);

    const MotionResult result = estimator.estimateMotion(ref.data(), cur.data(), width, height);

    std::cout << "  -> Uncorrelated scene peak: " << result.peakCorrelation << ", isConfident: " << result.isConfident
              << "\n";

    EXPECT_TRUE(!result.isConfident);
    EXPECT_TRUE(result.peakCorrelation < 0.15);

    std::cout << "  -> PASSED\n";
}

TEST(PhaseCorrelationTest, NullOrInvalidInputs)
{
    std::cout << "[Test] testNullOrInvalidInputs...\n";
    PhaseCorrelationEstimator estimator;
    std::uint8_t dummy[16] { 0 };

    const MotionResult res1 = estimator.estimateMotion(nullptr, dummy, 4, 4);
    EXPECT_TRUE(!res1.isConfident);
    EXPECT_TRUE(res1.peakCorrelation == 0.0);

    const MotionResult res2 = estimator.estimateMotion(dummy, nullptr, 4, 4);
    EXPECT_TRUE(!res2.isConfident);

    const MotionResult res3 = estimator.estimateMotion(dummy, dummy, 0, 4);
    EXPECT_TRUE(!res3.isConfident);

    const MotionResult res4 = estimator.estimateMotion(dummy, dummy, 4, -1);
    EXPECT_TRUE(!res4.isConfident);

    std::cout << "  -> PASSED\n";
}

} // namespace

