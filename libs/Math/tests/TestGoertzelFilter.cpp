/// @file TestGoertzelFilter.cpp
/// @brief Automated unit test suite for GoertzelFilter in PelcoDCore.

#include "GoertzelFilter.h"

#include <gtest/gtest.h>
#include <cmath>
#include <iostream>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace PelcoD;

namespace {

TEST(GoertzelFilterTest, TargetFrequencyDetection)
{
    std::cout << "[Test] testTargetFrequencyDetection...\n";
    const double fs = 100.0;
    const double f0 = 10.0;
    const std::size_t n = 100U;
    const double amplitude = 2.0;

    GoertzelFilter filter(f0, fs, n);
    EXPECT_TRUE(filter.getTargetFrequency() == f0);
    EXPECT_TRUE(filter.getSampleRate() == fs);
    EXPECT_TRUE(filter.getBlockSize() == n);

    bool blockFinished = false;
    for (std::size_t i = 0U; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        const double sample = amplitude * std::sin(2.0 * M_PI * f0 * t);
        if (filter.processSample(sample)) {
            blockFinished = true;
        }
    }
    EXPECT_TRUE(blockFinished);

    const double mag = filter.getMagnitude();
    std::cout << "  Detected magnitude: " << mag << " (expected: " << amplitude << ")\n";
    EXPECT_TRUE(std::abs(mag - amplitude) < 0.05);
    EXPECT_TRUE(std::abs(filter.getPower() - (amplitude * amplitude)) < 0.2);
    EXPECT_TRUE(filter.hasDetected(1.5));
    EXPECT_TRUE(!filter.hasDetected(2.5));

    std::cout << "  -> PASSED\n";
}

TEST(GoertzelFilterTest, OffTargetFrequencyRejection)
{
    std::cout << "[Test] testOffTargetFrequencyRejection...\n";
    const double fs = 100.0;
    const double f0 = 10.0;
    const double fOff = 25.0;
    const std::size_t n = 100U;
    const double amplitude = 2.0;

    GoertzelFilter filter(f0, fs, n);

    for (std::size_t i = 0U; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        const double sample = amplitude * std::sin(2.0 * M_PI * fOff * t);
        filter.processSample(sample);
    }

    const double mag = filter.getMagnitude();
    std::cout << "  Off-target magnitude at 25 Hz: " << mag << " (rejected from " << amplitude << ")\n";
    EXPECT_TRUE(mag < 0.1);
    EXPECT_TRUE(!filter.hasDetected(0.5));

    std::cout << "  -> PASSED\n";
}

TEST(GoertzelFilterTest, StreamingVsBatchEquivalence)
{
    std::cout << "[Test] testStreamingVsBatchEquivalence...\n";
    const double fs = 100.0;
    const double f0 = 15.0;
    const std::size_t n = 64U;

    GoertzelFilter filter(f0, fs, n);

    std::vector<double> samples(n, 0.0);
    for (std::size_t i = 0U; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        samples[i] = 1.75 * std::cos(2.0 * M_PI * f0 * t);
        filter.processSample(samples[i]);
    }

    const double streamMag = filter.getMagnitude();
    const double batchMag = filter.computeMagnitude(samples.data(), samples.size());

    std::cout << "  Stream magnitude: " << streamMag << ", Batch magnitude: " << batchMag << "\n";
    EXPECT_TRUE(std::abs(streamMag - batchMag) < 1e-10);

    std::cout << "  -> PASSED\n";
}

TEST(GoertzelFilterTest, ContinuousNonIntegerFrequency)
{
    std::cout << "[Test] testContinuousNonIntegerFrequency...\n";
    const double fs = 100.0;
    const double f0 = 12.35; // Non-integer bin (12.35 * 100 / 100 = 12.35)
    const std::size_t n = 100U;
    const double amplitude = 3.0;

    GoertzelFilter filter(f0, fs, n);

    for (std::size_t i = 0U; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        filter.processSample(amplitude * std::sin(2.0 * M_PI * f0 * t));
    }

    const double mag = filter.getMagnitude();
    std::cout << "  Non-integer frequency 12.35 Hz magnitude: " << mag << " (expected: " << amplitude << ")\n";
    EXPECT_TRUE(std::abs(mag - amplitude) < 0.08);

    std::cout << "  -> PASSED\n";
}

} // namespace

