#include "MotionProfiler.h"

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "MotionControlTypes.h"

namespace LensControl {
namespace {

    TEST(TestMotionProfiler, ConstraintsValidation)
    {
        MotionProfiler profiler;

        SCurveConstraints invalid;
        invalid.maxVelocity = VelocityTicksPerSec { -5.0F };
        EXPECT_FALSE(profiler.setConstraints(invalid));

        SCurveConstraints valid;
        valid.maxVelocity = VelocityTicksPerSec { 5000.0F };
        valid.maxAcceleration = AccelerationTicksPerSec2 { 20000.0F };
        valid.maxDeceleration = AccelerationTicksPerSec2 { 20000.0F };
        valid.maxJerk = JerkTicksPerSec3 { 100000.0F };
        EXPECT_TRUE(profiler.setConstraints(valid));
    }

    TEST(TestMotionProfiler, ZeroDistanceMove)
    {
        MotionProfiler profiler;
        EXPECT_TRUE(profiler.planTrajectory(1500.0F, 1500.0F));
        EXPECT_TRUE(profiler.isComplete());
        EXPECT_FLOAT_EQ(profiler.totalDuration(), 0.0F);

        const TrajectorySample sample = profiler.evaluateAt(0.0F);
        EXPECT_FLOAT_EQ(sample.position, 1500.0F);
        EXPECT_FLOAT_EQ(sample.velocity, 0.0F);
        EXPECT_FLOAT_EQ(sample.acceleration, 0.0F);
        EXPECT_FLOAT_EQ(sample.jerk, 0.0F);
    }

    TEST(TestMotionProfiler, FullSevenPhaseProfile)
    {
        SCurveConstraints limits;
        limits.maxVelocity = VelocityTicksPerSec { 10000.0F };
        limits.maxAcceleration = AccelerationTicksPerSec2 { 50000.0F };
        limits.maxDeceleration = AccelerationTicksPerSec2 { 50000.0F };
        limits.maxJerk = JerkTicksPerSec3 { 200000.0F };

        MotionProfiler profiler { limits };
        const float startPos = 0.0F;
        const float targetPos = 50000.0F;

        EXPECT_TRUE(profiler.planTrajectory(startPos, targetPos));
        EXPECT_FALSE(profiler.isComplete());
        EXPECT_GT(profiler.totalDuration(), 0.0F);

        const float tTotal = profiler.totalDuration();
        const float dt = 0.005F; // 5 ms evaluation step

        float prevPos = startPos;
        for (float t = 0.0F; t <= tTotal; t += dt) {
            const TrajectorySample s = profiler.evaluateAt(t);

            // Kinematic bounds
            EXPECT_GE(s.position, startPos - 0.01F);
            EXPECT_LE(s.position, targetPos + 0.01F);
            EXPECT_LE(s.velocity, limits.maxVelocity.value() + 0.1F);
            EXPECT_GE(s.velocity, -0.1F);
            EXPECT_LE(s.acceleration, limits.maxAcceleration.value() + 0.1F);
            EXPECT_GE(s.acceleration, -limits.maxDeceleration.value() - 0.1F);
            EXPECT_LE(std::abs(s.jerk), limits.maxJerk.value() + 0.1F);

            // Continuity check: position should monotonically increase
            EXPECT_GE(s.position, prevPos - 1e-4F);
            prevPos = s.position;
        }

        // Exact arrival at boundary
        const TrajectorySample finalSample = profiler.evaluateAt(tTotal);
        EXPECT_NEAR(finalSample.position, targetPos, 0.1F);
        EXPECT_NEAR(finalSample.velocity, 0.0F, 0.01F);
        EXPECT_NEAR(finalSample.acceleration, 0.0F, 0.01F);
    }

    TEST(TestMotionProfiler, ShortStrokeDegeneration)
    {
        SCurveConstraints limits;
        limits.maxVelocity = VelocityTicksPerSec { 10000.0F };
        limits.maxAcceleration = AccelerationTicksPerSec2 { 50000.0F };
        limits.maxDeceleration = AccelerationTicksPerSec2 { 50000.0F };
        limits.maxJerk = JerkTicksPerSec3 { 200000.0F };

        MotionProfiler profiler { limits };
        const float startPos = 1000.0F;
        const float targetPos = 1300.0F; // short move: 300 ticks

        EXPECT_TRUE(profiler.planTrajectory(startPos, targetPos));
        EXPECT_GT(profiler.totalDuration(), 0.0F);

        const float tTotal = profiler.totalDuration();
        const float midSample = profiler.evaluateAt(tTotal * 0.5F).velocity;

        // Peak velocity must be strictly less than V_max
        EXPECT_LT(midSample, limits.maxVelocity.value());
        EXPECT_GT(midSample, 0.0F);

        // Exact termination at target
        const TrajectorySample finalSample = profiler.evaluateAt(tTotal);
        EXPECT_NEAR(finalSample.position, targetPos, 0.1F);
        EXPECT_NEAR(finalSample.velocity, 0.0F, 0.01F);
    }

    TEST(TestMotionProfiler, MicroStepMove)
    {
        SCurveConstraints limits;
        limits.maxVelocity = VelocityTicksPerSec { 10000.0F };
        limits.maxAcceleration = AccelerationTicksPerSec2 { 50000.0F };
        limits.maxDeceleration = AccelerationTicksPerSec2 { 50000.0F };
        limits.maxJerk = JerkTicksPerSec3 { 200000.0F };

        MotionProfiler profiler { limits };
        const float startPos = 5000.0F;
        const float targetPos = 5002.0F; // tiny move: 2 ticks

        EXPECT_TRUE(profiler.planTrajectory(startPos, targetPos));
        EXPECT_GT(profiler.totalDuration(), 0.0F);

        const float tTotal = profiler.totalDuration();
        const TrajectorySample finalSample = profiler.evaluateAt(tTotal);
        EXPECT_NEAR(finalSample.position, targetPos, 0.05F);
        EXPECT_NEAR(finalSample.velocity, 0.0F, 0.01F);
    }

    TEST(TestMotionProfiler, NegativeDirectionMove)
    {
        SCurveConstraints limits;
        limits.maxVelocity = VelocityTicksPerSec { 5000.0F };
        limits.maxAcceleration = AccelerationTicksPerSec2 { 25000.0F };
        limits.maxDeceleration = AccelerationTicksPerSec2 { 25000.0F };
        limits.maxJerk = JerkTicksPerSec3 { 100000.0F };

        MotionProfiler profiler { limits };
        const float startPos = 30000.0F;
        const float targetPos = 10000.0F; // reverse move: -20000 ticks

        EXPECT_TRUE(profiler.planTrajectory(startPos, targetPos));
        EXPECT_GT(profiler.totalDuration(), 0.0F);

        const float tTotal = profiler.totalDuration();
        const TrajectorySample mid = profiler.evaluateAt(tTotal * 0.5F);
        EXPECT_LT(mid.velocity, 0.0F); // moving in negative direction

        const TrajectorySample finalSample = profiler.evaluateAt(tTotal);
        EXPECT_NEAR(finalSample.position, targetPos, 0.1F);
        EXPECT_NEAR(finalSample.velocity, 0.0F, 0.01F);
    }

    TEST(TestMotionProfiler, AdvanceSteppingAndCancel)
    {
        MotionProfiler profiler;
        EXPECT_TRUE(profiler.planTrajectory(0.0F, 1000.0F));

        const DeltaTimeSec dt { 0.02F };
        TrajectorySample s = profiler.advance(dt);
        EXPECT_GT(s.position, 0.0F);
        EXPECT_FALSE(profiler.isComplete());

        profiler.cancel();
        EXPECT_TRUE(profiler.isComplete());
    }

} // namespace
} // namespace LensControl
