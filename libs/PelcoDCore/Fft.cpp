/// @file Fft.cpp
/// @brief Implementation of Fast Fourier Transform (FFT), windowing, and PSD spectral analysis.

#include "Fft.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace PelcoD::Math {

bool isPowerOfTwo(std::size_t n) noexcept
{
    return (n > 0U) && ((n & (n - 1U)) == 0U);
}

std::size_t nextPowerOfTwo(std::size_t n) noexcept
{
    if (n == 0U) {
        return 1U;
    }
    if (isPowerOfTwo(n)) {
        return n;
    }
    std::size_t power = 1U;
    while (power < n) {
        power <<= 1U;
    }
    return power;
}

void fft(std::vector<Complex>& data, bool inverse)
{
    const std::size_t n = data.size();
    if (n <= 1U) {
        return;
    }

    if (!isPowerOfTwo(n)) {
        const std::size_t nextPow = nextPowerOfTwo(n);
        data.resize(nextPow, Complex { 0.0, 0.0 });
    }

    const std::size_t numElements = data.size();

    // Bit-reversal permutation
    for (std::size_t i = 1U, j = 0U; i < numElements; ++i) {
        std::size_t bit = numElements >> 1U;
        for (; (j & bit) != 0U; bit >>= 1U) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }

    // Cooley-Tukey butterfly stages
    for (std::size_t len = 2U; len <= numElements; len <<= 1U) {
        const double angle = (inverse ? 2.0 * M_PI : -2.0 * M_PI) / static_cast<double>(len);
        const Complex wlen(std::cos(angle), std::sin(angle));
        const std::size_t halfLen = len >> 1U;

        for (std::size_t i = 0U; i < numElements; i += len) {
            Complex w(1.0, 0.0);
            for (std::size_t j = 0U; j < halfLen; ++j) {
                const Complex u = data[i + j];
                const Complex v = data[i + j + halfLen] * w;
                data[i + j] = u + v;
                data[i + j + halfLen] = u - v;
                w *= wlen;
            }
        }
    }

    // Normalization for inverse FFT
    if (inverse) {
        const double scale = 1.0 / static_cast<double>(numElements);
        for (auto& val : data) {
            val *= scale;
        }
    }
}

std::vector<Complex> rfft(const std::vector<double>& realSignal)
{
    if (realSignal.empty()) {
        return {};
    }
    const std::size_t n = nextPowerOfTwo(realSignal.size());
    std::vector<Complex> data(n, Complex { 0.0, 0.0 });
    for (std::size_t i = 0U; i < realSignal.size(); ++i) {
        data[i] = Complex { realSignal[i], 0.0 };
    }
    fft(data, false);
    return data;
}

std::vector<double> irfft(const std::vector<Complex>& spectrum)
{
    if (spectrum.empty()) {
        return {};
    }
    std::vector<Complex> buffer = spectrum;
    fft(buffer, true);
    std::vector<double> result(buffer.size(), 0.0);
    for (std::size_t i = 0U; i < buffer.size(); ++i) {
        result[i] = buffer[i].real();
    }
    return result;
}

void applyWindow(std::vector<double>& signal, WindowType window)
{
    const std::size_t n = signal.size();
    if (n <= 1U || window == WindowType::Rectangular) {
        return;
    }

    const double denom = static_cast<double>(n - 1U);
    for (std::size_t i = 0U; i < n; ++i) {
        const double frac = static_cast<double>(i) / denom;
        double weight = 1.0;
        switch (window) {
        case WindowType::Hann:
            weight = 0.5 * (1.0 - std::cos(2.0 * M_PI * frac));
            break;
        case WindowType::Hamming:
            weight = 0.54 - 0.46 * std::cos(2.0 * M_PI * frac);
            break;
        case WindowType::Blackman:
            weight = 0.42 - 0.5 * std::cos(2.0 * M_PI * frac) + 0.08 * std::cos(4.0 * M_PI * frac);
            break;
        case WindowType::Rectangular:
        default:
            break;
        }
        signal[i] *= weight;
    }
}

std::vector<SpectralPeak> computePsd(const std::vector<double>& signal, double sampleRateHz, WindowType window)
{
    if (signal.empty() || sampleRateHz <= 0.0) {
        return {};
    }

    std::vector<double> winSig = signal;
    applyWindow(winSig, window);

    const std::vector<Complex> spectrum = rfft(winSig);
    const std::size_t n = spectrum.size();
    if (n == 0U) {
        return {};
    }

    const std::size_t halfBins = (n / 2U) + 1U;
    std::vector<SpectralPeak> peaks {};
    peaks.reserve(halfBins);

    double totalPower = 0.0;
    const double invN = 1.0 / static_cast<double>(n);

    // Compute single-sided power spectrum
    std::vector<double> powerSpectrum(halfBins, 0.0);
    for (std::size_t k = 0U; k < halfBins; ++k) {
        const double mag = std::abs(spectrum[k]) * invN;
        double power = mag * mag;
        if (k > 0U && k < n / 2U) {
            power *= 2.0; // Double energy for positive frequencies in single-sided spectrum
        }
        powerSpectrum[k] = power;
        totalPower += power;

        SpectralPeak peak {};
        peak.frequencyHz = (static_cast<double>(k) * sampleRateHz) / static_cast<double>(n);
        peak.magnitude = mag;
        peak.powerRatio = 0.0; // Computed below
        peaks.push_back(peak);
    }

    if (totalPower > 1e-15) {
        for (std::size_t k = 0U; k < halfBins; ++k) {
            peaks[k].powerRatio = powerSpectrum[k] / totalPower;
        }
    }

    // Sort peaks in descending order of powerRatio / magnitude
    std::sort(peaks.begin(), peaks.end(),
        [](const SpectralPeak& a, const SpectralPeak& b) { return a.magnitude > b.magnitude; });

    return peaks;
}

} // namespace PelcoD::Math
