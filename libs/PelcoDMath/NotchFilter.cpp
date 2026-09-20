/// @file NotchFilter.cpp
/// @brief Implementation of 2nd-order digital IIR biquad notch filter.

#include "NotchFilter.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace PelcoD {

NotchFilter::NotchFilter(double centerFreqHz, double sampleRateHz, double qFactor)
    : m_centerFreqHz(centerFreqHz)
    , m_sampleRateHz(sampleRateHz)
    , m_qFactor(std::max(0.1, qFactor))
{
    calculateCoefficients();
}

void NotchFilter::setParameters(double centerFreqHz, double sampleRateHz, double qFactor) noexcept
{
    m_centerFreqHz = centerFreqHz;
    m_sampleRateHz = std::max(1.0, sampleRateHz);
    m_qFactor = std::max(0.1, qFactor);
    calculateCoefficients();
}

void NotchFilter::calculateCoefficients() noexcept
{
    // Check Nyquist boundary
    const double nyquist = 0.5 * m_sampleRateHz;
    if (m_centerFreqHz <= 0.0 || m_centerFreqHz >= nyquist) {
        // Fallback to pass-through (all-pass identity)
        m_b0 = 1.0;
        m_b1 = 0.0;
        m_b2 = 0.0;
        m_a1 = 0.0;
        m_a2 = 0.0;
        return;
    }

    const double omega0 = (2.0 * M_PI * m_centerFreqHz) / m_sampleRateHz;
    const double cosOmega = std::cos(omega0);
    const double sinOmega = std::sin(omega0);
    const double alpha = sinOmega / (2.0 * m_qFactor);

    const double a0 = 1.0 + alpha;
    if (std::abs(a0) < 1e-12) {
        m_b0 = 1.0;
        m_b1 = 0.0;
        m_b2 = 0.0;
        m_a1 = 0.0;
        m_a2 = 0.0;
        return;
    }

    const double invA0 = 1.0 / a0;
    m_b0 = invA0;
    m_b1 = (-2.0 * cosOmega) * invA0;
    m_b2 = invA0;
    m_a1 = (-2.0 * cosOmega) * invA0;
    m_a2 = (1.0 - alpha) * invA0;
}

double NotchFilter::process(double sample) noexcept
{
    const double y = (m_b0 * sample) + (m_b1 * m_x1) + (m_b2 * m_x2) - (m_a1 * m_y1) - (m_a2 * m_y2);

    m_x2 = m_x1;
    m_x1 = sample;
    m_y2 = m_y1;
    m_y1 = y;

    return y;
}

void NotchFilter::reset() noexcept
{
    m_x1 = 0.0;
    m_x2 = 0.0;
    m_y1 = 0.0;
    m_y2 = 0.0;
}

} // namespace PelcoD
