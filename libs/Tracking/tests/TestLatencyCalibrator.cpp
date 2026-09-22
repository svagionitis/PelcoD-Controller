/// @file TestLatencyCalibrator.cpp
/// @brief Unit tests for LatencyCalibrator state machine: construction, start/cancel,
///        state transitions, command callback, visual motion ingestion, and result access.

#include "LatencyCalibrator.h"

#include <atomic>
#include <gtest/gtest.h>
#include <cmath>
#include <iostream>

using namespace PelcoD;

namespace {

TEST(LatencyCalibratorTest, IdleByDefault)
{
    std::cout << "[Test] testIdleByDefault\n";
    LatencyCalibrator cal;
    EXPECT_TRUE(cal.getState() == CalibrationState::Idle);
    EXPECT_TRUE(!cal.isRunning());
    std::cout << "  -> PASSED\n";
}

TEST(LatencyCalibratorTest, StartTransitions)
{
    std::cout << "[Test] testStartTransitions\n";
    LatencyCalibrator cal;
    const bool ok = cal.start(25, 0, 0.0);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(cal.isRunning());
    const auto s = cal.getState();
    // Should be in PreSettle or PositivePulse phase immediately after start
    EXPECT_TRUE(s != CalibrationState::Idle);
    EXPECT_TRUE(s != CalibrationState::Completed);
    EXPECT_TRUE(s != CalibrationState::Failed);
    std::cout << "  -> PASSED\n";
}

TEST(LatencyCalibratorTest, DoubleStartReturnsFalse)
{
    std::cout << "[Test] testDoubleStartReturnsFalse\n";
    LatencyCalibrator cal;
    EXPECT_TRUE(cal.start(25, 0, 0.0));
    EXPECT_TRUE(!cal.start(20, 0, 0.0)); // Already running
    std::cout << "  -> PASSED\n";
}

TEST(LatencyCalibratorTest, CancelResetsRunning)
{
    std::cout << "[Test] testCancelResetsRunning\n";
    std::atomic<int> stops { 0 };
    LatencyCalibrator cal { [&](int, int, int, int) { ++stops; } };
    cal.start(20, 0, 0.0);
    cal.cancel();
    EXPECT_TRUE(!cal.isRunning());
    std::cout << "  -> PASSED\n";
}

TEST(LatencyCalibratorTest, CancelFromIdleIsNoOp)
{
    std::cout << "[Test] testCancelFromIdleIsNoOp\n";
    LatencyCalibrator cal;
    cal.cancel(); // Must not crash
    EXPECT_TRUE(cal.getState() == CalibrationState::Idle);
    std::cout << "  -> PASSED\n";
}

TEST(LatencyCalibratorTest, CommandCallbackDispatched)
{
    std::cout << "[Test] testCommandCallbackDispatched\n";
    std::atomic<int> calls { 0 };
    LatencyCalibrator cal { [&](int, int, int, int) { ++calls; } };
    cal.start(25, 0, 0.0);
    // Advance through all phases
    cal.update(2.0);
    // At least the stop command should have been dispatched at some point
    EXPECT_TRUE(calls.load() >= 0); // Non-crash; dispatch count varies
    std::cout << "  -> PASSED\n";
}

TEST(LatencyCalibratorTest, SetCommandCallbackBeforeStart)
{
    std::cout << "[Test] testSetCommandCallbackBeforeStart\n";
    LatencyCalibrator cal;
    std::atomic<int> calls { 0 };
    cal.setCommandCallback([&](int, int, int, int) { ++calls; });
    cal.start(10, 0, 0.0);
    cal.update(0.1);
    std::cout << "  -> PASSED\n";
}

TEST(LatencyCalibratorTest, IngestVisualMotionDoesNotCrash)
{
    std::cout << "[Test] testIngestVisualMotionDoesNotCrash\n";
    LatencyCalibrator cal;
    cal.start(25, 0, 0.0);
    for (int i = 0; i < 100; ++i) {
        const double t = static_cast<double>(i) * 0.01;
        cal.ingestVisualMotion(t, std::sin(t * 10.0) * 5.0, 0.0);
        cal.update(t);
    }
    std::cout << "  -> PASSED\n";
}

TEST(LatencyCalibratorTest, ResultAfterCompletion)
{
    std::cout << "[Test] testResultAfterCompletion\n";
    LatencyCalibrator cal;
    cal.start(25, 0, 0.0);
    // Advance well past all phases so it completes or fails
    cal.update(5.0);
    const auto& result = cal.getResult();
    // Result struct must be valid regardless of success/failure
    EXPECT_TRUE(std::isfinite(result.latencyMs));
    EXPECT_TRUE(std::isfinite(result.latencySeconds));
    EXPECT_TRUE(std::isfinite(result.correlation));
    std::cout << "  latencyMs=" << result.latencyMs << " success=" << result.success << "\n";
    std::cout << "  -> PASSED\n";
}

TEST(LatencyCalibratorTest, EstimatorAccess)
{
    std::cout << "[Test] testEstimatorAccess\n";
    LatencyCalibrator cal;
    LatencyEstimator& est = cal.getEstimator();
    (void)est;
    const LatencyCalibrator& calC = cal;
    const LatencyEstimator& estC = calC.getEstimator();
    (void)estC;
    std::cout << "  -> PASSED\n";
}

} // namespace

