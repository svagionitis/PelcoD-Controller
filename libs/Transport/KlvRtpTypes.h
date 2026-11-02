#pragma once

/// @file KlvRtpTypes.h
/// @brief Strongly-typed domain value objects and configuration for RFC 6597 Standalone KLV over RTP.
/// @see IETF RFC 6597 "RTP Payload Format for SMPTE ST 336 KLV Data"
/// @see IETF RFC 3550 "RTP: A Transport Protocol for Real-Time Applications"

#include <cstddef>
#include <cstdint>
#include <string>

namespace Transport {

/// @class RtpSequenceNumber
/// @brief Strongly-typed 16-bit RTP Sequence Number with safe monotonic wrapping.
class RtpSequenceNumber {
public:
    /// @brief Constructs sequence number with initial value.
    /// @param[in] val Initial sequence number.
    constexpr explicit RtpSequenceNumber(std::uint16_t val = 0U) noexcept
        : m_val(val)
    {
    }

    /// @brief Returns underlying 16-bit sequence number.
    [[nodiscard]] constexpr std::uint16_t value() const noexcept
    {
        return m_val;
    }

    /// @brief Generates next sequence number advancing by 1 with modulo 65536 rollover.
    [[nodiscard]] constexpr RtpSequenceNumber next() const noexcept
    {
        return RtpSequenceNumber(static_cast<std::uint16_t>(m_val + 1U));
    }

    /// @brief Equality comparison operator.
    [[nodiscard]] constexpr bool operator==(const RtpSequenceNumber& other) const noexcept
    {
        return m_val == other.m_val;
    }

    /// @brief Inequality comparison operator.
    [[nodiscard]] constexpr bool operator!=(const RtpSequenceNumber& other) const noexcept
    {
        return m_val != other.m_val;
    }

private:
    std::uint16_t m_val { 0U };
};

/// @class RtpTimestamp90kHz
/// @brief Strongly-typed 32-bit RTP presentation timestamp operating on a 90 kHz clock.
class RtpTimestamp90kHz {
public:
    /// @brief Constructs timestamp with raw 32-bit tick value.
    /// @param[in] val Initial 90 kHz tick count.
    constexpr explicit RtpTimestamp90kHz(std::uint32_t val = 0U) noexcept
        : m_val(val)
    {
    }

    /// @brief Returns raw 32-bit tick value.
    [[nodiscard]] constexpr std::uint32_t value() const noexcept
    {
        return m_val;
    }

    /// @brief Converts microseconds since epoch (e.g. MISB ST 0601 Tag 2) to 90 kHz RTP ticks.
    /// @param[in] timeUs Microsecond epoch timestamp.
    /// @return Converted 32-bit 90 kHz timestamp with modulo 2^32 wrapping.
    [[nodiscard]] static constexpr RtpTimestamp90kHz fromMicroseconds(std::uint64_t timeUs) noexcept
    {
        const std::uint64_t div = timeUs / 100ULL;
        const std::uint64_t rem = timeUs % 100ULL;
        const std::uint64_t ticks = (div * 9ULL) + ((rem * 9ULL) / 100ULL);
        return RtpTimestamp90kHz(static_cast<std::uint32_t>(ticks & 0xFFFFFFFFULL));
    }

    /// @brief Equality comparison operator.
    [[nodiscard]] constexpr bool operator==(const RtpTimestamp90kHz& other) const noexcept
    {
        return m_val == other.m_val;
    }

    /// @brief Inequality comparison operator.
    [[nodiscard]] constexpr bool operator!=(const RtpTimestamp90kHz& other) const noexcept
    {
        return m_val != other.m_val;
    }

private:
    std::uint32_t m_val { 0U };
};

/// @class SsrcId
/// @brief Strongly-typed 32-bit Synchronization Source Identifier per RFC 3550.
class SsrcId {
public:
    /// @brief Constructs SSRC identifier.
    /// @param[in] val Raw 32-bit SSRC value.
    constexpr explicit SsrcId(std::uint32_t val = 0x12345678U) noexcept
        : m_val(val)
    {
    }

    /// @brief Returns raw SSRC value.
    [[nodiscard]] constexpr std::uint32_t value() const noexcept
    {
        return m_val;
    }

    /// @brief Equality comparison operator.
    [[nodiscard]] constexpr bool operator==(const SsrcId& other) const noexcept
    {
        return m_val == other.m_val;
    }

    /// @brief Inequality comparison operator.
    [[nodiscard]] constexpr bool operator!=(const SsrcId& other) const noexcept
    {
        return m_val != other.m_val;
    }

private:
    std::uint32_t m_val { 0x12345678U };
};

/// @class DynamicPayloadType
/// @brief Strongly-typed 7-bit Dynamic Payload Type (96–127) per RFC 3550.
class DynamicPayloadType {
public:
    /// @brief Constructs payload type with default dynamic type 96.
    /// @param[in] val Payload type number (clamped to 96-127).
    constexpr explicit DynamicPayloadType(std::uint8_t val = 96U) noexcept
        : m_val((val >= 96U && val <= 127U) ? val : 96U)
    {
    }

    /// @brief Returns raw payload type integer.
    [[nodiscard]] constexpr std::uint8_t value() const noexcept
    {
        return m_val;
    }

    /// @brief Equality comparison operator.
    [[nodiscard]] constexpr bool operator==(const DynamicPayloadType& other) const noexcept
    {
        return m_val == other.m_val;
    }

    /// @brief Inequality comparison operator.
    [[nodiscard]] constexpr bool operator!=(const DynamicPayloadType& other) const noexcept
    {
        return m_val != other.m_val;
    }

private:
    std::uint8_t m_val { 96U };
};

/// @struct KlvRtpConfig
/// @brief Configuration settings for RFC 6597 Standalone KLV over RTP publisher.
struct KlvRtpConfig {
    std::string destinationIp { "127.0.0.1" }; ///< Target IP address (unicast or multicast)
    std::uint16_t destinationPort { 5004U }; ///< Outbound UDP port (standard KLV RTP port 5004)
    DynamicPayloadType payloadType { DynamicPayloadType(96U) }; ///< Dynamic payload type (default 96)
    SsrcId ssrc { SsrcId(0x12345678U) }; ///< Synchronization Source identifier
    std::size_t maxPayloadSize { 1400U }; ///< Maximum payload bytes per packet before fragmentation
    bool enableMulticastLoopback { false }; ///< Multicast socket loopback flag
    std::uint8_t timeToLive { 64U }; ///< Multicast IP TTL
    std::string sessionName { "STANAG 4609 Standalone KLV Stream" }; ///< SDP session description name
};

/// @struct KlvRtpStats
/// @brief Operational statistics for KLV RTP transmission.
struct KlvRtpStats {
    std::uint64_t packetsSent { 0U }; ///< Total RTP packets emitted
    std::uint64_t bytesSent { 0U }; ///< Total bytes transmitted (including headers)
    std::uint64_t fragmentsSent { 0U }; ///< Total fragment packets emitted
    std::uint64_t messagesSent { 0U }; ///< Total logical KLV messages processed
};

} // namespace Transport
