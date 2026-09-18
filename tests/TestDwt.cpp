/// @file TestDwt.cpp
/// @brief Automated unit test suite for Discrete Wavelet Transform (DWT) and Transient Shock Detector in PelcoDCore.

#include "Dwt.h"
#include "TransientShockDetector.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace PelcoD;
using namespace PelcoD::Math;

namespace {

void testHaar1DRoundtripIdentity()
{
    std::cout << "[Test] testHaar1DRoundtripIdentity...\n";
    const std::size_t n = 32U;
    std::vector<double> signal(n);
    for (std::size_t i = 0U; i < n; ++i) {
        signal[i] = std::sin(2.0 * M_PI * static_cast<double>(i) / 8.0)
            + 0.5 * std::cos(2.0 * M_PI * static_cast<double>(i) / 3.0);
    }

    std::vector<double> cA;
    std::vector<double> cD;
    dwt1D(signal, cA, cD, WaveletType::Haar);

    assert(cA.size() == n / 2U);
    assert(cD.size() == n / 2U);

    const auto reconstructed = idwt1D(cA, cD, WaveletType::Haar);
    assert(reconstructed.size() == n);

    double maxError = 0.0;
    for (std::size_t i = 0U; i < n; ++i) {
        const double err = std::abs(signal[i] - reconstructed[i]);
        if (err > maxError) {
            maxError = err;
        }
    }

    std::cout << "  -> Haar 1D max error: " << maxError << "\n";
    assert(maxError < 1e-11);
    std::cout << "  -> PASSED\n";
}

void testDb41DRoundtripIdentity()
{
    std::cout << "[Test] testDb41DRoundtripIdentity...\n";
    const std::size_t n = 32U;
    std::vector<double> signal(n);
    for (std::size_t i = 0U; i < n; ++i) {
        signal[i] = 2.5 * std::sin(2.0 * M_PI * static_cast<double>(i) / 7.0)
            - 1.2 * std::cos(2.0 * M_PI * static_cast<double>(i) / 11.0);
    }

    std::vector<double> cA;
    std::vector<double> cD;
    dwt1D(signal, cA, cD, WaveletType::Db4);

    assert(cA.size() == n / 2U);
    assert(cD.size() == n / 2U);

    const auto reconstructed = idwt1D(cA, cD, WaveletType::Db4);
    assert(reconstructed.size() == n);

    double maxError = 0.0;
    for (std::size_t i = 0U; i < n; ++i) {
        const double err = std::abs(signal[i] - reconstructed[i]);
        if (err > maxError) {
            maxError = err;
        }
    }

    std::cout << "  -> Db4 1D max error: " << maxError << "\n";
    assert(maxError < 1e-10);
    std::cout << "  -> PASSED\n";
}

void testMultiLevelWavedecWaverec()
{
    std::cout << "[Test] testMultiLevelWavedecWaverec...\n";
    const std::size_t n = 128U;
    std::vector<double> signal(n);
    for (std::size_t i = 0U; i < n; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(n);
        signal[i] = std::sin(2.0 * M_PI * 5.0 * t) + 0.3 * std::sin(2.0 * M_PI * 30.0 * t);
    }

    const std::size_t levels = 3U;
    const auto decomp = wavedec(signal, levels, WaveletType::Db4);

    assert(decomp.cD.size() == 3U);
    assert(decomp.cD[0].size() == 64U); // Level 1 details
    assert(decomp.cD[1].size() == 32U); // Level 2 details
    assert(decomp.cD[2].size() == 16U); // Level 3 details
    assert(decomp.cA.size() == 16U); // Level 3 approximation

    const auto reconstructed = waverec(decomp);
    assert(reconstructed.size() == n);

    double maxError = 0.0;
    for (std::size_t i = 0U; i < n; ++i) {
        const double err = std::abs(signal[i] - reconstructed[i]);
        if (err > maxError) {
            maxError = err;
        }
    }

    std::cout << "  -> Multi-level (3-level) waverec max error: " << maxError << "\n";
    assert(maxError < 1e-10);
    std::cout << "  -> PASSED\n";
}

void test2DImageDwtRoundtrip()
{
    std::cout << "[Test] test2DImageDwtRoundtrip...\n";
    const std::size_t rows = 32U;
    const std::size_t cols = 32U;
    std::vector<double> image(rows * cols);

    for (std::size_t r = 0U; r < rows; ++r) {
        for (std::size_t c = 0U; c < cols; ++c) {
            const double v = std::sin(2.0 * M_PI * static_cast<double>(r) / 8.0)
                + std::cos(2.0 * M_PI * static_cast<double>(c) / 8.0);
            image[r * cols + c] = v;
        }
    }

    // Test Haar 2D
    const auto coeffsHaar = dwt2D(image, rows, cols, WaveletType::Haar);
    assert(coeffsHaar.rows == rows / 2U);
    assert(coeffsHaar.cols == cols / 2U);

    const auto recHaar = idwt2D(coeffsHaar, WaveletType::Haar);
    assert(recHaar.size() == rows * cols);

    double maxErrHaar = 0.0;
    for (std::size_t i = 0U; i < rows * cols; ++i) {
        const double err = std::abs(image[i] - recHaar[i]);
        if (err > maxErrHaar) {
            maxErrHaar = err;
        }
    }
    std::cout << "  -> 2D Haar max error: " << maxErrHaar << "\n";
    assert(maxErrHaar < 1e-10);

    // Test Db4 2D
    const auto coeffsDb4 = dwt2D(image, rows, cols, WaveletType::Db4);
    const auto recDb4 = idwt2D(coeffsDb4, WaveletType::Db4);
    assert(recDb4.size() == rows * cols);

    double maxErrDb4 = 0.0;
    for (std::size_t i = 0U; i < rows * cols; ++i) {
        const double err = std::abs(image[i] - recDb4[i]);
        if (err > maxErrDb4) {
            maxErrDb4 = err;
        }
    }
    std::cout << "  -> 2D Db4 max error: " << maxErrDb4 << "\n";
    assert(maxErrDb4 < 1e-10);
    std::cout << "  -> PASSED\n";
}

void testWaveletDenoiseVisuShrink()
{
    std::cout << "[Test] testWaveletDenoiseVisuShrink...\n";
    const std::size_t n = 128U;
    std::vector<double> clean(n);
    std::vector<double> noisy(n);

    uint32_t seed = 9876U;
    auto prng = [&seed]() -> double {
        seed = seed * 1664525U + 1013904223U;
        return (static_cast<double>(seed & 0xFFFFU) / 32768.0) - 1.0;
    };

    for (std::size_t i = 0U; i < n; ++i) {
        clean[i] = (i < n / 2U) ? 2.0 : -2.0; // Sharp step signal
        noisy[i] = clean[i] + 0.6 * prng(); // Add random noise
    }

    const auto denoised = waveletDenoise(noisy, 3U, WaveletType::Db4);
    assert(denoised.size() == n);

    double noisyMse = 0.0;
    double denoisedMse = 0.0;
    for (std::size_t i = 0U; i < n; ++i) {
        noisyMse += (noisy[i] - clean[i]) * (noisy[i] - clean[i]);
        denoisedMse += (denoised[i] - clean[i]) * (denoised[i] - clean[i]);
    }
    noisyMse /= static_cast<double>(n);
    denoisedMse /= static_cast<double>(n);

    std::cout << "  -> Noisy MSE: " << noisyMse << ", Denoised MSE: " << denoisedMse << "\n";
    assert(denoisedMse < noisyMse);
    std::cout << "  -> PASSED\n";
}

void testTransientShockDetector()
{
    std::cout << "[Test] testTransientShockDetector...\n";
    ShockDetectorConfig config {};
    config.windowSize = 32U;
    config.decompositionLevels = 2U;
    config.wavelet = WaveletType::Db4;
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
    assert(!falseAlarm);

    // Phase 2: Inject sudden sharp shock impulse at sample 41
    const auto shockEvent = detector.addSample(25.0);
    std::cout << "  -> Shock event triggered: " << shockEvent.isShockDetected
              << ", magnitude: " << shockEvent.shockMagnitude << ", energyRatio: " << shockEvent.energyRatio << "\n";

    assert(shockEvent.isShockDetected);
    assert(shockEvent.shockMagnitude > 5.0);
    assert(shockEvent.energyRatio > config.energyThresholdFactor);

    // Phase 3: Feed smooth baseline again and verify detector recovers
    bool recovered = false;
    for (std::size_t i = 0U; i < 40U; ++i) {
        const double sample = 1.0 * std::sin(2.0 * M_PI * 5.0 * static_cast<double>(i) / 50.0);
        const auto ev = detector.addSample(sample);
        if (!ev.isShockDetected) {
            recovered = true;
        }
    }
    assert(recovered);

    std::cout << "  -> PASSED\n";
}

void testEdgeAndOddLengthCases()
{
    std::cout << "[Test] testEdgeAndOddLengthCases...\n";
    // Odd length signal (e.g. 7 elements)
    std::vector<double> oddSignal { 1.0, 3.0, 5.0, 7.0, 9.0, 11.0, 13.0 };
    std::vector<double> cA;
    std::vector<double> cD;
    dwt1D(oddSignal, cA, cD, WaveletType::Haar);
    assert(cA.size() == 4U);

    const auto rec = idwt1D(cA, cD, WaveletType::Haar, oddSignal.size());
    assert(rec.size() == 7U);
    for (std::size_t i = 0U; i < 7U; ++i) {
        assert(std::abs(rec[i] - oddSignal[i]) < 1e-10);
    }

    // Empty inputs
    std::vector<double> emptyVec {};
    dwt1D(emptyVec, cA, cD);
    assert(cA.empty());
    assert(cD.empty());

    const auto recEmpty = idwt1D(cA, cD);
    assert(recEmpty.empty());

    std::cout << "  -> PASSED\n";
}

} // namespace

int main()
{
    std::cout << "========================================\n";
    std::cout << "Running Discrete Wavelet Transform Unit Tests\n";
    std::cout << "========================================\n";

    testHaar1DRoundtripIdentity();
    testDb41DRoundtripIdentity();
    testMultiLevelWavedecWaverec();
    test2DImageDwtRoundtrip();
    testWaveletDenoiseVisuShrink();
    testTransientShockDetector();
    testEdgeAndOddLengthCases();

    std::cout << "========================================\n";
    std::cout << "ALL DWT TESTS PASSED!\n";
    std::cout << "========================================\n";
    return 0;
}
