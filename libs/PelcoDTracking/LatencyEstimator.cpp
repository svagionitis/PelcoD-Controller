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
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_config.bufferCapacity = std::max<std::size_t>(32U, m_config.bufferCapacity);
    m_config.sampleRateHz = std::max(1.0, m_config.sampleRateHz);
    m_config.minLagMs = std::max(0.0, m_config.minLagMs);
    m_config.maxLagMs = std::max(m_config.minLagMs + 1.0, m_config.maxLagMs);
    m_config.confidenceThreshold = std::clamp(m_config.confidenceThreshold, 0.0, 1.0);
    m_config.smoothingAlpha = std::clamp(m_config.smoothingAlpha, 0.01, 1.0);
    m_config.minSignalVariance = std::max(1e-12, m_config.minSignalVariance);

    m_refBuffer.clear();
    m_respBuffer.clear();
    m_refQueue.clear();
    m_respQueue.clear();
    m_nextResampleTime = -1.0;
    m_count = 0U;
    m_estimatedLatencyMs = 0.0;
    m_peakCorrelation = 0.0;
    m_lastVarRef = 0.0;
    m_lastVarResp = 0.0;
    m_isConfident = false;
    m_latestCorrelationCurve.clear();
}

void LatencyEstimator::reset() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refBuffer.clear();
    m_respBuffer.clear();
    m_refQueue.clear();
    m_respQueue.clear();
    m_nextResampleTime = -1.0;
    m_count = 0U;
    m_estimatedLatencyMs = 0.0;
    m_peakCorrelation = 0.0;
    m_lastVarRef = 0.0;
    m_lastVarResp = 0.0;
    m_isConfident = false;
    m_latestCorrelationCurve.clear();
}

void LatencyEstimator::addSample(double reference, double response)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refBuffer.push_back(reference);
    m_respBuffer.push_back(response);
    ++m_count;

    if (m_refBuffer.size() > m_config.bufferCapacity) {
        m_refBuffer.erase(m_refBuffer.begin());
        m_respBuffer.erase(m_respBuffer.begin());
    }

    // Update cross-correlation once buffer is filled to capacity, with 25% hop interval
    const std::size_t hopSize = std::max<std::size_t>(4U, m_config.bufferCapacity / 4U);
    if (m_refBuffer.size() == m_config.bufferCapacity && (m_count % hopSize == 0U)) {
        // Evaluate without re-locking
        const std::size_t n = m_refBuffer.size();
        if (n >= 16U) {
            const double msPerSample = 1000.0 / m_config.sampleRateHz;
            const std::size_t minLag = static_cast<std::size_t>(std::floor(m_config.minLagMs / msPerSample));
            std::size_t maxLag = static_cast<std::size_t>(std::ceil(m_config.maxLagMs / msPerSample));

            if (maxLag >= n / 2U) {
                maxLag = (n / 2U) - 1U;
            }
            if (minLag < maxLag) {
                const std::size_t windowLen = n - maxLag;
                const std::size_t numLags = (maxLag - minLag) + 1U;
                m_latestCorrelationCurve.assign(numLags, 0.0);

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
                m_lastVarRef = varRef / static_cast<double>(windowLen);

                if (m_lastVarRef >= m_config.minSignalVariance) {
                    double bestScore = -1.0;
                    double bestCorr = 0.0;
                    std::size_t bestIndex = 0U;
                    double maxVarResp = 0.0;

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

                        if (varResp > maxVarResp) {
                            maxVarResp = varResp;
                        }

                        double corr = 0.0;
                        if (varResp > 1e-12) {
                            corr = cov / std::sqrt(varRef * varResp);
                        }
                        m_latestCorrelationCurve[lagIdx] = corr;

                        double score = 0.0;
                        if (m_config.polarity == PeakPolarity::Negative) {
                            score = -corr;
                        } else if (m_config.polarity == PeakPolarity::Positive) {
                            score = corr;
                        } else {
                            score = std::abs(corr);
                        }

                        if (score > bestScore) {
                            bestScore = score;
                            bestCorr = corr;
                            bestIndex = lagIdx;
                        }
                    }

                    m_lastVarResp = maxVarResp / static_cast<double>(windowLen);
                    m_peakCorrelation = bestCorr;

                    if (m_lastVarResp >= m_config.minSignalVariance) {
                        double refinedLag = static_cast<double>(minLag + bestIndex);
                        if (bestIndex > 0U && bestIndex + 1U < numLags) {
                            auto getScore = [this](double val) -> double {
                                if (m_config.polarity == PeakPolarity::Negative) {
                                    return -val;
                                }
                                if (m_config.polarity == PeakPolarity::Positive) {
                                    return val;
                                }
                                return std::abs(val);
                            };

                            const double s1 = getScore(m_latestCorrelationCurve[bestIndex - 1U]);
                            const double s2 = getScore(m_latestCorrelationCurve[bestIndex]);
                            const double s3 = getScore(m_latestCorrelationCurve[bestIndex + 1U]);
                            const double denom = 2.0 * (s1 - (2.0 * s2) + s3);
                            if (std::abs(denom) > 1e-9 && s2 >= s1 && s2 >= s3) {
                                const double delta = (s1 - s3) / denom;
                                if (std::abs(delta) <= 1.0) {
                                    refinedLag += delta;
                                }
                            }
                        }

                        const double rawLatencyMs = std::max(0.0, refinedLag * msPerSample);
                        if (bestScore >= m_config.confidenceThreshold) {
                            m_isConfident = true;
                            if (m_estimatedLatencyMs <= 1e-6) {
                                m_estimatedLatencyMs = rawLatencyMs;
                            } else {
                                m_estimatedLatencyMs = (m_config.smoothingAlpha * rawLatencyMs)
                                    + ((1.0 - m_config.smoothingAlpha) * m_estimatedLatencyMs);
                            }
                        } else {
                            m_isConfident = false;
                        }
                    } else {
                        m_isConfident = false;
                    }
                } else {
                    m_isConfident = false;
                    m_peakCorrelation = 0.0;
                }
            }
        }
    }
}

void LatencyEstimator::addTimestampedReference(double timestampSec, double reference)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refQueue.emplace_back(timestampSec, reference);
    processTimestampedQueuesLocked();
}

void LatencyEstimator::addTimestampedResponse(double timestampSec, double response)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_respQueue.emplace_back(timestampSec, response);
    processTimestampedQueuesLocked();
}

void LatencyEstimator::addTimestampedSample(double timestampSec, double reference, double response)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refQueue.emplace_back(timestampSec, reference);
    m_respQueue.emplace_back(timestampSec, response);
    processTimestampedQueuesLocked();
}

void LatencyEstimator::processTimestampedQueuesLocked()
{
    if (m_refQueue.empty() || m_respQueue.empty()) {
        return;
    }

    const double dt = 1.0 / m_config.sampleRateHz;
    if (m_nextResampleTime < 0.0) {
        m_nextResampleTime = std::max(m_refQueue.front().first, m_respQueue.front().first);
    }

    auto interpolate = [](const std::deque<std::pair<double, double>>& queue, double t) -> double {
        if (queue.size() == 1U || t <= queue.front().first) {
            return queue.front().second;
        }
        if (t >= queue.back().first) {
            return queue.back().second;
        }
        for (std::size_t i = 1U; i < queue.size(); ++i) {
            if (queue[i].first >= t) {
                const double t0 = queue[i - 1U].first;
                const double t1 = queue[i].first;
                const double span = t1 - t0;
                if (span <= 1e-9) {
                    return queue[i].second;
                }
                const double frac = (t - t0) / span;
                return queue[i - 1U].second + frac * (queue[i].second - queue[i - 1U].second);
            }
        }
        return queue.back().second;
    };

    while (!m_refQueue.empty() && !m_respQueue.empty() && m_refQueue.back().first >= m_nextResampleTime
        && m_respQueue.back().first >= m_nextResampleTime) {
        const double refVal = interpolate(m_refQueue, m_nextResampleTime);
        const double respVal = interpolate(m_respQueue, m_nextResampleTime);

        m_refBuffer.push_back(refVal);
        m_respBuffer.push_back(respVal);
        ++m_count;

        if (m_refBuffer.size() > m_config.bufferCapacity) {
            m_refBuffer.erase(m_refBuffer.begin());
            m_respBuffer.erase(m_respBuffer.begin());
        }

        m_nextResampleTime += dt;
    }

    // Prune stale queue entries that fall well behind next resample time
    const double pruneThreshold = m_nextResampleTime - 2.0;
    while (m_refQueue.size() > 2U && m_refQueue[1].first < pruneThreshold) {
        m_refQueue.pop_front();
    }
    while (m_respQueue.size() > 2U && m_respQueue[1].first < pruneThreshold) {
        m_respQueue.pop_front();
    }

    const std::size_t hopSize = std::max<std::size_t>(4U, m_config.bufferCapacity / 4U);
    if (m_refBuffer.size() == m_config.bufferCapacity && (m_count % hopSize == 0U)) {
        // Handled by update()
    }
}

void LatencyEstimator::update()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::size_t n = m_refBuffer.size();
    if (n < 16U) {
        return;
    }

    const double msPerSample = 1000.0 / m_config.sampleRateHz;
    const std::size_t minLag = static_cast<std::size_t>(std::floor(m_config.minLagMs / msPerSample));
    std::size_t maxLag = static_cast<std::size_t>(std::ceil(m_config.maxLagMs / msPerSample));

    if (maxLag >= n / 2U) {
        maxLag = (n / 2U) - 1U;
    }
    if (minLag >= maxLag) {
        return;
    }

    const std::size_t windowLen = n - maxLag;
    const std::size_t numLags = (maxLag - minLag) + 1U;
    m_latestCorrelationCurve.assign(numLags, 0.0);

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
    m_lastVarRef = varRef / static_cast<double>(windowLen);

    if (m_lastVarRef < m_config.minSignalVariance) {
        m_isConfident = false;
        m_peakCorrelation = 0.0;
        return;
    }

    double bestScore = -1.0;
    double bestCorr = 0.0;
    std::size_t bestIndex = 0U;
    double maxVarResp = 0.0;

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

        if (varResp > maxVarResp) {
            maxVarResp = varResp;
        }

        double corr = 0.0;
        if (varResp > 1e-12) {
            corr = cov / std::sqrt(varRef * varResp);
        }

        m_latestCorrelationCurve[lagIdx] = corr;

        double score = 0.0;
        if (m_config.polarity == PeakPolarity::Negative) {
            score = -corr;
        } else if (m_config.polarity == PeakPolarity::Positive) {
            score = corr;
        } else {
            score = std::abs(corr);
        }

        if (score > bestScore) {
            bestScore = score;
            bestCorr = corr;
            bestIndex = lagIdx;
        }
    }

    m_lastVarResp = maxVarResp / static_cast<double>(windowLen);
    m_peakCorrelation = bestCorr;

    if (m_lastVarResp < m_config.minSignalVariance) {
        m_isConfident = false;
        return;
    }

    // Parabolic interpolation for sub-sample precision around peak
    double refinedLag = static_cast<double>(minLag + bestIndex);
    if (bestIndex > 0U && bestIndex + 1U < numLags) {
        auto getScore = [this](double val) -> double {
            if (m_config.polarity == PeakPolarity::Negative) {
                return -val;
            }
            if (m_config.polarity == PeakPolarity::Positive) {
                return val;
            }
            return std::abs(val);
        };

        const double s1 = getScore(m_latestCorrelationCurve[bestIndex - 1U]);
        const double s2 = getScore(m_latestCorrelationCurve[bestIndex]);
        const double s3 = getScore(m_latestCorrelationCurve[bestIndex + 1U]);
        const double denom = 2.0 * (s1 - (2.0 * s2) + s3);
        if (std::abs(denom) > 1e-9 && s2 >= s1 && s2 >= s3) {
            const double delta = (s1 - s3) / denom;
            if (std::abs(delta) <= 1.0) {
                refinedLag += delta;
            }
        }
    }

    const double rawLatencyMs = std::max(0.0, refinedLag * msPerSample);

    if (bestScore >= m_config.confidenceThreshold) {
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
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_estimatedLatencyMs;
}

double LatencyEstimator::getEstimatedLatencySeconds() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_estimatedLatencyMs / 1000.0;
}

double LatencyEstimator::getPeakCorrelation() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_peakCorrelation;
}

double LatencyEstimator::getPeakCorrelationMagnitude() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::abs(m_peakCorrelation);
}

bool LatencyEstimator::isConfident() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isConfident;
}

std::vector<double> LatencyEstimator::getCorrelationCurve() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_latestCorrelationCurve;
}

double LatencyEstimator::getReferenceVariance() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastVarRef;
}

double LatencyEstimator::getResponseVariance() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastVarResp;
}

const LatencyEstimatorConfig& LatencyEstimator::getConfig() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

} // namespace PelcoD
