/// @file TestMultiZoneFocus.cpp
#include "MultiZoneFocusEvaluator.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace LensControl {
namespace {

    TEST(TestMultiZoneFocus, GridPartitioningCoversFrame)
    {
        FocusEvaluator evaluator { };
        MultiZoneFocusEvaluator multiZone { evaluator };

        constexpr std::size_t kWidth = 1920U;
        constexpr std::size_t kHeight = 1080U;

        for (std::size_t i = 0U; i < 9U; ++i) {
            const PixelRoi roi = MultiZoneFocusEvaluator::zonePixelRoi(i, kWidth, kHeight);
            EXPECT_TRUE(roi.isValid());
            EXPECT_LE(roi.x + roi.width, kWidth);
            EXPECT_LE(roi.y + roi.height, kHeight);
        }
    }

    TEST(TestMultiZoneFocus, CenterPrioritySelectsZone4)
    {
        FocusEvaluator evaluator { };
        MultiZoneFocusEvaluator multiZone { evaluator };
        multiZone.setArbitrationMode(MultiZoneArbitrationMode::CenterPriority);

        constexpr std::size_t kWidth = 90U;
        constexpr std::size_t kHeight = 90U;
        std::vector<std::uint8_t> frame(kWidth * kHeight, 128U);

        const MultiZoneResult result = multiZone.evaluateGrid(frame.data(), kWidth, kHeight);
        EXPECT_EQ(result.selectedZoneIndex, 4U);
    }

    TEST(TestMultiZoneFocus, TargetCenteringPrioritizesTargetZone)
    {
        FocusEvaluator evaluator { };
        MultiZoneFocusEvaluator multiZone { evaluator };
        multiZone.setArbitrationMode(MultiZoneArbitrationMode::TargetCentering);

        // Place target near top-left (Zone 0: x in [0..0.33], y in [0..0.33])
        multiZone.setTargetRoi(0.15F, 0.15F, 0.2F, 0.2F);

        constexpr std::size_t kWidth = 90U;
        constexpr std::size_t kHeight = 90U;
        std::vector<std::uint8_t> frame(kWidth * kHeight, 100U);

        const MultiZoneResult result = multiZone.evaluateGrid(frame.data(), kWidth, kHeight);
        EXPECT_EQ(result.selectedZoneIndex, 0U);
        EXPECT_GT(result.zones[0].targetWeight, 1.0F);
        EXPECT_LT(result.zones[8].targetWeight, 1.0F); // Peripheral attenuation
    }

    TEST(TestMultiZoneFocus, MotionSaliencyDetectsDynamicPixels)
    {
        FocusEvaluator evaluator { };
        MultiZoneFocusEvaluator multiZone { evaluator };
        multiZone.setArbitrationMode(MultiZoneArbitrationMode::MotionSaliency);

        constexpr std::size_t kWidth = 96U;
        constexpr std::size_t kHeight = 96U;
        std::vector<std::uint8_t> frame1(kWidth * kHeight, 100U);
        std::vector<std::uint8_t> frame2(kWidth * kHeight, 100U);

        // Frame 1: baseline
        const MultiZoneResult r1 = multiZone.evaluateGrid(frame1.data(), kWidth, kHeight);
        EXPECT_EQ(r1.selectedZoneIndex, 4U);

        // Frame 2: inject motion in Zone 2 (top-right: x in [64..95], y in [0..31])
        for (std::size_t y = 0U; y < 32U; ++y) {
            for (std::size_t x = 64U; x < 96U; ++x) {
                frame2[(y * kWidth) + x] = 220U;
            }
        }

        const MultiZoneResult result2 = multiZone.evaluateGrid(frame2.data(), kWidth, kHeight);
        EXPECT_GT(result2.zones[2].motionSaliency, 50.0F);
        EXPECT_EQ(result2.selectedZoneIndex, 2U);
    }

    TEST(TestMultiZoneFocus, ClosestSubjectForegroundArbitration)
    {
        FocusEvaluator evaluator { };
        MultiZoneFocusEvaluator multiZone { evaluator };
        multiZone.setArbitrationMode(MultiZoneArbitrationMode::ClosestSubject);

        constexpr std::size_t kWidth = 96U;
        constexpr std::size_t kHeight = 96U;

        std::vector<std::uint8_t> frameNearHigh(kWidth * kHeight, 128U);
        // Add strong high-contrast vertical bars in Zone 1 (top-center: x in [32..63], y in [0..31])
        for (std::size_t y = 0U; y < 32U; ++y) {
            for (std::size_t x = 32U; x < 64U; ++x) {
                frameNearHigh[(y * kWidth) + x] = static_cast<std::uint8_t>((x % 4U < 2U) ? 250U : 10U);
            }
        }

        std::vector<std::uint8_t> frameNearDrop(kWidth * kHeight, 128U);
        // Reduced contrast in Zone 1 to confirm peak
        for (std::size_t y = 0U; y < 32U; ++y) {
            for (std::size_t x = 32U; x < 64U; ++x) {
                frameNearDrop[(y * kWidth) + x] = static_cast<std::uint8_t>((x % 4U < 2U) ? 140U : 115U);
            }
        }

        // Step 1: Near position high metric
        const MultiZoneResult r1
            = multiZone.evaluateGrid(frameNearHigh.data(), kWidth, kHeight, 0U, MotorPosition { 1000U });
        (void)r1;

        // Step 2: Metric drops -> confirmed peak in Zone 1 at position 1000
        const MultiZoneResult r2
            = multiZone.evaluateGrid(frameNearDrop.data(), kWidth, kHeight, 0U, MotorPosition { 1200U });

        EXPECT_TRUE(r2.hasForegroundPeak);
        EXPECT_EQ(r2.selectedZoneIndex, 1U);
        EXPECT_EQ(r2.recommendedPosition.value(), 1000U);
    }

    TEST(TestMultiZoneFocus, ResetClearsInternalTracking)
    {
        FocusEvaluator evaluator { };
        MultiZoneFocusEvaluator multiZone { evaluator };

        constexpr std::size_t kWidth = 96U;
        constexpr std::size_t kHeight = 96U;
        std::vector<std::uint8_t> frame(kWidth * kHeight, 128U);

        const MultiZoneResult rInitial = multiZone.evaluateGrid(frame.data(), kWidth, kHeight);
        (void)rInitial;
        multiZone.reset();

        // After reset, no prev frame exists
        const MultiZoneResult r = multiZone.evaluateGrid(frame.data(), kWidth, kHeight);
        for (std::size_t i = 0U; i < 9U; ++i) {
            EXPECT_FLOAT_EQ(r.zones[i].motionSaliency, 0.0F);
        }
    }

} // namespace
} // namespace LensControl
