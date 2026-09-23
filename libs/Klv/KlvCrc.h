#pragma once

/// @file KlvCrc.h
/// @brief MISB ST 0601 CRC-16-CCITT checksum calculation and verification engine.

#include <cstddef>
#include <cstdint>

namespace Klv {

/// @class KlvCrc
/// @brief Computes and validates 16-bit CRC-CCITT (polynomial 0x1021) for MISB ST 0601 packets.
class KlvCrc {
public:
    /// @brief Computes standard CCITT 16-bit CRC over a byte sequence.
    /// @details Polynomial: x^16 + x^12 + x^5 + 1 (0x1021), initial value 0x0000.
    /// @param[in] data Pointer to the buffer.
    /// @param[in] size Size of the buffer in bytes.
    /// @return 16-bit calculated CRC value.
    [[nodiscard]] static std::uint16_t calculate(const std::uint8_t* data, std::size_t size) noexcept;

    /// @brief Validates a complete MISB ST 0601 packet including its appended Tag 1 CRC bytes.
    /// @param[in] packet Pointer to the start of the packet (starting with 16-byte UL).
    /// @param[in] packetSize Total size of the packet in bytes up to and including the 2-byte CRC.
    /// @return True if the packet checksum matches and packet remainder is zero; false otherwise.
    [[nodiscard]] static bool verifyPacket(const std::uint8_t* packet, std::size_t packetSize) noexcept;

    /// @brief Computes the 2-byte checksum to append to Tag 1 (0x01 0x02).
    /// @param[in] packetWithoutCrc Pointer to the packet up to and including the Tag 1 header (0x01 0x02).
    /// @param[in] size Size of the packet including the 0x01 0x02 Tag 1 header.
    /// @return 16-bit big-endian CRC value to append.
    [[nodiscard]] static std::uint16_t computeChecksumForTag1(const std::uint8_t* packetWithoutCrc,
                                                             std::size_t size) noexcept;
};

} // namespace Klv
