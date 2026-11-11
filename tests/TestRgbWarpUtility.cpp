/// @file TestRgbWarpUtility.cpp
/// @brief Unit tests for RgbWarpUtility bilinear warping and luma extraction.

#include <cstddef>
#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "RgbWarpUtility.h"
#include "StabilizerTypes.h"
#include "WarpEngine.h"

namespace StabilizerApp {

class RgbWarpUtilityTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        width_ = 64U;
        height_ = 64U;
        stride_ = width_ * 3U;
        totalBytes_ = static_cast<std::size_t>(stride_) * static_cast<std::size_t>(height_);

        src_.assign(totalBytes_, 0U);
        dst_.assign(totalBytes_, 0U);

        // Fill src with a recognizable pattern
        for (std::uint32_t y = 0U; y < height_; ++y) {
            for (std::uint32_t x = 0U; x < width_; ++x) {
                const std::size_t idx = (static_cast<std::size_t>(y) * stride_) + (static_cast<std::size_t>(x) * 3U);
                src_[idx] = static_cast<std::uint8_t>(x * 4U);
                src_[idx + 1U] = static_cast<std::uint8_t>(y * 4U);
                src_[idx + 2U] = static_cast<std::uint8_t>((x + y) * 2U);
            }
        }
    }

    std::uint32_t width_ { 64U };
    std::uint32_t height_ { 64U };
    std::uint32_t stride_ { 192U };
    std::size_t totalBytes_ { 0U };
    std::vector<std::uint8_t> src_ { };
    std::vector<std::uint8_t> dst_ { };
};

TEST_F(RgbWarpUtilityTest, ExtractLumaColorWeights)
{
    // Test pure colors
    std::vector<std::uint8_t> rgb(3U * 5U);
    std::vector<std::uint8_t> luma(5U);

    // 0: Red (255, 0, 0)
    rgb[0] = 255U;
    rgb[1] = 0U;
    rgb[2] = 0U;
    // 1: Green (0, 255, 0)
    rgb[3] = 0U;
    rgb[4] = 255U;
    rgb[5] = 0U;
    // 2: Blue (0, 0, 255)
    rgb[6] = 0U;
    rgb[7] = 0U;
    rgb[8] = 255U;
    // 3: White (255, 255, 255)
    rgb[9] = 255U;
    rgb[10] = 255U;
    rgb[11] = 255U;
    // 4: Black (0, 0, 0)
    rgb[12] = 0U;
    rgb[13] = 0U;
    rgb[14] = 0U;

    RgbWarpUtility::extractLuma(rgb.data(), luma.data(), 5U, 1U);

    // Y_red = (77*255 + 128) >> 8 = 77
    EXPECT_NEAR(luma[0], 77, 1);
    // Y_green = (150*255 + 128) >> 8 = 150
    EXPECT_NEAR(luma[1], 150, 1);
    // Y_blue = (29*255 + 128) >> 8 = 29
    EXPECT_NEAR(luma[2], 29, 1);
    // Y_white = 255
    EXPECT_EQ(luma[3], 255U);
    // Y_black = 0
    EXPECT_EQ(luma[4], 0U);
}

TEST_F(RgbWarpUtilityTest, IdentityWarpProducesExactCopy)
{
    Stabilizer::WarpConfig config;
    config.enableAutoCrop = false;
    config.borderMode = Stabilizer::BorderMode::Constant;

    const bool ok = RgbWarpUtility::warpRgb24(
        src_.data(), dst_.data(), width_, height_, stride_, Stabilizer::AffineTransform2D::identity(), config);

    ASSERT_TRUE(ok);
    EXPECT_EQ(src_, dst_);
}

TEST_F(RgbWarpUtilityTest, ConstantBorderModeFillsOutside)
{
    Stabilizer::WarpConfig config;
    config.enableAutoCrop = false;
    config.borderMode = Stabilizer::BorderMode::Constant;
    config.borderValue = 42U;

    // Translate by 20 pixels horizontally
    const Stabilizer::AffineTransform2D trans = Stabilizer::AffineTransform2D::translation(20.0F, 0.0F);

    const bool ok = RgbWarpUtility::warpRgb24(src_.data(), dst_.data(), width_, height_, stride_, trans, config);

    ASSERT_TRUE(ok);

    // Left border [0 .. 19] should be filled with borderValue 42
    for (std::uint32_t y = 0U; y < height_; ++y) {
        for (std::uint32_t x = 0U; x < 18U; ++x) {
            const std::size_t idx = (static_cast<std::size_t>(y) * stride_) + (static_cast<std::size_t>(x) * 3U);
            EXPECT_EQ(dst_[idx], 42U);
            EXPECT_EQ(dst_[idx + 1U], 42U);
            EXPECT_EQ(dst_[idx + 2U], 42U);
        }
    }
}

TEST_F(RgbWarpUtilityTest, InvalidDimensionsRejected)
{
    Stabilizer::WarpConfig config;
    EXPECT_FALSE(RgbWarpUtility::warpRgb24(
        nullptr, dst_.data(), width_, height_, stride_, Stabilizer::AffineTransform2D::identity(), config));
    EXPECT_FALSE(RgbWarpUtility::warpRgb24(
        src_.data(), nullptr, width_, height_, stride_, Stabilizer::AffineTransform2D::identity(), config));
    EXPECT_FALSE(RgbWarpUtility::warpRgb24(
        src_.data(), dst_.data(), 0U, height_, stride_, Stabilizer::AffineTransform2D::identity(), config));
    EXPECT_FALSE(RgbWarpUtility::warpRgb24(
        src_.data(), dst_.data(), width_, 0U, stride_, Stabilizer::AffineTransform2D::identity(), config));
}

} // namespace StabilizerApp
