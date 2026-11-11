/// @file TestFeatureDetector.cpp
/// @brief Unit tests for Shi-Tomasi feature detector and grid bucketing.
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "FeatureDetector.h"
#include "FrameView.h"
#include "StabilizerArena.h"
#include "StabilizerTypes.h"

using namespace Stabilizer;

TEST(TestFeatureDetector, ConfigValidation)
{
    FeatureDetectorConfig defaultCfg { };
    EXPECT_TRUE(defaultCfg.isValid());

    FeatureDetectorConfig invalidCols = defaultCfg;
    invalidCols.bucketing.gridCols = 0U;
    EXPECT_FALSE(invalidCols.isValid());

    invalidCols.bucketing.gridCols = 33U;
    EXPECT_FALSE(invalidCols.isValid());

    FeatureDetectorConfig invalidMargin = defaultCfg;
    invalidMargin.borderMargin = 2U;
    EXPECT_FALSE(invalidMargin.isValid());

    invalidMargin.borderMargin = 65U;
    EXPECT_FALSE(invalidMargin.isValid());

    FeatureDetectorConfig invalidCapacity = defaultCfg;
    invalidCapacity.maxTotalFeatures = 5U;
    EXPECT_FALSE(invalidCapacity.isValid());

    FeatureDetectorConfig invalidDist = defaultCfg;
    invalidDist.bucketing.minDistance = -1.0F;
    EXPECT_FALSE(invalidDist.isValid());

    FeatureDetector detector { defaultCfg };
    EXPECT_TRUE(detector.config().isValid());

    EXPECT_FALSE(detector.setConfig(invalidCols));
    EXPECT_TRUE(detector.setConfig(defaultCfg));
}

TEST(TestFeatureDetector, FlatImageZeroFeatures)
{
    FrameDimensions dims { 200U, 150U };
    std::vector<std::uint8_t> flatPixels(dims.totalBytes(), 128U);
    FrameView view { flatPixels.data(), dims };

    StabilizerArena arena { 4U * 1024U * 1024U };
    FeatureDetector detector { };

    FeaturePoint features[100];
    const std::size_t count = detector.detectFeatures(view, arena, features, 100U);

    EXPECT_EQ(count, 0U);
}

TEST(TestFeatureDetector, SyntheticCornerDetection)
{
    // Create a 200x200 image with a white square on black background
    FrameDimensions dims { 200U, 200U };
    std::vector<std::uint8_t> pixels(dims.totalBytes(), 20U);

    // Draw white rectangle [50..149] x [50..149] -> corners at (50,50), (149,50), (50,149), (149,149)
    for (std::uint32_t y = 50U; y < 150U; ++y) {
        for (std::uint32_t x = 50U; x < 150U; ++x) {
            pixels[(static_cast<std::size_t>(y) * dims.stride()) + x] = 230U;
        }
    }

    FrameView view { pixels.data(), dims };
    StabilizerArena arena { 4U * 1024U * 1024U };

    FeatureDetectorConfig cfg { };
    cfg.borderMargin = 8U;
    cfg.quality = QualityThreshold { 0.05F };
    cfg.bucketing.minDistance = 10.0F;
    FeatureDetector detector { cfg };

    FeaturePoint features[64];
    const std::size_t count = detector.detectFeatures(view, arena, features, 64U);

    // Should detect at least the 4 primary corners of the rectangle
    EXPECT_GE(count, 4U);

    // Verify that detected features have valid coordinates and scores
    for (std::size_t i = 0U; i < count; ++i) {
        EXPECT_TRUE(features[i].isValid());
        EXPECT_GT(features[i].score, 0.0F);
        EXPECT_GE(features[i].pt.x, 8.0F);
        EXPECT_LE(features[i].pt.x, 192.0F);
        EXPECT_GE(features[i].pt.y, 8.0F);
        EXPECT_LE(features[i].pt.y, 192.0F);
    }
}

TEST(TestFeatureDetector, BorderMarginExclusion)
{
    // Image where corners are drawn inside border margin (< 8 px from boundary)
    FrameDimensions dims { 100U, 100U };
    std::vector<std::uint8_t> pixels(dims.totalBytes(), 0U);

    // Corner at (4, 4) - inside default borderMargin 8
    for (std::uint32_t y = 4U; y < 20U; ++y) {
        for (std::uint32_t x = 4U; x < 20U; ++x) {
            pixels[(static_cast<std::size_t>(y) * dims.stride()) + x] = 255U;
        }
    }

    FrameView view { pixels.data(), dims };
    StabilizerArena arena { 2U * 1024U * 1024U };

    FeatureDetectorConfig cfg { };
    cfg.borderMargin = 10U; // Exclude up to 10 px
    FeatureDetector detector { cfg };

    FeaturePoint features[32];
    const std::size_t count = detector.detectFeatures(view, arena, features, 32U);

    // Corner at (4,4) must be excluded because it is within borderMargin
    for (std::size_t i = 0U; i < count; ++i) {
        EXPECT_GE(features[i].pt.x, 10.0F);
        EXPECT_GE(features[i].pt.y, 10.0F);
    }
}

TEST(TestFeatureDetector, GridBucketingCap)
{
    FrameDimensions dims { 300U, 300U };
    std::vector<std::uint8_t> pixels(dims.totalBytes(), 0U);

    // Draw a checkerboard pattern inside cell (0, 0) to produce many corners
    for (std::uint32_t y = 20U; y < 80U; y += 8U) {
        for (std::uint32_t x = 20U; x < 80U; x += 8U) {
            for (std::uint32_t dy = 0U; dy < 4U; ++dy) {
                for (std::uint32_t dx = 0U; dx < 4U; ++dx) {
                    pixels[(static_cast<std::size_t>(y + dy) * dims.stride()) + (x + dx)] = 255U;
                }
            }
        }
    }

    FrameView view { pixels.data(), dims };
    StabilizerArena arena { 4U * 1024U * 1024U };

    FeatureDetectorConfig cfg { };
    cfg.bucketing.gridCols = 3U; // Each cell is 100x100
    cfg.bucketing.gridRows = 3U;
    cfg.bucketing.maxFeaturesPerCell = 4U; // Cap at 4 features per cell
    cfg.bucketing.minDistance = 2.0F;
    FeatureDetector detector { cfg };

    FeaturePoint features[100];
    const std::size_t count = detector.detectFeatures(view, arena, features, 100U);

    // Count how many features belong to cellId 0
    std::uint32_t cell0Count = 0U;
    for (std::size_t i = 0U; i < count; ++i) {
        if (features[i].cellId == 0U) {
            ++cell0Count;
        }
    }

    // Must strictly respect maxFeaturesPerCell
    EXPECT_LE(cell0Count, 4U);
}

TEST(TestFeatureDetector, MinDistanceEnforcement)
{
    FrameDimensions dims { 200U, 200U };
    std::vector<std::uint8_t> pixels(dims.totalBytes(), 0U);

    // Draw two identical bright spots separated by only 3 pixels
    pixels[(50U * dims.stride()) + 50U] = 255U;
    pixels[(50U * dims.stride()) + 53U] = 255U;

    FrameView view { pixels.data(), dims };
    StabilizerArena arena { 4U * 1024U * 1024U };

    FeatureDetectorConfig cfg { };
    cfg.bucketing.minDistance = 15.0F; // minDistance is 15 pixels
    FeatureDetector detector { cfg };

    FeaturePoint features[16];
    const std::size_t count = detector.detectFeatures(view, arena, features, 16U);

    // If any features are found, verify mutual distances
    for (std::size_t i = 0U; i < count; ++i) {
        for (std::size_t j = i + 1U; j < count; ++j) {
            if (features[i].cellId == features[j].cellId) {
                const float distSq = features[i].pt.distanceSquared(features[j].pt);
                EXPECT_GE(distSq, 15.0F * 15.0F - 0.1F);
            }
        }
    }
}

TEST(TestFeatureDetector, InsufficientArenaGracefulFailure)
{
    FrameDimensions dims { 200U, 200U };
    std::vector<std::uint8_t> pixels(dims.totalBytes(), 100U);
    FrameView view { pixels.data(), dims };

    // Arena with only 128 bytes - cannot allocate required buffers
    StabilizerArena smallArena { 128U };
    FeatureDetector detector { };

    FeaturePoint features[16];
    const std::size_t count = detector.detectFeatures(view, smallArena, features, 16U);

    // Must return 0 without throwing or crashing
    EXPECT_EQ(count, 0U);
}
