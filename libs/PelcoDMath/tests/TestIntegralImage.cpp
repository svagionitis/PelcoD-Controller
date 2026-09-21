/// @file TestIntegralImage.cpp
/// @brief Automated unit test suite for Integral Images (Summed-Area Tables) in PelcoDCore.

#include "IntegralImage.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace PelcoD;

namespace {

void testKnownMatrixAnalytical()
{
    std::cout << "[Test] testKnownMatrixAnalytical...\n";
    // 4x4 matrix:
    //  1  2  3  4
    //  5  6  7  8
    //  9 10 11 12
    // 13 14 15 16
    const int width = 4;
    const int height = 4;
    std::vector<std::uint8_t> pixels(16);
    for (std::size_t i = 0U; i < 16U; ++i) {
        pixels[i] = static_cast<std::uint8_t>(i + 1U);
    }

    IntegralImage sat;
    sat.compute(pixels.data(), width, height, width, true);

    assert(sat.isValid());
    assert(sat.getWidth() == 4);
    assert(sat.getHeight() == 4);

    // Single pixel checks
    assert(sat.computeSum(0, 0, 1, 1) == 1ULL);
    assert(sat.computeSum(1, 1, 1, 1) == 6ULL);
    assert(sat.computeSum(3, 3, 1, 1) == 16ULL);

    // 2x2 sub-block at (1, 1) with width 2, height 2:
    // 6  7
    // 10 11 -> sum = 34
    assert(sat.computeSum(1, 1, 2, 2) == 34ULL);
    assert(sat.computeMean(1, 1, 2, 2) == 8.5);

    // Squared sum for 2x2 block: 6^2 + 7^2 + 10^2 + 11^2 = 36 + 49 + 100 + 121 = 306
    assert(sat.computeSquaredSum(1, 1, 2, 2) == 306ULL);

    // Variance for 2x2 block: (306 / 4) - 8.5^2 = 76.5 - 72.25 = 4.25
    assert(std::abs(sat.computeVariance(1, 1, 2, 2) - 4.25) < 1e-9);
    assert(std::abs(sat.computeStdDev(1, 1, 2, 2) - std::sqrt(4.25)) < 1e-9);

    // Entire 4x4 image
    // Sum = 16 * 17 / 2 = 136
    assert(sat.computeSum(0, 0, 4, 4) == 136ULL);
    assert(sat.computeMean(0, 0, 4, 4) == 8.5);

    // Squared sum = 16 * 17 * 33 / 6 = 1496
    assert(sat.computeSquaredSum(0, 0, 4, 4) == 1496ULL);

    // Variance = (1496 / 16) - (8.5)^2 = 93.5 - 72.25 = 21.25
    assert(std::abs(sat.computeVariance(0, 0, 4, 4) - 21.25) < 1e-9);

    // Rect overload check
    const Rect r { 1, 1, 2, 2 };
    assert(sat.computeSum(r) == 34ULL);
    assert(sat.computeMean(r) == 8.5);
    assert(sat.computeSquaredSum(r) == 306ULL);
    assert(std::abs(sat.computeVariance(r) - 4.25) < 1e-9);

    std::cout << "  -> PASSED\n";
}

void testBruteForceRandomBoxes()
{
    std::cout << "[Test] testBruteForceRandomBoxes...\n";
    const int width = 64;
    const int height = 48;
    const auto total = static_cast<std::size_t>(width * height);
    std::vector<std::uint8_t> pixels(total);

    std::uint32_t seed = 4242U;
    auto lcg = [&seed]() -> std::uint8_t {
        seed = seed * 1664525U + 1013904223U;
        return static_cast<std::uint8_t>((seed >> 16U) & 0xFFU);
    };

    for (std::size_t i = 0U; i < total; ++i) {
        pixels[i] = lcg();
    }

    IntegralImage sat;
    sat.compute(pixels.data(), width, height, width, true);

    // Test 50 random rectangular queries against brute-force loops
    for (int iter = 0; iter < 50; ++iter) {
        const int x = static_cast<int>(lcg() % width);
        const int y = static_cast<int>(lcg() % height);
        const int w = static_cast<int>(1 + (lcg() % (width - x)));
        const int h = static_cast<int>(1 + (lcg() % (height - y)));

        std::uint64_t expectedSum = 0U;
        std::uint64_t expectedSqSum = 0U;
        for (int r = y; r < y + h; ++r) {
            for (int c = x; c < x + w; ++c) {
                const auto val = static_cast<std::uint64_t>(pixels[static_cast<std::size_t>(r * width + c)]);
                expectedSum += val;
                expectedSqSum += (val * val);
            }
        }

        const std::uint64_t satSum = sat.computeSum(x, y, w, h);
        const std::uint64_t satSqSum = sat.computeSquaredSum(x, y, w, h);
        assert(satSum == expectedSum);
        assert(satSqSum == expectedSqSum);

        const double count = static_cast<double>(w * h);
        const double expectedMean = static_cast<double>(expectedSum) / count;
        const double satMean = sat.computeMean(x, y, w, h);
        assert(std::abs(satMean - expectedMean) < 1e-9);

        double expectedVar = (static_cast<double>(expectedSqSum) / count) - (expectedMean * expectedMean);
        expectedVar = std::max(0.0, expectedVar);
        const double satVar = sat.computeVariance(x, y, w, h);
        assert(std::abs(satVar - expectedVar) < 1e-8);
    }

    std::cout << "  -> PASSED\n";
}

void testBoundaryClippingAndInvalidInputs()
{
    std::cout << "[Test] testBoundaryClippingAndInvalidInputs...\n";
    const int width = 10;
    const int height = 10;
    std::vector<std::uint8_t> pixels(100, 10U); // Constant 10

    IntegralImage sat;
    sat.compute(pixels.data(), width, height);

    // Box partially outside top-left: [-2, -2, 5, 5] -> intersects [0, 0, 3, 3] = 3x3 = 9 pixels
    assert(sat.computeSum(-2, -2, 5, 5) == 90ULL);
    assert(sat.computeMean(-2, -2, 5, 5) == 10.0);

    // Box partially outside bottom-right: [8, 8, 5, 5] -> intersects [8, 8, 2, 2] = 2x2 = 4 pixels
    assert(sat.computeSum(8, 8, 5, 5) == 40ULL);
    assert(sat.computeMean(8, 8, 5, 5) == 10.0);

    // Box completely outside: [-20, -20, 5, 5]
    assert(sat.computeSum(-20, -20, 5, 5) == 0ULL);
    assert(sat.computeMean(-20, -20, 5, 5) == 0.0);

    // Degenerate zero or negative dimensions
    assert(sat.computeSum(2, 2, 0, 5) == 0ULL);
    assert(sat.computeSum(2, 2, 5, -1) == 0ULL);

    // Invalid uncomputed instance
    IntegralImage emptySat;
    assert(!emptySat.isValid());
    assert(emptySat.computeSum(0, 0, 5, 5) == 0ULL);
    assert(emptySat.computeMean(0, 0, 5, 5) == 0.0);

    std::cout << "  -> PASSED\n";
}

void testBoxBlurEquivalence()
{
    std::cout << "[Test] testBoxBlurEquivalence...\n";
    const int width = 32;
    const int height = 32;
    const auto total = static_cast<std::size_t>(width * height);
    std::vector<std::uint8_t> src(total);
    for (std::size_t i = 0U; i < total; ++i) {
        src[i] = static_cast<std::uint8_t>((i * 7U + 13U) % 256U);
    }

    IntegralImage sat;
    sat.compute(src.data(), width, height);

    const int radius = 2; // 5x5 box
    std::vector<std::uint8_t> blurSat(total, 0U);
    sat.boxBlur(blurSat.data(), radius);

    // Compare against naive direct 2D convolution
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int x0 = std::max(0, x - radius);
            const int y0 = std::max(0, y - radius);
            const int x1 = std::min(width - 1, x + radius);
            const int y1 = std::min(height - 1, y + radius);

            double sum = 0.0;
            int count = 0;
            for (int r = y0; r <= y1; ++r) {
                for (int c = x0; c <= x1; ++c) {
                    sum += static_cast<double>(src[static_cast<std::size_t>(r * width + c)]);
                    ++count;
                }
            }
            const auto expected = static_cast<std::uint8_t>(std::round(sum / static_cast<double>(count)));
            const std::uint8_t actual = blurSat[static_cast<std::size_t>(y * width + x)];
            assert(actual == expected);
        }
    }

    std::cout << "  -> PASSED\n";
}

void testAdaptiveThresholdBradley()
{
    std::cout << "[Test] testAdaptiveThresholdBradley...\n";
    const int width = 64;
    const int height = 64;
    std::vector<std::uint8_t> image(static_cast<std::size_t>(width * height));

    // Strong diagonal illumination gradient ramp: 20 to 200
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double ramp = 20.0 + 180.0 * (static_cast<double>(x + y) / static_cast<double>(width + height));
            image[static_cast<std::size_t>(y * width + x)] = static_cast<std::uint8_t>(ramp);
        }
    }

    // Insert high-contrast foreground spots (dark spot in dark region, dark spot in bright region)
    // Spot 1: dark spot in dark region (at x=10, y=10, background is ~45, spot is 10)
    for (int dy = 0; dy < 4; ++dy) {
        for (int dx = 0; dx < 4; ++dx) {
            image[static_cast<std::size_t>((10 + dy) * width + (10 + dx))] = 10U;
        }
    }

    // Spot 2: dark spot in bright region (at x=50, y=50, background is ~170, spot is 60)
    for (int dy = 0; dy < 4; ++dy) {
        for (int dx = 0; dx < 4; ++dx) {
            image[static_cast<std::size_t>((50 + dy) * width + (50 + dx))] = 60U;
        }
    }

    IntegralImage sat;
    sat.compute(image.data(), width, height);

    std::vector<std::uint8_t> binaryMask(static_cast<std::size_t>(width * height), 0U);
    sat.adaptiveThreshold(binaryMask.data(), 16, 0.15);

    // Both spots should be segmented as dark targets (0), while surrounding background is bright (255)
    assert(binaryMask[10 * width + 10] == 0U);
    assert(binaryMask[50 * width + 50] == 0U);

    // Background outside spots should be 255
    assert(binaryMask[25 * width + 25] == 255U);
    assert(binaryMask[60 * width + 60] == 255U);

    std::cout << "  -> PASSED\n";
}

void testLarge4KOverflowPrevention()
{
    std::cout << "[Test] testLarge4KOverflowPrevention...\n";
    const int width = 3840;
    const int height = 2160;
    const std::uint64_t totalPixels = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height); // 8,294,400

    // Constant buffer of 255
    std::vector<std::uint8_t> frame4k(static_cast<std::size_t>(totalPixels), 255U);

    IntegralImage sat;
    sat.compute(frame4k.data(), width, height, width, true);

    const std::uint64_t expectedTotalSum = totalPixels * 255ULL; // 2,115,072,000
    const std::uint64_t expectedTotalSqSum = totalPixels * (255ULL * 255ULL); // 539,343,360,000

    const std::uint64_t actualSum = sat.computeSum(0, 0, width, height);
    const std::uint64_t actualSqSum = sat.computeSquaredSum(0, 0, width, height);

    assert(actualSum == expectedTotalSum);
    assert(actualSqSum == expectedTotalSqSum);

    assert(std::abs(sat.computeMean(0, 0, width, height) - 255.0) < 1e-9);
    assert(sat.computeVariance(0, 0, width, height) == 0.0);

    std::cout << "  -> 4K Sum: " << actualSum << ", Squared Sum: " << actualSqSum << "\n";
    std::cout << "  -> PASSED\n";
}

} // namespace

int main()
{
    std::cout << "========================================\n";
    std::cout << "Running IntegralImage Unit Tests\n";
    std::cout << "========================================\n";

    testKnownMatrixAnalytical();
    testBruteForceRandomBoxes();
    testBoundaryClippingAndInvalidInputs();
    testBoxBlurEquivalence();
    testAdaptiveThresholdBradley();
    testLarge4KOverflowPrevention();

    std::cout << "========================================\n";
    std::cout << "ALL INTEGRAL IMAGE TESTS PASSED!\n";
    std::cout << "========================================\n";
    return 0;
}
