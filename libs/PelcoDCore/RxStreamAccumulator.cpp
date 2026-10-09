/// @file RxStreamAccumulator.cpp
/// @brief Implementation of Pelco-D byte stream accumulator and packet framer.

#include "RxStreamAccumulator.h"

#include <algorithm>
#include <chrono>
#include <glog/logging.h>

namespace PelcoD {

RxStreamAccumulator::RxStreamAccumulator(std::size_t maxBufferSize, std::chrono::milliseconds interByteTimeout)
    : m_maxBufferSize { (maxBufferSize > 0U) ? maxBufferSize : DefaultMaxBufferSize }
    , m_interByteTimeout { interByteTimeout }
{
}

std::vector<std::vector<std::uint8_t>> RxStreamAccumulator::push(
    const std::vector<std::uint8_t>& data, RxFrameExpectation expectation)
{
    return push(data.data(), data.size(), expectation);
}

std::vector<std::vector<std::uint8_t>> RxStreamAccumulator::push(
    const std::vector<std::uint8_t>& data, bool awaitingQuery)
{
    const RxFrameExpectation exp = awaitingQuery
        ? RxFrameExpectation::AwaitingQuery
        : RxFrameExpectation::StandardOnly;
    return push(data.data(), data.size(), exp);
}

std::vector<std::vector<std::uint8_t>> RxStreamAccumulator::push(
    const std::uint8_t* data, std::size_t size, bool awaitingQuery)
{
    const RxFrameExpectation exp = awaitingQuery
        ? RxFrameExpectation::AwaitingQuery
        : RxFrameExpectation::StandardOnly;
    return push(data, size, exp);
}

std::vector<std::vector<std::uint8_t>> RxStreamAccumulator::push(
    const std::uint8_t* data, std::size_t size, RxFrameExpectation expectation)
{
    if (data == nullptr || size == 0U) {
        return {};
    }

    std::scoped_lock lock(m_mutex);

    if (m_buffer.size() + size > m_maxBufferSize) {
        LOG(WARNING) << "RxStreamAccumulator overflow (" << (m_buffer.size() + size) << " > " << m_maxBufferSize
                     << " bytes): resetting accumulator buffer";
        m_buffer.clear();
    }

    m_buffer.insert(m_buffer.end(), data, data + size);
    m_lastRxTime = std::chrono::steady_clock::now();

    return extractLocked(expectation, false);
}

std::vector<std::vector<std::uint8_t>> RxStreamAccumulator::flushExpired(RxFrameExpectation expectation)
{
    std::scoped_lock lock(m_mutex);
    if (m_buffer.empty()) {
        return {};
    }

    const auto now = std::chrono::steady_clock::now();
    const bool timeoutElapsed = (now - m_lastRxTime) >= m_interByteTimeout;
    if (!timeoutElapsed) {
        return {};
    }

    return extractLocked(expectation, true);
}

std::vector<std::vector<std::uint8_t>> RxStreamAccumulator::flush(RxFrameExpectation expectation)
{
    std::scoped_lock lock(m_mutex);
    return extractLocked(expectation, true);
}

std::vector<std::vector<std::uint8_t>> RxStreamAccumulator::extractLocked(
    RxFrameExpectation expectation, bool forceFlush)
{
    std::vector<std::vector<std::uint8_t>> framesToDispatch;

    while (m_buffer.size() >= PelcoDFrame::GeneralResponseSize) {
        const auto syncIt = std::find(m_buffer.begin(), m_buffer.end(), PelcoDFrame::SyncByte);
        if (syncIt == m_buffer.end()) {
            m_discardedBytes.fetch_add(static_cast<std::uint64_t>(m_buffer.size()), std::memory_order_relaxed);
            m_buffer.clear();
            break;
        }

        if (syncIt != m_buffer.begin()) {
            const auto dropped = static_cast<std::uint64_t>(std::distance(m_buffer.begin(), syncIt));
            m_discardedBytes.fetch_add(dropped, std::memory_order_relaxed);
            m_buffer.erase(m_buffer.begin(), syncIt);
        }

        const std::size_t available = m_buffer.size();
        if (available < PelcoDFrame::GeneralResponseSize) {
            break;
        }

        bool frameExtracted = false;

        // 1. Try 7-byte standard frame first (most common PTZ replies, telemetry, ACKs)
        if (available >= PelcoDFrame::StandardFrameSize) {
            std::vector<std::uint8_t> frame(
                m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(PelcoDFrame::StandardFrameSize));
            if (PelcoDFrame::isValidFrame(frame)) {
                m_buffer.erase(
                    m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(PelcoDFrame::StandardFrameSize));
                framesToDispatch.push_back(std::move(frame));
                frameExtracted = true;
                continue;
            }
        }

        // 2. Try 18-byte query response if awaiting query or in AllFrames mode
        const bool queryAllowed = (expectation == RxFrameExpectation::AwaitingQuery)
            || (expectation == RxFrameExpectation::AllFrames);
        if (queryAllowed && available >= PelcoDFrame::QueryResponseSize) {
            std::vector<std::uint8_t> frame(
                m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(PelcoDFrame::QueryResponseSize));
            if (PelcoDFrame::isValidFrame(frame)) {
                m_buffer.erase(
                    m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(PelcoDFrame::QueryResponseSize));
                framesToDispatch.push_back(std::move(frame));
                frameExtracted = true;
                continue;
            }
        }

        // 3. Try 4-byte general response only if explicitly allowed and boundary lookahead or timeout confirmed
        const bool generalAllowed = (expectation == RxFrameExpectation::AllowGeneralResponse)
            || (expectation == RxFrameExpectation::AllFrames);
        if (generalAllowed && available >= PelcoDFrame::GeneralResponseSize) {
            bool candidateAllowed = false;

            if (forceFlush) {
                // Under explicit flush or expired silence, allow candidate if buffer ends exactly at 4 bytes
                // or if followed by another sync byte
                candidateAllowed = (available == PelcoDFrame::GeneralResponseSize)
                    || (m_buffer[PelcoDFrame::GeneralResponseSize] == PelcoDFrame::SyncByte);
            } else if (available > PelcoDFrame::GeneralResponseSize) {
                // In continuous stream, 4-byte frame is only valid if immediately followed by next sync byte
                candidateAllowed = (m_buffer[PelcoDFrame::GeneralResponseSize] == PelcoDFrame::SyncByte);
            }

            if (candidateAllowed) {
                std::vector<std::uint8_t> frame(
                    m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(PelcoDFrame::GeneralResponseSize));
                if (PelcoDFrame::isValidFrame(frame)) {
                    m_buffer.erase(
                        m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(PelcoDFrame::GeneralResponseSize));
                    framesToDispatch.push_back(std::move(frame));
                    frameExtracted = true;
                    continue;
                }
            }
        }

        // If no frame was extracted, check if we must wait for more bytes before declaring checksum error
        if (!frameExtracted) {
            const std::size_t maxExpectedSize = (expectation == RxFrameExpectation::AwaitingQuery)
                ? PelcoDFrame::QueryResponseSize
                : PelcoDFrame::StandardFrameSize;

            if (available < maxExpectedSize) {
                // Not enough bytes to evaluate standard/query frame; retain in buffer
                break;
            }

            // Available bytes >= maxExpectedSize and no candidate at index 0 was valid; slide by 1
            m_checksumErrors.fetch_add(1U, std::memory_order_relaxed);
            m_discardedBytes.fetch_add(1U, std::memory_order_relaxed);
            m_buffer.erase(m_buffer.begin());
        }
    }

    return framesToDispatch;
}

void RxStreamAccumulator::clear()
{
    std::scoped_lock lock(m_mutex);
    m_buffer.clear();
}

std::size_t RxStreamAccumulator::size() const
{
    std::scoped_lock lock(m_mutex);
    return m_buffer.size();
}

std::size_t RxStreamAccumulator::maxBufferSize() const noexcept
{
    return m_maxBufferSize;
}

std::chrono::milliseconds RxStreamAccumulator::interByteTimeout() const noexcept
{
    return m_interByteTimeout;
}

std::uint64_t RxStreamAccumulator::discardedBytes() const noexcept
{
    return m_discardedBytes.load(std::memory_order_relaxed);
}

std::uint64_t RxStreamAccumulator::checksumErrors() const noexcept
{
    return m_checksumErrors.load(std::memory_order_relaxed);
}

void RxStreamAccumulator::resetStats() noexcept
{
    m_discardedBytes.store(0U, std::memory_order_relaxed);
    m_checksumErrors.store(0U, std::memory_order_relaxed);
}

} // namespace PelcoD
