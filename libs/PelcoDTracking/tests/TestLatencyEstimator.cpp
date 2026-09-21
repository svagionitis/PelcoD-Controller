/// @file TestLatencyEstimator.cpp
/// @brief Automated unit test suite for LatencyEstimator in PelcoDCore.

#include "LatencyEstimator.h"

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

TEST(LatencyEstimatorTest, ExactSampleDelayRecovery)
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

    EXPECT_TRUE(estimator.isConfident());
    const double estimated = estimator.getEstimatedLatencyMs();
    const double corr = estimator.getPeakCorrelation();
    std::cout << "  Estimated latency: " << estimated << " ms (expected: " << expectedLatencyMs
              << " ms), Peak corr: " << corr << "\n";

    EXPECT_TRUE(std::abs(estimated - expectedLatencyMs) <= 1.0);
    EXPECT_TRUE(corr > 0.95);

    const auto curve = estimator.getCorrelationCurve();
    EXPECT_TRUE(!curve.empty());

    std::cout << "  -> PASSED\n";
}

TEST(LatencyEstimatorTest, SubSampleParabolicInterpolation)
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

    EXPECT_TRUE(estimator.isConfident());
    const double estimated = estimator.getEstimatedLatencyMs();
    std::cout << "  Sub-sample estimated latency: " << estimated << " ms (expected: " << expectedLatencyMs << " ms)\n";
    EXPECT_TRUE(std::abs(estimated - expectedLatencyMs) < 4.0);

    std::cout << "  -> PASSED\n";
}

TEST(LatencyEstimatorTest, UncorrelatedSignalsRejectConfidence)
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
    EXPECT_TRUE(!estimator.isConfident());
    EXPECT_TRUE(estimator.getPeakCorrelation() < 0.6);

    std::cout << "  -> PASSED\n";
}

TEST(LatencyEstimatorTest, DynamicLatencyTracking)
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
    EXPECT_TRUE(estimator.isConfident());
    EXPECT_TRUE(std::abs(estimator.getEstimatedLatencyMs() - 60.0) < 3.0);

    // Second phase: delay shifts to 120 ms (12 samples)
    for (int i = 150; i < 350; ++i) {
        const double ref = std::sin(static_cast<double>(i) * 0.15);
        const double resp = std::sin(static_cast<double>(i - 12) * 0.15);
        estimator.addSample(ref, resp);
    }
    estimator.update();
    EXPECT_TRUE(estimator.isConfident());
    std::cout << "  Dynamically shifted latency: " << estimator.getEstimatedLatencyMs() << " ms (expected: 120 ms)\n";
    EXPECT_TRUE(std::abs(estimator.getEstimatedLatencyMs() - 120.0) < 3.0);

    std::cout << "  -> PASSED\n";
}

TEST(LatencyEstimatorTest, InvertedNegativePolarity)
{
    std::cout << "[Test] testInvertedNegativePolarity (Camera Pan vs Optical Flow)...\n";
    const double fs = 100.0;
    const int delaySamples = 8; // 80 ms latency
    const double expectedLatencyMs = 80.0;

    LatencyEstimatorConfig cfg {};
    cfg.sampleRateHz = fs;
    cfg.bufferCapacity = 128U;
    cfg.minLagMs = 0.0;
    cfg.maxLagMs = 200.0;
    cfg.confidenceThreshold = 0.7;
    cfg.smoothingAlpha = 1.0;
    cfg.polarity = PeakPolarity::Negative;

    LatencyEstimator estimator(cfg);

    for (int i = 0; i < 200; ++i) {
        const double t = static_cast<double>(i) / fs;
        const double ref = std::sin(2.0 * M_PI * 2.0 * t);
        // Optical flow is inverted: scene shifts opposite to camera command
        const double resp = (i >= delaySamples) ? -std::sin(2.0 * M_PI * 2.0 * (t - (delaySamples / fs))) : 0.0;
        estimator.addSample(ref, resp);
    }
    estimator.update();

    EXPECT_TRUE(estimator.isConfident());
    const double estimated = estimator.getEstimatedLatencyMs();
    const double corr = estimator.getPeakCorrelation();
    std::cout << "  Negative polarity estimated latency: " << estimated << " ms (expected: " << expectedLatencyMs
              << " ms), Peak corr: " << corr << "\n";

    EXPECT_TRUE(corr < -0.90);
    EXPECT_TRUE(std::abs(estimated - expectedLatencyMs) <= 1.0);
    EXPECT_TRUE(std::abs(estimator.getEstimatedLatencySeconds() - 0.080) <= 0.002);
    EXPECT_TRUE(estimator.getPeakCorrelationMagnitude() > 0.90);

    std::cout << "  -> PASSED\n";
}

TEST(LatencyEstimatorTest, TimestampedAsynchronousResampling)
{
    std::cout << "[Test] testTimestampedAsynchronousResampling...\n";
    const double delaySec = 0.075; // 75 ms delay

    LatencyEstimatorConfig cfg {};
    cfg.sampleRateHz = 50.0;
    cfg.bufferCapacity = 128U;
    cfg.minLagMs = 0.0;
    cfg.maxLagMs = 250.0;
    cfg.confidenceThreshold = 0.7;
    cfg.smoothingAlpha = 1.0;
    cfg.polarity = PeakPolarity::Absolute;

    LatencyEstimator estimator(cfg);

    // Ingest reference at 25 Hz (commands every 40 ms) and response at 30 Hz (frames every 33.3 ms)
    double tRef = 0.0;
    double tResp = 0.0;

    for (int step = 0; step < 200; ++step) {
        tRef += 0.040;
        const double refVal = std::sin(2.0 * M_PI * 1.5 * tRef);
        estimator.addTimestampedReference(tRef, refVal);

        tResp += 0.033333;
        // Delayed signal
        const double delayedT = tResp - delaySec;
        const double respVal = (delayedT >= 0.0) ? std::sin(2.0 * M_PI * 1.5 * delayedT) : 0.0;
        estimator.addTimestampedResponse(tResp, respVal);
    }

    estimator.update();
    EXPECT_TRUE(estimator.isConfident());
    const double estimated = estimator.getEstimatedLatencyMs();
    std::cout << "  Asynchronous resampled latency: " << estimated << " ms (expected: 75 ms)\n";
    EXPECT_TRUE(std::abs(estimated - 75.0) < 5.0);

    std::cout << "  -> PASSED\n";
}

TEST(LatencyEstimatorTest, LowVarianceRejection)
{
    std::cout << "[Test] testLowVarianceRejection...\n";
    LatencyEstimatorConfig cfg {};
    cfg.minSignalVariance = 1e-4;
    cfg.confidenceThreshold = 0.5;

    LatencyEstimator estimator(cfg);

    // Flat constant signal
    for (int i = 0; i < 100; ++i) {
        estimator.addSample(5.0, 5.0);
    }
    estimator.update();

    EXPECT_TRUE(!estimator.isConfident());
    EXPECT_TRUE(estimator.getReferenceVariance() < 1e-6);

    std::cout << "  -> PASSED\n";
}

} // namespace

#include "LatencyCalibrator.h"

namespace {

TEST(LatencyEstimatorTest, LatencyCalibratorDoubletSequence)
{
    std::cout << "[Test] testLatencyCalibratorDoubletSequence...\n";
    double simulatedPanSpeed = 0.0;
    LatencyCalibrator calibrator([&simulatedPanSpeed](int panDir, int panSpeed, int, int) {
        simulatedPanSpeed = static_cast<double>(panDir * panSpeed);
    });

    const double trueDelaySec = 0.100; // 100 ms latency
    double now = 1000.0;
    EXPECT_TRUE(calibrator.start(30, 0, now));
    EXPECT_TRUE(calibrator.isRunning());

    std::deque<std::pair<double, double>> simulatedHistory;

    // Simulate 1.5 seconds at 50 Hz (20 ms steps)
    for (int i = 0; i < 75; ++i) {
        now += 0.020;
        calibrator.update(now);

        simulatedHistory.emplace_back(now, simulatedPanSpeed);

        // Delayed optical flow: inverted camera command with 100 ms delay
        const double delayedTime = now - trueDelaySec;
        double delayedSpeed = 0.0;
        if (!simulatedHistory.empty()) {
            if (delayedTime <= simulatedHistory.front().first) {
                delayedSpeed = simulatedHistory.front().second;
            } else if (delayedTime >= simulatedHistory.back().first) {
                delayedSpeed = simulatedHistory.back().second;
            } else {
                for (std::size_t j = 1; j < simulatedHistory.size(); ++j) {
                    if (simulatedHistory[j].first >= delayedTime) {
                        const double t0 = simulatedHistory[j - 1].first;
                        const double t1 = simulatedHistory[j].first;
                        const double v0 = simulatedHistory[j - 1].second;
                        const double v1 = simulatedHistory[j].second;
                        const double frac = (delayedTime - t0) / (t1 - t0);
                        delayedSpeed = v0 + frac * (v1 - v0);
                        break;
                    }
                }
            }
        }
        // Inverted response (pan right -> scene moves left)
        const double opticalFlowX = -delayedSpeed;
        calibrator.ingestVisualMotion(now, opticalFlowX);
    }

    EXPECT_TRUE(!calibrator.isRunning());
    const auto result = calibrator.getResult();
    std::cout << "  Calibrator result: success=" << result.success << ", latency=" << result.latencyMs
              << " ms, corr=" << result.correlation << "\n"
              << std::flush;

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(std::abs(result.latencyMs - 100.0) < 6.0);
    EXPECT_TRUE(result.correlation < -0.80);

    std::cout << "  -> PASSED\n";
}

} // namespace

