#include "LatencyTracker.h"

#include <algorithm>
#include <chrono>

namespace Video {

namespace {

    constexpr double kNsPerMs { 1'000'000.0 };

} // namespace

std::int64_t steadyNowNs() noexcept
{
    const auto sinceEpoch { std::chrono::steady_clock::now().time_since_epoch() };
    return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(sinceEpoch).count());
}

LatencyTracker::LatencyTracker(std::size_t window)
    : m_samples(std::max<std::size_t>(window, 1U), 0.0)
{
}

void LatencyTracker::addSample(double ms)
{
    if (!(ms >= 0.0)) {
        return; // rejects negatives and NaN
    }
    const std::lock_guard<std::mutex> lock { m_mutex };
    pushLocked(ms);
}

bool LatencyTracker::addInterval(std::int64_t startNs, std::int64_t endNs)
{
    if ((startNs <= 0) || (endNs < startNs)) {
        return false;
    }
    const double ms { static_cast<double>(endNs - startNs) / kNsPerMs };
    const std::lock_guard<std::mutex> lock { m_mutex };
    pushLocked(ms);
    return true;
}

LatencyStats LatencyTracker::snapshot() const
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    LatencyStats stats {};
    if (m_filled == 0U) {
        return stats;
    }

    double sum { 0.0 };
    double lo { m_samples[0U] };
    double hi { m_samples[0U] };
    for (std::size_t i { 0U }; i < m_filled; ++i) {
        const double v { m_samples[i] };
        sum += v;
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }

    stats.lastMs = m_lastMs;
    stats.avgMs = sum / static_cast<double>(m_filled);
    stats.minMs = lo;
    stats.maxMs = hi;
    stats.count = m_count;
    return stats;
}

void LatencyTracker::reset()
{
    const std::lock_guard<std::mutex> lock { m_mutex };
    std::fill(m_samples.begin(), m_samples.end(), 0.0);
    m_next = 0U;
    m_filled = 0U;
    m_lastMs = 0.0;
    m_count = 0U;
}

void LatencyTracker::pushLocked(double ms)
{
    m_samples[m_next] = ms;
    m_next = (m_next + 1U) % m_samples.size();
    m_filled = std::min(m_filled + 1U, m_samples.size());
    m_lastMs = ms;
    ++m_count;
}

} // namespace Video
