#include "PayloadHal.h"
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

TEST(TestLrfSafetyInterlock, DisarmedLaserFiringRejected) {
    SimulatedPayload payload {};
    auto lrf = payload.lrf();
    ASSERT_NE(lrf, nullptr);

    // Initial state: disarmed
    EXPECT_FALSE(lrf->isArmed());

    // Single shot must be rejected if disarmed
    bool triggered = lrf->triggerSingleMeasurement();
    EXPECT_FALSE(triggered);

    // Continuous mode must also fail
    bool contOk = lrf->setContinuousMode(LrfMode::Continuous1Hz);
    EXPECT_FALSE(contOk);
}

TEST(TestLrfSafetyInterlock, ArmedLaserFiringSucceeds) {
    SimulatedPayload payload {};
    auto lrf = payload.lrf();
    ASSERT_NE(lrf, nullptr);

    EXPECT_TRUE(lrf->armLaser());
    EXPECT_TRUE(lrf->isArmed());

    LrfTargetMeasurement captured {};
    int callCount { 0 };
    lrf->registerMeasurementCallback([&](const LrfTargetMeasurement& m) {
        captured = m;
        callCount++;
    });

    EXPECT_TRUE(lrf->triggerSingleMeasurement());
    EXPECT_EQ(callCount, 1);
    EXPECT_TRUE(captured.valid);
    EXPECT_GT(captured.slantRangeMeters, 0.0);
    EXPECT_EQ(captured.pulseCounter, 1U);

    // Fire second shot
    EXPECT_TRUE(lrf->triggerSingleMeasurement());
    EXPECT_EQ(callCount, 2);
    EXPECT_EQ(captured.pulseCounter, 2U);

    // Disarm laser -> subsequent fires must fail
    EXPECT_TRUE(lrf->disarmLaser());
    EXPECT_FALSE(lrf->isArmed());
    EXPECT_FALSE(lrf->triggerSingleMeasurement());
    EXPECT_EQ(callCount, 2); // No new measurement emitted
}

TEST(TestLrfSafetyInterlock, RangeGatingRejection) {
    SimulatedPayload payload {};
    auto lrf = payload.lrf();
    ASSERT_NE(lrf, nullptr);

    EXPECT_TRUE(lrf->armLaser());
    payload.setSimulatedSlantRange(1500.0); // 1500m target

    // Set gate window to 100m - 1000m (target is outside gate)
    EXPECT_TRUE(lrf->setRangeGating(100.0, 1000.0));

    LrfTargetMeasurement captured {};
    lrf->registerMeasurementCallback([&](const LrfTargetMeasurement& m) {
        captured = m;
    });

    EXPECT_TRUE(lrf->triggerSingleMeasurement());
    EXPECT_FALSE(captured.valid); // Gating rejected the echo
    EXPECT_DOUBLE_EQ(captured.slantRangeMeters, 0.0);

    // Now widen gate to 100m - 2000m (target inside gate)
    EXPECT_TRUE(lrf->setRangeGating(100.0, 2000.0));
    EXPECT_TRUE(lrf->triggerSingleMeasurement());
    EXPECT_TRUE(captured.valid);
    EXPECT_DOUBLE_EQ(captured.slantRangeMeters, 1500.0);
}

} // namespace
} // namespace PayloadHal
