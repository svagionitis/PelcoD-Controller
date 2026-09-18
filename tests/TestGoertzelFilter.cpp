/// @file TestGoertzelFilter.cpp
/// @brief Automated unit test suite for GoertzelFilter in PelcoDCore.

#include "GoertzelFilter.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace PelcoD;

namespace {

void testTargetFrequencyDetection()
{
    std::cout << "[Test] testTargetFrequencyDetection...\n";
    const double fs = 100.0;
    const double f0 = 10.0;
    const std::size_t n = 100U;
    const double amplitude = 2.0;

    GoertzelFilter filter(f0, fs, n);
    assert(filter.getTargetFrequency() == f0);
    assert(filter.getSampleRate() == fs);
    assert(filter.getBlockSize() == n);

    bool blockFinished = false;
    for (std::size_t i = 0U; i < n; ++i) {
        const double t = static_cast<double>(i) / fs;
        const double sample = amplitude * std::sin(2.0 * M_PI * f0 * t);
        if (filter.processSample(sample)) {
            blockFinished = true;
        }
    }
    assert(blockFinished);

    const double mag = filter.getMagnitude();
    std::cout << "  Detected magnitude: " << mag << " (expected: " << amplitude << ")\n";
    assert(std::abs(mag - amplitude) < 0.05);
    assert(std::abs(filter.getPower() - (amplitude * amplitude)) < 0.2);
    assert(filter.hasDetected(1.5));
    assert(!filter.hasDetected(2.5));

    std::cout << "  -> PASSED\n";
}

void testOffTargetFrequencyRejection()
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
    assert(mag < 0.1);
    assert(!filter.hasDetected(0.5));

    std::cout << "  -> PASSED\n";
}

void testStreamingVsBatchEquivalence()
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
    assert(std::abs(streamMag - batchMag) < 1e-10);

    std::cout << "  -> PASSED\n";
}

void testContinuousNonIntegerFrequency()
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
    assert(std::abs(mag - amplitude) < 0.08);

    std::cout << "  -> PASSED\n";
}

} // namespace

int main()
{
    std::cout << "Running TestGoertzelFilter Test Suite\n";
    testTargetFrequencyDetection();
    testOffTargetFrequencyRejection();
    testStreamingVsBatchEquivalence();
    testContinuousNonIntegerFrequency();
    std::cout << "All TestGoertzelFilter Tests Passed!\n";
    return 0;
}
