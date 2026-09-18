/// @file TestStft.cpp
/// @brief Comprehensive automated unit test suite for Short-Time Fourier Transform (STFT) and SpectrogramColorMap.

#include "SpectrogramColorMap.h"
#include "Stft.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

namespace {

constexpr double TEST_EPSILON = 1e-4;
constexpr double PI = 3.14159265358979323846;

bool approxEqual(double a, double b, double eps = TEST_EPSILON)
{
    return std::abs(a - b) <= eps;
}

// ============================================================================
// 1. Single Stationary Tone Test
// ============================================================================
void testSingleStationaryTone()
{
    std::cout << "[Test] Single stationary tone tracking (5 Hz tone at 50 Hz sample rate)..." << std::endl;

    PelcoD::StftConfig config {};
    config.windowSize = 128U;
    config.hopSize = 32U;
    config.sampleRateHz = 50.0;
    config.windowType = PelcoD::Math::WindowType::Hann;
    config.detrend = true;

    PelcoD::Stft stft(config);
    assert(!stft.hasFrames());

    const double f0 = 5.0; // 5 Hz
    const double dt = 1.0 / config.sampleRateHz;

    // Feed 256 samples (5.12 seconds of 5 Hz oscillation)
    for (int i = 0; i < 256; ++i) {
        double t = static_cast<double>(i) * dt;
        double sample = 2.5 * std::sin(2.0 * PI * f0 * t);
        stft.addSample(sample, t);
    }

    assert(stft.hasFrames());
    auto history = stft.getHistory();
    assert(history.size() >= 4);

    // Each frame should lock onto 5 Hz (within bin resolution df = 50 / 128 = 0.39 Hz)
    for (const auto& frame : history) {
        assert(approxEqual(frame.peakFrequencyHz, f0, 0.45));
        assert(frame.powerRatio > 0.60); // Over 60% of total energy in peak bin
        assert(frame.spectralFlatness < 0.05); // Low flatness for pure harmonic tone
    }

    std::cout << "  -> Passed!" << std::endl;
}

// ============================================================================
// 2. Linear Frequency Chirp Sweep Test
// ============================================================================
void testLinearFrequencyChirp()
{
    std::cout << "[Test] Linear frequency chirp sweep (2 Hz to 18 Hz)..." << std::endl;

    PelcoD::StftConfig config {};
    config.windowSize = 64U;
    config.hopSize = 16U;
    config.sampleRateHz = 50.0;

    PelcoD::Stft stft(config);

    const double fStart = 2.0;
    const double fEnd = 18.0;
    const double duration = 8.0; // 8 seconds (400 samples)
    const std::size_t numSamples = 400U;
    const double dt = 1.0 / config.sampleRateHz;

    for (std::size_t i = 0; i < numSamples; ++i) {
        double t = static_cast<double>(i) * dt;
        // Instantaneous phase for linear chirp: phi(t) = 2*pi*(f0*t + 0.5 * (f1 - f0)/T * t^2)
        double phase = 2.0 * PI * (fStart * t + 0.5 * ((fEnd - fStart) / duration) * t * t);
        stft.addSample(std::sin(phase), t);
    }

    auto history = stft.getHistory();
    assert(history.size() >= 5);

    // Initial frame should be near 2-4 Hz; final frame should be near 16-18 Hz
    double firstFreq = history.front().peakFrequencyHz;
    double lastFreq = history.back().peakFrequencyHz;

    assert(firstFreq < 6.0);
    assert(lastFreq > 14.0);
    assert(lastFreq > firstFreq); // Monotonic increase

    std::cout << "  -> Passed! (First frame: " << firstFreq << " Hz, Last frame: " << lastFreq << " Hz)" << std::endl;
}

// ============================================================================
// 3. Two-Tone Resolution Test
// ============================================================================
void testTwoToneResolution()
{
    std::cout << "[Test] Two-tone simultaneous frequency resolution..." << std::endl;

    PelcoD::StftConfig config {};
    config.windowSize = 128U;
    config.hopSize = 64U;
    config.sampleRateHz = 50.0;

    PelcoD::Stft stft(config);

    const double f1 = 4.0;
    const double f2 = 15.0;
    const double dt = 1.0 / config.sampleRateHz;

    for (int i = 0; i < 200; ++i) {
        double t = static_cast<double>(i) * dt;
        double val = std::sin(2.0 * PI * f1 * t) + std::sin(2.0 * PI * f2 * t);
        stft.addSample(val, t);
    }

    auto latest = stft.getLatestFrame();
    auto freqs = stft.getFrequencyBinsHz();

    // Find local maxima in spectrum
    double powerAtF1 = 0.0;
    double powerAtF2 = 0.0;

    for (std::size_t k = 0; k < freqs.size(); ++k) {
        if (approxEqual(freqs[k], f1, 0.45)) {
            powerAtF1 = std::max(powerAtF1, latest.powerSpectrum[k]);
        }
        if (approxEqual(freqs[k], f2, 0.45)) {
            powerAtF2 = std::max(powerAtF2, latest.powerSpectrum[k]);
        }
    }

    assert(powerAtF1 > 0.1);
    assert(powerAtF2 > 0.1);

    std::cout << "  -> Passed!" << std::endl;
}

// ============================================================================
// 4. Hop Size and Overlap Ratio Test
// ============================================================================
void testHopSizeAndOverlap()
{
    std::cout << "[Test] Hop size and overlap frame count verification..." << std::endl;

    PelcoD::StftConfig config {};
    config.windowSize = 64U;
    config.hopSize = 32U; // 50% overlap
    config.sampleRateHz = 100.0;

    PelcoD::Stft stft(config);

    // Feeding exactly 64 samples should produce 1 frame
    for (int i = 0; i < 64; ++i) {
        stft.addSample(1.0);
    }
    assert(stft.getHistory().size() == 1);

    // Feeding next 32 samples (hop size) should produce 2nd frame
    for (int i = 0; i < 32; ++i) {
        stft.addSample(1.0);
    }
    assert(stft.getHistory().size() == 2);

    // Feeding next 32 samples should produce 3rd frame
    for (int i = 0; i < 32; ++i) {
        stft.addSample(1.0);
    }
    assert(stft.getHistory().size() == 3);

    std::cout << "  -> Passed!" << std::endl;
}

// ============================================================================
// 5. Spectral Flatness Test (Pure Tone vs White Noise)
// ============================================================================
void testSpectralFlatness()
{
    std::cout << "[Test] Spectral flatness (Wiener entropy) discrimination..." << std::endl;

    PelcoD::StftConfig config {};
    config.windowSize = 128U;
    config.hopSize = 64U;
    config.sampleRateHz = 100.0;

    PelcoD::Stft stftTone(config);
    PelcoD::Stft stftNoise(config);

    // 1. Pure tone
    for (int i = 0; i < 150; ++i) {
        double t = static_cast<double>(i) / config.sampleRateHz;
        stftTone.addSample(std::sin(2.0 * PI * 12.0 * t));
    }
    double toneFlatness = stftTone.getLatestFrame().spectralFlatness;

    // 2. Uniform pseudorandom white noise
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    for (int i = 0; i < 150; ++i) {
        stftNoise.addSample(dist(rng));
    }
    double noiseFlatness = stftNoise.getLatestFrame().spectralFlatness;

    assert(toneFlatness < 0.05);
    assert(noiseFlatness > 0.40);
    assert(noiseFlatness > 8.0 * toneFlatness);

    std::cout << "  -> Passed! (Tone flatness: " << toneFlatness << ", Noise flatness: " << noiseFlatness << ")"
              << std::endl;
}

// ============================================================================
// 6. Colormap Mapping Test
// ============================================================================
void testColormaps()
{
    std::cout << "[Test] Colormap stop interpolation and ANSI escape generation..." << std::endl;

    using Preset = PelcoD::SpectrogramColorMap::Preset;

    // Boundary checks
    auto c0 = PelcoD::SpectrogramColorMap::mapNormalized(0.0, Preset::Inferno);
    auto c1 = PelcoD::SpectrogramColorMap::mapNormalized(1.0, Preset::Inferno);
    assert(c0.r == 0 && c0.g == 0 && c0.b == 4);
    assert(c1.r == 252 && c1.g == 255 && c1.b == 164);

    // Decibel mapping
    auto cMidDb = PelcoD::SpectrogramColorMap::mapDb(-30.0, -60.0, 0.0, Preset::Viridis);
    auto cMidNorm = PelcoD::SpectrogramColorMap::mapNormalized(0.5, Preset::Viridis);
    assert(cMidDb == cMidNorm);

    // ANSI half-block string generation
    std::string ansi = PelcoD::SpectrogramColorMap::mapHalfBlockAnsi(0.8, 0.2, Preset::TacticalGreen);
    assert(ansi.find("\x1b[38;2;") != std::string::npos);
    assert(ansi.find("\x1b[48;2;") != std::string::npos);
    assert(ansi.find("\xE2\x96\x80") != std::string::npos);

    std::cout << "  -> Passed!" << std::endl;
}

// ============================================================================
// 7. Rolling History Retention Cap Test
// ============================================================================
void testHistoryCap()
{
    std::cout << "[Test] Rolling history capacity cap enforcement..." << std::endl;

    PelcoD::StftConfig config {};
    config.windowSize = 32U;
    config.hopSize = 16U;
    config.maxHistoryFrames = 10U;

    PelcoD::Stft stft(config);

    // Feed 500 samples (would generate > 25 frames if uncapped)
    for (int i = 0; i < 500; ++i) {
        stft.addSample(static_cast<double>(i));
    }

    assert(stft.getHistory().size() == 10U);

    stft.reset();
    assert(!stft.hasFrames());
    assert(stft.getHistory().empty());

    std::cout << "  -> Passed!" << std::endl;
}

} // namespace

int main()
{
    std::cout << "====================================================" << std::endl;
    std::cout << "Starting Short-Time Fourier Transform (STFT) Tests" << std::endl;
    std::cout << "====================================================" << std::endl;

    testSingleStationaryTone();
    testLinearFrequencyChirp();
    testTwoToneResolution();
    testHopSizeAndOverlap();
    testSpectralFlatness();
    testColormaps();
    testHistoryCap();

    std::cout << "====================================================" << std::endl;
    std::cout << "ALL STFT & SPECTROGRAM TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "====================================================" << std::endl;

    return 0;
}
