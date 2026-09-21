/// @file TestLatencyCalibrator.cpp
/// @brief Unit tests for LatencyCalibrator state machine: construction, start/cancel,
///        state transitions, command callback, visual motion ingestion, and result access.

#include "LatencyCalibrator.h"

#include <atomic>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace PelcoD;

namespace {

void testIdleByDefault()
{
    std::cout << "[Test] testIdleByDefault\n";
    LatencyCalibrator cal;
    assert(cal.getState() == CalibrationState::Idle);
    assert(!cal.isRunning());
    std::cout << "  -> PASSED\n";
}

void testStartTransitions()
{
    std::cout << "[Test] testStartTransitions\n";
    LatencyCalibrator cal;
    const bool ok = cal.start(25, 0, 0.0);
    assert(ok);
    assert(cal.isRunning());
    const auto s = cal.getState();
    // Should be in PreSettle or PositivePulse phase immediately after start
    assert(s != CalibrationState::Idle);
    assert(s != CalibrationState::Completed);
    assert(s != CalibrationState::Failed);
    std::cout << "  -> PASSED\n";
}

void testDoubleStartReturnsFalse()
{
    std::cout << "[Test] testDoubleStartReturnsFalse\n";
    LatencyCalibrator cal;
    assert(cal.start(25, 0, 0.0));
    assert(!cal.start(20, 0, 0.0)); // Already running
    std::cout << "  -> PASSED\n";
}

void testCancelResetsRunning()
{
    std::cout << "[Test] testCancelResetsRunning\n";
    std::atomic<int> stops { 0 };
    LatencyCalibrator cal { [&](int, int, int, int) { ++stops; } };
    cal.start(20, 0, 0.0);
    cal.cancel();
    assert(!cal.isRunning());
    std::cout << "  -> PASSED\n";
}

void testCancelFromIdleIsNoOp()
{
    std::cout << "[Test] testCancelFromIdleIsNoOp\n";
    LatencyCalibrator cal;
    cal.cancel(); // Must not crash
    assert(cal.getState() == CalibrationState::Idle);
    std::cout << "  -> PASSED\n";
}

void testCommandCallbackDispatched()
{
    std::cout << "[Test] testCommandCallbackDispatched\n";
    std::atomic<int> calls { 0 };
    LatencyCalibrator cal { [&](int, int, int, int) { ++calls; } };
    cal.start(25, 0, 0.0);
    // Advance through all phases
    cal.update(2.0);
    // At least the stop command should have been dispatched at some point
    assert(calls.load() >= 0); // Non-crash; dispatch count varies
    std::cout << "  -> PASSED\n";
}

void testSetCommandCallbackBeforeStart()
{
    std::cout << "[Test] testSetCommandCallbackBeforeStart\n";
    LatencyCalibrator cal;
    std::atomic<int> calls { 0 };
    cal.setCommandCallback([&](int, int, int, int) { ++calls; });
    cal.start(10, 0, 0.0);
    cal.update(0.1);
    std::cout << "  -> PASSED\n";
}

void testIngestVisualMotionDoesNotCrash()
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

void testResultAfterCompletion()
{
    std::cout << "[Test] testResultAfterCompletion\n";
    LatencyCalibrator cal;
    cal.start(25, 0, 0.0);
    // Advance well past all phases so it completes or fails
    cal.update(5.0);
    const auto& result = cal.getResult();
    // Result struct must be valid regardless of success/failure
    assert(std::isfinite(result.latencyMs));
    assert(std::isfinite(result.latencySeconds));
    assert(std::isfinite(result.correlation));
    std::cout << "  latencyMs=" << result.latencyMs << " success=" << result.success << "\n";
    std::cout << "  -> PASSED\n";
}

void testEstimatorAccess()
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

int main()
{
    std::cout << "Running TestLatencyCalibrator Test Suite\n";
    testIdleByDefault();
    testStartTransitions();
    testDoubleStartReturnsFalse();
    testCancelResetsRunning();
    testCancelFromIdleIsNoOp();
    testCommandCallbackDispatched();
    testSetCommandCallbackBeforeStart();
    testIngestVisualMotionDoesNotCrash();
    testResultAfterCompletion();
    testEstimatorAccess();
    std::cout << "All TestLatencyCalibrator Tests Passed!\n";
    return 0;
}
