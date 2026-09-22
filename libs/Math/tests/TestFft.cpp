/// @file TestFft.cpp
/// @brief Automated unit test suite for Fast Fourier Transform (FFT), windowing, and PSD analysis in PelcoDCore.

#include "Fft.h"

#include <cmath>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Math;

namespace {

TEST(FftTest, PowerOfTwoUtilities)
{
    std::cout << "[Test] testPowerOfTwoUtilities...\n";
    EXPECT_TRUE(isPowerOfTwo(1));
    EXPECT_TRUE(isPowerOfTwo(2));
    EXPECT_TRUE(isPowerOfTwo(4));
    EXPECT_TRUE(isPowerOfTwo(64));
    EXPECT_TRUE(isPowerOfTwo(128));
    EXPECT_TRUE(isPowerOfTwo(1024));

    EXPECT_TRUE(!isPowerOfTwo(0));
    EXPECT_TRUE(!isPowerOfTwo(3));
    EXPECT_TRUE(!isPowerOfTwo(5));
    EXPECT_TRUE(!isPowerOfTwo(100));

    EXPECT_TRUE(nextPowerOfTwo(0) == 1);
    EXPECT_TRUE(nextPowerOfTwo(1) == 1);
    EXPECT_TRUE(nextPowerOfTwo(2) == 2);
    EXPECT_TRUE(nextPowerOfTwo(3) == 4);
    EXPECT_TRUE(nextPowerOfTwo(15) == 16);
    EXPECT_TRUE(nextPowerOfTwo(50) == 64);
    EXPECT_TRUE(nextPowerOfTwo(64) == 64);
    EXPECT_TRUE(nextPowerOfTwo(65) == 128);
    std::cout << "  -> PASSED\n";
}

TEST(FftTest, FftIfftIdentity)
{
    std::cout << "[Test] testFftIfftIdentity...\n";
    const std::size_t n = 128U;
    std::vector<double> original(n, 0.0);
    for (std::size_t i = 0U; i < n; ++i) {
        original[i] = std::sin(2.0 * M_PI * 5.0 * static_cast<double>(i) / static_cast<double>(n))
            + 0.5 * std::cos(2.0 * M_PI * 12.0 * static_cast<double>(i) / static_cast<double>(n));
    }

    const auto spectrum = rfft(original);
    EXPECT_TRUE(spectrum.size() == n);

    const auto reconstructed = irfft(spectrum);
    EXPECT_TRUE(reconstructed.size() == n);

    for (std::size_t i = 0U; i < n; ++i) {
        EXPECT_TRUE(std::abs(original[i] - reconstructed[i]) < 1e-10);
    }
    std::cout << "  -> PASSED\n";
}

TEST(FftTest, ImpulseResponse)
{
    std::cout << "[Test] testImpulseResponse...\n";
    const std::size_t n = 64U;
    std::vector<Complex> impulse(n, Complex { 0.0, 0.0 });
    impulse[0] = Complex { 1.0, 0.0 };

    fft(impulse, false);

    // FFT of delta[n] is 1 for all frequencies
    for (std::size_t i = 0U; i < n; ++i) {
        EXPECT_TRUE(std::abs(impulse[i].real() - 1.0) < 1e-10);
        EXPECT_TRUE(std::abs(impulse[i].imag()) < 1e-10);
    }
    std::cout << "  -> PASSED\n";
}

TEST(FftTest, SineWaveSpectralPeak)
{
    std::cout << "[Test] testSineWaveSpectralPeak...\n";
    const double sampleRate = 100.0;
    const double targetFreq = 10.0;
    const std::size_t n = 128U;

    std::vector<double> signal(n, 0.0);
    for (std::size_t i = 0U; i < n; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        signal[i] = 2.0 * std::sin(2.0 * M_PI * targetFreq * t);
    }

    const auto peaks = computePsd(signal, sampleRate, WindowType::Hann);
    EXPECT_TRUE(!peaks.empty());

    // Primary peak should be located at approximately 10 Hz
    const auto& topPeak = peaks.front();
    EXPECT_TRUE(std::abs(topPeak.frequencyHz - targetFreq) <= (sampleRate / static_cast<double>(n)));
    // Substantial concentration of spectral power in this peak
    EXPECT_TRUE(topPeak.powerRatio > 0.4);

    std::cout << "  Detected peak at " << topPeak.frequencyHz << " Hz (expected " << targetFreq
              << " Hz), power ratio: " << topPeak.powerRatio << "\n";
    std::cout << "  -> PASSED\n";
}

TEST(FftTest, WindowingFunctions)
{
    std::cout << "[Test] testWindowingFunctions...\n";
    const std::size_t n = 64U;

    std::vector<double> hannSig(n, 1.0);
    applyWindow(hannSig, WindowType::Hann);
    // Endpoints for Hann must be close to zero
    EXPECT_TRUE(std::abs(hannSig.front()) < 1e-10);
    EXPECT_TRUE(std::abs(hannSig.back()) < 1e-10);
    // Center point must be close to 1.0
    EXPECT_TRUE(std::abs(hannSig[n / 2U] - 1.0) < 0.05);

    std::vector<double> hammingSig(n, 1.0);
    applyWindow(hammingSig, WindowType::Hamming);
    // Hamming endpoints are ~0.08
    EXPECT_TRUE(std::abs(hammingSig.front() - 0.08) < 0.01);
    EXPECT_TRUE(std::abs(hammingSig.back() - 0.08) < 0.01);

    std::vector<double> blackmanSig(n, 1.0);
    applyWindow(blackmanSig, WindowType::Blackman);
    EXPECT_TRUE(std::abs(blackmanSig.front()) < 0.01);
    EXPECT_TRUE(std::abs(blackmanSig.back()) < 0.01);

    std::cout << "  -> PASSED\n";
}

TEST(FftTest, NonPowerOfTwoZeroPadding)
{
    std::cout << "[Test] testNonPowerOfTwoZeroPadding...\n";
    std::vector<double> unaligned(50U, 1.5);
    const auto spectrum = rfft(unaligned);
    EXPECT_TRUE(spectrum.size() == 64U); // Padded to 64

    const auto reconstructed = irfft(spectrum);
    EXPECT_TRUE(reconstructed.size() == 64U);
    // First 50 samples match input
    for (std::size_t i = 0U; i < 50U; ++i) {
        EXPECT_TRUE(std::abs(reconstructed[i] - 1.5) < 1e-10);
    }
    // Remaining samples are zero
    for (std::size_t i = 50U; i < 64U; ++i) {
        EXPECT_TRUE(std::abs(reconstructed[i]) < 1e-10);
    }
    std::cout << "  -> PASSED\n";
}

} // namespace
