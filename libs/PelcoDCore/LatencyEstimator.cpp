/// @file LatencyEstimator.cpp
/// @brief Implementation of cross-correlation latency estimator with sub-sample parabolic refinement.

#include "LatencyEstimator.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace PelcoD {

LatencyEstimator::LatencyEstimator(LatencyEstimatorConfig config)
{
    setConfig(config);
}

void LatencyEstimator::setConfig(const LatencyEstimatorConfig& config)
{
    m_config = config;
    m_config.bufferCapacity = std::max(32UL, m_config.bufferCapacity);
    m_config.sampleRateHz = std::max(1.0, m_config.sampleRateHz);
    m_config.minLagMs = std::max(0.0, m_config.minLagMs);
    m_config.maxLagMs = std::max(m_config.minLagMs + 1.0, m_config.maxLagMs);
    m_config.smoothingAlpha = std::clamp(m_config.smoothingAlpha, 0.01, 1.0);
    reset();
}

void LatencyEstimator::reset() noexcept
{
    m_refBuffer.clear();
    m_respBuffer.clear();
    m_count = 0U;
    m_estimatedLatencyMs = 0.0;
    m_peakCorrelation = 0.0;
    m_isConfident = false;
    m_latestCorrelationCurve.clear();
}

void LatencyEstimator::addSample(double reference, double response)
{
    m_refBuffer.push_back(reference);
    m_respBuffer.push_back(response);
    ++m_count;

    if (m_refBuffer.size() > m_config.bufferCapacity) {
        m_refBuffer.erase(m_refBuffer.begin());
        m_respBuffer.erase(m_respBuffer.begin());
    }

    // Update cross-correlation once buffer is filled to capacity, with 25% hop interval
    const std::size_t hopSize = std::max(4UL, m_config.bufferCapacity / 4U);
    if (m_refBuffer.size() == m_config.bufferCapacity && (m_count % hopSize == 0U)) {
        update();
    }
}

void LatencyEstimator::update()
{
    const std::size_t n = m_refBuffer.size();
    if (n < 16U) {
        return;
    }

    const double msPerSample = 1000.0 / m_config.sampleRateHz;
    const std::size_t minLag = static_cast<std::size_t>(std::floor(m_config.minLagMs / msPerSample));
    std::size_t maxLag = static_cast<std::size_t>(std::ceil(m_config.maxLagMs / msPerSample));

    // Ensure window length is at least half of the buffer
    if (maxLag >= n / 2U) {
        maxLag = (n / 2U) - 1U;
    }
    if (minLag >= maxLag) {
        return;
    }

    const std::size_t windowLen = n - maxLag;
    const std::size_t numLags = (maxLag - minLag) + 1U;
    m_latestCorrelationCurve.assign(numLags, 0.0);

    // Compute mean and variance of reference signal over comparison window
    double sumRef = 0.0;
    for (std::size_t i = 0U; i < windowLen; ++i) {
        sumRef += m_refBuffer[i];
    }
    const double meanRef = sumRef / static_cast<double>(windowLen);

    double varRef = 0.0;
    for (std::size_t i = 0U; i < windowLen; ++i) {
        const double diff = m_refBuffer[i] - meanRef;
        varRef += diff * diff;
    }

    if (varRef < 1e-12) {
        m_isConfident = false;
        m_peakCorrelation = 0.0;
        return;
    }

    double bestCorr = -2.0;
    std::size_t bestIndex = 0U;

    // Evaluate Pearson cross-correlation for each search lag
    for (std::size_t lagIdx = 0U; lagIdx < numLags; ++lagIdx) {
        const std::size_t lag = minLag + lagIdx;

        double sumResp = 0.0;
        for (std::size_t i = 0U; i < windowLen; ++i) {
            sumResp += m_respBuffer[i + lag];
        }
        const double meanResp = sumResp / static_cast<double>(windowLen);

        double cov = 0.0;
        double varResp = 0.0;
        for (std::size_t i = 0U; i < windowLen; ++i) {
            const double dRef = m_refBuffer[i] - meanRef;
            const double dResp = m_respBuffer[i + lag] - meanResp;
            cov += dRef * dResp;
            varResp += dResp * dResp;
        }

        double corr = 0.0;
        if (varResp > 1e-12) {
            corr = cov / std::sqrt(varRef * varResp);
        }

        m_latestCorrelationCurve[lagIdx] = corr;
        if (corr > bestCorr) {
            bestCorr = corr;
            bestIndex = lagIdx;
        }
    }

    m_peakCorrelation = bestCorr;

    // Parabolic interpolation for sub-sample precision around peak
    double refinedLag = static_cast<double>(minLag + bestIndex);
    if (bestIndex > 0U && bestIndex + 1U < numLags) {
        const double y1 = m_latestCorrelationCurve[bestIndex - 1U];
        const double y2 = m_latestCorrelationCurve[bestIndex];
        const double y3 = m_latestCorrelationCurve[bestIndex + 1U];
        const double denom = 2.0 * (y1 - (2.0 * y2) + y3);
        if (std::abs(denom) > 1e-9 && y2 >= y1 && y2 >= y3) {
            const double delta = (y1 - y3) / denom;
            if (std::abs(delta) <= 1.0) {
                refinedLag += delta;
            }
        }
    }

    const double rawLatencyMs = std::max(0.0, refinedLag * msPerSample);

    if (m_peakCorrelation >= m_config.confidenceThreshold) {
        m_isConfident = true;
        if (m_estimatedLatencyMs <= 1e-6) {
            m_estimatedLatencyMs = rawLatencyMs;
        } else {
            m_estimatedLatencyMs
                = (m_config.smoothingAlpha * rawLatencyMs) + ((1.0 - m_config.smoothingAlpha) * m_estimatedLatencyMs);
        }
    } else {
        m_isConfident = false;
    }
}

double LatencyEstimator::getEstimatedLatencyMs() const noexcept
{
    return m_estimatedLatencyMs;
}

double LatencyEstimator::getPeakCorrelation() const noexcept
{
    return m_peakCorrelation;
}

bool LatencyEstimator::isConfident() const noexcept
{
    return m_isConfident;
}

std::vector<double> LatencyEstimator::getCorrelationCurve() const
{
    return m_latestCorrelationCurve;
}

const LatencyEstimatorConfig& LatencyEstimator::getConfig() const noexcept
{
    return m_config;
}

} // namespace PelcoD
