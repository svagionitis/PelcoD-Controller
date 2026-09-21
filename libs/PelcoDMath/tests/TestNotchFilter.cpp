/// @file TestNotchFilter.cpp
/// @brief Unit tests for the 2nd-order IIR biquad notch filter: construction, attenuation, pass-band,
///        parameter setters, reset, and boundary/fallback behavior.

#include "NotchFilter.h"

#include <gtest/gtest.h>
#include <cmath>
#include <iostream>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

/// @brief Generate a pure sine wave at the given frequency.
std::vector<double> makeSine(double freqHz, double sampleRateHz, std::size_t numSamples)
{
    std::vector<double> out(numSamples, 0.0);
    for (std::size_t i = 0U; i < numSamples; ++i) {
        out[i] = std::sin(2.0 * M_PI * freqHz * static_cast<double>(i) / sampleRateHz);
    }
    return out;
}

/// @brief Compute RMS energy of a signal (skipping the first skipSamples for transient warmup).
double rms(const std::vector<double>& v, std::size_t skipSamples = 0U)
{
    double sum = 0.0;
    const std::size_t n = v.size();
    for (std::size_t i = skipSamples; i < n; ++i) {
        sum += v[i] * v[i];
    }
    return std::sqrt(sum / static_cast<double>(n - skipSamples));
}

// ---------------------------------------------------------------------------

TEST(NotchFilterTest, ConstructorDefaults)
{
    std::cout << "[Test] testConstructorDefaults\n";
    PelcoD::NotchFilter f;
    EXPECT_TRUE(f.getCenterFrequency() == 10.0);
    EXPECT_TRUE(f.getSampleRate() == 50.0);
    EXPECT_TRUE(f.getQFactor() == 5.0);
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, ConstructorCustomParams)
{
    std::cout << "[Test] testConstructorCustomParams\n";
    PelcoD::NotchFilter f { 20.0, 200.0, 8.0 };
    EXPECT_TRUE(f.getCenterFrequency() == 20.0);
    EXPECT_TRUE(f.getSampleRate() == 200.0);
    EXPECT_TRUE(f.getQFactor() == 8.0);
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, QFactorClampedAtConstruction)
{
    std::cout << "[Test] testQFactorClampedAtConstruction\n";
    // Q <= 0 must be clamped to 0.1
    PelcoD::NotchFilter f { 10.0, 100.0, 0.0 };
    EXPECT_TRUE(f.getQFactor() >= 0.1);
    PelcoD::NotchFilter f2 { 10.0, 100.0, -5.0 };
    EXPECT_TRUE(f2.getQFactor() >= 0.1);
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, SetParametersUpdatesGetters)
{
    std::cout << "[Test] testSetParametersUpdatesGetters\n";
    PelcoD::NotchFilter f;
    f.setParameters(25.0, 100.0, 3.0);
    EXPECT_TRUE(f.getCenterFrequency() == 25.0);
    EXPECT_TRUE(f.getSampleRate() == 100.0);
    EXPECT_TRUE(f.getQFactor() == 3.0);
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, SetParametersQClamp)
{
    std::cout << "[Test] testSetParametersQClamp\n";
    PelcoD::NotchFilter f;
    f.setParameters(10.0, 100.0, -1.0);
    EXPECT_TRUE(f.getQFactor() >= 0.1);
    f.setParameters(10.0, 100.0, 0.0);
    EXPECT_TRUE(f.getQFactor() >= 0.1);
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, SetParametersSampleRateClamp)
{
    std::cout << "[Test] testSetParametersSampleRateClamp\n";
    PelcoD::NotchFilter f;
    f.setParameters(10.0, 0.0, 5.0); // sample rate clamped to 1 Hz
    EXPECT_TRUE(f.getSampleRate() >= 1.0);
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, NotchFrequencyAttenuated)
{
    std::cout << "[Test] testNotchFrequencyAttenuated\n";
    const double centerHz = 50.0;
    const double sampleRateHz = 1000.0;
    const double qFactor = 10.0;
    const std::size_t N = 4000U;
    const std::size_t warmup = 500U;

    PelcoD::NotchFilter f { centerHz, sampleRateHz, qFactor };
    const auto signal = makeSine(centerHz, sampleRateHz, N);
    std::vector<double> out(N);
    for (std::size_t i = 0U; i < N; ++i) {
        out[i] = f.process(signal[i]);
    }

    const double rmsIn = rms(signal, warmup);
    const double rmsOut = rms(out, warmup);
    // The notch should attenuate the center frequency significantly (> 20 dB = 10x power)
    EXPECT_TRUE(rmsOut < rmsIn * 0.1);
    std::cout << "  RMS in=" << rmsIn << " out=" << rmsOut << " ratio=" << (rmsOut / rmsIn) << "\n";
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, PassBandPreserved)
{
    std::cout << "[Test] testPassBandPreserved\n";
    const double centerHz = 50.0;
    const double sampleRateHz = 1000.0;
    const double passFreq = 200.0; // Far from notch
    const std::size_t N = 4000U;
    const std::size_t warmup = 200U;

    PelcoD::NotchFilter f { centerHz, sampleRateHz, 10.0 };
    const auto signal = makeSine(passFreq, sampleRateHz, N);
    std::vector<double> out(N);
    for (std::size_t i = 0U; i < N; ++i) {
        out[i] = f.process(signal[i]);
    }

    const double rmsIn = rms(signal, warmup);
    const double rmsOut = rms(out, warmup);
    // Pass-band should be within 10% of original amplitude
    EXPECT_TRUE(rmsOut > rmsIn * 0.9);
    std::cout << "  RMS in=" << rmsIn << " out=" << rmsOut << "\n";
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, ResetClearsState)
{
    std::cout << "[Test] testResetClearsState\n";
    PelcoD::NotchFilter f { 50.0, 1000.0, 10.0 };
    // Push some samples through to warm up the delay line
    for (int i = 0; i < 200; ++i) {
        (void)f.process(1.0);
    }
    f.reset();
    // After reset, filtering a zero input must produce zero output
    const double out = f.process(0.0);
    EXPECT_TRUE(std::abs(out) < 1e-12);
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, FallbackPassThroughAboveNyquist)
{
    std::cout << "[Test] testFallbackPassThroughAboveNyquist\n";
    // Center frequency >= Nyquist → filter falls back to all-pass identity
    PelcoD::NotchFilter f { 600.0, 1000.0, 5.0 }; // Nyquist = 500 Hz; center = 600 > Nyquist
    for (int i = 0; i < 100; ++i) {
        const double in = static_cast<double>(i) * 0.01;
        const double out = f.process(in);
        // Identity: output should equal input (after warmup; check immediate pass-through on first)
        (void)out;
    }
    // Simple sanity: a constant-1 signal through an identity filter → constant 1
    PelcoD::NotchFilter f2 { 999.0, 1000.0, 5.0 };
    double prev = 0.0;
    for (int i = 0; i < 1000; ++i) {
        prev = f2.process(1.0);
    }
    EXPECT_TRUE(std::abs(prev - 1.0) < 1e-9);
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, FallbackPassThroughBelowZero)
{
    std::cout << "[Test] testFallbackPassThroughBelowZero\n";
    // Center frequency <= 0 → identity pass-through
    PelcoD::NotchFilter f { 0.0, 1000.0, 5.0 };
    double prev = 0.0;
    for (int i = 0; i < 1000; ++i) {
        prev = f.process(1.0);
    }
    EXPECT_TRUE(std::abs(prev - 1.0) < 1e-9);

    PelcoD::NotchFilter f2 { -10.0, 1000.0, 5.0 };
    double prev2 = 0.0;
    for (int i = 0; i < 1000; ++i) {
        prev2 = f2.process(1.0);
    }
    EXPECT_TRUE(std::abs(prev2 - 1.0) < 1e-9);
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, ProcessZeroInputYieldsZero)
{
    std::cout << "[Test] testProcessZeroInputYieldsZero\n";
    PelcoD::NotchFilter f { 50.0, 1000.0, 5.0 };
    // All-zero input → all-zero output (filter is linear, no offset)
    for (int i = 0; i < 100; ++i) {
        const double out = f.process(0.0);
        EXPECT_TRUE(std::abs(out) < 1e-12);
    }
    std::cout << "  -> PASSED\n";
}

TEST(NotchFilterTest, ReconfigureChangesAttenuation)
{
    std::cout << "[Test] testReconfigureChangesAttenuation\n";
    // Configure notch at 100 Hz, then verify 100 Hz is attenuated; then move notch to 200 Hz
    // and verify 100 Hz is now passed through.
    const double sampleRateHz = 2000.0;
    const std::size_t N = 5000U;
    const std::size_t warmup = 1000U;

    PelcoD::NotchFilter f { 100.0, sampleRateHz, 10.0 };
    const auto sig100 = makeSine(100.0, sampleRateHz, N);
    std::vector<double> out1(N);
    for (std::size_t i = 0U; i < N; ++i) {
        out1[i] = f.process(sig100[i]);
    }
    const double rmsAtt = rms(out1, warmup);
    EXPECT_TRUE(rmsAtt < rms(sig100, warmup) * 0.1); // Should attenuate

    f.reset();
    f.setParameters(200.0, sampleRateHz, 10.0); // Move notch to 200 Hz
    std::vector<double> out2(N);
    for (std::size_t i = 0U; i < N; ++i) {
        out2[i] = f.process(sig100[i]);
    }
    const double rmsPass = rms(out2, warmup);
    EXPECT_TRUE(rmsPass > rms(sig100, warmup) * 0.9); // Should now pass 100 Hz
    std::cout << "  -> PASSED\n";
}

} // namespace

