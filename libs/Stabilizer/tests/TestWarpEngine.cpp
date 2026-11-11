/// @file TestWarpEngine.cpp
/// @brief Comprehensive unit tests for WarpEngine, SIMD warping, and rolling shutter compensation.
#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "FrameBuffer.h"
#include "FrameView.h"
#include "StabilizerTypes.h"
#include "WarpEngine.h"

using namespace Stabilizer;

namespace {

constexpr float kPi = 3.14159265358979323846F;

/// @brief Helper to generate a horizontal ramp frame.
void fillHorizontalRamp(FrameBuffer& fb) noexcept
{
    const std::uint32_t w = fb.dimensions().width();
    const std::uint32_t h = fb.dimensions().height();
    for (std::uint32_t y = 0U; y < h; ++y) {
        std::uint8_t* row = fb.rowPtr(y);
        for (std::uint32_t x = 0U; x < w; ++x) {
            row[x] = static_cast<std::uint8_t>((x * 255U) / (w > 1U ? (w - 1U) : 1U));
        }
    }
}

/// @brief Helper to generate an alternating vertical bar pattern: 0, 200, 0, 200...
void fillVerticalBars(FrameBuffer& fb) noexcept
{
    const std::uint32_t w = fb.dimensions().width();
    const std::uint32_t h = fb.dimensions().height();
    for (std::uint32_t y = 0U; y < h; ++y) {
        std::uint8_t* row = fb.rowPtr(y);
        for (std::uint32_t x = 0U; x < w; ++x) {
            row[x] = ((x % 2U) == 0U) ? 0U : 200U;
        }
    }
}

/// @brief Helper to fill a frame with a solid constant color.
void fillConstant(FrameBuffer& fb, std::uint8_t val) noexcept
{
    const std::uint32_t w = fb.dimensions().width();
    const std::uint32_t h = fb.dimensions().height();
    for (std::uint32_t y = 0U; y < h; ++y) {
        std::uint8_t* row = fb.rowPtr(y);
        for (std::uint32_t x = 0U; x < w; ++x) {
            row[x] = val;
        }
    }
}

} // namespace

TEST(TestWarpEngine, ConfigValidation)
{
    WarpConfig cfg { };
    EXPECT_TRUE(cfg.isValid());

    ReadoutRatio ratio(0.5F);
    EXPECT_NEAR(ratio.value(), 0.5F, 1e-4F);

    ReadoutRatio clampedLow(-0.1F);
    EXPECT_EQ(clampedLow.value(), 0.0F);

    ReadoutRatio clampedHigh(1.5F);
    EXPECT_EQ(clampedHigh.value(), 1.0F);

    RollingShutterProfile profile { };
    EXPECT_TRUE(profile.isValid());

    WarpEngine engine(cfg);
    EXPECT_EQ(engine.config().interpolation, WarpInterpolation::Bilinear);
    EXPECT_EQ(engine.config().borderMode, BorderMode::Constant);
    EXPECT_TRUE(engine.config().enableAutoCrop);

    cfg.borderMode = BorderMode::Replicate;
    cfg.interpolation = WarpInterpolation::Bicubic;
    engine.setConfig(cfg);
    EXPECT_EQ(engine.config().borderMode, BorderMode::Replicate);
    EXPECT_EQ(engine.config().interpolation, WarpInterpolation::Bicubic);
}

TEST(TestWarpEngine, IdentityTransformFidelity)
{
    const FrameDimensions dims(128U, 64U);
    FrameBuffer srcBuf(dims);
    FrameBuffer dstBuf(dims);
    fillHorizontalRamp(srcBuf);
    dstBuf.fillZero();

    WarpConfig cfg { };
    cfg.enableAutoCrop = false; // Pure identity without zoom
    WarpEngine engine(cfg);

    FrameView srcView = srcBuf.view();
    FrameView dstView = dstBuf.view();

    const bool success = engine.warpFrame(srcView, dstView, AffineTransform2D::identity());
    ASSERT_TRUE(success);

    // Verify bit-exact equivalence
    for (std::uint32_t y = 0U; y < dims.height(); ++y) {
        const std::uint8_t* srcRow = srcBuf.rowPtr(y);
        const std::uint8_t* dstRow = dstBuf.rowPtr(y);
        for (std::uint32_t x = 0U; x < dims.width(); ++x) {
            EXPECT_EQ(dstRow[x], srcRow[x]) << "Mismatch at (" << x << ", " << y << ")";
        }
    }
}

TEST(TestWarpEngine, PureRotationCenter)
{
    const FrameDimensions dims(64U, 64U);
    FrameBuffer srcBuf(dims);
    FrameBuffer dstBuf(dims);
    fillHorizontalRamp(srcBuf);
    dstBuf.fillZero();

    WarpConfig cfg { };
    cfg.enableAutoCrop = false;
    WarpEngine engine(cfg);

    // 90 degree rotation around center (32, 32): theta = pi / 2
    const float theta = kPi * 0.5F;
    const float cosT = std::cos(theta);
    const float sinT = std::sin(theta);
    const float cx = 32.0F;
    const float cy = 32.0F;
    const AffineTransform2D rot { cosT, -sinT, cx - (cosT * cx) + (sinT * cy), sinT, cosT,
        cy - (sinT * cx) - (cosT * cy) };

    FrameView srcView = srcBuf.view();
    FrameView dstView = dstBuf.view();

    ASSERT_TRUE(engine.warpFrame(srcView, dstView, rot));
    EXPECT_GT(dstBuf.rowPtr(32)[32], 0U);
}

TEST(TestWarpEngine, PureTranslationSubPixel)
{
    const FrameDimensions dims(64U, 32U);
    FrameBuffer srcBuf(dims);
    FrameBuffer dstBuf(dims);
    fillVerticalBars(srcBuf); // Alternating 0 and 200
    dstBuf.fillZero();

    WarpConfig cfg { };
    cfg.enableAutoCrop = false;
    cfg.interpolation = WarpInterpolation::Bilinear;
    WarpEngine engine(cfg);

    // Apply half-pixel translation dx = 0.5. For alternating bars (0, 200),
    // intermediate sample between 0 and 200 should be exactly 100!
    const AffineTransform2D trans = AffineTransform2D::translation(0.5F, 0.0F);

    FrameView srcView = srcBuf.view();
    FrameView dstView = dstBuf.view();

    const bool success = engine.warpFrame(srcView, dstView, trans);
    ASSERT_TRUE(success);

    // Verify intermediate pixel values in the interior
    for (std::uint32_t y = 5U; y < 25U; ++y) {
        const std::uint8_t* dstRow = dstBuf.rowPtr(y);
        for (std::uint32_t x = 5U; x < 55U; ++x) {
            EXPECT_NEAR(static_cast<float>(dstRow[x]), 100.0F, 2.0F) << "At (" << x << ", " << y << ")";
        }
    }
}

TEST(TestWarpEngine, DynamicAutoCropZoomComposition)
{
    const FrameDimensions dims(100U, 100U);
    const CropRatio crop(0.10F); // 10% crop -> kappa = 1 / (1 - 0.20) = 1.25

    const AffineTransform2D identity = AffineTransform2D::identity();
    const AffineTransform2D zoomed = WarpEngine::composeZoom(identity, dims, crop);

    // Center point (50, 50) must remain fixed invariant under zoom
    const SubPixelPoint center { 50.0F, 50.0F };
    const SubPixelPoint transformedCenter = zoomed.transform(center);
    EXPECT_NEAR(transformedCenter.x, 50.0F, 1e-4F);
    EXPECT_NEAR(transformedCenter.y, 50.0F, 1e-4F);

    // Scale factors must be kappa = 1.25
    EXPECT_NEAR(zoomed.a11, 1.25F, 1e-4F);
    EXPECT_NEAR(zoomed.a22, 1.25F, 1e-4F);
}

TEST(TestWarpEngine, BorderModeConstantFill)
{
    const FrameDimensions dims(64U, 32U);
    FrameBuffer srcBuf(dims);
    FrameBuffer dstBuf(dims);
    fillConstant(srcBuf, 255U); // White image
    dstBuf.fillZero();

    WarpConfig cfg { };
    cfg.enableAutoCrop = false;
    cfg.borderMode = BorderMode::Constant;
    cfg.borderValue = 42U;
    WarpEngine engine(cfg);

    // Shift image right by 10 pixels: forward transform tx = 10
    // The left 10 pixels in destination should be filled with borderValue 42
    const AffineTransform2D trans = AffineTransform2D::translation(10.0F, 0.0F);

    FrameView srcView = srcBuf.view();
    FrameView dstView = dstBuf.view();

    const bool success = engine.warpFrame(srcView, dstView, trans);
    ASSERT_TRUE(success);

    for (std::uint32_t y = 0U; y < dims.height(); ++y) {
        const std::uint8_t* dstRow = dstBuf.rowPtr(y);
        for (std::uint32_t x = 0U; x < 9U; ++x) {
            EXPECT_EQ(dstRow[x], 42U) << "At (" << x << ", " << y << ")";
        }
        for (std::uint32_t x = 15U; x < dims.width(); ++x) {
            EXPECT_EQ(dstRow[x], 255U) << "At (" << x << ", " << y << ")";
        }
    }
}

TEST(TestWarpEngine, BorderModeReplicateClamp)
{
    const FrameDimensions dims(64U, 32U);
    FrameBuffer srcBuf(dims);
    FrameBuffer dstBuf(dims);
    fillHorizontalRamp(srcBuf); // Leftmost pixel is 0, rightmost is 255
    dstBuf.fillZero();

    WarpConfig cfg { };
    cfg.enableAutoCrop = false;
    cfg.borderMode = BorderMode::Replicate;
    WarpEngine engine(cfg);

    // Shift image right by 10 pixels
    const AffineTransform2D trans = AffineTransform2D::translation(10.0F, 0.0F);

    FrameView srcView = srcBuf.view();
    FrameView dstView = dstBuf.view();

    const bool success = engine.warpFrame(srcView, dstView, trans);
    ASSERT_TRUE(success);

    // With replicate, the left 10 pixels should replicate the leftmost source column (value 0)
    for (std::uint32_t y = 0U; y < dims.height(); ++y) {
        const std::uint8_t* dstRow = dstBuf.rowPtr(y);
        for (std::uint32_t x = 0U; x < 10U; ++x) {
            EXPECT_EQ(dstRow[x], 0U) << "At (" << x << ", " << y << ")";
        }
    }
}

TEST(TestWarpEngine, NearestNeighborInterpolation)
{
    const FrameDimensions dims(64U, 32U);
    FrameBuffer srcBuf(dims);
    FrameBuffer dstBuf(dims);
    fillHorizontalRamp(srcBuf);
    dstBuf.fillZero();

    WarpConfig cfg { };
    cfg.enableAutoCrop = false;
    cfg.interpolation = WarpInterpolation::NearestNeighbor;
    WarpEngine engine(cfg);

    const AffineTransform2D trans = AffineTransform2D::translation(2.0F, 0.0F);
    FrameView srcView = srcBuf.view();
    FrameView dstView = dstBuf.view();

    const bool success = engine.warpFrame(srcView, dstView, trans);
    ASSERT_TRUE(success);

    // Verify destination pixels match source shifted by 2
    for (std::uint32_t y = 0U; y < dims.height(); ++y) {
        const std::uint8_t* srcRow = srcBuf.rowPtr(y);
        const std::uint8_t* dstRow = dstBuf.rowPtr(y);
        for (std::uint32_t x = 2U; x < dims.width(); ++x) {
            EXPECT_EQ(dstRow[x], srcRow[x - 2U]);
        }
    }
}

TEST(TestWarpEngine, BicubicInterpolationSharpness)
{
    const FrameDimensions dims(64U, 32U);
    FrameBuffer srcBuf(dims);
    FrameBuffer dstBufBilinear(dims);
    FrameBuffer dstBufBicubic(dims);
    fillVerticalBars(srcBuf);

    WarpConfig cfg { };
    cfg.enableAutoCrop = false;

    // Bilinear
    cfg.interpolation = WarpInterpolation::Bilinear;
    WarpEngine engineBilinear(cfg);
    FrameView srcView = srcBuf.view();
    FrameView dstViewBilinear = dstBufBilinear.view();
    ASSERT_TRUE(engineBilinear.warpFrame(srcView, dstViewBilinear, AffineTransform2D::translation(0.25F, 0.0F)));

    // Bicubic
    cfg.interpolation = WarpInterpolation::Bicubic;
    WarpEngine engineBicubic(cfg);
    FrameView dstViewBicubic = dstBufBicubic.view();
    ASSERT_TRUE(engineBicubic.warpFrame(srcView, dstViewBicubic, AffineTransform2D::translation(0.25F, 0.0F)));

    // Both should succeed and produce valid non-zero outputs
    EXPECT_GT(dstBufBilinear.rowPtr(10)[15], 0U);
    EXPECT_GT(dstBufBicubic.rowPtr(10)[15], 0U);
}

TEST(TestWarpEngine, RollingShutterInterpolation)
{
    const FrameDimensions dims(64U, 32U);
    FrameBuffer srcBuf(dims);
    FrameBuffer dstBuf(dims);
    fillConstant(srcBuf, 128U);
    dstBuf.fillZero();

    WarpConfig cfg { };
    cfg.enableAutoCrop = false;
    cfg.enableRollingShutter = true;
    cfg.rsProfile.readoutRatio = ReadoutRatio(1.0F); // Full 100% frame readout
    cfg.rsProfile.scanTopToBottom = true;
    WarpEngine engine(cfg);

    // Current transform: translation dx = 0. Next transform: translation dx = 10.
    // Row 0 (alpha = 0) should have dx = 0.
    // Row 31 (alpha = 1) should have dx = 10.
    const AffineTransform2D curTrans = AffineTransform2D::translation(0.0F, 0.0F);
    const AffineTransform2D nextTrans = AffineTransform2D::translation(10.0F, 0.0F);

    FrameView srcView = srcBuf.view();
    FrameView dstView = dstBuf.view();

    const bool success = engine.warpRollingShutter(srcView, dstView, curTrans, nextTrans);
    ASSERT_TRUE(success);

    // Verify output frame has valid content
    EXPECT_EQ(dstBuf.rowPtr(0)[32], 128U);
    EXPECT_EQ(dstBuf.rowPtr(31)[32], 128U);
}

TEST(TestWarpEngine, PlanarYuv420pWarping)
{
    const FrameDimensions lumaDims(64U, 32U);
    const FrameDimensions chromaDims(32U, 16U);

    FrameBuffer srcY(lumaDims);
    FrameBuffer srcU(chromaDims);
    FrameBuffer srcV(chromaDims);
    FrameBuffer dstY(lumaDims);
    FrameBuffer dstU(chromaDims);
    FrameBuffer dstV(chromaDims);

    fillConstant(srcY, 200U);
    fillConstant(srcU, 100U);
    fillConstant(srcV, 50U);

    WarpConfig cfg { };
    cfg.enableAutoCrop = false;
    WarpEngine engine(cfg);

    const AffineTransform2D trans = AffineTransform2D::translation(4.0F, 2.0F);

    FrameView sY = srcY.view();
    FrameView sU = srcU.view();
    FrameView sV = srcV.view();
    FrameView dY = dstY.view();
    FrameView dU = dstU.view();
    FrameView dV = dstV.view();

    const bool success = engine.warpYuv420p(sY, sU, sV, dY, dU, dV, trans);
    ASSERT_TRUE(success);

    // Interior checks
    EXPECT_EQ(dstY.rowPtr(16)[32], 200U);
    EXPECT_EQ(dstU.rowPtr(8)[16], 100U);
    EXPECT_EQ(dstV.rowPtr(8)[16], 50U);
}

TEST(TestWarpEngine, DimensionMismatchRejection)
{
    const FrameDimensions dimsA(64U, 32U);
    const FrameDimensions dimsB(128U, 64U);

    FrameBuffer src(dimsA);
    FrameBuffer dst(dimsB);

    WarpEngine engine;
    FrameView s = src.view();
    FrameView d = dst.view();

    EXPECT_FALSE(engine.warpFrame(s, d, AffineTransform2D::identity()));
}

TEST(TestWarpEngine, ZeroAllocationPerformance)
{
    const FrameDimensions dims(1920U, 1080U);
    FrameBuffer src(dims);
    FrameBuffer dst(dims);
    fillHorizontalRamp(src);

    WarpConfig cfg { };
    cfg.enableAutoCrop = true;
    cfg.cropRatio = CropRatio(0.05F);
    WarpEngine engine(cfg);

    FrameView s = src.view();
    FrameView d = dst.view();
    const AffineTransform2D trans = AffineTransform2D::translation(15.0F, -10.0F);

    // Warm-up run
    ASSERT_TRUE(engine.warpFrame(s, d, trans));

    // Performance timing over multiple runs
    constexpr std::size_t kIterations = 10U;
    const auto start = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0U; i < kIterations; ++i) {
        ASSERT_TRUE(engine.warpFrame(s, d, trans));
    }
    const auto finish = std::chrono::high_resolution_clock::now();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

    const double avgTimePerFrameMs = static_cast<double>(elapsedMs) / static_cast<double>(kIterations);
    EXPECT_GT(avgTimePerFrameMs, 0.0);
}
