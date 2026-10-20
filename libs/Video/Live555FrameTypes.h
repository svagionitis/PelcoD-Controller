#pragma once

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <memory>
#include <vector>

namespace Video {

/// @struct QueuedFrame
/// @brief Immutable reference-counted frame unit stored in the ring buffer.
struct QueuedFrame {
    std::shared_ptr<const std::vector<std::uint8_t>> data { nullptr }; ///< Frame payload
    double ptsSeconds { 0.0 }; ///< Presentation timestamp (seconds)
    bool isKeyFrame { false }; ///< True if SPS/PPS/IDR or TS PAT/PMT/IDR
    std::uint64_t sequenceNumber { 0U }; ///< Monotonically increasing sequence ID
    std::chrono::steady_clock::time_point arrivalTime {}; ///< Host arrival time for latency tracking
};

/// @struct FrameQueueStats
/// @brief Telemetry metrics for queue performance and client consumption.
struct FrameQueueStats {
    std::uint64_t totalPushed { 0U }; ///< Total frames submitted by producer
    std::uint64_t totalDropped { 0U }; ///< Frames dropped due to lagging readers
    std::size_t currentDepth { 0U }; ///< Current frame count stored in ring buffer
    std::size_t activeReaders { 0U }; ///< Currently attached reader instances
    std::uint64_t latestSequence { 0U }; ///< Latest sequence number queued
    std::uint64_t oldestSequence { 0U }; ///< Oldest valid sequence number still in buffer
};

} // namespace Video
