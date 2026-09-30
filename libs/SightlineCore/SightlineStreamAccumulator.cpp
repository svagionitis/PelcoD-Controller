/// @file SightlineStreamAccumulator.cpp
/// @brief Implementation of Sightline SLA stream accumulator and framing parser.

#include "SightlineStreamAccumulator.h"
#include "SightlineTypes.h"

#include <algorithm>

namespace Sightline {

SightlineStreamAccumulator::SightlineStreamAccumulator(std::size_t maxBufferSize)
    : m_maxBufferSize { (maxBufferSize >= 128U) ? maxBufferSize : DefaultMaxBufferSize }
{
    m_buffer.reserve(1024U);
}

std::vector<std::vector<std::uint8_t>> SightlineStreamAccumulator::push(
    const std::vector<std::uint8_t>& data, bool validateChecksum)
{
    return push(data.data(), data.size(), validateChecksum);
}

std::vector<std::vector<std::uint8_t>> SightlineStreamAccumulator::push(
    const std::uint8_t* data, std::size_t size, bool validateChecksum)
{
    if (data == nullptr || size == 0U) {
        return {};
    }

    std::scoped_lock lock(m_mutex);
    std::vector<std::vector<std::uint8_t>> extractedPackets {};

    m_buffer.insert(m_buffer.end(), data, data + size);

    // Buffer overflow protection
    if (m_buffer.size() > m_maxBufferSize) {
        const auto pruneLen = m_buffer.size() - 512U;
        m_discardedBytes.fetch_add(static_cast<std::uint64_t>(pruneLen), std::memory_order_relaxed);
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(pruneLen));
    }

    while (m_buffer.size() >= 4U) {
        // 1. Locate sync header: 0x51 followed by 0xAC
        std::size_t syncIdx = 0U;
        bool syncFound = false;

        for (std::size_t i = 0U; (i + 1U) < m_buffer.size(); ++i) {
            if (m_buffer[i] == HeaderByte1 && m_buffer[i + 1U] == HeaderByte2) {
                syncIdx = i;
                syncFound = true;
                break;
            }
        }

        if (!syncFound) {
            // Retain last byte in case it is 0x51
            const std::size_t toDrop = (m_buffer.back() == HeaderByte1) ? (m_buffer.size() - 1U) : m_buffer.size();
            if (toDrop > 0U) {
                m_discardedBytes.fetch_add(static_cast<std::uint64_t>(toDrop), std::memory_order_relaxed);
                m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(toDrop));
            }
            break;
        }

        // Discard any noise prior to sync header
        if (syncIdx > 0U) {
            m_discardedBytes.fetch_add(static_cast<std::uint64_t>(syncIdx), std::memory_order_relaxed);
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(syncIdx));
        }

        if (m_buffer.size() < 4U) {
            // Need at least header + length byte(s)
            break;
        }

        // 2. Decode packet length
        const std::uint8_t lenLow = m_buffer[2U];
        std::size_t payloadAndCsLen { 0U };
        std::size_t headerSize { 3U };

        if ((lenLow & 0x80U) == 0U) {
            // Normal 1-byte length (< 128)
            payloadAndCsLen = static_cast<std::size_t>(lenLow);
            headerSize = 3U;
        } else {
            // Extended 2-byte length (>= 128)
            if (m_buffer.size() < 4U) {
                break;
            }
            const std::uint8_t lenHigh = m_buffer[3U];
            payloadAndCsLen = (static_cast<std::size_t>(lenHigh) << 7U) | static_cast<std::size_t>(lenLow & 0x7FU);
            headerSize = 4U;
        }

        // Length must be at least 2 bytes (MessageId + Checksum)
        if (payloadAndCsLen < 2U) {
            // Corrupt length; discard sync header and scan forward
            m_discardedBytes.fetch_add(1U, std::memory_order_relaxed);
            m_buffer.erase(m_buffer.begin());
            continue;
        }

        const std::size_t totalPacketLen = headerSize + payloadAndCsLen;
        if (m_buffer.size() < totalPacketLen) {
            // Incomplete frame, await more bytes
            break;
        }

        // 3. Checksum verification
        const std::size_t crcPayloadLen = payloadAndCsLen - 1U;
        const std::uint8_t* crcPayloadPtr = m_buffer.data() + headerSize;
        const std::uint8_t receivedCs = m_buffer[headerSize + crcPayloadLen];

        if (!validateChecksum || SightlineCrc8::validate(crcPayloadPtr, crcPayloadLen, receivedCs)) {
            std::vector<std::uint8_t> packet(
                m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(totalPacketLen));
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(totalPacketLen));
            m_packetsExtracted.fetch_add(1U, std::memory_order_relaxed);
            extractedPackets.push_back(std::move(packet));
        } else {
            // CRC failure: discard leading 0x51 and hunt for next valid packet
            m_checksumErrors.fetch_add(1U, std::memory_order_relaxed);
            m_discardedBytes.fetch_add(1U, std::memory_order_relaxed);
            m_buffer.erase(m_buffer.begin());
        }
    }

    return extractedPackets;
}

void SightlineStreamAccumulator::clear()
{
    std::scoped_lock lock(m_mutex);
    m_buffer.clear();
}

std::uint64_t SightlineStreamAccumulator::discardedBytes() const noexcept
{
    return m_discardedBytes.load(std::memory_order_relaxed);
}

std::uint64_t SightlineStreamAccumulator::checksumErrors() const noexcept
{
    return m_checksumErrors.load(std::memory_order_relaxed);
}

std::uint64_t SightlineStreamAccumulator::packetsExtracted() const noexcept
{
    return m_packetsExtracted.load(std::memory_order_relaxed);
}

void SightlineStreamAccumulator::resetStats() noexcept
{
    m_discardedBytes.store(0U, std::memory_order_relaxed);
    m_checksumErrors.store(0U, std::memory_order_relaxed);
    m_packetsExtracted.store(0U, std::memory_order_relaxed);
}

std::size_t SightlineStreamAccumulator::size() const
{
    std::scoped_lock lock(m_mutex);
    return m_buffer.size();
}

std::size_t SightlineStreamAccumulator::maxBufferSize() const noexcept
{
    return m_maxBufferSize;
}

} // namespace Sightline
