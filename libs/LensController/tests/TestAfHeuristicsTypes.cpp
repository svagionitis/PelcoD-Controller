/// @file TestAfHeuristicsTypes.cpp
#include "AfHeuristicsTypes.h"

#include <cstdint>

#include <gtest/gtest.h>

namespace LensControl {
namespace {

    TEST(TestAfHeuristicsTypes, AngularVelocityValuesAndComparisons)
    {
        constexpr AngularVelocityDegPerSec wDefault { };
        EXPECT_FLOAT_EQ(wDefault.value(), 0.0F);

        constexpr AngularVelocityDegPerSec w1 { 1.5F };
        constexpr AngularVelocityDegPerSec w2 { 3.0F };
        constexpr AngularVelocityDegPerSec w3 { 1.5F };

        EXPECT_FLOAT_EQ(w1.value(), 1.5F);
        EXPECT_TRUE(w1 == w3);
        EXPECT_TRUE(w1 != w2);
        EXPECT_TRUE(w1 < w2);
        EXPECT_TRUE(w1 <= w3);
        EXPECT_TRUE(w2 > w1);
        EXPECT_TRUE(w2 >= w3);
    }

    TEST(TestAfHeuristicsTypes, DistanceMetersValuesAndInfinity)
    {
        constexpr DistanceMeters dDefault { };
        EXPECT_FLOAT_EQ(dDefault.value(), 0.0F);
        EXPECT_FALSE(dDefault.isInfinite());

        constexpr DistanceMeters dFinite { 10.5F };
        EXPECT_FLOAT_EQ(dFinite.value(), 10.5F);
        EXPECT_FALSE(dFinite.isInfinite());

        constexpr DistanceMeters dInf = DistanceMeters::infinity();
        EXPECT_TRUE(dInf.isInfinite());
        EXPECT_TRUE(dFinite < dInf);
        EXPECT_TRUE(dInf > dFinite);

        constexpr DistanceMeters dSame { 10.5F };
        EXPECT_TRUE(dFinite == dSame);
        EXPECT_TRUE(dFinite != dInf);
        EXPECT_TRUE(dFinite <= dSame);
        EXPECT_TRUE(dFinite >= dSame);
    }

    TEST(TestAfHeuristicsTypes, FocalLengthMmValuesAndSanity)
    {
        constexpr FocalLengthMm fDefault { };
        EXPECT_FLOAT_EQ(fDefault.value(), 1.0F);

        constexpr FocalLengthMm fNegative { -10.0F };
        EXPECT_FLOAT_EQ(fNegative.value(), 0.0F);

        constexpr FocalLengthMm fTele { 200.0F };
        constexpr FocalLengthMm fWide { 24.0F };
        EXPECT_TRUE(fWide < fTele);
        EXPECT_TRUE(fTele > fWide);
        EXPECT_TRUE(fWide != fTele);
        EXPECT_TRUE(fWide == FocalLengthMm { 24.0F });
        EXPECT_TRUE(fWide <= FocalLengthMm { 24.0F });
        EXPECT_TRUE(fWide >= FocalLengthMm { 24.0F });
    }

    TEST(TestAfHeuristicsTypes, FNumberValuesAndApertureClamping)
    {
        constexpr FNumber fDefault { };
        EXPECT_FLOAT_EQ(fDefault.value(), 1.6F);

        constexpr FNumber fInvalid { 0.1F };
        EXPECT_FLOAT_EQ(fInvalid.value(), 1.6F);

        constexpr FNumber fStop28 { 2.8F };
        constexpr FNumber fStop56 { 5.6F };
        EXPECT_TRUE(fStop28 < fStop56);
        EXPECT_TRUE(fStop56 > fStop28);
        EXPECT_TRUE(fStop28 == FNumber { 2.8F });
        EXPECT_TRUE(fStop28 != fStop56);
        EXPECT_TRUE(fStop28 <= FNumber { 2.8F });
        EXPECT_TRUE(fStop28 >= FNumber { 2.8F });
    }

    TEST(TestAfHeuristicsTypes, CircleOfConfusionValues)
    {
        constexpr CircleOfConfusionMm cDefault { };
        EXPECT_FLOAT_EQ(cDefault.value(), 0.0069F);

        constexpr CircleOfConfusionMm cZero { 0.0F };
        EXPECT_FLOAT_EQ(cZero.value(), 0.0069F);

        constexpr CircleOfConfusionMm c1 { 0.01F };
        constexpr CircleOfConfusionMm c2 { 0.02F };
        EXPECT_TRUE(c1 < c2);
        EXPECT_TRUE(c2 > c1);
        EXPECT_TRUE(c1 == CircleOfConfusionMm { 0.01F });
        EXPECT_TRUE(c1 != c2);
        EXPECT_TRUE(c1 <= CircleOfConfusionMm { 0.01F });
        EXPECT_TRUE(c1 >= CircleOfConfusionMm { 0.01F });
    }

    TEST(TestAfHeuristicsTypes, StructInitializations)
    {
        const DofLimits dof { };
        EXPECT_FLOAT_EQ(dof.nearLimit.value(), 0.0F);
        EXPECT_FLOAT_EQ(dof.farLimit.value(), 0.0F);
        EXPECT_FALSE(dof.isFarInfinite);

        const AdaptiveStepConfig steps { };
        EXPECT_EQ(steps.coarseStep, 300U);
        EXPECT_EQ(steps.midStep, 100U);
        EXPECT_EQ(steps.fineStep, 20U);
        EXPECT_FLOAT_EQ(steps.scaleFactor, 1.0F);

        const ZoneScore zone { };
        EXPECT_FLOAT_EQ(zone.motionSaliency, 0.0F);
        EXPECT_FLOAT_EQ(zone.targetWeight, 1.0F);
        EXPECT_FALSE(zone.peakConfirmed);

        const MultiZoneResult mz { };
        EXPECT_EQ(mz.selectedZoneIndex, 4U);
        EXPECT_EQ(mz.compositeMetric, 0U);
        EXPECT_FALSE(mz.hasForegroundPeak);
    }

} // namespace
} // namespace LensControl
