/// @file Stft.cpp
/// @brief Implementation of Short-Time Fourier Transform (STFT) and real-time streaming spectrogram engine.

#include "Stft.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Math {

Stft::Stft(StftConfig config)
{
    setConfig(config);
}

void Stft::setConfig(const StftConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;

    // Window size must be a valid power of 2 (minimum 16)
    if (!isPowerOfTwo(m_config.windowSize) || m_config.windowSize < 16U) {
        m_config.windowSize = std::max<std::size_t>(16U, nextPowerOfTwo(m_config.windowSize));
    }

    // Hop size clamped to [1, windowSize]
    m_config.hopSize = std::clamp<std::size_t>(m_config.hopSize, 1U, m_config.windowSize);

    // Sample rate must be positive
    if (m_config.sampleRateHz <= 0.0) {
        m_config.sampleRateHz = 50.0;
    }

    if (m_config.maxHistoryFrames < 2U) {
        m_config.maxHistoryFrames = 2U;
    }

    m_sampleBuffer.clear();
    m_sampleBuffer.reserve(m_config.windowSize);
    m_samplesSinceLastHop = 0U;
    m_currentTimestamp = 0.0;
    m_history.clear();
}

StftConfig Stft::getConfig() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void Stft::addSample(double sample, double timestamp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (timestamp >= 0.0) {
        m_currentTimestamp = timestamp;
    } else {
        m_currentTimestamp += (1.0 / m_config.sampleRateHz);
    }

    m_sampleBuffer.push_back(sample);
    if (m_sampleBuffer.size() > m_config.windowSize) {
        m_sampleBuffer.erase(m_sampleBuffer.begin());
    }

    ++m_samplesSinceLastHop;

    if (m_sampleBuffer.size() == m_config.windowSize && m_samplesSinceLastHop >= m_config.hopSize) {
        processWindow(m_currentTimestamp);
        m_samplesSinceLastHop = 0U;
    }
}

void Stft::addSamples(const std::vector<double>& samples, double startTime)
{
    if (samples.empty()) {
        return;
    }

    const double dt = 1.0 / m_config.sampleRateHz;
    double t = (startTime >= 0.0) ? startTime : m_currentTimestamp;

    for (double s : samples) {
        addSample(s, t);
        t += dt;
    }
}

std::vector<SpectrogramFrame> Stft::getHistory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return { m_history.begin(), m_history.end() };
}

bool Stft::hasFrames() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_history.empty();
}

SpectrogramFrame Stft::getLatestFrame() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.empty()) {
        return SpectrogramFrame {};
    }
    return m_history.back();
}

std::vector<double> Stft::getFrequencyBinsHz() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::size_t numBins = m_config.windowSize / 2U + 1U;
    const double binWidthHz = m_config.sampleRateHz / static_cast<double>(m_config.windowSize);

    std::vector<double> freqs(numBins, 0.0);
    for (std::size_t k = 0U; k < numBins; ++k) {
        freqs[k] = static_cast<double>(k) * binWidthHz;
    }
    return freqs;
}

void Stft::reset() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sampleBuffer.clear();
    m_samplesSinceLastHop = 0U;
    m_currentTimestamp = 0.0;
    m_history.clear();
}

void Stft::processWindow(double windowEndTime)
{
    const std::size_t N = m_config.windowSize;
    const std::size_t numBins = N / 2U + 1U;
    const double binWidthHz = m_config.sampleRateHz / static_cast<double>(N);

    std::vector<double> windowData = m_sampleBuffer;

    // 1. Detrend / mean subtraction to suppress 0 Hz DC spike
    if (m_config.detrend && !windowData.empty()) {
        const double mean = std::accumulate(windowData.begin(), windowData.end(), 0.0)
            / static_cast<double>(windowData.size());
        for (auto& v : windowData) {
            v -= mean;
        }
    }

    // 2. Apply window function (Hann, Hamming, Blackman, etc.)
    applyWindow(windowData, m_config.windowType);

    // 3. Compute forward FFT
    const auto complexSpectrum = rfft(windowData);

    // 4. Compute single-sided power spectrum and dB spectrum
    SpectrogramFrame frame;
    frame.timestamp = windowEndTime;
    frame.powerSpectrum.resize(numBins, 0.0);
    frame.dbSpectrum.resize(numBins, m_config.minDb);

    double totalPower = 0.0;
    double maxPower = -1.0;
    std::size_t peakBin = 0U;

    for (std::size_t k = 0U; k < numBins; ++k) {
        const double mag = std::abs(complexSpectrum[k]);
        double power = (mag * mag) / static_cast<double>(N);

        // Double AC energy for single-sided representation (bins 1 to N/2 - 1)
        if (k > 0U && k < (N / 2U)) {
            power *= 2.0;
        }

        frame.powerSpectrum[k] = power;
        totalPower += power;

        // Skip DC bin (k=0) when determining dominant AC peak if detrended
        if (k > 0U && power > maxPower) {
            maxPower = power;
            peakBin = k;
        }

        // Decibel conversion with dynamic range clamping
        const double db = 10.0 * std::log10(std::max(1e-12, power));
        frame.dbSpectrum[k] = std::clamp(db, m_config.minDb, m_config.maxDb);
    }

    // Fallback if signal is completely silent or all DC
    if (maxPower < 0.0) {
        maxPower = frame.powerSpectrum[0];
        peakBin = 0U;
    }

    frame.totalEnergy = totalPower;
    frame.peakFrequencyHz = static_cast<double>(peakBin) * binWidthHz;
    frame.peakMagnitude = std::abs(complexSpectrum[peakBin]);
    frame.powerRatio = (totalPower > 1e-12) ? std::clamp(maxPower / totalPower, 0.0, 1.0) : 0.0;

    // 5. Spectral Centroid (Center of Mass)
    double weightedFreqSum = 0.0;
    double magSum = 0.0;
    for (std::size_t k = 1U; k < numBins; ++k) {
        const double mag = std::abs(complexSpectrum[k]);
        weightedFreqSum += (static_cast<double>(k) * binWidthHz) * mag;
        magSum += mag;
    }
    frame.spectralCentroidHz = (magSum > 1e-12) ? (weightedFreqSum / magSum) : 0.0;

    // 6. Spectral Flatness (Wiener Entropy: geometric mean / arithmetic mean)
    if (numBins > 1U) {
        double logSum = 0.0;
        double linearSum = 0.0;
        const std::size_t acBins = numBins - 1U;

        for (std::size_t k = 1U; k < numBins; ++k) {
            const double p = std::max(1e-12, frame.powerSpectrum[k]);
            logSum += std::log(p);
            linearSum += p;
        }

        const double geomMean = std::exp(logSum / static_cast<double>(acBins));
        const double arithMean = linearSum / static_cast<double>(acBins);
        frame.spectralFlatness = (arithMean > 1e-12) ? std::clamp(geomMean / arithMean, 0.0, 1.0) : 0.0;
    }

    // 7. Enqueue into rolling history
    m_history.push_back(std::move(frame));
    while (m_history.size() > m_config.maxHistoryFrames) {
        m_history.pop_front();
    }
}

} // namespace Math
