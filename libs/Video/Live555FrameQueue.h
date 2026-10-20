#pragma once

#include "Live555FrameTypes.h"

#include <cstddef>
#include <cstdint>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace Video {

class FrameQueueReader;

/// @class Live555FrameQueue
/// @brief Thread-safe, bounded, multi-reader circular ring buffer for live frame broadcasting.
/// @details Supports single or multiple concurrent producer threads pushing frames,
///          while permitting multiple independent readers (each corresponding to an active
///          RTSP client session) to poll frames without copying payloads.
class Live555FrameQueue final {
public:
    /// @brief Callback signature for waking up Live555 TaskScheduler.
    using EventTriggerFunc = std::function<void()>;

    /// @brief Constructs queue with bounded maximum capacity.
    /// @param[in] capacity Maximum number of frames retained in circular buffer (minimum 2).
    explicit Live555FrameQueue(std::size_t capacity = 30U);

    ~Live555FrameQueue();

    Live555FrameQueue(const Live555FrameQueue&) = delete;
    Live555FrameQueue& operator=(const Live555FrameQueue&) = delete;
    Live555FrameQueue(Live555FrameQueue&&) = delete;
    Live555FrameQueue& operator=(Live555FrameQueue&&) = delete;

    /// @brief Sets an asynchronous trigger callback invoked when a new frame is queued.
    /// @param[in] trigger Callback invoking TaskScheduler::triggerEvent().
    void setEventTrigger(EventTriggerFunc trigger);

    /// @brief Enqueues a new frame into the circular ring buffer.
    /// @param[in] frameData Raw byte buffer of NALU or TS packets.
    /// @param[in] size Size of frame buffer in bytes.
    /// @param[in] ptsSeconds Presentation timestamp in seconds.
    /// @param[in] isKeyFrame True if frame contains SPS/PPS/IDR or TS PAT/PMT/IDR.
    /// @return True on success, false if size == 0 or invalid.
    [[nodiscard]] bool push(const std::uint8_t* frameData, std::size_t size, double ptsSeconds, bool isKeyFrame);

    /// @brief Enqueues a pre-shared frame buffer into the circular ring buffer (zero-copy).
    /// @param[in] sharedData Shared pointer to immutable payload.
    /// @param[in] ptsSeconds Presentation timestamp in seconds.
    /// @param[in] isKeyFrame True if frame contains keyframe markers.
    /// @return True on success.
    [[nodiscard]] bool push(
        std::shared_ptr<const std::vector<std::uint8_t>> sharedData, double ptsSeconds, bool isKeyFrame);

    /// @brief Creates an attached reader instance for an RTSP client subsession.
    /// @param[in] syncToKeyframe If true, reader skips immediately to newest keyframe.
    /// @return Unique pointer to the reader.
    [[nodiscard]] std::unique_ptr<FrameQueueReader> createReader(bool syncToKeyframe = true);

    /// @brief Clears all queued frames and resets sequence numbers.
    void clear();

    /// @brief Queries current queue metrics and telemetry.
    /// @return Current queue statistics snapshot.
    [[nodiscard]] FrameQueueStats getStats() const;

    /// @brief Returns the maximum capacity of the ring buffer.
    /// @return Capacity in frames.
    [[nodiscard]] std::size_t capacity() const noexcept;

private:
    friend class FrameQueueReader;

    [[nodiscard]] std::optional<QueuedFrame> readNext(
        std::uint64_t& readerSeq, bool& droppedFrames, bool syncToKeyframe);

    void detachReader() noexcept;

    mutable std::mutex m_mutex {};
    std::size_t m_capacity { 30U };
    std::vector<QueuedFrame> m_ringBuffer {};
    std::size_t m_count { 0U };
    std::uint64_t m_latestSequence { 0U };
    std::uint64_t m_latestKeyframeSequence { 0U };
    std::uint64_t m_totalPushed { 0U };
    std::uint64_t m_totalDropped { 0U };
    std::size_t m_activeReaders { 0U };
    EventTriggerFunc m_eventTrigger {};
};

/// @class FrameQueueReader
/// @brief RAII reader handle tracking individual client consumption progress.
class FrameQueueReader final {
public:
    /// @brief Constructs reader attached to a queue.
    /// @param[in] queue Reference to parent queue.
    /// @param[in] syncToKeyframe If true, synchronizes on first read to newest keyframe.
    explicit FrameQueueReader(Live555FrameQueue& queue, bool syncToKeyframe);

    ~FrameQueueReader();

    FrameQueueReader(const FrameQueueReader&) = delete;
    FrameQueueReader& operator=(const FrameQueueReader&) = delete;
    FrameQueueReader(FrameQueueReader&& other) noexcept;
    FrameQueueReader& operator=(FrameQueueReader&& other) noexcept;

    /// @brief Reads the next available frame in sequence.
    /// @param[out] outDropped Set to true if reader fell behind ring buffer and dropped frames.
    /// @return Optional frame containing payload, or std::nullopt if no new frames are ready.
    [[nodiscard]] std::optional<QueuedFrame> getNextFrame(bool& outDropped);

    /// @brief Forces the reader to resynchronize with the most recent keyframe.
    void syncToLatestKey() noexcept;

    /// @brief Retrieves the last read sequence number.
    /// @return Sequence number.
    [[nodiscard]] std::uint64_t lastSequence() const noexcept;

private:
    Live555FrameQueue* m_queue { nullptr };
    std::uint64_t m_readerSequence { 0U };
    bool m_requireKeyframeSync { true };
};

} // namespace Video
