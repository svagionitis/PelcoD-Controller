/// @file TestFft.cpp
/// @brief Automated unit test suite for Fast Fourier Transform (FFT), windowing, and PSD analysis in PelcoDCore.

#include "Fft.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace PelcoD::Math;

namespace {

void testPowerOfTwoUtilities()
{
    std::cout << "[Test] testPowerOfTwoUtilities...\n";
    assert(isPowerOfTwo(1));
    assert(isPowerOfTwo(2));
    assert(isPowerOfTwo(4));
    assert(isPowerOfTwo(64));
    assert(isPowerOfTwo(128));
    assert(isPowerOfTwo(1024));

    assert(!isPowerOfTwo(0));
    assert(!isPowerOfTwo(3));
    assert(!isPowerOfTwo(5));
    assert(!isPowerOfTwo(100));

    assert(nextPowerOfTwo(0) == 1);
    assert(nextPowerOfTwo(1) == 1);
    assert(nextPowerOfTwo(2) == 2);
    assert(nextPowerOfTwo(3) == 4);
    assert(nextPowerOfTwo(15) == 16);
    assert(nextPowerOfTwo(50) == 64);
    assert(nextPowerOfTwo(64) == 64);
    assert(nextPowerOfTwo(65) == 128);
    std::cout << "  -> PASSED\n";
}

void testFftIfftIdentity()
{
    std::cout << "[Test] testFftIfftIdentity...\n";
    const std::size_t n = 128U;
    std::vector<double> original(n, 0.0);
    for (std::size_t i = 0U; i < n; ++i) {
        original[i] = std::sin(2.0 * M_PI * 5.0 * static_cast<double>(i) / static_cast<double>(n))
            + 0.5 * std::cos(2.0 * M_PI * 12.0 * static_cast<double>(i) / static_cast<double>(n));
    }

    const auto spectrum = rfft(original);
    assert(spectrum.size() == n);

    const auto reconstructed = irfft(spectrum);
    assert(reconstructed.size() == n);

    for (std::size_t i = 0U; i < n; ++i) {
        assert(std::abs(original[i] - reconstructed[i]) < 1e-10);
    }
    std::cout << "  -> PASSED\n";
}

void testImpulseResponse()
{
    std::cout << "[Test] testImpulseResponse...\n";
    const std::size_t n = 64U;
    std::vector<Complex> impulse(n, Complex { 0.0, 0.0 });
    impulse[0] = Complex { 1.0, 0.0 };

    fft(impulse, false);

    // FFT of delta[n] is 1 for all frequencies
    for (std::size_t i = 0U; i < n; ++i) {
        assert(std::abs(impulse[i].real() - 1.0) < 1e-10);
        assert(std::abs(impulse[i].imag()) < 1e-10);
    }
    std::cout << "  -> PASSED\n";
}

void testSineWaveSpectralPeak()
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
    assert(!peaks.empty());

    // Primary peak should be located at approximately 10 Hz
    const auto& topPeak = peaks.front();
    assert(std::abs(topPeak.frequencyHz - targetFreq) <= (sampleRate / static_cast<double>(n)));
    // Substantial concentration of spectral power in this peak
    assert(topPeak.powerRatio > 0.4);

    std::cout << "  Detected peak at " << topPeak.frequencyHz << " Hz (expected " << targetFreq
              << " Hz), power ratio: " << topPeak.powerRatio << "\n";
    std::cout << "  -> PASSED\n";
}

void testWindowingFunctions()
{
    std::cout << "[Test] testWindowingFunctions...\n";
    const std::size_t n = 64U;

    std::vector<double> hannSig(n, 1.0);
    applyWindow(hannSig, WindowType::Hann);
    // Endpoints for Hann must be close to zero
    assert(std::abs(hannSig.front()) < 1e-10);
    assert(std::abs(hannSig.back()) < 1e-10);
    // Center point must be close to 1.0
    assert(std::abs(hannSig[n / 2U] - 1.0) < 0.05);

    std::vector<double> hammingSig(n, 1.0);
    applyWindow(hammingSig, WindowType::Hamming);
    // Hamming endpoints are ~0.08
    assert(std::abs(hammingSig.front() - 0.08) < 0.01);
    assert(std::abs(hammingSig.back() - 0.08) < 0.01);

    std::vector<double> blackmanSig(n, 1.0);
    applyWindow(blackmanSig, WindowType::Blackman);
    assert(std::abs(blackmanSig.front()) < 0.01);
    assert(std::abs(blackmanSig.back()) < 0.01);

    std::cout << "  -> PASSED\n";
}

void testNonPowerOfTwoZeroPadding()
{
    std::cout << "[Test] testNonPowerOfTwoZeroPadding...\n";
    std::vector<double> unaligned(50U, 1.5);
    const auto spectrum = rfft(unaligned);
    assert(spectrum.size() == 64U); // Padded to 64

    const auto reconstructed = irfft(spectrum);
    assert(reconstructed.size() == 64U);
    // First 50 samples match input
    for (std::size_t i = 0U; i < 50U; ++i) {
        assert(std::abs(reconstructed[i] - 1.5) < 1e-10);
    }
    // Remaining samples are zero
    for (std::size_t i = 50U; i < 64U; ++i) {
        assert(std::abs(reconstructed[i]) < 1e-10);
    }
    std::cout << "  -> PASSED\n";
}

} // namespace

int main()
{
    std::cout << "Running TestFft Test Suite\n";
    testPowerOfTwoUtilities();
    testFftIfftIdentity();
    testImpulseResponse();
    testSineWaveSpectralPeak();
    testWindowingFunctions();
    testNonPowerOfTwoZeroPadding();
    std::cout << "All TestFft Tests Passed!\n";
    return 0;
}
