#include "ActuatorPidController.h"

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "MotionControlTypes.h"

namespace LensControl {
namespace {

    TEST(TestActuatorPidController, ProportionalResponse)
    {
        PidGains gains;
        gains.kp = 2.0F;
        gains.ki = 0.0F;
        gains.kd = 0.0F;
        gains.stictionBreakaway = 0.0F;
        gains.deadband = 0.0F;

        ActuatorPidController pid { gains };
        const DeltaTimeSec dt { 0.02F };

        // Initial cycle initializes previous actual to 100.0F
        (void)pid.compute(100.0F, 100.0F, dt);

        // Error = 150 - 100 = +50 => effort = 2.0 * 50 = 100
        const float effort = pid.compute(150.0F, 100.0F, dt);
        EXPECT_FLOAT_EQ(pid.lastPTerm(), 100.0F);
        EXPECT_FLOAT_EQ(effort, 100.0F);
    }

    TEST(TestActuatorPidController, DerivativeKickImmunity)
    {
        PidGains gains;
        gains.kp = 0.0F;
        gains.ki = 0.0F;
        gains.kd = 1.0F;
        gains.stictionBreakaway = 0.0F;
        gains.deadband = 0.0F;
        gains.derivativeFilterAlpha = 1.0F; // unfiltered

        ActuatorPidController pid { gains };
        const DeltaTimeSec dt { 0.02F };

        // Cycle 1: initial actual = 100
        (void)pid.compute(100.0F, 100.0F, dt);

        // Cycle 2: setpoint steps from 100 to 500, but actual is STILL 100!
        // Since derivative is on measurement, d(actual)/dt = 0, so D-term MUST be 0!
        const float effortSetpointStep = pid.compute(500.0F, 100.0F, dt);
        EXPECT_FLOAT_EQ(pid.lastDTerm(), 0.0F);
        EXPECT_FLOAT_EQ(effortSetpointStep, 0.0F);

        // Cycle 3: actual moves from 100 to 110 (v = 10 / 0.02 = 500)
        // D-term = -Kd * v = -1.0 * 500 = -500
        const float effortActualMoving = pid.compute(500.0F, 110.0F, dt);
        EXPECT_FLOAT_EQ(pid.lastDTerm(), -500.0F);
        EXPECT_LT(effortActualMoving, 0.0F);
    }

    TEST(TestActuatorPidController, IntegralAccumulationAndAntiWindup)
    {
        PidGains gains;
        gains.kp = 0.0F;
        gains.ki = 10.0F;
        gains.kd = 0.0F;
        gains.outputMin = -100.0F;
        gains.outputMax = 100.0F;
        gains.integralMax = 100.0F;
        gains.stictionBreakaway = 0.0F;
        gains.deadband = 0.0F;

        ActuatorPidController pid { gains };
        const DeltaTimeSec dt { 0.1F };

        // Initial cycle
        (void)pid.compute(10.0F, 0.0F, dt);

        // Constant error of 10. Each 0.1s adds: ki * error * dt = 10 * 10 * 0.1 = 10.
        for (int i = 0; i < 9; ++i) {
            (void)pid.compute(10.0F, 0.0F, dt);
        }
        // Now integral should be near 90..100
        const float effortNearSat = pid.compute(10.0F, 0.0F, dt);
        EXPECT_NEAR(effortNearSat, 100.0F, 1.0F);

        // Continue pushing in saturation: anti-windup must freeze integral at 100
        for (int i = 0; i < 50; ++i) {
            (void)pid.compute(10.0F, 0.0F, dt);
        }
        EXPECT_FLOAT_EQ(pid.lastITerm(), 100.0F);
        EXPECT_FLOAT_EQ(pid.compute(10.0F, 0.0F, dt), 100.0F);
    }

    TEST(TestActuatorPidController, StictionBreakawayKick)
    {
        PidGains gains;
        gains.kp = 0.1F;
        gains.ki = 0.0F;
        gains.kd = 0.0F;
        gains.stictionBreakaway = 25.0F;
        gains.deadband = 1.0F;
        gains.movingVelocityThreshold = 2.0F;

        ActuatorPidController pid { gains };
        const DeltaTimeSec dt { 0.02F };

        (void)pid.compute(100.0F, 90.0F, dt);

        // Actuator stopped at 90, setpoint 100 (error = 10 > deadband, v = 0 < movingThreshold)
        const float effortStopped = pid.compute(100.0F, 90.0F, dt);
        EXPECT_FLOAT_EQ(pid.lastStictionTerm(), 25.0F);
        // Total effort = 0.1 * 10 + 25 = 26
        EXPECT_NEAR(effortStopped, 26.0F, 0.1F);

        // Once actuator moves fast (actual goes 90 -> 95 in 0.02s => v = 250 > threshold)
        const float effortMoving = pid.compute(100.0F, 95.0F, dt);
        // Stiction kick must vanish!
        EXPECT_FLOAT_EQ(pid.lastStictionTerm(), 0.0F);
        EXPECT_NEAR(effortMoving, 0.1F * 5.0F, 0.1F);
    }

    TEST(TestActuatorPidController, InPositionSettlingDetection)
    {
        PidGains gains;
        gains.kp = 1.0F;
        gains.deadband = 2.0F;
        gains.movingVelocityThreshold = 1.0F;
        gains.settlingCyclesRequired = 3U;

        ActuatorPidController pid { gains };
        const DeltaTimeSec dt { 0.02F };

        // Cycle 1: within deadband (settled count = 1)
        (void)pid.compute(100.0F, 100.5F, dt);
        EXPECT_FALSE(pid.isInPosition());

        // Cycle 2: within deadband (settled count = 2)
        (void)pid.compute(100.0F, 100.5F, dt);
        EXPECT_FALSE(pid.isInPosition());

        // Cycle 3: within deadband (settled count = 3): meets requirement of 3 cycles!
        const float effortSettled = pid.compute(100.0F, 100.5F, dt);
        EXPECT_TRUE(pid.isInPosition());
        EXPECT_FLOAT_EQ(effortSettled, 0.0F); // zero effort when settled

        // Disturbance moves actual out of deadband (100 -> 105)
        (void)pid.compute(100.0F, 105.0F, dt);
        EXPECT_FALSE(pid.isInPosition());
    }

} // namespace
} // namespace LensControl
