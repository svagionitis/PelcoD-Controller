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

    // Amortized compaction: if read offset has advanced significantly, compact buffer
    if (m_readOffset > 0U) {
        if (m_readOffset >= m_buffer.size()) {
            m_buffer.clear();
            m_readOffset = 0U;
        } else if (m_readOffset >= 512U || m_buffer.size() > (m_maxBufferSize / 2U)) {
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(m_readOffset));
            m_readOffset = 0U;
        }
    }

    const std::size_t activeBuffered = m_buffer.size() - m_readOffset;
    if (activeBuffered + size > m_maxBufferSize) {
        LOG(WARNING) << "RxStreamAccumulator overflow (" << (activeBuffered + size) << " > " << m_maxBufferSize
                     << " bytes): resetting accumulator buffer";
        m_buffer.clear();
        m_readOffset = 0U;
    }

    m_buffer.insert(m_buffer.end(), data, data + size);
    m_lastRxTime = std::chrono::steady_clock::now();

    return extractLocked(expectation, false);
}

std::vector<std::vector<std::uint8_t>> RxStreamAccumulator::flushExpired(RxFrameExpectation expectation)
{
    std::scoped_lock lock(m_mutex);
    const std::size_t activeBuffered = (m_buffer.size() >= m_readOffset) ? (m_buffer.size() - m_readOffset) : 0U;
    if (activeBuffered == 0U) {
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

    while ((m_buffer.size() - m_readOffset) >= PelcoDFrame::GeneralResponseSize) {
        const auto bufBegin = m_buffer.begin() + static_cast<std::ptrdiff_t>(m_readOffset);
        const auto bufEnd = m_buffer.end();

        const auto syncIt = std::find(bufBegin, bufEnd, PelcoDFrame::SyncByte);
        if (syncIt == bufEnd) {
            m_discardedBytes.fetch_add(static_cast<std::uint64_t>(std::distance(bufBegin, bufEnd)), std::memory_order_relaxed);
            m_readOffset = m_buffer.size();
            break;
        }

        if (syncIt != bufBegin) {
            const auto dropped = static_cast<std::uint64_t>(std::distance(bufBegin, syncIt));
            m_discardedBytes.fetch_add(dropped, std::memory_order_relaxed);
            m_readOffset += static_cast<std::size_t>(dropped);
        }

        const std::size_t available = m_buffer.size() - m_readOffset;
        if (available < PelcoDFrame::GeneralResponseSize) {
            break;
        }

        bool frameExtracted = false;
        const auto frameStart = m_buffer.begin() + static_cast<std::ptrdiff_t>(m_readOffset);

        // 1. Try 7-byte standard frame first (most common PTZ replies, telemetry, ACKs)
        if (available >= PelcoDFrame::StandardFrameSize) {
            std::vector<std::uint8_t> frame(
                frameStart, frameStart + static_cast<std::ptrdiff_t>(PelcoDFrame::StandardFrameSize));
            if (PelcoDFrame::isValidFrame(frame)) {
                m_readOffset += PelcoDFrame::StandardFrameSize;
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
                frameStart, frameStart + static_cast<std::ptrdiff_t>(PelcoDFrame::QueryResponseSize));
            if (PelcoDFrame::isValidFrame(frame)) {
                m_readOffset += PelcoDFrame::QueryResponseSize;
                framesToDispatch.push_back(std::move(frame));
                frameExtracted = true;
                continue;
            }
        }

        // 3. Try 4-byte general response if allowed by expectation OR boundary lookahead confirms next frame
        const bool generalAllowed = (expectation == RxFrameExpectation::AllowGeneralResponse)
            || (expectation == RxFrameExpectation::AllFrames)
            || (available > PelcoDFrame::GeneralResponseSize
                && m_buffer[m_readOffset + PelcoDFrame::GeneralResponseSize] == PelcoDFrame::SyncByte);
        if (generalAllowed && available >= PelcoDFrame::GeneralResponseSize) {
            bool candidateAllowed = false;

            if (forceFlush) {
                candidateAllowed = (available == PelcoDFrame::GeneralResponseSize)
                    || (m_buffer[m_readOffset + PelcoDFrame::GeneralResponseSize] == PelcoDFrame::SyncByte);
            } else if (available > PelcoDFrame::GeneralResponseSize) {
                candidateAllowed = (m_buffer[m_readOffset + PelcoDFrame::GeneralResponseSize] == PelcoDFrame::SyncByte);
            }

            if (candidateAllowed) {
                std::vector<std::uint8_t> frame(
                    frameStart, frameStart + static_cast<std::ptrdiff_t>(PelcoDFrame::GeneralResponseSize));
                if (PelcoDFrame::isValidFrame(frame)) {
                    m_readOffset += PelcoDFrame::GeneralResponseSize;
                    framesToDispatch.push_back(std::move(frame));
                    frameExtracted = true;
                    continue;
                }
            }
        }

        // If no frame was extracted, check if we must wait for more bytes before declaring checksum error
        if (!frameExtracted) {
            bool couldBeQuery = (expectation == RxFrameExpectation::AwaitingQuery);
            if (couldBeQuery && available >= PelcoDFrame::GeneralResponseSize) {
                bool sawNull = false;
                for (std::size_t i { 2U }; i < std::min(available, static_cast<std::size_t>(17U)); ++i) {
                    const std::uint8_t b = m_buffer[m_readOffset + i];
                    if (b == 0x00U) {
                        sawNull = true;
                    } else if (sawNull || b < 32U || b > 126U) {
                        couldBeQuery = false;
                        break;
                    }
                }
            }

            const std::size_t maxExpectedSize = couldBeQuery
                ? PelcoDFrame::QueryResponseSize
                : PelcoDFrame::StandardFrameSize;

            if (available < maxExpectedSize) {
                // Not enough bytes to evaluate standard/query frame; retain in buffer
                break;
            }

            // Available bytes >= maxExpectedSize and no candidate at index 0 was valid; slide by 1
            m_checksumErrors.fetch_add(1U, std::memory_order_relaxed);
            m_discardedBytes.fetch_add(1U, std::memory_order_relaxed);
            m_readOffset += 1U;
        }
    }

    // Post-extraction compaction: if all consumed, reset buffer and offset
    if (m_readOffset >= m_buffer.size()) {
        m_buffer.clear();
        m_readOffset = 0U;
    } else if (m_readOffset >= 512U) {
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(m_readOffset));
        m_readOffset = 0U;
    }

    return framesToDispatch;
}

void RxStreamAccumulator::clear()
{
    std::scoped_lock lock(m_mutex);
    m_buffer.clear();
    m_readOffset = 0U;
}

std::size_t RxStreamAccumulator::size() const
{
    std::scoped_lock lock(m_mutex);
    return (m_buffer.size() >= m_readOffset) ? (m_buffer.size() - m_readOffset) : 0U;
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
