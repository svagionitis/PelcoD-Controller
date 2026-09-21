/// @file TestDctSharpness.cpp
/// @brief Automated unit test suite for 2D 8x8 DCT and DctSharpnessEvaluator in PelcoDCore.

#include "Dct.h"
#include "DctSharpnessEvaluator.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <cmath>
#include <iostream>
#include <vector>

using namespace PelcoD;

namespace {

TEST(DctSharpnessTest, DctIdctOrthogonality)
{
    std::cout << "[Test] testDctIdctOrthogonality...\n";
    double original[8][8] {};
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            original[i][j] = (i * 12.5) + (j * 7.3) - 45.0;
        }
    }

    double transformed[8][8] {};
    Math::dct8x8(original, transformed);

    double reconstructed[8][8] {};
    Math::idct8x8(transformed, reconstructed);

    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            EXPECT_TRUE(std::abs(original[i][j] - reconstructed[i][j]) < 1e-9);
        }
    }
    std::cout << "  -> PASSED\n";
}

TEST(DctSharpnessTest, SingleBlockEvaluation)
{
    std::cout << "[Test] testSingleBlockEvaluation...\n";
    DctSharpnessEvaluator evaluator;

    // 1. Completely flat / untextured block (all pixels = 128)
    std::uint8_t flatBlock[64] {};
    std::fill(flatBlock, flatBlock + 64, static_cast<std::uint8_t>(128));
    const double flatScore = evaluator.evaluateBlock(flatBlock, 8);
    std::cout << "  Flat block score: " << flatScore << " (expected ~0)\n";
    EXPECT_TRUE(flatScore < 1e-9);

    // 2. High-contrast sharp edge block (left half 20, right half 220)
    std::uint8_t edgeBlock[64] {};
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            edgeBlock[y * 8 + x] = (x < 4) ? 20 : 220;
        }
    }
    const double edgeScore = evaluator.evaluateBlock(edgeBlock, 8);
    std::cout << "  Sharp edge block score: " << edgeScore << " (expected high)\n";
    EXPECT_TRUE(edgeScore > 100.0);

    std::cout << "  -> PASSED\n";
}

TEST(DctSharpnessTest, DefocusBlurMonotonicity)
{
    std::cout << "[Test] testDefocusBlurMonotonicity...\n";
    const std::size_t width = 64U;
    const std::size_t height = 64U;

    // Synthesize high-frequency checkerboard pattern
    std::vector<std::uint8_t> sharpImage(width * height, 0);
    for (std::size_t y = 0U; y < height; ++y) {
        for (std::size_t x = 0U; x < width; ++x) {
            const bool check = ((x / 4U) + (y / 4U)) % 2U == 0U;
            sharpImage[y * width + x] = check ? 230U : 25U;
        }
    }

    // Apply mild 3x3 box blur
    std::vector<std::uint8_t> mildBlur(width * height, 0);
    for (std::size_t y = 1U; y < height - 1U; ++y) {
        for (std::size_t x = 1U; x < width - 1U; ++x) {
            int sum = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const std::size_t sy = static_cast<std::size_t>(static_cast<int>(y) + dy);
                    const std::size_t sx = static_cast<std::size_t>(static_cast<int>(x) + dx);
                    sum += sharpImage[sy * width + sx];
                }
            }
            mildBlur[y * width + x] = static_cast<std::uint8_t>(sum / 9);
        }
    }

    // Apply heavy 7x7 box blur
    std::vector<std::uint8_t> heavyBlur(width * height, 0);
    for (std::size_t y = 3U; y < height - 3U; ++y) {
        for (std::size_t x = 3U; x < width - 3U; ++x) {
            int sum = 0;
            for (int dy = -3; dy <= 3; ++dy) {
                for (int dx = -3; dx <= 3; ++dx) {
                    const std::size_t sy = static_cast<std::size_t>(static_cast<int>(y) + dy);
                    const std::size_t sx = static_cast<std::size_t>(static_cast<int>(x) + dx);
                    sum += sharpImage[sy * width + sx];
                }
            }
            heavyBlur[y * width + x] = static_cast<std::uint8_t>(sum / 49);
        }
    }

    DctSharpnessConfig cfg {};
    cfg.roiNormX = 0.1;
    cfg.roiNormY = 0.1;
    cfg.roiNormWidth = 0.8;
    cfg.roiNormHeight = 0.8;
    cfg.blockStride = 8;

    DctSharpnessEvaluator evaluator(cfg);

    const auto resSharp = evaluator.evaluate(
        sharpImage.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width));
    const auto resMild = evaluator.evaluate(
        mildBlur.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width));
    const auto resHeavy = evaluator.evaluate(
        heavyBlur.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width));

    std::cout << "  Sharp image score: " << resSharp.normalizedScore << "\n";
    std::cout << "  Mild blur score:  " << resMild.normalizedScore << "\n";
    std::cout << "  Heavy blur score: " << resHeavy.normalizedScore << "\n";

    // Strict monotonic decay as defocus blur increases
    EXPECT_TRUE(resSharp.normalizedScore > resMild.normalizedScore);
    EXPECT_TRUE(resMild.normalizedScore > resHeavy.normalizedScore);
    EXPECT_TRUE(resSharp.normalizedScore > 2.0 * resHeavy.normalizedScore);

    std::cout << "  -> PASSED\n";
}

TEST(DctSharpnessTest, IlluminationInvariance)
{
    std::cout << "[Test] testIlluminationInvariance...\n";
    const std::size_t width = 32U;
    const std::size_t height = 32U;

    // Base pattern: checkerboard with amplitude 200
    std::vector<std::uint8_t> bright(width * height, 0);
    std::vector<std::uint8_t> dim(width * height, 0);

    for (std::size_t y = 0U; y < height; ++y) {
        for (std::size_t x = 0U; x < width; ++x) {
            const bool check = ((x / 4U) + (y / 4U)) % 2U == 0U;
            bright[y * width + x] = check ? 200U : 40U;
            dim[y * width + x] = check ? 100U : 20U; // 50% scale
        }
    }

    DctSharpnessConfig cfg {};
    cfg.roiNormX = 0.0;
    cfg.roiNormY = 0.0;
    cfg.roiNormWidth = 1.0;
    cfg.roiNormHeight = 1.0;
    cfg.enableNormalization = true;

    DctSharpnessEvaluator evaluator(cfg);

    const auto resBright
        = evaluator.evaluate(bright.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width));
    const auto resDim
        = evaluator.evaluate(dim.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width));

    std::cout << "  Bright raw: " << resBright.rawScore << ", norm: " << resBright.normalizedScore << "\n";
    std::cout << "  Dim raw:    " << resDim.rawScore << ", norm: " << resDim.normalizedScore << "\n";

    // Raw score scales with brightness/contrast
    EXPECT_TRUE(std::abs((resBright.rawScore / resDim.rawScore) - 2.0) < 0.1);

    // Normalized score must remain invariant to illumination scaling
    EXPECT_TRUE(std::abs(resBright.normalizedScore - resDim.normalizedScore) < 0.01);

    std::cout << "  -> PASSED\n";
}

TEST(DctSharpnessTest, RoiTargeting)
{
    std::cout << "[Test] testRoiTargeting...\n";
    const std::size_t width = 64U;
    const std::size_t height = 64U;

    // Image where center is sharp pattern and background is uniform gray
    std::vector<std::uint8_t> image(width * height, 128U);
    for (std::size_t y = 20U; y < 44U; ++y) {
        for (std::size_t x = 20U; x < 44U; ++x) {
            const bool check = ((x / 4U) + (y / 4U)) % 2U == 0U;
            image[y * width + x] = check ? 230U : 25U;
        }
    }

    // Evaluator targeting center
    DctSharpnessConfig centerCfg {};
    centerCfg.roiNormX = 0.3;
    centerCfg.roiNormY = 0.3;
    centerCfg.roiNormWidth = 0.4;
    centerCfg.roiNormHeight = 0.4;
    DctSharpnessEvaluator centerEval(centerCfg);

    // Evaluator targeting corner (flat gray)
    DctSharpnessConfig cornerCfg {};
    cornerCfg.roiNormX = 0.0;
    cornerCfg.roiNormY = 0.0;
    cornerCfg.roiNormWidth = 0.25;
    cornerCfg.roiNormHeight = 0.25;
    DctSharpnessEvaluator cornerEval(cornerCfg);

    const auto centerRes
        = centerEval.evaluate(image.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width));
    const auto cornerRes
        = cornerEval.evaluate(image.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width));

    std::cout << "  Center target sharpness: " << centerRes.rawScore << "\n";
    std::cout << "  Corner background sharpness: " << cornerRes.rawScore << "\n";

    EXPECT_TRUE(centerRes.rawScore > 10.0);
    EXPECT_TRUE(cornerRes.rawScore < 1e-6);

    std::cout << "  -> PASSED\n";
}

} // namespace

