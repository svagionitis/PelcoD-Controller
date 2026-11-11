/// @file TestFramePyramid.cpp
/// @brief Unit tests for FramePyramid multi-scale Gaussian downsampling.
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "FrameBuffer.h"
#include "FramePyramid.h"
#include "FrameView.h"
#include "StabilizerTypes.h"

using namespace Stabilizer;

TEST(TestFramePyramid, LevelDimensions)
{
    FrameDimensions baseDims { 1920U, 1080U };
    FramePyramid pyramid { baseDims };

    EXPECT_EQ(pyramid.levelCount(), 3U);
    EXPECT_EQ(pyramid.baseDimensions(), baseDims);

    EXPECT_EQ(pyramid.levelDimensions(0U), FrameDimensions(1920U, 1080U));
    EXPECT_EQ(pyramid.levelDimensions(1U), FrameDimensions(960U, 540U));
    EXPECT_EQ(pyramid.levelDimensions(2U), FrameDimensions(480U, 270U));
    EXPECT_EQ(pyramid.levelDimensions(3U), FrameDimensions { });
}

TEST(TestFramePyramid, ConstantImageDownsampling)
{
    FrameDimensions dims { 128U, 128U };
    std::vector<std::uint8_t> testImage(dims.totalBytes(), 150U);
    FrameView baseView { testImage.data(), dims };

    FramePyramid pyramid { dims };
    ASSERT_TRUE(pyramid.build(baseView));

    FrameView lvl0 = pyramid.level(0U);
    FrameView lvl1 = pyramid.level(1U);
    FrameView lvl2 = pyramid.level(2U);

    EXPECT_EQ(lvl0.width(), 128U);
    EXPECT_EQ(lvl0.height(), 128U);
    EXPECT_EQ(lvl1.width(), 64U);
    EXPECT_EQ(lvl1.height(), 64U);
    EXPECT_EQ(lvl2.width(), 32U);
    EXPECT_EQ(lvl2.height(), 32U);

    // Constant 150 filtered by normalized binomial filter must remain 150
    EXPECT_EQ(lvl1.pixelClamped(10, 10), 150U);
    EXPECT_EQ(lvl1.pixelClamped(0, 0), 150U);
    EXPECT_EQ(lvl2.pixelClamped(5, 5), 150U);
}

TEST(TestFramePyramid, DimensionMismatchHandling)
{
    FrameDimensions dims { 128U, 128U };
    FramePyramid pyramid { dims };

    FrameDimensions wrongDims { 256U, 256U };
    std::vector<std::uint8_t> wrongImage(wrongDims.totalBytes(), 100U);
    FrameView wrongView { wrongImage.data(), wrongDims };

    EXPECT_FALSE(pyramid.build(wrongView));

    FrameView emptyView { };
    EXPECT_FALSE(pyramid.build(emptyView));
}
