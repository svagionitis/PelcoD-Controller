/// @file TransientShockDetector.cpp
/// @brief Implementation of real-time Transient Shock & Vibration Impulse Detector.

#include "TransientShockDetector.h"

#include <algorithm>
#include <cmath>

namespace Tracking {

TransientShockDetector::TransientShockDetector(ShockDetectorConfig config)
    : m_config(config)
{
    m_buffer.reserve(std::max(m_config.windowSize, std::size_t { 16U }));
}

void TransientShockDetector::reset()
{
    m_buffer.clear();
    m_baselineEnergy = 1.0;
    m_sampleCount = 0U;
}

void TransientShockDetector::setConfig(const ShockDetectorConfig& config)
{
    m_config = config;
    m_buffer.reserve(std::max(m_config.windowSize, std::size_t { 16U }));
}

const ShockDetectorConfig& TransientShockDetector::getConfig() const noexcept
{
    return m_config;
}

double TransientShockDetector::getBaselineEnergy() const noexcept
{
    return m_baselineEnergy;
}

ShockEvent TransientShockDetector::addSample(double sample)
{
    const std::size_t win = std::max(m_config.windowSize, std::size_t { 16U });
    m_buffer.push_back(sample);
    if (m_buffer.size() > win) {
        m_buffer.erase(m_buffer.begin());
    }

    if (m_buffer.size() < win) {
        return ShockEvent { false, 0.0, 0.0, 1.0 };
    }

    const auto decomp = Math::wavedec(m_buffer, m_config.decompositionLevels, m_config.wavelet);
    if (decomp.cD.empty()) {
        return ShockEvent { false, 0.0, 0.0, 1.0 };
    }

    // Examine level-1 detail coefficients for high-frequency shock energy
    const auto& cD1 = decomp.cD[0];
    double energy = 0.0;
    double maxMag = 0.0;

    for (const auto val : cD1) {
        energy += (val * val);
        const double absVal = std::abs(val);
        if (absVal > maxMag) {
            maxMag = absVal;
        }
    }

    if (!cD1.empty()) {
        energy /= static_cast<double>(cD1.size());
    }

    const double energyRatio = energy / std::max(m_baselineEnergy, 1e-6);
    const bool isShock = (energyRatio >= m_config.energyThresholdFactor) && (energy >= m_config.minShockEnergy);

    // Only adapt baseline background energy when no transient shock is firing
    if (!isShock) {
        m_baselineEnergy = (0.95 * m_baselineEnergy) + (0.05 * energy);
        if (m_baselineEnergy < 1e-6) {
            m_baselineEnergy = 1e-6;
        }
    }

    ++m_sampleCount;
    return ShockEvent { isShock, maxMag, energy, energyRatio };
}

} // namespace Tracking
