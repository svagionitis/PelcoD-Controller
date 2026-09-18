/// @file OscillationDetector.cpp
/// @brief Implementation of sliding-window oscillation and PID hunting detector.

#include "OscillationDetector.h"

#include <algorithm>
#include <numeric>

namespace PelcoD {

OscillationDetector::OscillationDetector(OscillationConfig config)
{
    setConfig(config);
}

void OscillationDetector::setConfig(const OscillationConfig& config)
{
    m_config = config;
    const std::size_t winSize = Math::nextPowerOfTwo(std::max(16UL, m_config.windowSize));
    m_config.windowSize = winSize;
    m_buffer.assign(winSize, 0.0);
    reset();
}

void OscillationDetector::reset() noexcept
{
    std::fill(m_buffer.begin(), m_buffer.end(), 0.0);
    m_writeIndex = 0U;
    m_sampleCount = 0U;
    m_isHunting = false;
    m_dominantFreqHz = 0.0;
    m_dominantPowerRatio = 0.0;
    m_consecutiveDetections = 0;
    m_latestPeaks.clear();
}

void OscillationDetector::addSample(double sample)
{
    if (m_buffer.empty()) {
        return;
    }

    m_buffer[m_writeIndex] = sample;
    m_writeIndex = (m_writeIndex + 1U) % m_buffer.size();
    ++m_sampleCount;

    // Trigger analysis once window has filled, with 50% hop size
    const std::size_t hopSize = std::max(4UL, m_buffer.size() / 4U);
    if (m_sampleCount >= m_buffer.size() && (m_sampleCount % hopSize == 0U)) {
        analyzeSpectrum();
    }
}

void OscillationDetector::update()
{
    if (m_sampleCount >= m_buffer.size()) {
        analyzeSpectrum();
    }
}

void OscillationDetector::analyzeSpectrum()
{
    const std::size_t n = m_buffer.size();
    if (n == 0U) {
        return;
    }

    // Unroll ring buffer in chronological order
    std::vector<double> linearSignal(n, 0.0);
    for (std::size_t i = 0U; i < n; ++i) {
        linearSignal[i] = m_buffer[(m_writeIndex + i) % n];
    }

    // Remove DC component (mean subtraction) to isolate AC oscillation energy
    const double sum = std::accumulate(linearSignal.begin(), linearSignal.end(), 0.0);
    const double mean = sum / static_cast<double>(n);
    for (auto& val : linearSignal) {
        val -= mean;
    }

    m_latestPeaks = Math::computePsd(linearSignal, m_config.sampleRateHz, m_config.windowType);

    // Find the most prominent peak within the specified hunting frequency range
    const Math::SpectralPeak* huntingPeak = nullptr;
    for (const auto& peak : m_latestPeaks) {
        if (peak.frequencyHz >= m_config.minHuntingFreqHz && peak.frequencyHz <= m_config.maxHuntingFreqHz) {
            if (!huntingPeak || peak.magnitude > huntingPeak->magnitude) {
                huntingPeak = &peak;
            }
        }
    }

    if (huntingPeak && huntingPeak->powerRatio >= m_config.powerRatioThreshold) {
        m_dominantFreqHz = huntingPeak->frequencyHz;
        m_dominantPowerRatio = huntingPeak->powerRatio;
        ++m_consecutiveDetections;
        if (m_consecutiveDetections >= m_config.consecutiveThreshold) {
            m_isHunting = true;
        }
    } else {
        if (m_consecutiveDetections > 0) {
            --m_consecutiveDetections;
        }
        if (m_consecutiveDetections == 0) {
            m_isHunting = false;
        }
    }
}

bool OscillationDetector::isHunting() const noexcept
{
    return m_isHunting;
}

double OscillationDetector::getDominantFrequency() const noexcept
{
    return m_dominantFreqHz;
}

double OscillationDetector::getOscillationRatio() const noexcept
{
    return m_dominantPowerRatio;
}

std::vector<Math::SpectralPeak> OscillationDetector::getLatestPeaks() const
{
    return m_latestPeaks;
}

bool OscillationDetector::autoAttenuate(PidController& pid, double reductionFactor)
{
    if (!m_isHunting) {
        return false;
    }

    const double factor = std::clamp(reductionFactor, 0.1, 0.99);
    pid.setGains(pid.getKp() * factor, pid.getKi(), pid.getKd() * factor, pid.getKff());

    m_consecutiveDetections = 0;
    m_isHunting = false;
    return true;
}

} // namespace PelcoD
