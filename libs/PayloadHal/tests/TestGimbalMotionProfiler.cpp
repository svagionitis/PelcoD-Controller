#include "PayloadHal.h"
#include "GimbalMotionProfiler.h"
#include "sim/SimulatedPayload.h"
#include <gtest/gtest.h>
#include <cmath>
#include <vector>

namespace PayloadHal {
namespace {

TEST(TestGimbalMotionProfiler, SCurveMathContinuityAndBounds)
{
    GimbalMotionProfiler profiler;
    AxisMotionConstraints constraints{60.0, 120.0, 300.0}; // vMax=60, aMax=120, jMax=300

    AxisTrajectory traj = profiler.planAxisMove(0.0, 90.0, constraints);
    EXPECT_GT(traj.totalDuration(), 0.0);
    EXPECT_DOUBLE_EQ(traj.startPosition(), 0.0);
    EXPECT_DOUBLE_EQ(traj.targetPosition(), 90.0);
    EXPECT_FALSE(traj.isFinished(0.0));
    EXPECT_TRUE(traj.isFinished(traj.totalDuration()));

    // Sample across the entire trajectory at 5ms intervals
    const double dt = 0.005;
    double prevPos = 0.0;
    for (double t = 0.0; t <= traj.totalDuration() + 0.05; t += dt) {
        KinematicState1D s = traj.sample(t);

        // Position must be monotonically non-decreasing
        EXPECT_GE(s.positionDeg, prevPos - 1e-6);
        prevPos = s.positionDeg;

        // Kinematic physical bounds
        EXPECT_LE(std::abs(s.velocityDegPerSec), constraints.maxVelocityDegPerSec + 1e-4);
        EXPECT_LE(std::abs(s.accelerationDegPerSec2), constraints.maxAccelerationDegPerSec2 + 1e-4);
        EXPECT_LE(std::abs(s.jerkDegPerSec3), constraints.maxJerkDegPerSec3 + 1e-4);
    }

    // Boundary conditions
    KinematicState1D sStart = traj.sample(0.0);
    EXPECT_NEAR(sStart.positionDeg, 0.0, 1e-5);
    EXPECT_NEAR(sStart.velocityDegPerSec, 0.0, 1e-5);
    EXPECT_NEAR(sStart.accelerationDegPerSec2, 0.0, 1e-5);

    KinematicState1D sEnd = traj.sample(traj.totalDuration());
    EXPECT_NEAR(sEnd.positionDeg, 90.0, 1e-4);
    EXPECT_NEAR(sEnd.velocityDegPerSec, 0.0, 1e-4);
    EXPECT_NEAR(sEnd.accelerationDegPerSec2, 0.0, 1e-4);
}

TEST(TestGimbalMotionProfiler, NegativeDirectionMove)
{
    GimbalMotionProfiler profiler;
    AxisMotionConstraints constraints{45.0, 90.0, 200.0};

    AxisTrajectory traj = profiler.planAxisMove(45.0, -30.0, constraints);
    EXPECT_GT(traj.totalDuration(), 0.0);

    const double dt = 0.01;
    double prevPos = 45.0;
    for (double t = 0.0; t <= traj.totalDuration(); t += dt) {
        KinematicState1D s = traj.sample(t);
        // Position must decrease monotonically
        EXPECT_LE(s.positionDeg, prevPos + 1e-6);
        prevPos = s.positionDeg;

        // Velocity must be <= 0
        EXPECT_LE(s.velocityDegPerSec, 1e-5);
        EXPECT_LE(std::abs(s.velocityDegPerSec), constraints.maxVelocityDegPerSec + 1e-4);
    }

    KinematicState1D sEnd = traj.sample(traj.totalDuration());
    EXPECT_NEAR(sEnd.positionDeg, -30.0, 1e-4);
    EXPECT_NEAR(sEnd.velocityDegPerSec, 0.0, 1e-4);
}

TEST(TestGimbalMotionProfiler, ShortMoveDegenerateCases)
{
    GimbalMotionProfiler profiler;
    AxisMotionConstraints constraints{60.0, 120.0, 300.0};

    // Very small move: 0.1 degrees (triangular jerk)
    AxisTrajectory trajShort = profiler.planAxisMove(10.0, 10.1, constraints);
    EXPECT_GT(trajShort.totalDuration(), 0.0);

    // Verify it doesn't overshoot
    for (double t = 0.0; t <= trajShort.totalDuration() + 0.1; t += 0.005) {
        KinematicState1D s = trajShort.sample(t);
        EXPECT_GE(s.positionDeg, 10.0 - 1e-6);
        EXPECT_LE(s.positionDeg, 10.1 + 1e-5);
    }
    KinematicState1D sEndShort = trajShort.sample(trajShort.totalDuration());
    EXPECT_NEAR(sEndShort.positionDeg, 10.1, 1e-4);
    EXPECT_NEAR(sEndShort.velocityDegPerSec, 0.0, 1e-4);

    // Zero-distance move
    AxisTrajectory trajZero = profiler.planAxisMove(25.0, 25.0, constraints);
    EXPECT_DOUBLE_EQ(trajZero.totalDuration(), 0.0);
    KinematicState1D sZero = trajZero.sample(0.5);
    EXPECT_DOUBLE_EQ(sZero.positionDeg, 25.0);
    EXPECT_DOUBLE_EQ(sZero.velocityDegPerSec, 0.0);
}

TEST(TestGimbalMotionProfiler, LinearTrapezoidalProfile)
{
    GimbalMotionProfiler profiler;
    profiler.setAlgorithm(ProfileAlgorithm::LinearTrapezoidal);
    EXPECT_EQ(profiler.algorithm(), ProfileAlgorithm::LinearTrapezoidal);

    AxisMotionConstraints constraints{50.0, 100.0, 250.0};
    AxisTrajectory traj = profiler.planAxisMove(0.0, 100.0, constraints);

    EXPECT_GT(traj.totalDuration(), 0.0);
    EXPECT_EQ(traj.algorithm(), ProfileAlgorithm::LinearTrapezoidal);

    // Check boundary positions
    KinematicState1D s0 = traj.sample(0.0);
    EXPECT_NEAR(s0.positionDeg, 0.0, 1e-5);

    KinematicState1D sEnd = traj.sample(traj.totalDuration());
    EXPECT_NEAR(sEnd.positionDeg, 100.0, 1e-4);
    EXPECT_NEAR(sEnd.velocityDegPerSec, 0.0, 1e-4);

    // Trapezoidal short move (triangular velocity profile)
    AxisTrajectory trajTri = profiler.planAxisMove(0.0, 2.0, constraints);
    EXPECT_GT(trajTri.totalDuration(), 0.0);
    KinematicState1D sTriEnd = trajTri.sample(trajTri.totalDuration());
    EXPECT_NEAR(sTriEnd.positionDeg, 2.0, 1e-4);
    EXPECT_NEAR(sTriEnd.velocityDegPerSec, 0.0, 1e-4);
}

TEST(TestGimbalMotionProfiler, MultiAxisDurationSynchronization)
{
    GimbalMotionProfiler profiler;
    GimbalMotionConstraints constraints;
    constraints.pan = {60.0, 120.0, 300.0};
    constraints.tilt = {30.0, 60.0, 150.0};
    constraints.roll = {20.0, 40.0, 100.0};
    profiler.setConstraints(constraints);

    GimbalKinematicState startState {};
    startState.pan.positionDeg = 0.0;
    startState.tilt.positionDeg = 0.0;
    startState.roll.positionDeg = 0.0;

    // Pan moves 90 deg, Tilt moves 15 deg, Roll moves 5 deg
    CoordinatedGimbalTrajectory coordinated = profiler.planCoordinatedMove(
        startState, 90.0, 15.0, 5.0, true);

    EXPECT_GT(coordinated.totalDurationSec, 0.0);

    // All axes must share the exact same total duration when synchronized
    EXPECT_NEAR(coordinated.panTrajectory.totalDuration(), coordinated.totalDurationSec, 1e-4);
    EXPECT_NEAR(coordinated.tiltTrajectory.totalDuration(), coordinated.totalDurationSec, 1e-4);
    EXPECT_NEAR(coordinated.rollTrajectory.totalDuration(), coordinated.totalDurationSec, 1e-4);

    // Sample at completion
    GimbalKinematicState finalState = coordinated.sample(coordinated.totalDurationSec);
    EXPECT_NEAR(finalState.pan.positionDeg, 90.0, 1e-4);
    EXPECT_NEAR(finalState.tilt.positionDeg, 15.0, 1e-4);
    EXPECT_NEAR(finalState.roll.positionDeg, 5.0, 1e-4);
    EXPECT_NEAR(finalState.pan.velocityDegPerSec, 0.0, 1e-4);
    EXPECT_NEAR(finalState.tilt.velocityDegPerSec, 0.0, 1e-4);
    EXPECT_NEAR(finalState.roll.velocityDegPerSec, 0.0, 1e-4);

    EXPECT_TRUE(coordinated.isFinished(coordinated.totalDurationSec));
}

TEST(TestGimbalMotionProfiler, StreamingVelocityFilterStep)
{
    GimbalMotionProfiler profiler;
    AxisMotionConstraints constraints{60.0, 120.0, 300.0};

    KinematicState1D state {};
    state.positionDeg = 0.0;
    state.velocityDegPerSec = 0.0;
    state.accelerationDegPerSec2 = 0.0;

    const double dt = 0.02; // 50 Hz control loop
    const double targetVel = 40.0;

    // Filter step inputs over 2 seconds
    for (int step = 0; step < 100; ++step) {
        state = profiler.filterVelocityStep(state, targetVel, dt, constraints);

        // Ensure physical constraints are never violated during transition
        EXPECT_LE(std::abs(state.velocityDegPerSec), constraints.maxVelocityDegPerSec + 1e-4);
        EXPECT_LE(std::abs(state.accelerationDegPerSec2), constraints.maxAccelerationDegPerSec2 + 1e-4);
        EXPECT_LE(std::abs(state.jerkDegPerSec3), constraints.maxJerkDegPerSec3 + 1e-4);
    }

    // Velocity should have settled at targetVel
    EXPECT_NEAR(state.velocityDegPerSec, targetVel, 1e-4);
    EXPECT_NEAR(state.accelerationDegPerSec2, 0.0, 1e-4);
}

TEST(TestGimbalMotionProfiler, StreamingVelocityFilterReversal)
{
    GimbalMotionProfiler profiler;
    AxisMotionConstraints constraints{50.0, 100.0, 250.0};

    KinematicState1D state {};
    state.velocityDegPerSec = 40.0; // Currently moving forward
    state.accelerationDegPerSec2 = 0.0;

    const double dt = 0.02;
    const double targetVel = -40.0; // Command full reversal

    for (int step = 0; step < 120; ++step) {
        state = profiler.filterVelocityStep(state, targetVel, dt, constraints);

        EXPECT_LE(std::abs(state.velocityDegPerSec), constraints.maxVelocityDegPerSec + 1e-4);
        EXPECT_LE(std::abs(state.accelerationDegPerSec2), constraints.maxAccelerationDegPerSec2 + 1e-4);
    }

    // Should smoothly reach -40 deg/s without overshoot or instability
    EXPECT_NEAR(state.velocityDegPerSec, targetVel, 1e-4);
    EXPECT_NEAR(state.accelerationDegPerSec2, 0.0, 1e-4);
}

TEST(TestGimbalMotionProfiler, SimulatedPayloadIntegration)
{
    SimulatedPayload payload;
    ASSERT_TRUE(payload.connect());

    auto profiler = payload.motionProfiler();
    ASSERT_NE(profiler, nullptr);

    // Verify constraints
    GimbalMotionConstraints constraints = profiler->constraints();
    EXPECT_TRUE(constraints.isValid());
    EXPECT_GT(constraints.pan.maxVelocityDegPerSec, 0.0);

    // Plan coordinated move through the payload's profiler
    GimbalKinematicState curState {};
    curState.pan.positionDeg = 0.0;
    curState.tilt.positionDeg = 0.0;
    curState.roll.positionDeg = 0.0;

    CoordinatedGimbalTrajectory traj = profiler->planCoordinatedMove(curState, 45.0, -10.0, 0.0, true);
    EXPECT_GT(traj.totalDurationSec, 0.0);

    GimbalKinematicState mid = traj.sample(traj.totalDurationSec * 0.5);
    EXPECT_GT(mid.pan.positionDeg, 0.0);
    EXPECT_LT(mid.pan.positionDeg, 45.0);

    payload.disconnect();
}

} // namespace
} // namespace PayloadHal
