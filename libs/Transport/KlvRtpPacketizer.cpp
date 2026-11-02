/// @file KlvRtpPacketizer.cpp
/// @brief Implementation of RFC 6597 RTP packetizer for SMPTE ST 336 KLV frames.

#include "KlvRtpPacketizer.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>

#include "KlvRtpTypes.h"

namespace Transport {

KlvRtpPacketizer::KlvRtpPacketizer(DynamicPayloadType pt, SsrcId ssrc, std::size_t maxPayloadSize) noexcept
    : m_payloadType(pt)
    , m_ssrc(ssrc)
    , m_maxPayloadSize(maxPayloadSize > 0U ? std::min(maxPayloadSize, kMaxPacketBufferSize - kRtpHeaderSize) : 1400U)
{
}

std::size_t KlvRtpPacketizer::packetize(const std::uint8_t* klvData, std::size_t klvSize, RtpTimestamp90kHz timestamp,
    RtpSequenceNumber& inOutSeq, const PacketConsumer& consumer)
{
    if (klvData == nullptr || klvSize == 0U) {
        return 0U;
    }

    std::array<std::uint8_t, kMaxPacketBufferSize> buffer {};
    std::size_t bytesRemaining { klvSize };
    std::size_t offset { 0U };
    std::size_t packetCount { 0U };

    while (bytesRemaining > 0U) {
        const std::size_t chunkSize = std::min(bytesRemaining, m_maxPayloadSize);
        const bool isLast = (chunkSize == bytesRemaining);
        const RtpSequenceNumber currentSeq = inOutSeq;
        inOutSeq = inOutSeq.next();

        // 12-byte RFC 3550 RTP Header
        buffer[0] = 0x80U; // V=2, P=0, X=0, CC=0
        buffer[1] = static_cast<std::uint8_t>((isLast ? 0x80U : 0x00U) | (m_payloadType.value() & 0x7FU));
        buffer[2] = static_cast<std::uint8_t>((currentSeq.value() >> 8U) & 0xFFU);
        buffer[3] = static_cast<std::uint8_t>(currentSeq.value() & 0xFFU);
        buffer[4] = static_cast<std::uint8_t>((timestamp.value() >> 24U) & 0xFFU);
        buffer[5] = static_cast<std::uint8_t>((timestamp.value() >> 16U) & 0xFFU);
        buffer[6] = static_cast<std::uint8_t>((timestamp.value() >> 8U) & 0xFFU);
        buffer[7] = static_cast<std::uint8_t>(timestamp.value() & 0xFFU);
        buffer[8] = static_cast<std::uint8_t>((m_ssrc.value() >> 24U) & 0xFFU);
        buffer[9] = static_cast<std::uint8_t>((m_ssrc.value() >> 16U) & 0xFFU);
        buffer[10] = static_cast<std::uint8_t>((m_ssrc.value() >> 8U) & 0xFFU);
        buffer[11] = static_cast<std::uint8_t>(m_ssrc.value() & 0xFFU);

        // Copy chunk payload
        std::memcpy(buffer.data() + kRtpHeaderSize, klvData + offset, chunkSize);

        consumer(buffer.data(), kRtpHeaderSize + chunkSize);

        offset += chunkSize;
        bytesRemaining -= chunkSize;
        ++packetCount;
    }

    return packetCount;
}

std::size_t KlvRtpPacketizer::maxPayloadSize() const noexcept
{
    return m_maxPayloadSize;
}

DynamicPayloadType KlvRtpPacketizer::payloadType() const noexcept
{
    return m_payloadType;
}

SsrcId KlvRtpPacketizer::ssrc() const noexcept
{
    return m_ssrc;
}

} // namespace Transport
