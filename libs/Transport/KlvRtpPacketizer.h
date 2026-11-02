#pragma once

/// @file KlvRtpPacketizer.h
/// @brief Zero-allocation RFC 6597 RTP packetizer for SMPTE ST 336 KLV frames.
/// @see IETF RFC 6597 "RTP Payload Format for SMPTE ST 336 KLV Data"
/// @see IETF RFC 3550 Section 5.1

#include <cstddef>
#include <cstdint>
#include <functional>

#include "KlvRtpTypes.h"

namespace Transport {

/// @class KlvRtpPacketizer
/// @brief Slices arbitrary SMPTE ST 336 KLV payloads into standards-compliant RFC 6597 RTP datagrams.
class KlvRtpPacketizer {
public:
    /// @brief Standard 12-byte fixed RTP header size per RFC 3550 Section 5.1.
    static constexpr std::size_t kRtpHeaderSize { 12U };

    /// @brief Maximum supported single datagram buffer size on stack.
    static constexpr std::size_t kMaxPacketBufferSize { 2048U };

    /// @brief Callback consuming an assembled RTP packet (header + payload slice).
    using PacketConsumer = std::function<void(const std::uint8_t* packetData, std::size_t packetSize)>;

    /// @brief Constructs packetizer with dynamic payload type, SSRC, and max payload slice size.
    /// @param[in] pt Dynamic payload type (typically 96).
    /// @param[in] ssrc Synchronization source identifier.
    /// @param[in] maxPayloadSize Maximum KLV payload bytes per RTP packet before fragmenting (default 1400).
    explicit KlvRtpPacketizer(DynamicPayloadType pt = DynamicPayloadType(96U), SsrcId ssrc = SsrcId(0x12345678U),
        std::size_t maxPayloadSize = 1400U) noexcept;

    /// @brief Slices and packages a KLV frame into one or more RFC 6597 RTP packets.
    /// @param[in] klvData Pointer to raw SMPTE ST 336 KLV frame bytes.
    /// @param[in] klvSize Length of KLV frame in bytes.
    /// @param[in] timestamp 90 kHz RTP presentation timestamp.
    /// @param[in,out] inOutSeq In-out sequence number advanced by 1 for each emitted packet.
    /// @param[in] consumer Functor called once per generated RTP packet.
    /// @return Number of RTP packets generated.
    std::size_t packetize(const std::uint8_t* klvData, std::size_t klvSize, RtpTimestamp90kHz timestamp,
        RtpSequenceNumber& inOutSeq, const PacketConsumer& consumer);

    /// @brief Returns the configured max payload size.
    [[nodiscard]] std::size_t maxPayloadSize() const noexcept;

    /// @brief Returns the configured dynamic payload type.
    [[nodiscard]] DynamicPayloadType payloadType() const noexcept;

    /// @brief Returns the configured SSRC.
    [[nodiscard]] SsrcId ssrc() const noexcept;

private:
    DynamicPayloadType m_payloadType { DynamicPayloadType(96U) };
    SsrcId m_ssrc { SsrcId(0x12345678U) };
    std::size_t m_maxPayloadSize { 1400U };
};

} // namespace Transport
