#pragma once

/// @file SightlineCrc8.h
/// @brief 8-bit CRC calculation using the Sightline SLA protocol lookup polynomial.

#include <array>
#include <cstddef>
#include <cstdint>

namespace Sightline {

/// @class SightlineCrc8
/// @brief Implements the official CRC-8 calculation defined in IDD-SLA-Protocol_3_11_6.
class SightlineCrc8 {
public:
    /// @brief Initial seed value for Sightline CRC-8 calculation.
    static constexpr std::uint8_t InitialSeed { 0x01U };

    /// @brief Standard 256-byte lookup polynomial table.
    static const std::array<std::uint8_t, 256> Table;

    /// @brief Computes the CRC-8 checksum over a contiguous memory buffer.
    /// @param[in] data Pointer to byte sequence.
    /// @param[in] size Number of bytes.
    /// @param[in] seed Initial CRC seed (defaults to 0x01).
    /// @return 8-bit checksum value.
    [[nodiscard]] static std::uint8_t compute(
        const std::uint8_t* data, std::size_t size, std::uint8_t seed = InitialSeed) noexcept;

    /// @brief Validates that the checksum matches the trailing checksum byte.
    /// @param[in] data Pointer to byte sequence (excluding trailing checksum).
    /// @param[in] size Number of bytes in payload.
    /// @param[in] expectedChecksum The checksum byte to test against.
    /// @return True if computed checksum equals expectedChecksum.
    [[nodiscard]] static bool validate(
        const std::uint8_t* data, std::size_t size, std::uint8_t expectedChecksum) noexcept;
};

} // namespace Sightline
