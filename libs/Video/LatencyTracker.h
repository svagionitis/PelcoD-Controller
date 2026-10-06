#pragma once

/// @file LatencyTracker.h
/// @brief Qt-free rolling latency accumulator and monotonic clock helper for video pipeline timing.

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace Video {

/// @struct LatencyStats
/// @brief Snapshot of rolling latency statistics in milliseconds.
struct LatencyStats {
    double lastMs { 0.0 }; ///< Most recent accepted sample.
    double avgMs { 0.0 }; ///< Arithmetic mean over the rolling window.
    double minMs { 0.0 }; ///< Minimum over the rolling window.
    double maxMs { 0.0 }; ///< Maximum over the rolling window.
    std::uint64_t count { 0U }; ///< Total accepted samples since construction or last reset.
};

/// @brief Reads the process-wide monotonic clock.
/// @details Uses std::chrono::steady_clock so stamps taken on a decoder thread and on a
///          render thread are directly comparable. Never affected by wall-clock changes.
/// @return Nanoseconds since the steady_clock epoch.
[[nodiscard]] std::int64_t steadyNowNs() noexcept;

/// @class LatencyTracker
/// @brief Fixed-capacity rolling window of latency samples with last/avg/min/max statistics.
/// @details Storage is pre-allocated at construction; addSample() never allocates.
///          A requested window of zero is clamped to one.
/// @note Thread-safe: all public methods are guarded by an internal mutex, so samples may
///       be produced on a render thread while snapshots are read on the GUI thread.
class LatencyTracker {
public:
    /// @brief Default rolling window length in samples (~4 s at 30 FPS).
    static constexpr std::size_t kDefaultWindow { 120U };

    /// @brief Constructs a tracker with a pre-allocated rolling window.
    /// @param[in] window Number of samples retained for statistics (clamped to >= 1).
    explicit LatencyTracker(std::size_t window = kDefaultWindow);

    /// @brief Adds a latency sample in milliseconds.
    /// @details Negative values are rejected silently.
    /// @param[in] ms Latency sample in milliseconds.
    void addSample(double ms);

    /// @brief Adds a sample computed from two steadyNowNs() stamps.
    /// @param[in] startNs Start stamp in nanoseconds (must be > 0).
    /// @param[in] endNs End stamp in nanoseconds (must be >= startNs).
    /// @return True if the interval was valid and recorded.
    [[nodiscard]] bool addInterval(std::int64_t startNs, std::int64_t endNs);

    /// @brief Computes statistics over the current rolling window.
    /// @return LatencyStats snapshot (all zero when empty).
    [[nodiscard]] LatencyStats snapshot() const;

    /// @brief Discards all samples and resets the total count.
    void reset();

private:
    void pushLocked(double ms);

    mutable std::mutex m_mutex {};
    std::vector<double> m_samples {};
    std::size_t m_next { 0U };
    std::size_t m_filled { 0U };
    double m_lastMs { 0.0 };
    std::uint64_t m_count { 0U };
};

} // namespace Video
