#include "MotionControlTypes.h"

#include <cstdint>

#include <gtest/gtest.h>

namespace LensControl {
namespace {

    TEST(TestMotionControlTypes, VelocityTicksPerSecValues)
    {
        constexpr VelocityTicksPerSec vDefault;
        EXPECT_FLOAT_EQ(vDefault.value(), 0.0F);
        EXPECT_TRUE(vDefault.isZero());

        constexpr VelocityTicksPerSec v1 { 100.5F };
        constexpr VelocityTicksPerSec v2 { 200.0F };
        constexpr VelocityTicksPerSec v3 { 100.5F };

        EXPECT_FLOAT_EQ(v1.value(), 100.5F);
        EXPECT_FALSE(v1.isZero());
        EXPECT_TRUE(v1 == v3);
        EXPECT_TRUE(v1 != v2);
        EXPECT_TRUE(v1 < v2);
        EXPECT_TRUE(v1 <= v3);
        EXPECT_TRUE(v2 > v1);
        EXPECT_TRUE(v2 >= v3);
    }

    TEST(TestMotionControlTypes, AccelerationTicksPerSec2Values)
    {
        constexpr AccelerationTicksPerSec2 aDefault;
        EXPECT_FLOAT_EQ(aDefault.value(), 0.0F);
        EXPECT_TRUE(aDefault.isZero());

        constexpr AccelerationTicksPerSec2 a1 { 500.0F };
        constexpr AccelerationTicksPerSec2 a2 { 1000.0F };

        EXPECT_FLOAT_EQ(a1.value(), 500.0F);
        EXPECT_FALSE(a1.isZero());
        EXPECT_TRUE(a1 < a2);
        EXPECT_TRUE(a1 <= a2);
        EXPECT_TRUE(a2 > a1);
        EXPECT_TRUE(a2 >= a1);
    }

    TEST(TestMotionControlTypes, JerkTicksPerSec3Values)
    {
        constexpr JerkTicksPerSec3 jDefault;
        EXPECT_FLOAT_EQ(jDefault.value(), 0.0F);
        EXPECT_TRUE(jDefault.isZero());

        constexpr JerkTicksPerSec3 j1 { 25000.0F };
        constexpr JerkTicksPerSec3 j2 { 50000.0F };

        EXPECT_FLOAT_EQ(j1.value(), 25000.0F);
        EXPECT_FALSE(j1.isZero());
        EXPECT_TRUE(j1 < j2);
        EXPECT_TRUE(j1 != j2);
    }

    TEST(TestMotionControlTypes, DeltaTimeSecStrictBounds)
    {
        constexpr DeltaTimeSec dtDefault;
        EXPECT_FLOAT_EQ(dtDefault.value(), 0.02F);

        constexpr DeltaTimeSec dtZero { 0.0F };
        EXPECT_GE(dtZero.value(), DeltaTimeSec::kMinDeltaTime);

        constexpr DeltaTimeSec dtNegative { -5.0F };
        EXPECT_GE(dtNegative.value(), DeltaTimeSec::kMinDeltaTime);

        constexpr DeltaTimeSec dtCustom { 0.05F };
        EXPECT_FLOAT_EQ(dtCustom.value(), 0.05F);
    }

    TEST(TestMotionControlTypes, SCurveConstraintsValidation)
    {
        SCurveConstraints valid;
        EXPECT_TRUE(valid.isValid());

        SCurveConstraints invalidVel;
        invalidVel.maxVelocity = VelocityTicksPerSec { 0.0F };
        EXPECT_FALSE(invalidVel.isValid());

        SCurveConstraints invalidAcc;
        invalidAcc.maxAcceleration = AccelerationTicksPerSec2 { -10.0F };
        EXPECT_FALSE(invalidAcc.isValid());

        SCurveConstraints invalidJerk;
        invalidJerk.maxJerk = JerkTicksPerSec3 { 0.0F };
        EXPECT_FALSE(invalidJerk.isValid());
    }

    TEST(TestMotionControlTypes, PidGainsAndFeedforwardDefaults)
    {
        PidGains pid;
        EXPECT_GT(pid.kp, 0.0F);
        EXPECT_GT(pid.integralMax, 0.0F);
        EXPECT_LT(pid.outputMin, 0.0F);
        EXPECT_GT(pid.outputMax, 0.0F);

        FeedforwardConfig ff;
        EXPECT_GT(ff.kp, 0.0F);
        EXPECT_GT(ff.filterAlpha, 0.0F);
        EXPECT_LE(ff.filterAlpha, 1.0F);
        EXPECT_GT(ff.maxFocusVelocity, 0.0F);
    }

} // namespace
} // namespace LensControl
