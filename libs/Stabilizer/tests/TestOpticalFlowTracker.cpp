/// @file TestOpticalFlowTracker.cpp
/// @brief Unit tests for Pyramidal Lucas-Kanade optical flow tracker.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "FeatureDetector.h"
#include "FrameBuffer.h"
#include "FramePyramid.h"
#include "FrameView.h"
#include "OpticalFlowTracker.h"
#include "StabilizerArena.h"
#include "StabilizerTypes.h"

using namespace Stabilizer;

namespace {

void drawSmoothPattern(std::vector<std::uint8_t>& pixels, const FrameDimensions& dims, float offsetX, float offsetY)
{
    constexpr float kPi = 3.14159265358979323846F;
    for (std::uint32_t y = 0U; y < dims.height(); ++y) {
        const float fy = static_cast<float>(y) - offsetY;
        for (std::uint32_t x = 0U; x < dims.width(); ++x) {
            const float fx = static_cast<float>(x) - offsetX;

            const float v1 = std::sin(fx * (2.0F * kPi / 96.0F)) * std::cos(fy * (2.0F * kPi / 96.0F));
            const float v2 = std::sin((fx * (2.0F * kPi / 36.0F)) + (fy * (2.0F * kPi / 48.0F)));
            const float val = 128.0F + (60.0F * v1) + (50.0F * v2);
            const auto clamped = static_cast<std::uint8_t>(std::clamp(val, 0.0F, 255.0F));
            pixels[(static_cast<std::size_t>(y) * dims.stride()) + x] = clamped;
        }
    }
}

void drawCheckerboard(std::vector<std::uint8_t>& pixels, const FrameDimensions& dims, float offsetX, float offsetY)
{
    for (std::uint32_t y = 0U; y < dims.height(); ++y) {
        for (std::uint32_t x = 0U; x < dims.width(); ++x) {
            const auto shiftedX = static_cast<std::int32_t>(std::floor(static_cast<float>(x) - offsetX));
            const auto shiftedY = static_cast<std::int32_t>(std::floor(static_cast<float>(y) - offsetY));
            const bool check = (((shiftedX / 16) + (shiftedY / 16)) % 2) != 0;
            pixels[(static_cast<std::size_t>(y) * dims.stride()) + x] = check ? 220U : 35U;
        }
    }
}

void drawSpot(std::vector<std::uint8_t>& pixels, const FrameDimensions& dims, float cx, float cy, float radius)
{
    const auto rSq = radius * radius;
    for (std::uint32_t y = 0U; y < dims.height(); ++y) {
        for (std::uint32_t x = 0U; x < dims.width(); ++x) {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float dSq = (dx * dx) + (dy * dy);
            if (dSq <= rSq) {
                const auto val = static_cast<std::uint8_t>(255.0F * (1.0F - (dSq / rSq)));
                const std::size_t idx = (static_cast<std::size_t>(y) * dims.stride()) + x;
                pixels[idx] = std::max(pixels[idx], val);
            }
        }
    }
}

} // namespace

TEST(TestOpticalFlowTracker, ConfigValidation)
{
    OpticalFlowConfig defaultCfg { };
    EXPECT_TRUE(defaultCfg.isValid());

    OpticalFlowConfig invalidLevels = defaultCfg;
    invalidLevels.numPyramidLevels = 0U;
    EXPECT_FALSE(invalidLevels.isValid());

    invalidLevels.numPyramidLevels = 4U;
    EXPECT_FALSE(invalidLevels.isValid());

    OpticalFlowConfig invalidIter = defaultCfg;
    invalidIter.maxIterations = 0U;
    EXPECT_FALSE(invalidIter.isValid());

    invalidIter.maxIterations = 101U;
    EXPECT_FALSE(invalidIter.isValid());

    OpticalFlowConfig invalidEps = defaultCfg;
    invalidEps.minEpsilon = 0.0F;
    EXPECT_FALSE(invalidEps.isValid());

    OpticalFlowConfig invalidEigen = defaultCfg;
    invalidEigen.minEigenvalue = 0.0F;
    EXPECT_FALSE(invalidEigen.isValid());

    OpticalFlowConfig invalidFb = defaultCfg;
    invalidFb.maxForwardBackwardError = 0.01F;
    EXPECT_FALSE(invalidFb.isValid());

    OpticalFlowTracker tracker { defaultCfg };
    EXPECT_TRUE(tracker.config().isValid());
    EXPECT_FALSE(tracker.setConfig(invalidLevels));
    EXPECT_TRUE(tracker.setConfig(defaultCfg));
}

TEST(TestOpticalFlowTracker, FlowWindowSizeBounds)
{
    FlowWindowSize minWin { 1U };
    EXPECT_EQ(minWin.halfWidth(), 2U);
    EXPECT_EQ(minWin.fullWidth(), 5U);
    EXPECT_EQ(minWin.totalPixels(), 25U);

    FlowWindowSize maxWin { 25U };
    EXPECT_EQ(maxWin.halfWidth(), 16U);
    EXPECT_EQ(maxWin.fullWidth(), 33U);
    EXPECT_EQ(maxWin.totalPixels(), 33U * 33U);

    FlowWindowSize normWin { 5U };
    EXPECT_EQ(normWin.halfWidth(), 5U);
    EXPECT_EQ(normWin.fullWidth(), 11U);
    EXPECT_EQ(normWin.totalPixels(), 121U);
}

TEST(TestOpticalFlowTracker, IdentityZeroMotion)
{
    const FrameDimensions dims { 256U, 256U };
    std::vector<std::uint8_t> pixels(dims.totalBytes(), 0U);
    drawCheckerboard(pixels, dims, 0.0F, 0.0F);

    FrameView view { pixels.data(), dims };
    FramePyramid pyr1 { dims };
    FramePyramid pyr2 { dims };
    EXPECT_TRUE(pyr1.build(view));
    EXPECT_TRUE(pyr2.build(view));

    FeaturePoint features[4];
    features[0].pt = SubPixelPoint { 64.0F, 64.0F };
    features[0].score = 100.0F;
    features[1].pt = SubPixelPoint { 128.0F, 64.0F };
    features[1].score = 100.0F;
    features[2].pt = SubPixelPoint { 64.0F, 128.0F };
    features[2].score = 100.0F;
    features[3].pt = SubPixelPoint { 128.0F, 128.0F };
    features[3].score = 100.0F;

    StabilizerArena arena { 4U * 1024U * 1024U };
    OpticalFlowTracker tracker { };

    TrackedCorrespondence matches[4];
    const std::size_t tracked = tracker.trackFeatures(pyr1, pyr2, features, 4U, matches, arena);

    EXPECT_EQ(tracked, 4U);
    for (std::size_t i = 0U; i < 4U; ++i) {
        EXPECT_EQ(matches[i].status, TrackingStatus::Tracked);
        EXPECT_NEAR(matches[i].displacement.dx, 0.0F, 0.05F);
        EXPECT_NEAR(matches[i].displacement.dy, 0.0F, 0.05F);
        EXPECT_LE(matches[i].forwardBackwardError, 0.10F);
    }
}

TEST(TestOpticalFlowTracker, PureTranslationSubPixel)
{
    const FrameDimensions dims { 256U, 256U };
    const float shiftX = 0.75F;
    const float shiftY = -0.50F;

    std::vector<std::uint8_t> pixels1(dims.totalBytes(), 0U);
    std::vector<std::uint8_t> pixels2(dims.totalBytes(), 0U);

    drawSmoothPattern(pixels1, dims, 0.0F, 0.0F);
    drawSmoothPattern(pixels2, dims, shiftX, shiftY);

    FrameView view1 { pixels1.data(), dims };
    FrameView view2 { pixels2.data(), dims };

    FramePyramid pyr1 { dims };
    FramePyramid pyr2 { dims };
    EXPECT_TRUE(pyr1.build(view1));
    EXPECT_TRUE(pyr2.build(view2));

    FeaturePoint features[3];
    features[0].pt = SubPixelPoint { 80.0F, 80.0F };
    features[0].score = 100.0F;
    features[1].pt = SubPixelPoint { 112.0F, 96.0F };
    features[1].score = 100.0F;
    features[2].pt = SubPixelPoint { 144.0F, 128.0F };
    features[2].score = 100.0F;

    StabilizerArena arena { 4U * 1024U * 1024U };
    OpticalFlowConfig cfg { };
    cfg.enableBidirectional = true;
    cfg.maxForwardBackwardError = 0.50F;
    OpticalFlowTracker tracker { cfg };

    TrackedCorrespondence matches[3];
    const std::size_t tracked = tracker.trackFeatures(pyr1, pyr2, features, 3U, matches, arena);

    EXPECT_GE(tracked, 2U);
    for (std::size_t i = 0U; i < 3U; ++i) {
        if (matches[i].isTracked()) {
            EXPECT_NEAR(matches[i].displacement.dx, shiftX, 0.15F);
            EXPECT_NEAR(matches[i].displacement.dy, shiftY, 0.15F);
            EXPECT_LE(matches[i].forwardBackwardError, 0.50F);
        }
    }
}

TEST(TestOpticalFlowTracker, LargeDisplacementPyramid)
{
    const FrameDimensions dims { 384U, 384U };
    const float shiftX = 12.0F;
    const float shiftY = -8.0F;

    std::vector<std::uint8_t> pixels1(dims.totalBytes(), 0U);
    std::vector<std::uint8_t> pixels2(dims.totalBytes(), 0U);

    drawSmoothPattern(pixels1, dims, 0.0F, 0.0F);
    drawSmoothPattern(pixels2, dims, shiftX, shiftY);

    FrameView view1 { pixels1.data(), dims };
    FrameView view2 { pixels2.data(), dims };

    FramePyramid pyr1 { dims };
    FramePyramid pyr2 { dims };
    EXPECT_TRUE(pyr1.build(view1));
    EXPECT_TRUE(pyr2.build(view2));

    FeaturePoint features[2];
    features[0].pt = SubPixelPoint { 160.0F, 160.0F };
    features[0].score = 100.0F;
    features[1].pt = SubPixelPoint { 192.0F, 192.0F };
    features[1].score = 100.0F;

    StabilizerArena arena { 4U * 1024U * 1024U };

    // With 3 levels, coarse-to-fine should resolve the 12-pixel displacement
    OpticalFlowConfig cfg { };
    cfg.numPyramidLevels = 3U;
    cfg.windowSize = FlowWindowSize { 7U };
    OpticalFlowTracker tracker { cfg };

    TrackedCorrespondence matches[2];
    const std::size_t tracked = tracker.trackFeatures(pyr1, pyr2, features, 2U, matches, arena);

    EXPECT_GE(tracked, 1U);
    for (std::size_t i = 0U; i < 2U; ++i) {
        if (matches[i].isTracked()) {
            EXPECT_NEAR(matches[i].displacement.dx, shiftX, 0.5F);
            EXPECT_NEAR(matches[i].displacement.dy, shiftY, 0.5F);
        }
    }
}

TEST(TestOpticalFlowTracker, IllConditionedFlatRejection)
{
    const FrameDimensions dims { 128U, 128U };
    std::vector<std::uint8_t> flatPixels(dims.totalBytes(), 120U);

    FrameView view { flatPixels.data(), dims };
    FramePyramid pyr1 { dims };
    FramePyramid pyr2 { dims };
    EXPECT_TRUE(pyr1.build(view));
    EXPECT_TRUE(pyr2.build(view));

    FeaturePoint feature { };
    feature.pt = SubPixelPoint { 64.0F, 64.0F };
    feature.score = 50.0F;

    StabilizerArena arena { 1U * 1024U * 1024U };
    OpticalFlowTracker tracker { };

    TrackedCorrespondence match { };
    const std::size_t tracked = tracker.trackFeatures(pyr1, pyr2, &feature, 1U, &match, arena);

    EXPECT_EQ(tracked, 0U);
    EXPECT_EQ(match.status, TrackingStatus::LostIllConditioned);
}

TEST(TestOpticalFlowTracker, OutOfBoundsRejection)
{
    const FrameDimensions dims { 128U, 128U };
    std::vector<std::uint8_t> pixels(dims.totalBytes(), 0U);
    drawCheckerboard(pixels, dims, 0.0F, 0.0F);

    FrameView view { pixels.data(), dims };
    FramePyramid pyr1 { dims };
    FramePyramid pyr2 { dims };
    EXPECT_TRUE(pyr1.build(view));
    EXPECT_TRUE(pyr2.build(view));

    // Feature too close to the boundary (< window halfWidth)
    FeaturePoint features[2];
    features[0].pt = SubPixelPoint { 2.0F, 2.0F }; // Out of bounds
    features[0].score = 50.0F;
    features[1].pt = SubPixelPoint { 126.0F, 126.0F }; // Out of bounds
    features[1].score = 50.0F;

    StabilizerArena arena { 1U * 1024U * 1024U };
    OpticalFlowTracker tracker { };

    TrackedCorrespondence matches[2];
    const std::size_t tracked = tracker.trackFeatures(pyr1, pyr2, features, 2U, matches, arena);

    EXPECT_EQ(tracked, 0U);
    EXPECT_EQ(matches[0].status, TrackingStatus::LostOutOfBounds);
    EXPECT_EQ(matches[1].status, TrackingStatus::LostOutOfBounds);
}

TEST(TestOpticalFlowTracker, ForwardBackwardOcclusionRejection)
{
    const FrameDimensions dims { 200U, 200U };
    std::vector<std::uint8_t> pixels1(dims.totalBytes(), 0U);
    std::vector<std::uint8_t> pixels2(dims.totalBytes(), 0U);

    // Frame 1 has a bright spot at (100, 100)
    drawSpot(pixels1, dims, 100.0F, 100.0F, 12.0F);

    // Frame 2 has the spot at (100, 100) completely erased / replaced by a different pattern (simulating occlusion)
    drawSpot(pixels2, dims, 50.0F, 50.0F, 12.0F); // spot elsewhere

    FrameView view1 { pixels1.data(), dims };
    FrameView view2 { pixels2.data(), dims };

    FramePyramid pyr1 { dims };
    FramePyramid pyr2 { dims };
    EXPECT_TRUE(pyr1.build(view1));
    EXPECT_TRUE(pyr2.build(view2));

    FeaturePoint feature { };
    feature.pt = SubPixelPoint { 100.0F, 100.0F };
    feature.score = 150.0F;

    StabilizerArena arena { 2U * 1024U * 1024U };
    OpticalFlowConfig cfg { };
    cfg.enableBidirectional = true;
    cfg.maxForwardBackwardError = 0.50F;
    OpticalFlowTracker tracker { cfg };

    TrackedCorrespondence match { };
    const std::size_t tracked = tracker.trackFeatures(pyr1, pyr2, &feature, 1U, &match, arena);

    // Must be rejected because forward-backward loop consistency will fail
    EXPECT_EQ(tracked, 0U);
    EXPECT_TRUE((match.status == TrackingStatus::LostForwardBackward)
        || (match.status == TrackingStatus::LostConvergence) || (match.status == TrackingStatus::LostIllConditioned));
}

TEST(TestOpticalFlowTracker, InsufficientArenaGracefulFailure)
{
    const FrameDimensions dims { 128U, 128U };
    std::vector<std::uint8_t> pixels(dims.totalBytes(), 0U);
    drawCheckerboard(pixels, dims, 0.0F, 0.0F);

    FrameView view { pixels.data(), dims };
    FramePyramid pyr1 { dims };
    FramePyramid pyr2 { dims };
    EXPECT_TRUE(pyr1.build(view));
    EXPECT_TRUE(pyr2.build(view));

    FeaturePoint feature { };
    feature.pt = SubPixelPoint { 64.0F, 64.0F };
    feature.score = 50.0F;

    // Arena with only 16 bytes - cannot allocate patch buffers
    StabilizerArena smallArena { 16U };
    OpticalFlowTracker tracker { };

    TrackedCorrespondence match { };
    const std::size_t tracked = tracker.trackFeatures(pyr1, pyr2, &feature, 1U, &match, smallArena);

    // Must return 0 without crashing
    EXPECT_EQ(tracked, 0U);
}
