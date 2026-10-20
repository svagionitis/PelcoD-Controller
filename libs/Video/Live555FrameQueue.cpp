#include "Live555FrameQueue.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace Video {

Live555FrameQueue::Live555FrameQueue(std::size_t capacity)
    : m_capacity { (capacity >= 2U) ? capacity : 2U }
{
    m_ringBuffer.resize(m_capacity);
}

Live555FrameQueue::~Live555FrameQueue()
{
    clear();
}

void Live555FrameQueue::setEventTrigger(EventTriggerFunc trigger)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventTrigger = std::move(trigger);
}

bool Live555FrameQueue::push(const std::uint8_t* frameData, std::size_t size, double ptsSeconds, bool isKeyFrame)
{
    if ((frameData == nullptr) || (size == 0U)) {
        return false;
    }

    auto payload { std::make_shared<std::vector<std::uint8_t>>(size) };
    std::memcpy(payload->data(), frameData, size);

    return push(std::move(payload), ptsSeconds, isKeyFrame);
}

bool Live555FrameQueue::push(
    std::shared_ptr<const std::vector<std::uint8_t>> sharedData, double ptsSeconds, bool isKeyFrame)
{
    if (!sharedData || sharedData->empty()) {
        return false;
    }

    EventTriggerFunc triggerCopy {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        ++m_latestSequence;
        const auto slot { static_cast<std::size_t>(m_latestSequence % m_capacity) };

        QueuedFrame frame {};
        frame.data = std::move(sharedData);
        frame.ptsSeconds = ptsSeconds;
        frame.isKeyFrame = isKeyFrame;
        frame.sequenceNumber = m_latestSequence;
        frame.arrivalTime = std::chrono::steady_clock::now();

        m_ringBuffer[slot] = std::move(frame);

        if (m_count < m_capacity) {
            ++m_count;
        }

        if (isKeyFrame) {
            m_latestKeyframeSequence = m_latestSequence;
        }

        ++m_totalPushed;
        triggerCopy = m_eventTrigger;
    }

    if (triggerCopy) {
        triggerCopy();
    }

    return true;
}

std::unique_ptr<FrameQueueReader> Live555FrameQueue::createReader(bool syncToKeyframe)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_activeReaders;
    }
    return std::make_unique<FrameQueueReader>(*this, syncToKeyframe);
}

void Live555FrameQueue::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& slot : m_ringBuffer) {
        slot = QueuedFrame {};
    }
    m_count = 0U;
    m_latestSequence = 0U;
    m_latestKeyframeSequence = 0U;
}

FrameQueueStats Live555FrameQueue::getStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    FrameQueueStats stats {};
    stats.totalPushed = m_totalPushed;
    stats.totalDropped = m_totalDropped;
    stats.currentDepth = m_count;
    stats.activeReaders = m_activeReaders;
    stats.latestSequence = m_latestSequence;
    stats.oldestSequence = (m_count == 0U)
        ? 0U
        : ((m_count < m_capacity) ? 1U : (m_latestSequence - static_cast<std::uint64_t>(m_capacity) + 1U));
    return stats;
}

std::size_t Live555FrameQueue::capacity() const noexcept
{
    return m_capacity;
}

std::optional<QueuedFrame> Live555FrameQueue::readNext(
    std::uint64_t& readerSeq, bool& droppedFrames, bool syncToKeyframe)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    droppedFrames = false;

    if (m_count == 0U) {
        return std::nullopt;
    }

    const std::uint64_t oldestSeq {
        (m_count < m_capacity) ? 1U : (m_latestSequence - static_cast<std::uint64_t>(m_capacity) + 1U)
    };

    if (syncToKeyframe) {
        if ((m_latestKeyframeSequence >= oldestSeq) && (m_latestKeyframeSequence > 0U)) {
            readerSeq = m_latestKeyframeSequence;
        } else if (readerSeq == 0U) {
            readerSeq = m_latestSequence;
        }
    }

    if ((readerSeq < oldestSeq) && (readerSeq != 0U)) {
        droppedFrames = true;
        const std::uint64_t droppedCount { oldestSeq - readerSeq };
        m_totalDropped += droppedCount;

        if ((m_latestKeyframeSequence >= oldestSeq) && (m_latestKeyframeSequence > 0U)) {
            readerSeq = m_latestKeyframeSequence;
        } else {
            readerSeq = oldestSeq;
        }
    } else if (readerSeq == 0U) {
        readerSeq = oldestSeq;
    }

    if (readerSeq > m_latestSequence) {
        return std::nullopt;
    }

    const auto slot { static_cast<std::size_t>(readerSeq % m_capacity) };
    const QueuedFrame& frame { m_ringBuffer[slot] };

    if (frame.sequenceNumber != readerSeq) {
        return std::nullopt;
    }

    QueuedFrame result { frame };
    ++readerSeq;

    return result;
}

void Live555FrameQueue::detachReader() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_activeReaders > 0U) {
        --m_activeReaders;
    }
}

// -----------------------------------------------------------------------------
// FrameQueueReader Implementation
// -----------------------------------------------------------------------------

FrameQueueReader::FrameQueueReader(Live555FrameQueue& queue, bool syncToKeyframe)
    : m_queue { &queue }
    , m_requireKeyframeSync { syncToKeyframe }
{
}

FrameQueueReader::~FrameQueueReader()
{
    if (m_queue != nullptr) {
        m_queue->detachReader();
        m_queue = nullptr;
    }
}

FrameQueueReader::FrameQueueReader(FrameQueueReader&& other) noexcept
    : m_queue { other.m_queue }
    , m_readerSequence { other.m_readerSequence }
    , m_requireKeyframeSync { other.m_requireKeyframeSync }
{
    other.m_queue = nullptr;
    other.m_readerSequence = 0U;
    other.m_requireKeyframeSync = false;
}

FrameQueueReader& FrameQueueReader::operator=(FrameQueueReader&& other) noexcept
{
    if (this != &other) {
        if (m_queue != nullptr) {
            m_queue->detachReader();
        }
        m_queue = other.m_queue;
        m_readerSequence = other.m_readerSequence;
        m_requireKeyframeSync = other.m_requireKeyframeSync;

        other.m_queue = nullptr;
        other.m_readerSequence = 0U;
        other.m_requireKeyframeSync = false;
    }
    return *this;
}

std::optional<QueuedFrame> FrameQueueReader::getNextFrame(bool& outDropped)
{
    if (m_queue == nullptr) {
        outDropped = false;
        return std::nullopt;
    }

    auto frame { m_queue->readNext(m_readerSequence, outDropped, m_requireKeyframeSync) };
    if (frame.has_value()) {
        m_requireKeyframeSync = false;
    }
    return frame;
}

void FrameQueueReader::syncToLatestKey() noexcept
{
    m_requireKeyframeSync = true;
}

std::uint64_t FrameQueueReader::lastSequence() const noexcept
{
    return m_readerSequence;
}

} // namespace Video
