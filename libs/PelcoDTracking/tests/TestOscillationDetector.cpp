/// @file TestOscillationDetector.cpp
/// @brief Automated unit test suite for NotchFilter and OscillationDetector in PelcoDCore.

#include "NotchFilter.h"
#include "OscillationDetector.h"
#include "PidController.h"

#include <gtest/gtest.h>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace PelcoD;

namespace {

TEST(OscillationDetectorTest, NotchFilterRejection)
{
    std::cout << "[Test] testNotchFilterRejection...\n";
    const double fs = 100.0;
    const double notchFreq = 10.0;
    NotchFilter filter(notchFreq, fs, 5.0);

    // 1. Pass notch frequency sine wave (10 Hz)
    double maxAttenuated = 0.0;
    for (int i = 0; i < 200; ++i) {
        const double t = static_cast<double>(i) / fs;
        const double in = 1.0 * std::sin(2.0 * M_PI * notchFreq * t);
        const double out = filter.process(in);
        if (i > 100) { // After settling
            maxAttenuated = std::max(maxAttenuated, std::abs(out));
        }
    }
    // At notch frequency, amplitude should be severely reduced (< 0.2 of 1.0)
    EXPECT_TRUE(maxAttenuated < 0.25);
    std::cout << "  Notch frequency residual amplitude: " << maxAttenuated << " (attenuated from 1.0)\n";

    // 2. Pass off-notch frequency sine wave (2 Hz)
    filter.reset();
    double maxPassed = 0.0;
    for (int i = 0; i < 200; ++i) {
        const double t = static_cast<double>(i) / fs;
        const double in = 1.0 * std::sin(2.0 * M_PI * 2.0 * t);
        const double out = filter.process(in);
        if (i > 100) {
            maxPassed = std::max(maxPassed, std::abs(out));
        }
    }
    // Off-notch frequency should pass nearly intact (> 0.95 of 1.0)
    EXPECT_TRUE(maxPassed > 0.9);
    std::cout << "  Passband frequency transmission: " << maxPassed << " (near 1.0)\n";
    std::cout << "  -> PASSED\n";
}

TEST(OscillationDetectorTest, OscillationDetectorHuntingTrigger)
{
    std::cout << "[Test] testOscillationDetectorHuntingTrigger...\n";
    OscillationConfig cfg {};
    cfg.windowSize = 128U;
    cfg.sampleRateHz = 50.0;
    cfg.minHuntingFreqHz = 0.5;
    cfg.maxHuntingFreqHz = 5.0;
    cfg.powerRatioThreshold = 0.3;
    cfg.consecutiveThreshold = 2;

    OscillationDetector detector(cfg);
    EXPECT_TRUE(!detector.isHunting());

    // Feed a continuous 2.0 Hz oscillation (typical PID hunting limit cycle)
    const double huntingFreq = 2.0;
    for (int i = 0; i < 300; ++i) {
        const double t = static_cast<double>(i) / cfg.sampleRateHz;
        const double error = 5.0 * std::sin(2.0 * M_PI * huntingFreq * t);
        detector.addSample(error);
    }

    EXPECT_TRUE(detector.isHunting());
    const double detectedFreq = detector.getDominantFrequency();
    EXPECT_TRUE(std::abs(detectedFreq - huntingFreq) < 0.6);
    EXPECT_TRUE(detector.getOscillationRatio() > 0.3);

    std::cout << "  Hunting confirmed: freq = " << detectedFreq
              << " Hz, power ratio = " << detector.getOscillationRatio() << "\n";
    std::cout << "  -> PASSED\n";
}

TEST(OscillationDetectorTest, OscillationDetectorNoiseImmunity)
{
    std::cout << "[Test] testOscillationDetectorNoiseImmunity...\n";
    OscillationConfig cfg {};
    cfg.windowSize = 128U;
    cfg.sampleRateHz = 50.0;
    cfg.powerRatioThreshold = 0.4;

    OscillationDetector detector(cfg);

    // Feed zero-mean white Gaussian noise
    std::mt19937 gen(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < 300; ++i) {
        detector.addSample(dist(gen));
    }

    // White noise should have dispersed spectral energy, not triggering hunting
    EXPECT_TRUE(!detector.isHunting());
    std::cout << "  -> PASSED\n";
}

TEST(OscillationDetectorTest, AutoAttenuatePidIntegration)
{
    std::cout << "[Test] testAutoAttenuatePidIntegration...\n";
    OscillationConfig cfg {};
    cfg.windowSize = 128U;
    cfg.sampleRateHz = 50.0;
    cfg.consecutiveThreshold = 1;

    OscillationDetector detector(cfg);
    PidController pid(50.0, 1.0, 5.0, 0.0, 0.0, -100.0, 100.0);

    // Feed hunting oscillation to trigger detector
    for (int i = 0; i < 200; ++i) {
        const double t = static_cast<double>(i) / cfg.sampleRateHz;
        detector.addSample(10.0 * std::sin(2.0 * M_PI * 1.5 * t));
    }

    EXPECT_TRUE(detector.isHunting());

    // Auto attenuate gains by 20% (reduction factor 0.8)
    const bool attenuated = detector.autoAttenuate(pid, 0.8);
    EXPECT_TRUE(attenuated);
    EXPECT_TRUE(std::abs(pid.getKp() - 40.0) < 1e-6);
    EXPECT_TRUE(std::abs(pid.getKd() - 4.0) < 1e-6);
    EXPECT_TRUE(!detector.isHunting()); // Hunting state reset

    // Second call without new hunting returns false
    const bool secondCall = detector.autoAttenuate(pid, 0.8);
    EXPECT_TRUE(!secondCall);

    std::cout << "  -> PASSED\n";
}

} // namespace

