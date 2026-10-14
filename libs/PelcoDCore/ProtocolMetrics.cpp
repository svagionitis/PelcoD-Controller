#include "ProtocolMetrics.h"

#include <algorithm>

namespace PelcoD {

void ProtocolMetrics::recordQuerySent() noexcept
{
    m_queriesSent.fetch_add(1U, std::memory_order_relaxed);
}

void ProtocolMetrics::recordQuerySuccess(std::chrono::microseconds durationUs) noexcept
{
    m_queriesCompleted.fetch_add(1U, std::memory_order_relaxed);
    const auto dur = static_cast<std::uint64_t>(std::max<std::int64_t>(0, durationUs.count()));
    m_lastRttUs.store(dur, std::memory_order_relaxed);
    m_totalRttUs.fetch_add(dur, std::memory_order_relaxed);

    std::uint64_t currentMin = m_minRttUs.load(std::memory_order_relaxed);
    while (currentMin == 0U || dur < currentMin) {
        if (m_minRttUs.compare_exchange_weak(currentMin, dur, std::memory_order_relaxed, std::memory_order_relaxed)) {
            break;
        }
    }

    std::uint64_t currentMax = m_maxRttUs.load(std::memory_order_relaxed);
    while (dur > currentMax) {
        if (m_maxRttUs.compare_exchange_weak(currentMax, dur, std::memory_order_relaxed, std::memory_order_relaxed)) {
            break;
        }
    }
}

void ProtocolMetrics::recordQueryTimeout() noexcept
{
    m_queryTimeouts.fetch_add(1U, std::memory_order_relaxed);
}

void ProtocolMetrics::recordQueryRetry() noexcept
{
    m_queryRetries.fetch_add(1U, std::memory_order_relaxed);
}

PelcoDProtocolStats ProtocolMetrics::getStats() const noexcept
{
    PelcoDProtocolStats stats {};
    stats.queriesSent = m_queriesSent.load(std::memory_order_relaxed);
    stats.queriesCompleted = m_queriesCompleted.load(std::memory_order_relaxed);
    stats.queryTimeouts = m_queryTimeouts.load(std::memory_order_relaxed);
    stats.queryRetries = m_queryRetries.load(std::memory_order_relaxed);

    const std::uint64_t completed = stats.queriesCompleted;
    const std::uint64_t totalUs = m_totalRttUs.load(std::memory_order_relaxed);
    if (completed > 0U) {
        stats.avgRttMs = static_cast<double>(totalUs) / (1000.0 * static_cast<double>(completed));
    }
    stats.lastRttMs = static_cast<double>(m_lastRttUs.load(std::memory_order_relaxed)) / 1000.0;
    stats.minRttMs = static_cast<double>(m_minRttUs.load(std::memory_order_relaxed)) / 1000.0;
    stats.maxRttMs = static_cast<double>(m_maxRttUs.load(std::memory_order_relaxed)) / 1000.0;
    return stats;
}

void ProtocolMetrics::resetStats() noexcept
{
    m_queriesSent.store(0U, std::memory_order_relaxed);
    m_queriesCompleted.store(0U, std::memory_order_relaxed);
    m_queryTimeouts.store(0U, std::memory_order_relaxed);
    m_queryRetries.store(0U, std::memory_order_relaxed);
    m_totalRttUs.store(0U, std::memory_order_relaxed);
    m_lastRttUs.store(0U, std::memory_order_relaxed);
    m_minRttUs.store(0U, std::memory_order_relaxed);
    m_maxRttUs.store(0U, std::memory_order_relaxed);
}

} // namespace PelcoD
