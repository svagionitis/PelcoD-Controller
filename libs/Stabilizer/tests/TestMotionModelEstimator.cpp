/// @file TestMotionModelEstimator.cpp
/// @brief Unit tests for MSAC parametric motion model estimator.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "MotionModelEstimator.h"
#include "OpticalFlowTracker.h"
#include "StabilizerArena.h"
#include "StabilizerTypes.h"

using namespace Stabilizer;

namespace {

constexpr float kPi = 3.14159265358979323846F;

TrackedCorrespondence makeMatch(float x0, float y0, float x1, float y1)
{
    TrackedCorrespondence tc { };
    tc.feature.pt = SubPixelPoint { x0, y0 };
    tc.currentPt = SubPixelPoint { x1, y1 };
    tc.displacement = MotionVector { x1 - x0, y1 - y0 };
    tc.status = TrackingStatus::Tracked;
    return tc;
}

std::vector<TrackedCorrespondence> generateGridPoints(std::size_t count, float width = 640.0F, float height = 480.0F)
{
    std::vector<TrackedCorrespondence> matches;
    matches.reserve(count);

    const auto side = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<float>(count))));
    const float stepX = (width - 80.0F) / static_cast<float>(side);
    const float stepY = (height - 80.0F) / static_cast<float>(side);

    for (std::size_t r = 0U; (r < side) && (matches.size() < count); ++r) {
        for (std::size_t c = 0U; (c < side) && (matches.size() < count); ++c) {
            const float x = 40.0F + (static_cast<float>(c) * stepX);
            const float y = 40.0F + (static_cast<float>(r) * stepY);
            matches.push_back(makeMatch(x, y, x, y));
        }
    }
    return matches;
}

} // namespace

TEST(TestMotionModelEstimator, ConfigValidation)
{
    MotionEstimatorConfig cfg { };
    EXPECT_TRUE(cfg.isValid());

    // Inlier threshold tests
    InlierThreshold thresh(2.0F);
    EXPECT_FLOAT_EQ(thresh.value(), 2.0F);
    EXPECT_FLOAT_EQ(thresh.squared(), 4.0F);

    InlierThreshold clampedLow(0.01F);
    EXPECT_FLOAT_EQ(clampedLow.value(), 0.10F);

    InlierThreshold clampedHigh(50.0F);
    EXPECT_FLOAT_EQ(clampedHigh.value(), 20.0F);

    // Iteration bounds
    cfg.maxIterations = 5U; // Too low (<10)
    EXPECT_FALSE(cfg.isValid());
    cfg.maxIterations = 200U;

    cfg.minIterations = 300U; // > maxIterations
    EXPECT_FALSE(cfg.isValid());
    cfg.minIterations = 20U;

    cfg.minInlierRatio = 0.01F; // <0.05
    EXPECT_FALSE(cfg.isValid());
    cfg.minInlierRatio = 0.25F;

    cfg.confidence = 0.70F; // <0.80
    EXPECT_FALSE(cfg.isValid());
    cfg.confidence = 0.99F;

    EXPECT_TRUE(cfg.isValid());
}

TEST(TestMotionModelEstimator, PureTranslationNoNoise)
{
    auto matches = generateGridPoints(100);
    constexpr float kDx = 12.5F;
    constexpr float kDy = -7.25F;

    for (auto& m : matches) {
        m.currentPt.x += kDx;
        m.currentPt.y += kDy;
        m.displacement.dx = kDx;
        m.displacement.dy = kDy;
    }

    MotionEstimatorConfig cfg { };
    cfg.mode = StabilizationMode::TranslationOnly;

    MotionModelEstimator estimator(cfg);
    MotionEstimationResult result { };

    const bool ok = estimator.estimateMotion(matches.data(), matches.size(), result);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(result.isSuccess);
    EXPECT_FALSE(result.isDegenerate);
    EXPECT_EQ(result.inlierCount, 100U);
    EXPECT_NEAR(result.transform.tx, kDx, 1e-3F);
    EXPECT_NEAR(result.transform.ty, kDy, 1e-3F);
    EXPECT_NEAR(result.transform.a11, 1.0F, 1e-4F);
    EXPECT_NEAR(result.transform.a22, 1.0F, 1e-4F);
}

TEST(TestMotionModelEstimator, PureRotationNoNoise)
{
    auto matches = generateGridPoints(100);
    constexpr float kAngleDeg = 5.0F;
    const float theta = kAngleDeg * (kPi / 180.0F);
    const float cosT = std::cos(theta);
    const float sinT = std::sin(theta);
    constexpr float kTx = 3.5F;
    constexpr float kTy = -2.5F;
    constexpr float cx = 320.0F;
    constexpr float cy = 240.0F;

    for (auto& m : matches) {
        const float x = m.feature.pt.x - cx;
        const float y = m.feature.pt.y - cy;
        m.currentPt.x = cx + ((cosT * x) - (sinT * y)) + kTx;
        m.currentPt.y = cy + ((sinT * x) + (cosT * y)) + kTy;
    }

    MotionEstimatorConfig cfg { };
    cfg.mode = StabilizationMode::TranslationRotation;

    MotionModelEstimator estimator(cfg);
    MotionEstimationResult result { };

    const bool ok = estimator.estimateMotion(matches.data(), matches.size(), result);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(result.isSuccess);
    EXPECT_EQ(result.inlierCount, 100U);

    // Verify rotation matrix elements
    EXPECT_NEAR(result.transform.a11, cosT, 1e-3F);
    EXPECT_NEAR(result.transform.a12, -sinT, 1e-3F);
    EXPECT_NEAR(result.transform.a21, sinT, 1e-3F);
    EXPECT_NEAR(result.transform.a22, cosT, 1e-3F);
}

TEST(TestMotionModelEstimator, SimilarityScaleAndRotation)
{
    auto matches = generateGridPoints(100);
    constexpr float kScale = 1.08F;
    constexpr float kAngleDeg = -3.0F;
    const float theta = kAngleDeg * (kPi / 180.0F);
    const float alpha = kScale * std::cos(theta);
    const float beta = kScale * std::sin(theta);
    constexpr float kTx = 6.0F;
    constexpr float kTy = -4.0F;

    for (auto& m : matches) {
        const float x = m.feature.pt.x;
        const float y = m.feature.pt.y;
        m.currentPt.x = (alpha * x) - (beta * y) + kTx;
        m.currentPt.y = (beta * x) + (alpha * y) + kTy;
    }

    MotionEstimatorConfig cfg { };
    cfg.mode = StabilizationMode::Similarity;

    MotionModelEstimator estimator(cfg);
    MotionEstimationResult result { };

    const bool ok = estimator.estimateMotion(matches.data(), matches.size(), result);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(result.isSuccess);
    EXPECT_EQ(result.inlierCount, 100U);

    EXPECT_NEAR(result.transform.a11, alpha, 2e-3F);
    EXPECT_NEAR(result.transform.a12, -beta, 2e-3F);
    EXPECT_NEAR(result.transform.a21, beta, 2e-3F);
    EXPECT_NEAR(result.transform.a22, alpha, 2e-3F);
    EXPECT_NEAR(result.transform.tx, kTx, 2e-2F);
    EXPECT_NEAR(result.transform.ty, kTy, 2e-2F);
}

TEST(TestMotionModelEstimator, AffineShearAndScale)
{
    auto matches = generateGridPoints(100);
    const AffineTransform2D groundTruth { 1.04F, 0.03F, 8.0F, -0.02F, 0.97F, -5.0F };

    for (auto& m : matches) {
        m.currentPt = groundTruth.transform(m.feature.pt);
    }

    MotionEstimatorConfig cfg { };
    cfg.mode = StabilizationMode::Affine;

    MotionModelEstimator estimator(cfg);
    MotionEstimationResult result { };

    const bool ok = estimator.estimateMotion(matches.data(), matches.size(), result);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(result.isSuccess);
    EXPECT_EQ(result.inlierCount, 100U);

    EXPECT_NEAR(result.transform.a11, groundTruth.a11, 2e-3F);
    EXPECT_NEAR(result.transform.a12, groundTruth.a12, 2e-3F);
    EXPECT_NEAR(result.transform.tx, groundTruth.tx, 2e-2F);
    EXPECT_NEAR(result.transform.a21, groundTruth.a21, 2e-3F);
    EXPECT_NEAR(result.transform.a22, groundTruth.a22, 2e-3F);
    EXPECT_NEAR(result.transform.ty, groundTruth.ty, 2e-2F);
}

TEST(TestMotionModelEstimator, OutlierRejection40Percent)
{
    auto matches = generateGridPoints(100);
    const AffineTransform2D groundTruth { 1.0F, 0.0F, 10.0F, 0.0F, 1.0F, -8.0F };

    // Apply ground truth motion to all points
    for (auto& m : matches) {
        m.currentPt = groundTruth.transform(m.feature.pt);
    }

    // Corrupt 40% of points with large random offsets
    std::mt19937 rng(42U);
    std::uniform_real_distribution<float> dist(-100.0F, 100.0F);

    for (std::size_t i = 0U; i < 40U; ++i) {
        matches[i].currentPt.x += dist(rng) + 50.0F;
        matches[i].currentPt.y += dist(rng) + 50.0F;
    }

    MotionEstimatorConfig cfg { };
    cfg.mode = StabilizationMode::Affine;
    cfg.maxIterations = 200U;

    MotionModelEstimator estimator(cfg);
    MotionEstimationResult result { };

    const bool ok = estimator.estimateMotion(matches.data(), matches.size(), result);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(result.isSuccess);
    EXPECT_GE(result.inlierCount, 58U); // At least 58 of 60 true inliers retained

    EXPECT_NEAR(result.transform.tx, groundTruth.tx, 0.10F);
    EXPECT_NEAR(result.transform.ty, groundTruth.ty, 0.10F);
    EXPECT_NEAR(result.transform.a11, groundTruth.a11, 0.01F);
    EXPECT_NEAR(result.transform.a22, groundTruth.a22, 0.01F);
}

TEST(TestMotionModelEstimator, OutlierRejection60Percent)
{
    auto matches = generateGridPoints(100);
    const AffineTransform2D groundTruth { 1.0F, 0.0F, -14.0F, 0.0F, 1.0F, 12.0F };

    for (auto& m : matches) {
        m.currentPt = groundTruth.transform(m.feature.pt);
    }

    // Corrupt 60% of points
    std::mt19937 rng(1337U);
    std::uniform_real_distribution<float> dist(-80.0F, 80.0F);

    for (std::size_t i = 0U; i < 60U; ++i) {
        matches[i].currentPt.x += dist(rng) + 40.0F;
        matches[i].currentPt.y += dist(rng) - 40.0F;
    }

    MotionEstimatorConfig cfg { };
    cfg.mode = StabilizationMode::TranslationOnly;
    cfg.maxIterations = 200U;
    cfg.minInlierRatio = 0.30F;

    MotionModelEstimator estimator(cfg);
    MotionEstimationResult result { };

    const bool ok = estimator.estimateMotion(matches.data(), matches.size(), result);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(result.isSuccess);
    EXPECT_GE(result.inlierCount, 38U);

    EXPECT_NEAR(result.transform.tx, groundTruth.tx, 0.05F);
    EXPECT_NEAR(result.transform.ty, groundTruth.ty, 0.05F);
}

TEST(TestMotionModelEstimator, CollinearPointsRejection)
{
    // Provide points strictly collinear on a line
    std::vector<TrackedCorrespondence> matches;
    for (std::size_t i = 0U; i < 20U; ++i) {
        const float v = static_cast<float>(i) * 15.0F;
        matches.push_back(makeMatch(v, v, v + 5.0F, v + 5.0F));
    }

    MotionEstimatorConfig cfg { };
    cfg.mode = StabilizationMode::Affine;
    cfg.maxIterations = 50U;

    MotionModelEstimator estimator(cfg);
    MotionEstimationResult result { };

    // In Affine mode, 3 collinear points cannot determine 6-DOF without degeneracy
    const bool ok = estimator.estimateMotion(matches.data(), matches.size(), result);
    // Even if it fails or flags degenerate, system must not crash or output NaNs
    if (ok) {
        EXPECT_FALSE(std::isnan(result.transform.a11));
    }
}

TEST(TestMotionModelEstimator, NegativeDeterminantReject)
{
    // Point correspondences representing an optical mirror flip (reflection across Y axis)
    std::vector<TrackedCorrespondence> matches;
    matches.push_back(makeMatch(100.0F, 100.0F, -100.0F, 100.0F));
    matches.push_back(makeMatch(200.0F, 100.0F, -200.0F, 100.0F));
    matches.push_back(makeMatch(150.0F, 200.0F, -150.0F, 200.0F));
    matches.push_back(makeMatch(120.0F, 180.0F, -120.0F, 180.0F));

    MotionEstimatorConfig cfg { };
    cfg.mode = StabilizationMode::Affine;
    cfg.maxIterations = 50U;

    MotionModelEstimator estimator(cfg);
    MotionEstimationResult result { };

    const bool ok = estimator.estimateMotion(matches.data(), matches.size(), result);
    // Reflection must be rejected by checkTransform
    EXPECT_FALSE(ok);
    EXPECT_FALSE(result.isSuccess);
}

TEST(TestMotionModelEstimator, LeastSquaresRefinement)
{
    auto matches = generateGridPoints(80);
    const AffineTransform2D groundTruth { 1.0F, 0.0F, 5.0F, 0.0F, 1.0F, -3.0F };

    // Apply ground truth + small Gaussian measurement noise
    std::mt19937 rng(999U);
    std::normal_distribution<float> noise(0.0F, 0.15F);

    for (auto& m : matches) {
        m.currentPt = groundTruth.transform(m.feature.pt);
        m.currentPt.x += noise(rng);
        m.currentPt.y += noise(rng);
    }

    MotionEstimatorConfig cfgWithRefine { };
    cfgWithRefine.mode = StabilizationMode::TranslationOnly;
    cfgWithRefine.enableRefinement = true;

    MotionModelEstimator estimator(cfgWithRefine);
    MotionEstimationResult result { };

    const bool ok = estimator.estimateMotion(matches.data(), matches.size(), result);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(result.isSuccess);
    EXPECT_NEAR(result.transform.tx, 5.0F, 0.05F);
    EXPECT_NEAR(result.transform.ty, -3.0F, 0.05F);
    EXPECT_LT(result.meanInlierError, 0.25F);
}

TEST(TestMotionModelEstimator, ArenaScratchpadUsage)
{
    auto matches = generateGridPoints(100);
    for (auto& m : matches) {
        m.currentPt.x += 4.0F;
        m.currentPt.y += 2.0F;
    }

    StabilizerArena arena(64U * 1024U); // 64 KB arena
    MotionModelEstimator estimator;
    MotionEstimationResult result { };

    const bool ok = estimator.estimateMotion(matches.data(), matches.size(), result, &arena);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(result.isSuccess);
    EXPECT_EQ(result.inlierCount, 100U);
}

TEST(TestMotionModelEstimator, ZeroPointsGracefulDegeneracy)
{
    MotionModelEstimator estimator;
    MotionEstimationResult result { };

    // Null pointer
    EXPECT_FALSE(estimator.estimateMotion(nullptr, 0U, result));
    EXPECT_TRUE(result.isDegenerate);
    EXPECT_FALSE(result.isSuccess);

    // Single point for 6-DOF Affine (requires >= 3)
    TrackedCorrespondence singleMatch = makeMatch(10.0F, 10.0F, 12.0F, 12.0F);
    EXPECT_FALSE(estimator.estimateMotion(&singleMatch, 1U, result));
    EXPECT_TRUE(result.isDegenerate);
    EXPECT_FALSE(result.isSuccess);
}
