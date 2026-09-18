/// @file TestLatencyEstimator.cpp
/// @brief Automated unit test suite for LatencyEstimator in PelcoDCore.

#include "LatencyEstimator.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace PelcoD;

namespace {

void testExactSampleDelayRecovery()
{
    std::cout << "[Test] testExactSampleDelayRecovery...\n";
    const double fs = 100.0; // 10 ms per sample
    const int delaySamples = 10; // 100 ms expected latency
    const double expectedLatencyMs = 100.0;

    LatencyEstimatorConfig cfg {};
    cfg.sampleRateHz = fs;
    cfg.bufferCapacity = 128U;
    cfg.minLagMs = 0.0;
    cfg.maxLagMs = 300.0;
    cfg.confidenceThreshold = 0.7;
    cfg.smoothingAlpha = 1.0; // Instant latch

    LatencyEstimator estimator(cfg);

    std::vector<double> refSignal(200U, 0.0);
    std::vector<double> respSignal(200U, 0.0);

    for (std::size_t i = 0U; i < 200U; ++i) {
        const double t = static_cast<double>(i) / fs;
        refSignal[i] = std::sin(2.0 * M_PI * 1.5 * t) + 0.5 * std::cos(2.0 * M_PI * 4.2 * t);
        if (static_cast<int>(i) >= delaySamples) {
            respSignal[i] = refSignal[i - delaySamples];
        }
    }

    for (std::size_t i = 0U; i < 200U; ++i) {
        estimator.addSample(refSignal[i], respSignal[i]);
    }
    estimator.update();

    assert(estimator.isConfident());
    const double estimated = estimator.getEstimatedLatencyMs();
    const double corr = estimator.getPeakCorrelation();
    std::cout << "  Estimated latency: " << estimated << " ms (expected: " << expectedLatencyMs
              << " ms), Peak corr: " << corr << "\n";

    assert(std::abs(estimated - expectedLatencyMs) <= 1.0);
    assert(corr > 0.95);

    const auto curve = estimator.getCorrelationCurve();
    assert(!curve.empty());

    std::cout << "  -> PASSED\n";
}

void testSubSampleParabolicInterpolation()
{
    std::cout << "[Test] testSubSampleParabolicInterpolation...\n";
    const double fs = 50.0; // 20 ms per sample
    // 3.5 samples delay = 70 ms expected latency
    const double expectedLatencyMs = 70.0;

    LatencyEstimatorConfig cfg {};
    cfg.sampleRateHz = fs;
    cfg.bufferCapacity = 128U;
    cfg.minLagMs = 0.0;
    cfg.maxLagMs = 200.0;
    cfg.confidenceThreshold = 0.6;
    cfg.smoothingAlpha = 1.0;

    LatencyEstimator estimator(cfg);

    std::vector<double> ref(200U, 0.0);
    std::vector<double> resp(200U, 0.0);

    for (std::size_t i = 0U; i < 200U; ++i) {
        const double t = static_cast<double>(i) / fs;
        ref[i] = std::sin(2.0 * M_PI * 1.2 * t);
        if (i >= 5U) {
            // Half-sample delay interpolation between lag 3 and lag 4
            resp[i] = 0.5 * ref[i - 3U] + 0.5 * ref[i - 4U];
        }
    }

    for (std::size_t i = 0U; i < 200U; ++i) {
        estimator.addSample(ref[i], resp[i]);
    }
    estimator.update();

    assert(estimator.isConfident());
    const double estimated = estimator.getEstimatedLatencyMs();
    std::cout << "  Sub-sample estimated latency: " << estimated << " ms (expected: " << expectedLatencyMs << " ms)\n";
    assert(std::abs(estimated - expectedLatencyMs) < 4.0);

    std::cout << "  -> PASSED\n";
}

void testUncorrelatedSignalsRejectConfidence()
{
    std::cout << "[Test] testUncorrelatedSignalsRejectConfidence...\n";
    LatencyEstimatorConfig cfg {};
    cfg.sampleRateHz = 50.0;
    cfg.bufferCapacity = 128U;
    cfg.confidenceThreshold = 0.6;

    LatencyEstimator estimator(cfg);

    std::mt19937 gen(12345);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < 200; ++i) {
        const double ref = std::sin(static_cast<double>(i) * 0.1);
        const double noise = dist(gen);
        estimator.addSample(ref, noise);
    }
    estimator.update();

    std::cout << "  Noise peak correlation: " << estimator.getPeakCorrelation() << "\n";
    assert(!estimator.isConfident());
    assert(estimator.getPeakCorrelation() < 0.6);

    std::cout << "  -> PASSED\n";
}

void testDynamicLatencyTracking()
{
    std::cout << "[Test] testDynamicLatencyTracking...\n";
    const double fs = 100.0;
    LatencyEstimatorConfig cfg {};
    cfg.sampleRateHz = fs;
    cfg.bufferCapacity = 128U;
    cfg.minLagMs = 0.0;
    cfg.maxLagMs = 250.0;
    cfg.confidenceThreshold = 0.7;
    cfg.smoothingAlpha = 0.5;

    LatencyEstimator estimator(cfg);

    // Initial phase: 60 ms delay (6 samples)
    for (int i = 0; i < 150; ++i) {
        const double ref = std::sin(static_cast<double>(i) * 0.15);
        const double resp = (i >= 6) ? std::sin(static_cast<double>(i - 6) * 0.15) : 0.0;
        estimator.addSample(ref, resp);
    }
    estimator.update();
    assert(estimator.isConfident());
    assert(std::abs(estimator.getEstimatedLatencyMs() - 60.0) < 3.0);

    // Second phase: delay shifts to 120 ms (12 samples)
    for (int i = 150; i < 350; ++i) {
        const double ref = std::sin(static_cast<double>(i) * 0.15);
        const double resp = std::sin(static_cast<double>(i - 12) * 0.15);
        estimator.addSample(ref, resp);
    }
    estimator.update();
    assert(estimator.isConfident());
    std::cout << "  Dynamically shifted latency: " << estimator.getEstimatedLatencyMs() << " ms (expected: 120 ms)\n";
    assert(std::abs(estimator.getEstimatedLatencyMs() - 120.0) < 3.0);

    std::cout << "  -> PASSED\n";
}

} // namespace

int main()
{
    std::cout << "Running TestLatencyEstimator Test Suite\n";
    testExactSampleDelayRecovery();
    testSubSampleParabolicInterpolation();
    testUncorrelatedSignalsRejectConfidence();
    testDynamicLatencyTracking();
    std::cout << "All TestLatencyEstimator Tests Passed!\n";
    return 0;
}
