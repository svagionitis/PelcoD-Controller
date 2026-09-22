/// @file GoertzelFilter.cpp
/// @brief Implementation of 2nd-order IIR Goertzel filter for single-frequency spectral evaluation.

#include "GoertzelFilter.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Math {

GoertzelFilter::GoertzelFilter(double targetFreqHz, double sampleRateHz, std::size_t blockSize)
    : m_targetFreqHz(targetFreqHz)
    , m_sampleRateHz(std::max(1.0, sampleRateHz))
    , m_blockSize(std::max<std::size_t>(4U, blockSize))
{
    calculateCoefficients();
}

void GoertzelFilter::setParameters(double targetFreqHz, double sampleRateHz, std::size_t blockSize) noexcept
{
    m_targetFreqHz = targetFreqHz;
    m_sampleRateHz = std::max(1.0, sampleRateHz);
    m_blockSize = std::max<std::size_t>(4U, blockSize);
    calculateCoefficients();
    reset();
}

void GoertzelFilter::calculateCoefficients() noexcept
{
    const double omega = (2.0 * M_PI * m_targetFreqHz) / m_sampleRateHz;
    m_cosOmega = std::cos(omega);
    m_sinOmega = std::sin(omega);
    m_coeff = 2.0 * m_cosOmega;
}

void GoertzelFilter::reset() noexcept
{
    m_s1 = 0.0;
    m_s2 = 0.0;
    m_sampleIndex = 0U;
    m_latchedMagnitude = 0.0;
    m_latchedPower = 0.0;
}

bool GoertzelFilter::processSample(double sample) noexcept
{
    const double s0 = sample + (m_coeff * m_s1) - m_s2;
    m_s2 = m_s1;
    m_s1 = s0;
    ++m_sampleIndex;

    if (m_sampleIndex >= m_blockSize) {
        double rawPower = (m_s1 * m_s1) + (m_s2 * m_s2) - (m_coeff * m_s1 * m_s2);
        if (rawPower < 0.0) {
            rawPower = 0.0;
        }

        const double invN = 1.0 / static_cast<double>(m_blockSize);
        // Single-sided scaling: factor of 2 for AC frequencies, factor of 1 for DC/Nyquist
        const bool isDcOrNyquist
            = (m_targetFreqHz <= 1e-6) || (std::abs(m_targetFreqHz - (0.5 * m_sampleRateHz)) < 1e-6);
        const double scale = isDcOrNyquist ? invN : (2.0 * invN);

        m_latchedMagnitude = std::sqrt(rawPower) * scale;
        m_latchedPower = m_latchedMagnitude * m_latchedMagnitude;

        m_s1 = 0.0;
        m_s2 = 0.0;
        m_sampleIndex = 0U;
        return true;
    }

    return false;
}

double GoertzelFilter::computeMagnitude(const double* data, std::size_t size) const noexcept
{
    if (!data || size == 0U) {
        return 0.0;
    }

    double s1 = 0.0;
    double s2 = 0.0;
    for (std::size_t i = 0U; i < size; ++i) {
        const double s0 = data[i] + (m_coeff * s1) - s2;
        s2 = s1;
        s1 = s0;
    }

    double rawPower = (s1 * s1) + (s2 * s2) - (m_coeff * s1 * s2);
    if (rawPower < 0.0) {
        rawPower = 0.0;
    }

    const double invN = 1.0 / static_cast<double>(size);
    const bool isDcOrNyquist = (m_targetFreqHz <= 1e-6) || (std::abs(m_targetFreqHz - (0.5 * m_sampleRateHz)) < 1e-6);
    const double scale = isDcOrNyquist ? invN : (2.0 * invN);

    return std::sqrt(rawPower) * scale;
}

double GoertzelFilter::computePower(const double* data, std::size_t size) const noexcept
{
    const double mag = computeMagnitude(data, size);
    return mag * mag;
}

double GoertzelFilter::getMagnitude() const noexcept
{
    return m_latchedMagnitude;
}

double GoertzelFilter::getPower() const noexcept
{
    return m_latchedPower;
}

bool GoertzelFilter::hasDetected(double thresholdMagnitude) const noexcept
{
    return m_latchedMagnitude >= thresholdMagnitude;
}

double GoertzelFilter::getTargetFrequency() const noexcept
{
    return m_targetFreqHz;
}

double GoertzelFilter::getSampleRate() const noexcept
{
    return m_sampleRateHz;
}

std::size_t GoertzelFilter::getBlockSize() const noexcept
{
    return m_blockSize;
}

} // namespace Math
