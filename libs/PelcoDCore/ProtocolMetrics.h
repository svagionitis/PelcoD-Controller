#pragma once

/// @file ProtocolMetrics.h
/// @brief Tracks round-trip latency, query execution counters, and protocol telemetry.

#include "PelcoDStats.h"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace PelcoD {

/// @class ProtocolMetrics
/// @brief Thread-safe performance monitor capturing query and latency statistics.
/// @details Accumulates queries sent, completed, timed out, and retried, along with
///          microsecond-resolution round-trip time (RTT) statistics.
class ProtocolMetrics final {
public:
    /// @brief Default constructor initializing all counters to zero.
    ProtocolMetrics() noexcept = default;

    /// @brief Destructor.
    ~ProtocolMetrics() noexcept = default;

    ProtocolMetrics(const ProtocolMetrics&) = delete;
    ProtocolMetrics& operator=(const ProtocolMetrics&) = delete;
    ProtocolMetrics(ProtocolMetrics&&) = delete;
    ProtocolMetrics& operator=(ProtocolMetrics&&) = delete;

    /// @brief Records the transmission of a query frame.
    void recordQuerySent() noexcept;

    /// @brief Records successful query completion with round-trip duration.
    /// @param[in] durationUs Elapsed query duration in microseconds.
    void recordQuerySuccess(std::chrono::microseconds durationUs) noexcept;

    /// @brief Records a query timeout event.
    void recordQueryTimeout() noexcept;

    /// @brief Records a query retry attempt.
    void recordQueryRetry() noexcept;

    /// @brief Captures a snapshot of current protocol performance counters.
    /// @return Frozen snapshot containing query and latency metrics.
    [[nodiscard]] PelcoDProtocolStats getStats() const noexcept;

    /// @brief Resets all internal performance and latency metrics to zero.
    void resetStats() noexcept;

private:
    std::atomic<std::uint64_t> m_queriesSent { 0U };
    std::atomic<std::uint64_t> m_queriesCompleted { 0U };
    std::atomic<std::uint64_t> m_queryTimeouts { 0U };
    std::atomic<std::uint64_t> m_queryRetries { 0U };
    std::atomic<std::uint64_t> m_totalRttUs { 0U };
    std::atomic<std::uint64_t> m_lastRttUs { 0U };
    std::atomic<std::uint64_t> m_minRttUs { 0U };
    std::atomic<std::uint64_t> m_maxRttUs { 0U };
};

} // namespace PelcoD
