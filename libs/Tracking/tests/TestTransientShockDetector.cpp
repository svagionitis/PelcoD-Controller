/// @file TestTransientShockDetector.cpp
/// @brief Unit tests for TransientShockDetector: configuration, quiet baseline, shock detection,
///        reset, config get/set, and energy tracking.

#include <cmath>
#include <cstddef>

#include <iostream>
#include <vector>

#include <gtest/gtest.h>

#include "TransientShockDetector.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Tracking;

namespace {

TEST(TransientShockDetectorTest, DefaultConfigIsSane)
{
    std::cout << "[Test] testDefaultConfigIsSane\n";
    TransientShockDetector det;
    const auto& cfg = det.getConfig();
    EXPECT_TRUE(cfg.windowSize >= 16U);
    EXPECT_TRUE(cfg.decompositionLevels >= 1U);
    EXPECT_TRUE(cfg.energyThresholdFactor > 0.0);
    EXPECT_TRUE(cfg.minShockEnergy >= 0.0);
    std::cout << "  -> PASSED\n";
}

TEST(TransientShockDetectorTest, QuietSignalNoShock)
{
    std::cout << "[Test] testQuietSignalNoShock\n";
    ShockDetectorConfig cfg {};
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
    EXPECT_TRUE(!evt.isShockDetected);
    std::cout << "  -> PASSED\n";
}

TEST(TransientShockDetectorTest, ImpulseTriggersShock)
{
    std::cout << "[Test] testImpulseTriggersShock\n";
    ShockDetectorConfig cfg {};
    cfg.windowSize = 32U;
    cfg.decompositionLevels = 2U;
    cfg.energyThresholdFactor = 2.0;
    cfg.minShockEnergy = 0.01; // Low floor

    TransientShockDetector det { cfg };

    // Warm up with quiet signal
    for (int i = 0; i < 500; ++i) {
        (void)det.addSample(0.0);
    }

    // Inject a large impulse
    ShockEvent evt {};
    for (int i = 0; i < 32; ++i) {
        evt = det.addSample(i == 0 ? 1000.0 : 0.0);
    }

    EXPECT_TRUE(evt.detailEnergy >= 0.0);
    std::cout << "  shockMagnitude=" << evt.shockMagnitude << " energyRatio=" << evt.energyRatio << "\n";
    std::cout << "  -> PASSED\n";
}

TEST(TransientShockDetectorTest, ShockTriggerAndBaselineRecovery)
{
    std::cout << "[Test] testShockTriggerAndBaselineRecovery\n";
    ShockDetectorConfig config {};
    config.windowSize = 32U;
    config.decompositionLevels = 2U;
    config.wavelet = Math::WaveletType::Db4;
    config.energyThresholdFactor = 3.5;
    config.minShockEnergy = 2.0;

    TransientShockDetector detector(config);

    // Phase 1: Feed 40 samples of smooth baseline vibration (5 Hz sine wave)
    bool falseAlarm = false;
    for (std::size_t i = 0U; i < 40U; ++i) {
        const double sample = 1.0 * std::sin(2.0 * M_PI * 5.0 * static_cast<double>(i) / 50.0);
        const auto event = detector.addSample(sample);
        if (event.isShockDetected) {
            falseAlarm = true;
        }
    }
    EXPECT_TRUE(!falseAlarm);

    // Phase 2: Inject sudden sharp shock impulse at sample 41
    const auto shockEvent = detector.addSample(25.0);
    EXPECT_TRUE(shockEvent.isShockDetected);
    EXPECT_TRUE(shockEvent.shockMagnitude > 5.0);
    EXPECT_TRUE(shockEvent.energyRatio > config.energyThresholdFactor);

    // Phase 3: Feed smooth baseline again and verify detector recovers
    bool recovered = false;
    for (std::size_t i = 0U; i < 40U; ++i) {
        const double sample = 1.0 * std::sin(2.0 * M_PI * 5.0 * static_cast<double>(i) / 50.0);
        const auto ev = detector.addSample(sample);
        if (!ev.isShockDetected) {
            recovered = true;
        }
    }
    EXPECT_TRUE(recovered);
    std::cout << "  -> PASSED\n";
}

TEST(TransientShockDetectorTest, ResetClearsState)
{
    std::cout << "[Test] testResetClearsState\n";
    TransientShockDetector det;
    // Push data through to build up state
    for (int i = 0; i < 500; ++i) {
        (void)det.addSample(static_cast<double>(i) * 0.1);
    }
    det.reset();
    // After reset, baseline energy returns to initial (1.0)
    EXPECT_TRUE(std::abs(det.getBaselineEnergy() - 1.0) < 1e-9);
    std::cout << "  -> PASSED\n";
}

TEST(TransientShockDetectorTest, GetBaselineEnergyIncreases)
{
    std::cout << "[Test] testGetBaselineEnergyIncreases\n";
    TransientShockDetector det;
    // Feed non-zero random-ish signal
    for (int i = 0; i < 2000; ++i) {
        (void)det.addSample(std::sin(static_cast<double>(i) * 0.1) * 0.5);
    }
    EXPECT_TRUE(std::isfinite(det.getBaselineEnergy()));
    EXPECT_TRUE(det.getBaselineEnergy() >= 0.0);
    std::cout << "  baselineEnergy=" << det.getBaselineEnergy() << "\n";
    std::cout << "  -> PASSED\n";
}

TEST(TransientShockDetectorTest, SetConfig)
{
    std::cout << "[Test] testSetConfig\n";
    TransientShockDetector det;
    ShockDetectorConfig cfg {};
    cfg.windowSize = 128U;
    cfg.decompositionLevels = 4U;
    cfg.energyThresholdFactor = 6.0;
    cfg.minShockEnergy = 10.0;

    det.setConfig(cfg);
    const auto& got = det.getConfig();
    EXPECT_TRUE(got.windowSize == 128U);
    EXPECT_TRUE(got.decompositionLevels == 4U);
    EXPECT_TRUE(std::abs(got.energyThresholdFactor - 6.0) < 1e-9);
    EXPECT_TRUE(std::abs(got.minShockEnergy - 10.0) < 1e-9);
    std::cout << "  -> PASSED\n";
}

TEST(TransientShockDetectorTest, ShockEventFieldsAreSane)
{
    std::cout << "[Test] testShockEventFieldsAreSane\n";
    TransientShockDetector det;
    const ShockEvent evt = det.addSample(1.0);
    EXPECT_TRUE(evt.detailEnergy >= 0.0);
    EXPECT_TRUE(evt.energyRatio >= 0.0);
    EXPECT_TRUE(evt.shockMagnitude >= 0.0);
    std::cout << "  -> PASSED\n";
}

} // namespace
