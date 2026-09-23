#include "KlvStreamScanner.h"
#include "KlvBer.h"
#include "KlvCrc.h"
#include <algorithm>
#include <cstring>

namespace Klv {

void KlvStreamScanner::setRawPacketCallback(RawPacketCallback callback) {
    m_rawPacketCallback = std::move(callback);
}

void KlvStreamScanner::setMessageCallback(MessageCallback callback) {
    m_messageCallback = std::move(callback);
}

void KlvStreamScanner::setMaxBufferSize(std::size_t maxBytes) noexcept {
    m_maxBufferSize = std::max(maxBytes, static_cast<std::size_t>(4096U));
}

void KlvStreamScanner::reset() {
    m_buffer.clear();
}

std::size_t KlvStreamScanner::bufferedBytes() const noexcept {
    return m_buffer.size();
}

std::size_t KlvStreamScanner::processBytes(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0U) {
        return 0U;
    }

    m_buffer.insert(m_buffer.end(), data, data + size);

    if (m_buffer.size() > m_maxBufferSize) {
        // Prune the older half of the buffer to protect against unbounded growth
        const std::size_t keepSize = m_maxBufferSize / 2U;
        m_buffer.erase(m_buffer.begin(), m_buffer.end() - keepSize);
    }

    return scanAndDispatch();
}

std::size_t KlvStreamScanner::scanAndDispatch() {
    std::size_t packetsDispatched = 0U;

    while (m_buffer.size() >= kUniversalLabelSize + 2U) {
        // Find Universal Label prefix (first 12 bytes)
        auto it = std::search(m_buffer.begin(), m_buffer.end(),
                              kMisb0601UniversalLabel.begin(),
                              kMisb0601UniversalLabel.begin() + kMisb0601PrefixSize);

        if (it == m_buffer.end()) {
            // No UL found. Preserve at most 15 trailing bytes to handle boundary overlap
            if (m_buffer.size() >= kUniversalLabelSize) {
                const std::size_t discardCount = m_buffer.size() - (kUniversalLabelSize - 1U);
                m_buffer.erase(m_buffer.begin(), m_buffer.begin() + discardCount);
            }
            break;
        }

        // Discard any garbage leading bytes prior to the matched UL
        if (it != m_buffer.begin()) {
            m_buffer.erase(m_buffer.begin(), it);
        }

        if (m_buffer.size() < kUniversalLabelSize + 2U) {
            // Need more bytes to decode BER length
            break;
        }

        // Decode BER length
        std::size_t payloadLength { 0U };
        std::size_t berLenConsumed { 0U };
        const std::uint8_t* berPtr = m_buffer.data() + kUniversalLabelSize;
        const std::size_t availableAfterUl = m_buffer.size() - kUniversalLabelSize;

        if (!KlvBer::decodeLength(berPtr, availableAfterUl, payloadLength, berLenConsumed)) {
            // If the initial byte of BER length is long form and we just don't have enough bytes yet
            if (availableAfterUl < 5U && (berPtr[0] & 0x80U) != 0U) {
                break; // Wait for more bytes
            }
            // Truly malformed BER length: skip this false-positive UL by 1 byte
            m_buffer.erase(m_buffer.begin());
            continue;
        }

        const std::size_t totalPacketSize = kUniversalLabelSize + berLenConsumed + payloadLength;
        if (m_buffer.size() < totalPacketSize) {
            // Complete packet has not fully arrived yet; wait for remaining bytes
            break;
        }

        // Full packet is buffered. Check CRC-16
        const std::uint8_t* packetData = m_buffer.data();
        if (KlvCrc::verifyPacket(packetData, totalPacketSize)) {
            // Valid packet!
            if (m_rawPacketCallback) {
                m_rawPacketCallback(packetData, totalPacketSize);
            }

            if (m_messageCallback) {
                UasDatalinkMessage msg {};
                if (KlvParser::parse(packetData, totalPacketSize, msg, false) == KlvStatus::Success) {
                    m_messageCallback(msg);
                }
            }

            packetsDispatched++;
            m_buffer.erase(m_buffer.begin(), m_buffer.begin() + totalPacketSize);
        } else {
            // CRC mismatch: false positive header or corrupted packet; advance by 1 byte
            m_buffer.erase(m_buffer.begin());
        }
    }

    return packetsDispatched;
}

} // namespace Klv
