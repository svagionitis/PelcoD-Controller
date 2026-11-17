/// @file TestOpticalGeometry.cpp
#include "OpticalGeometryCalculator.h"

#include <gtest/gtest.h>

namespace LensControl {
namespace {

    TEST(TestOpticalGeometry, HyperfocalDistanceCalculation)
    {
        OpticalGeometryCalculator::OpticsConfig cfg { };
        cfg.aperture = FNumber { 1.6F };
        cfg.coc = CircleOfConfusionMm { 0.0069F };
        OpticalGeometryCalculator calc { cfg };

        // f = 25 mm, N = 1.6, c = 0.0069 mm
        // H = (25^2 * 0.001) / (1.6 * 0.0069) + 0.025 = 0.625 / 0.01104 + 0.025 = 56.612 + 0.025 = 56.637 m
        const DistanceMeters h = calc.calculateHyperfocal(FocalLengthMm { 25.0F });
        EXPECT_NEAR(h.value(), 56.637F, 0.05F);
    }

    TEST(TestOpticalGeometry, DepthOfFieldFiniteBounds)
    {
        OpticalGeometryCalculator::OpticsConfig cfg { };
        cfg.aperture = FNumber { 1.6F };
        cfg.coc = CircleOfConfusionMm { 0.0069F };
        OpticalGeometryCalculator calc { cfg };

        // d = 10 m, f = 25 mm
        const DofLimits dof = calc.calculateDof(DistanceMeters { 10.0F }, FocalLengthMm { 25.0F });
        EXPECT_FALSE(dof.isFarInfinite);
        EXPECT_GT(dof.nearLimit.value(), 0.0F);
        EXPECT_LT(dof.nearLimit.value(), 10.0F);
        EXPECT_GT(dof.farLimit.value(), 10.0F);
        EXPECT_NEAR(dof.depthOfField.value(), dof.farLimit.value() - dof.nearLimit.value(), 1e-4F);
    }

    TEST(TestOpticalGeometry, FarLimitInfinityWhenBeyondHyperfocal)
    {
        OpticalGeometryCalculator::OpticsConfig cfg { };
        cfg.aperture = FNumber { 1.6F };
        cfg.coc = CircleOfConfusionMm { 0.0069F };
        OpticalGeometryCalculator calc { cfg };

        // H ~ 56.6 m, target at 70 m (beyond H)
        const DofLimits dof = calc.calculateDof(DistanceMeters { 70.0F }, FocalLengthMm { 25.0F });
        EXPECT_TRUE(dof.isFarInfinite);
        EXPECT_TRUE(dof.farLimit.isInfinite());
        EXPECT_TRUE(dof.depthOfField.isInfinite());
        EXPECT_GT(dof.nearLimit.value(), 0.0F);
        EXPECT_LT(dof.nearLimit.value(), 70.0F);
    }

    TEST(TestOpticalGeometry, WideAngleLimitDetection)
    {
        OpticalGeometryCalculator::OpticsConfig cfg { };
        cfg.wideLimitZoom = MotorPosition { 5000U };
        OpticalGeometryCalculator calc { cfg };

        EXPECT_TRUE(calc.isWideAngle(MotorPosition { 0U }));
        EXPECT_TRUE(calc.isWideAngle(MotorPosition { 2500U }));
        EXPECT_TRUE(calc.isWideAngle(MotorPosition { 5000U }));
        EXPECT_FALSE(calc.isWideAngle(MotorPosition { 5001U }));
        EXPECT_FALSE(calc.isWideAngle(MotorPosition { 40000U }));
    }

    TEST(TestOpticalGeometry, AdaptiveStepScaling)
    {
        OpticalGeometryCalculator::OpticsConfig cfg { };
        cfg.aperture = FNumber { 1.6F };
        cfg.coc = CircleOfConfusionMm { 0.0069F };
        cfg.minFocalLength = FocalLengthMm { 4.3F };
        cfg.maxFocalLength = FocalLengthMm { 129.0F };
        cfg.nominalSubjectDist = DistanceMeters { 25.0F };
        OpticalGeometryCalculator calc { cfg };

        // At full telephoto (129 mm), scaling should be 1.0x
        const AdaptiveStepConfig teleSteps = calc.calculateSteps(FocalLengthMm { 129.0F }, 300U, 100U, 20U);
        EXPECT_FLOAT_EQ(teleSteps.scaleFactor, 1.0F);
        EXPECT_EQ(teleSteps.coarseStep, 300U);
        EXPECT_EQ(teleSteps.midStep, 100U);
        EXPECT_EQ(teleSteps.fineStep, 20U);

        // At wide focal length (4.3 mm), scaling should expand (up to 10x clamp)
        const AdaptiveStepConfig wideSteps = calc.calculateSteps(FocalLengthMm { 4.3F }, 300U, 100U, 20U);
        EXPECT_GT(wideSteps.scaleFactor, 1.0F);
        EXPECT_LE(wideSteps.scaleFactor, 10.0F);
        EXPECT_GT(wideSteps.coarseStep, 300U);
        EXPECT_GT(wideSteps.midStep, 100U);
        EXPECT_GT(wideSteps.fineStep, 20U);
    }

    TEST(TestOpticalGeometry, InvalidZeroAndNegativeInputs)
    {
        OpticalGeometryCalculator calc { };
        const DistanceMeters h = calc.calculateHyperfocal(FocalLengthMm { 0.0F });
        EXPECT_FLOAT_EQ(h.value(), 0.0F);

        const DofLimits dofZero = calc.calculateDof(DistanceMeters { 0.0F }, FocalLengthMm { 25.0F });
        EXPECT_FLOAT_EQ(dofZero.nearLimit.value(), 0.0F);
        EXPECT_FLOAT_EQ(dofZero.farLimit.value(), 0.0F);
    }

} // namespace
} // namespace LensControl
