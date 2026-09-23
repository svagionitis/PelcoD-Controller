#pragma once

/// @file KlvEncoder.h
/// @brief MISB ST 0601 UAS Datalink Local Set serializer and packet generator.

#include "KlvTypes.h"
#include <cstdint>
#include <vector>

namespace Klv {

/// @class KlvEncoder
/// @brief Encodes high-level telemetry and geodetic measurements into standard SMPTE ST 336 / MISB ST 0601 byte packets.
class KlvEncoder {
public:
    /// @brief Encodes a UAS Datalink message into a complete, validated STANAG 4609 / MISB ST 0601 KLV packet.
    /// @param[in] message Strongly-typed telemetry message data.
    /// @return Byte vector containing the 16-byte UL, BER length, payload tags, and Tag 1 CRC-16 checksum.
    [[nodiscard]] static std::vector<std::uint8_t> encode(const UasDatalinkMessage& message);

    /// @brief Encodes the nested MISB ST 0102 Security Classification Local Set (Tag 48).
    /// @param[in] security Security classification and caveats structure.
    /// @return Serialized inner byte vector for Tag 48 value.
    [[nodiscard]] static std::vector<std::uint8_t> encodeSecurityLocalSet(const SecurityMetadata& security);

private:
    static void appendTagUint8(std::uint32_t tag, std::uint8_t value, std::vector<std::uint8_t>& out);
    static void appendTagUint16(std::uint32_t tag, std::uint16_t value, std::vector<std::uint8_t>& out);
    static void appendTagInt16(std::uint32_t tag, std::int16_t value, std::vector<std::uint8_t>& out);
    static void appendTagUint32(std::uint32_t tag, std::uint32_t value, std::vector<std::uint8_t>& out);
    static void appendTagInt32(std::uint32_t tag, std::int32_t value, std::vector<std::uint8_t>& out);
    static void appendTagUint64(std::uint32_t tag, std::uint64_t value, std::vector<std::uint8_t>& out);
    static void appendTagString(std::uint32_t tag, const std::string& value, std::vector<std::uint8_t>& out);
    static void appendTagBytes(std::uint32_t tag, const std::vector<std::uint8_t>& value, std::vector<std::uint8_t>& out);
};

} // namespace Klv
