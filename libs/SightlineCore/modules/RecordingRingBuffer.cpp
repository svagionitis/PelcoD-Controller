/// @file RecordingRingBuffer.cpp
/// @brief Implementation of bounded ring buffer for asynchronous recording I/O.

#include "RecordingRingBuffer.h"

#include <utility>

namespace Sightline {

RecordingRingBuffer::RecordingRingBuffer(std::size_t capacity)
    : m_capacity { (capacity > 0U) ? capacity : 1U }
    , m_storage(m_capacity)
{
}

bool RecordingRingBuffer::push(RecordingChunk&& chunk)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count >= m_capacity) {
        ++m_droppedCount;
        return false;
    }

    m_storage[m_head] = std::move(chunk);
    m_head = (m_head + 1U) % m_capacity;
    ++m_count;
    return true;
}

std::optional<RecordingChunk> RecordingRingBuffer::pop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_count == 0U) {
        return std::nullopt;
    }

    RecordingChunk item = std::move(m_storage[m_tail]);
    m_tail = (m_tail + 1U) % m_capacity;
    --m_count;
    return item;
}

std::uint8_t RecordingRingBuffer::utilizationPercent() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_capacity == 0U) {
        return 0U;
    }
    const auto percent = static_cast<std::uint8_t>((m_count * 100ULL) / m_capacity);
    return percent;
}

std::uint64_t RecordingRingBuffer::droppedCount() const noexcept
{
    return m_droppedCount.load(std::memory_order_relaxed);
}

std::size_t RecordingRingBuffer::size() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_count;
}

std::size_t RecordingRingBuffer::capacity() const noexcept
{
    return m_capacity;
}

bool RecordingRingBuffer::empty() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_count == 0U;
}

bool RecordingRingBuffer::full() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_count >= m_capacity;
}

void RecordingRingBuffer::clear() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_head = 0U;
    m_tail = 0U;
    m_count = 0U;
    m_droppedCount.store(0ULL, std::memory_order_relaxed);
}

} // namespace Sightline
