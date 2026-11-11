/// @file TestPeakFitter.cpp
/// @brief GoogleTest suite for Sub-Step Parabolic and Gaussian Peak Interpolator.

#include <cstddef>
#include <cstdint>

#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include "AutoFocusTypes.h"
#include "LensTypes.h"
#include "PeakFitter.h"

using namespace LensControl;

TEST(TestPeakFitter, SymmetricParabolicPeak)
{
    // Points at z = 2480, 2500, 2520 (uniform h = 20) with symmetrical peak at 2500
    SamplePoint p1 { };
    p1.pos = MotorPosition { 2480U };
    p1.score.metric = 8000U;

    SamplePoint p2 { };
    p2.pos = MotorPosition { 2500U };
    p2.score.metric = 10000U;

    SamplePoint p3 { };
    p3.pos = MotorPosition { 2520U };
    p3.score.metric = 8000U;

    const PeakFitResult result = PeakFitter::fitParabolic(p1, p2, p3);

    EXPECT_TRUE(result.isValid);
    EXPECT_EQ(result.vertex.value(), 2500U);
    EXPECT_NEAR(result.subStepOffset, 0.0F, 0.001F);
    EXPECT_GE(result.peakScore, 10000.0F);
    EXPECT_LT(result.curvature, 0.0F);
}

TEST(TestPeakFitter, AsymmetricParabolicShift)
{
    // Points at z = 2480, 2500, 2520 where p3 has higher metric than p1 -> vertex shifted right
    SamplePoint p1 { };
    p1.pos = MotorPosition { 2480U };
    p1.score.metric = 7000U;

    SamplePoint p2 { };
    p2.pos = MotorPosition { 2500U };
    p2.score.metric = 10000U;

    SamplePoint p3 { };
    p3.pos = MotorPosition { 2520U };
    p3.score.metric = 9000U;

    const PeakFitResult result = PeakFitter::fitParabolic(p1, p2, p3);

    EXPECT_TRUE(result.isValid);
    EXPECT_GT(result.subStepOffset, 0.0F);
    EXPECT_GT(result.vertex.value(), 2500U);
    EXPECT_LE(result.vertex.value(), 2510U);
    EXPECT_GT(result.peakScore, 10000.0F);
    EXPECT_LT(result.curvature, 0.0F);
}

TEST(TestPeakFitter, NonUniformSpacing)
{
    // Asymmetric spacing: h1 = 10 (2490->2500), h2 = 30 (2500->2530)
    SamplePoint p1 { };
    p1.pos = MotorPosition { 2490U };
    p1.score.metric = 8500U;

    SamplePoint p2 { };
    p2.pos = MotorPosition { 2500U };
    p2.score.metric = 10000U;

    SamplePoint p3 { };
    p3.pos = MotorPosition { 2530U };
    p3.score.metric = 8000U;

    const PeakFitResult result = PeakFitter::fitParabolic(p1, p2, p3);

    EXPECT_TRUE(result.isValid);
    EXPECT_LT(result.curvature, 0.0F);
    EXPECT_GE(result.vertex.value(), 2490U);
    EXPECT_LE(result.vertex.value(), 2530U);
}

TEST(TestPeakFitter, RejectNonConvexTrough)
{
    // Concave trough: center point is lower than neighbors (local minimum)
    SamplePoint p1 { };
    p1.pos = MotorPosition { 1000U };
    p1.score.metric = 8000U;

    SamplePoint p2 { };
    p2.pos = MotorPosition { 1020U };
    p2.score.metric = 5000U;

    SamplePoint p3 { };
    p3.pos = MotorPosition { 1040U };
    p3.score.metric = 7000U;

    const PeakFitResult result = PeakFitter::fitParabolic(p1, p2, p3);

    EXPECT_FALSE(result.isValid);
    EXPECT_EQ(result.vertex.value(), 1020U);
}

TEST(TestPeakFitter, RejectFlatLine)
{
    // Flat line: all scores equal
    SamplePoint p1 { };
    p1.pos = MotorPosition { 1000U };
    p1.score.metric = 5000U;

    SamplePoint p2 { };
    p2.pos = MotorPosition { 1020U };
    p2.score.metric = 5000U;

    SamplePoint p3 { };
    p3.pos = MotorPosition { 1040U };
    p3.score.metric = 5000U;

    const PeakFitResult result = PeakFitter::fitParabolic(p1, p2, p3);

    EXPECT_FALSE(result.isValid);
}

TEST(TestPeakFitter, GaussianPeakFit)
{
    SamplePoint p1 { };
    p1.pos = MotorPosition { 2000U };
    p1.score.metric = 4000U;

    SamplePoint p2 { };
    p2.pos = MotorPosition { 2050U };
    p2.score.metric = 12000U;

    SamplePoint p3 { };
    p3.pos = MotorPosition { 2100U };
    p3.score.metric = 4500U;

    const PeakFitResult result = PeakFitter::fitGaussian(p1, p2, p3);

    EXPECT_TRUE(result.isValid);
    EXPECT_GE(result.vertex.value(), 2040U);
    EXPECT_LE(result.vertex.value(), 2060U);
    EXPECT_GE(result.peakScore, 12000.0F);
}

TEST(TestPeakFitter, MultiPointFitFindsSummit)
{
    std::array<SamplePoint, 6> samples { };
    const std::array<std::uint16_t, 6> positions { 1000U, 1020U, 1040U, 1060U, 1080U, 1100U };
    const std::array<std::uint32_t, 6> metrics { 2000U, 4500U, 9800U, 8200U, 3100U, 1500U };

    for (std::size_t i = 0U; i < 6U; ++i) {
        samples[i].pos = MotorPosition { positions[i] };
        samples[i].score.metric = metrics[i];
    }

    const PeakFitResult result = PeakFitter::fitMultiPoint(samples.data(), samples.size());

    EXPECT_TRUE(result.isValid);
    // Peak is between 1040 and 1060
    EXPECT_GE(result.vertex.value(), 1035U);
    EXPECT_LE(result.vertex.value(), 1055U);
    EXPECT_GE(result.peakScore, 9800.0F);
}

TEST(TestPeakFitter, MultiPointEdgeCases)
{
    EXPECT_FALSE(PeakFitter::fitMultiPoint(nullptr, 0U).isValid);

    SamplePoint single { };
    single.pos = MotorPosition { 500U };
    single.score.metric = 1000U;

    const PeakFitResult rSingle = PeakFitter::fitMultiPoint(&single, 1U);
    EXPECT_FALSE(rSingle.isValid);
    EXPECT_EQ(rSingle.vertex.value(), 500U);
}
