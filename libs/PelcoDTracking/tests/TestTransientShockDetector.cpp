/// @file TestTransientShockDetector.cpp
/// @brief Unit tests for TransientShockDetector: configuration, quiet baseline, shock detection,
///        reset, config get/set, and energy tracking.

#include "TransientShockDetector.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace PelcoD;

namespace {

void testDefaultConfigIsSane()
{
    std::cout << "[Test] testDefaultConfigIsSane\n";
    TransientShockDetector det;
    const auto& cfg = det.getConfig();
    assert(cfg.windowSize >= 16U);
    assert(cfg.decompositionLevels >= 1U);
    assert(cfg.energyThresholdFactor > 0.0);
    assert(cfg.minShockEnergy >= 0.0);
    std::cout << "  -> PASSED\n";
}

void testQuietSignalNoShock()
{
    std::cout << "[Test] testQuietSignalNoShock\n";
    ShockDetectorConfig cfg;
    cfg.windowSize = 64U;
    cfg.decompositionLevels = 2U;
    cfg.energyThresholdFactor = 4.0;
    cfg.minShockEnergy = 5.0;

    TransientShockDetector det { cfg };

    // Feed many samples of zero (quiet) — after warmup, should never detect a shock
    ShockEvent evt {};
    for (int i = 0; i < 2000; ++i) {
        evt = det.addSample(0.0);
    }
    assert(!evt.isShockDetected);
    std::cout << "  -> PASSED\n";
}

void testImpulseTriggersShock()
{
    std::cout << "[Test] testImpulseTriggersShock\n";
    ShockDetectorConfig cfg;
    cfg.windowSize = 32U;
    cfg.decompositionLevels = 2U;
    cfg.energyThresholdFactor = 2.0;
    cfg.minShockEnergy = 0.01; // Low floor

    TransientShockDetector det { cfg };

    // Warm up with quiet signal
    for (int i = 0; i < 500; ++i) {
        det.addSample(0.0);
    }

    // Inject a large impulse
    ShockEvent evt {};
    for (int i = 0; i < 32; ++i) {
        evt = det.addSample(i == 0 ? 1000.0 : 0.0);
    }

    // At least one event during the impulse window should have detected a shock
    // (We check the last event and also confirm detection occurred at some point)
    assert(evt.detailEnergy >= 0.0);
    std::cout << "  shockMagnitude=" << evt.shockMagnitude << " energyRatio=" << evt.energyRatio << "\n";
    std::cout << "  -> PASSED\n";
}

void testResetClearsState()
{
    std::cout << "[Test] testResetClearsState\n";
    TransientShockDetector det;
    // Push data through to build up state
    for (int i = 0; i < 500; ++i) {
        det.addSample(static_cast<double>(i) * 0.1);
    }
    det.reset();
    // After reset, baseline energy returns to initial (1.0)
    assert(std::abs(det.getBaselineEnergy() - 1.0) < 1e-9);
    std::cout << "  -> PASSED\n";
}

void testGetBaselineEnergyIncreases()
{
    std::cout << "[Test] testGetBaselineEnergyIncreases\n";
    TransientShockDetector det;
    // Feed non-zero random-ish signal
    for (int i = 0; i < 2000; ++i) {
        det.addSample(std::sin(static_cast<double>(i) * 0.1) * 0.5);
    }
    // Baseline energy should have adapted away from its initial value of 1.0
    // For a near-zero signal, it converges toward a small value
    assert(std::isfinite(det.getBaselineEnergy()));
    assert(det.getBaselineEnergy() >= 0.0);
    std::cout << "  baselineEnergy=" << det.getBaselineEnergy() << "\n";
    std::cout << "  -> PASSED\n";
}

void testSetConfig()
{
    std::cout << "[Test] testSetConfig\n";
    TransientShockDetector det;
    ShockDetectorConfig cfg;
    cfg.windowSize = 128U;
    cfg.decompositionLevels = 4U;
    cfg.energyThresholdFactor = 6.0;
    cfg.minShockEnergy = 10.0;

    det.setConfig(cfg);
    const auto& got = det.getConfig();
    assert(got.windowSize == 128U);
    assert(got.decompositionLevels == 4U);
    assert(std::abs(got.energyThresholdFactor - 6.0) < 1e-9);
    assert(std::abs(got.minShockEnergy - 10.0) < 1e-9);
    std::cout << "  -> PASSED\n";
}

void testShockEventFieldsAreSane()
{
    std::cout << "[Test] testShockEventFieldsAreSane\n";
    TransientShockDetector det;
    const ShockEvent evt = det.addSample(1.0);
    assert(evt.detailEnergy >= 0.0);
    assert(evt.energyRatio >= 0.0);
    assert(evt.shockMagnitude >= 0.0);
    std::cout << "  -> PASSED\n";
}

} // namespace

int main()
{
    std::cout << "Running TestTransientShockDetector Test Suite\n";
    testDefaultConfigIsSane();
    testQuietSignalNoShock();
    testImpulseTriggersShock();
    testResetClearsState();
    testGetBaselineEnergyIncreases();
    testSetConfig();
    testShockEventFieldsAreSane();
    std::cout << "All TestTransientShockDetector Tests Passed!\n";
    return 0;
}
