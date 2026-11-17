#include "ParfocalFeedforwardController.h"

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "LensTypes.h"
#include "MotionControlTypes.h"
#include "ZoomTrackFocusEngine.h"
#include "ZoomTrackTypes.h"

namespace LensControl {
namespace {

    TrackCurve makeLinearCurve()
    {
        TrackCurve curve;
        curve.knotCount = 3U;
        curve.knots[0].zoom = MotorPosition { 0U };
        curve.knots[0].focus = MotorPosition { 1000U };
        curve.knots[1].zoom = MotorPosition { 10000U };
        curve.knots[1].focus = MotorPosition { 3000U }; // slope = 2000 / 10000 = 0.2
        curve.knots[2].zoom = MotorPosition { 20000U };
        curve.knots[2].focus = MotorPosition { 5000U }; // slope = 0.2
        ZoomTrackFocusEngine::computePchip(curve);
        return curve;
    }

    TEST(TestParfocalFeedforward, InitializationAndReset)
    {
        ParfocalFeedforwardController controller;
        ZoomTrackFocusEngine engine;
        EXPECT_TRUE(engine.addCurve(makeLinearCurve()));

        const DeltaTimeSec dt { 0.02F };

        // First cycle initializes previous zoom, returns 0.0F
        const float cmd0 = controller.update(MotorPosition { 1000U }, MotorPosition { 1200U }, engine, dt);
        EXPECT_FLOAT_EQ(cmd0, 0.0F);
        EXPECT_FLOAT_EQ(controller.zoomVelocity().value(), 0.0F);

        controller.reset();
        // After reset, next update should also return 0.0F as re-initialization
        const float cmdReset = controller.update(MotorPosition { 2000U }, MotorPosition { 1400U }, engine, dt);
        EXPECT_FLOAT_EQ(cmdReset, 0.0F);
    }

    TEST(TestParfocalFeedforward, DeadbandSuppressesStationaryJitter)
    {
        FeedforwardConfig config;
        config.kp = 0.0F; // isolate feedforward
        config.deadbandZoomVelocity = 10.0F;
        config.filterAlpha = 1.0F; // raw pass-through

        ParfocalFeedforwardController controller { config };
        ZoomTrackFocusEngine engine;
        EXPECT_TRUE(engine.addCurve(makeLinearCurve()));

        const DeltaTimeSec dt { 0.02F };
        (void)controller.update(MotorPosition { 1000U }, MotorPosition { 1200U }, engine, dt);

        // Micro-jitter: 1 encoder step in 20 ms -> velocity = 1 / 0.02 = 50 steps/sec?
        // Wait: 1 step in 0.1s -> velocity = 10 steps/sec.
        // If delta is 0 steps:
        const float cmdStationary = controller.update(MotorPosition { 1000U }, MotorPosition { 1200U }, engine, dt);
        EXPECT_FLOAT_EQ(cmdStationary, 0.0F);
        EXPECT_FLOAT_EQ(controller.lastFeedforwardTerm(), 0.0F);

        // Small jitter below deadband: delta = 0.1 ticks (not possible with uint16),
        // but if dt = 0.5s, delta = 1 tick -> vel = 2 ticks/sec < deadband (10)
        const DeltaTimeSec largeDt { 0.5F };
        const float cmdJitter = controller.update(MotorPosition { 1001U }, MotorPosition { 1200U }, engine, largeDt);
        EXPECT_FLOAT_EQ(cmdJitter, 0.0F);
        EXPECT_FLOAT_EQ(controller.lastFeedforwardTerm(), 0.0F);
    }

    TEST(TestParfocalFeedforward, LinearZoomFeedforwardTracking)
    {
        FeedforwardConfig config;
        config.kp = 0.0F; // isolate feedforward
        config.filterAlpha = 1.0F; // unfiltered
        config.deadbandZoomVelocity = 1.0F;
        config.maxFocusVelocity = 500.0F;

        ParfocalFeedforwardController controller { config };
        ZoomTrackFocusEngine engine;
        EXPECT_TRUE(engine.addCurve(makeLinearCurve()));

        const DeltaTimeSec dt { 0.02F };

        // Slope df/dz = 0.2
        // Move zoom by 100 ticks per 0.02s => dz/dt = 5000 ticks/sec
        // Expected v_focus = 0.2 * 5000 = 1000 ticks/sec, clamped to 500
        (void)controller.update(MotorPosition { 5000U }, MotorPosition { 2000U }, engine, dt);

        // Moderate speed: 10 ticks per 0.02s => dz/dt = 500 ticks/sec
        // Expected v_focus = 0.2 * 500 = 100 ticks/sec
        const float cmd = controller.update(MotorPosition { 5010U }, MotorPosition { 2002U }, engine, dt);
        EXPECT_NEAR(controller.zoomVelocity().value(), 500.0F, 1.0F);
        EXPECT_NEAR(controller.lastFeedforwardTerm(), 100.0F, 1.0F);
        EXPECT_NEAR(cmd, 100.0F, 1.0F);
    }

    TEST(TestParfocalFeedforward, FeedbackClosesFocusError)
    {
        FeedforwardConfig config;
        config.kp = 0.5F;
        config.deadbandZoomVelocity = 10.0F;
        config.maxFocusVelocity = 255.0F;

        ParfocalFeedforwardController controller { config };
        ZoomTrackFocusEngine engine;
        EXPECT_TRUE(engine.addCurve(makeLinearCurve()));

        const DeltaTimeSec dt { 0.02F };

        // Zoom at 5000. Expected focus from curve: 1000 + 0.2 * 5000 = 2000.
        (void)controller.update(MotorPosition { 5000U }, MotorPosition { 1900U }, engine, dt);

        // Zoom stays stationary at 5000, but focus is at 1900 (error = 2000 - 1900 = +100).
        const float cmd = controller.update(MotorPosition { 5000U }, MotorPosition { 1900U }, engine, dt);

        // Feedforward term should be 0 (zoom stationary)
        EXPECT_FLOAT_EQ(controller.lastFeedforwardTerm(), 0.0F);
        // Feedback term: kp * 100 = 0.5 * 100 = +50
        EXPECT_NEAR(controller.lastFeedbackTerm(), 50.0F, 0.5F);
        EXPECT_NEAR(cmd, 50.0F, 0.5F);
    }

    TEST(TestParfocalFeedforward, SaturationClamping)
    {
        FeedforwardConfig config;
        config.kp = 10.0F; // huge gain to cause saturation
        config.maxFocusVelocity = 150.0F;

        ParfocalFeedforwardController controller { config };
        ZoomTrackFocusEngine engine;
        EXPECT_TRUE(engine.addCurve(makeLinearCurve()));

        const DeltaTimeSec dt { 0.02F };
        (void)controller.update(MotorPosition { 5000U }, MotorPosition { 1000U }, engine, dt);

        // Huge error: 2000 - 1000 = 1000 => 10 * 1000 = 10000, clamped to 150
        const float cmd = controller.update(MotorPosition { 5000U }, MotorPosition { 1000U }, engine, dt);
        EXPECT_FLOAT_EQ(cmd, 150.0F);

        // Reverse huge error => clamped to -150
        const float cmdNeg = controller.update(MotorPosition { 5000U }, MotorPosition { 3000U }, engine, dt);
        EXPECT_FLOAT_EQ(cmdNeg, -150.0F);
    }

} // namespace
} // namespace LensControl
