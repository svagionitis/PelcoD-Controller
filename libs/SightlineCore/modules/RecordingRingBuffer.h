#pragma once

/// @file RecordingRingBuffer.h
/// @brief Thread-safe bounded ring buffer for decoupling video encoding from storage I/O.
/// @details Implements a Single-Producer Single-Consumer (SPSC) circular queue with atomic pointers.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace Sightline {

/// @struct RecordingChunk
/// @brief Bounded memory packet passed from encoder thread to storage writer thread.
struct RecordingChunk {
    std::uint64_t timestampUs { 0ULL };       ///< Microsecond precision timestamp
    std::uint8_t cameraIndex { 0U };          ///< Originating sensor channel
    bool isFlushSentinel { false };           ///< If true, triggers immediate buffer flush and close
    std::vector<std::uint8_t> data {};        ///< Encapsulated TS packets or snapshot payload
};

/// @class RecordingRingBuffer
/// @brief High-throughput ring buffer decoupling real-time video threads from disk writes.
class RecordingRingBuffer {
public:
    /// @brief Constructs ring buffer with bounded capacity.
    /// @param[in] capacity Maximum number of chunks accommodated before overrun.
    explicit RecordingRingBuffer(std::size_t capacity = 1024U);
    ~RecordingRingBuffer() = default;

    RecordingRingBuffer(const RecordingRingBuffer&) = delete;
    RecordingRingBuffer& operator=(const RecordingRingBuffer&) = delete;
    RecordingRingBuffer(RecordingRingBuffer&&) = delete;
    RecordingRingBuffer& operator=(RecordingRingBuffer&&) = delete;

    /// @brief Enqueues a chunk from the producer (encoder) thread without blocking.
    /// @param[in,out] chunk Chunk to enqueue (moved).
    /// @return True if enqueued, false if buffer is saturated (overrun).
    [[nodiscard]] bool push(RecordingChunk&& chunk);

    /// @brief Dequeues a chunk on the consumer (storage worker) thread.
    /// @return Extracted chunk, or std::nullopt if buffer is empty.
    [[nodiscard]] std::optional<RecordingChunk> pop();

    /// @brief Calculates current queue fullness percentage.
    /// @return Integer percentage from 0 to 100.
    [[nodiscard]] std::uint8_t utilizationPercent() const noexcept;

    /// @brief Retrieves cumulative dropped chunks counter caused by queue overrun.
    /// @return Cumulative dropped count.
    [[nodiscard]] std::uint64_t droppedCount() const noexcept;

    /// @brief Returns current number of unconsumed chunks in the queue.
    /// @return Current item count.
    [[nodiscard]] std::size_t size() const noexcept;

    /// @brief Returns total queue capacity.
    /// @return Queue capacity.
    [[nodiscard]] std::size_t capacity() const noexcept;

    /// @brief Checks whether the ring buffer is empty.
    /// @return True if empty.
    [[nodiscard]] bool empty() const noexcept;

    /// @brief Checks whether the ring buffer is full.
    /// @return True if full.
    [[nodiscard]] bool full() const noexcept;

    /// @brief Resets queue to empty state and resets drop counter.
    void clear() noexcept;

private:
    const std::size_t m_capacity;
    std::vector<RecordingChunk> m_storage;
    mutable std::mutex m_mutex;
    std::size_t m_head { 0U };
    std::size_t m_tail { 0U };
    std::size_t m_count { 0U };
    std::atomic<std::uint64_t> m_droppedCount { 0ULL };
};

} // namespace Sightline
