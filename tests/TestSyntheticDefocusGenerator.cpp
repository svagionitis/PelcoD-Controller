/// @file TestSyntheticDefocusGenerator.cpp
/// @brief Unit tests for SyntheticDefocusGenerator optical blur simulation and metric peak response.

#include <cstddef>
#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "FocusEvaluator.h"
#include "LensTypes.h"
#include "SyntheticDefocusGenerator.h"

namespace LensApp {
namespace {

    TEST(TestSyntheticDefocusGenerator, DefaultConfigurationIsValid)
    {
        const SyntheticDefocusConfig config { };
        EXPECT_TRUE(config.isValid());
        EXPECT_EQ(config.width, 1280U);
        EXPECT_EQ(config.height, 720U);
        EXPECT_EQ(config.targetPlaneFocus, 32768U);
    }

    TEST(TestSyntheticDefocusGenerator, InFocusPeakSharpnessResponse)
    {
        SyntheticDefocusConfig config { };
        config.width = 320U;
        config.height = 240U;
        config.noiseSigma = 0.0F; // Clean for metric determinism
        config.vignetteFactor = 0.0F;
        config.targetPlaneFocus = 32768U;

        SyntheticDefocusGenerator gen(config);

        const std::size_t byteCount = 320U * 240U * 3U;
        std::vector<std::uint8_t> frame(byteCount, 0U);
        std::vector<std::uint8_t> luma(320U * 240U, 0U);

        LensControl::FocusEvaluator evaluator;
        evaluator.setMetricType(LensControl::FocusMetricType::SobelGradientSquared);

        auto computeScore = [&](std::uint16_t focusPos) -> double {
            gen.updateMotorState(focusPos, 0U);
            EXPECT_TRUE(gen.generateNextFrame(frame.data(), 0.033));

            for (std::size_t i = 0U; i < 320U * 240U; ++i) {
                const std::size_t idx = i * 3U;
                luma[i] = static_cast<std::uint8_t>(
                    (77U * frame[idx] + 150U * frame[idx + 1U] + 29U * frame[idx + 2U]) >> 8U);
            }

            const auto score = evaluator.evaluate(luma.data(), 320U, 240U, 320U, 0U, 0U);
            return static_cast<double>(score.metric);
        };

        const double scoreInFocus = computeScore(32768U);
        const double scoreNear1 = computeScore(28000U);
        const double scoreNear2 = computeScore(20000U);
        const double scoreFar1 = computeScore(38000U);
        const double scoreFar2 = computeScore(46000U);

        // Peak must occur at target focal plane
        EXPECT_GT(scoreInFocus, scoreNear1);
        EXPECT_GT(scoreNear1, scoreNear2);
        EXPECT_GT(scoreInFocus, scoreFar1);
        EXPECT_GT(scoreFar1, scoreFar2);
    }

    TEST(TestSyntheticDefocusGenerator, ZoomCouplingNarrowsDepthOfField)
    {
        SyntheticDefocusConfig config { };
        config.width = 320U;
        config.height = 240U;
        config.noiseSigma = 0.0F;
        config.vignetteFactor = 0.0F;
        config.targetPlaneFocus = 32768U;
        config.zoomCoupling = 4.0F;

        SyntheticDefocusGenerator gen(config);

        const std::size_t byteCount = 320U * 240U * 3U;
        std::vector<std::uint8_t> frame(byteCount, 0U);
        std::vector<std::uint8_t> luma(320U * 240U, 0U);

        LensControl::FocusEvaluator evaluator;
        evaluator.setMetricType(LensControl::FocusMetricType::SobelGradientSquared);

        auto computeScoreWithZoom = [&](std::uint16_t focusPos, std::uint16_t zoomPos) -> double {
            gen.updateMotorState(focusPos, zoomPos);
            EXPECT_TRUE(gen.generateNextFrame(frame.data(), 0.033));

            for (std::size_t i = 0U; i < 320U * 240U; ++i) {
                const std::size_t idx = i * 3U;
                luma[i] = static_cast<std::uint8_t>(
                    (77U * frame[idx] + 150U * frame[idx + 1U] + 29U * frame[idx + 2U]) >> 8U);
            }

            const auto score = evaluator.evaluate(luma.data(), 320U, 240U, 320U, 0U, 0U);
            return static_cast<double>(score.metric);
        };

        // At same focus offset (30000 vs 32768), telephoto zoom (60000) should have much lower sharpness than wide zoom
        // (0)
        const double scoreWide = computeScoreWithZoom(30000U, 0U);
        const double scoreTele = computeScoreWithZoom(30000U, 60000U);

        EXPECT_GT(scoreWide, scoreTele);
    }

    TEST(TestSyntheticDefocusGenerator, PatternSwitchingAndNoiseInjection)
    {
        SyntheticDefocusConfig config { };
        config.width = 160U;
        config.height = 120U;
        config.pattern = DefocusPatternType::IsoChart;
        config.noiseSigma = 5.0F;

        SyntheticDefocusGenerator gen(config);

        const std::size_t byteCount = 160U * 120U * 3U;
        std::vector<std::uint8_t> frame(byteCount, 0U);

        const double pts1 = gen.generateNextFrame(frame.data(), 0.033);
        EXPECT_GT(pts1, 0.0);

        // Switch to natural texture
        config.pattern = DefocusPatternType::NaturalTexture;
        gen.setConfig(config);
        const double pts2 = gen.generateNextFrame(frame.data(), 0.033);
        EXPECT_GT(pts2, 0.0);
    }

} // namespace
} // namespace LensApp
