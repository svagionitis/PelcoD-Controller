#pragma once

/// @file KlvBer.h
/// @brief Basic Encoding Rules (BER) length and OID tag serialization and deserialization.

#include "KlvTypes.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Klv {

/// @class KlvBer
/// @brief Utility class providing ITU-T X.690 / SMPTE ST 336 BER length and tag processing.
class KlvBer {
public:
    /// @brief Computes the number of bytes required to encode a length value in BER format.
    /// @param[in] length Length value to encode.
    /// @return Number of bytes (1 for short form <128, 2-5 for long form).
    [[nodiscard]] static std::size_t encodedLengthSize(std::size_t length) noexcept;

    /// @brief Encodes a length value using BER (short or long form).
    /// @param[in] length Length value to encode.
    /// @param[out] out Destination vector where BER bytes will be appended.
    static void encodeLength(std::size_t length, std::vector<std::uint8_t>& out);

    /// @brief Decodes a BER length from a memory buffer.
    /// @param[in] data Pointer to the start of the BER length bytes.
    /// @param[in] size Remaining buffer size available in bytes.
    /// @param[out] decodedLength Output parameter receiving the decoded length.
    /// @param[out] bytesConsumed Output parameter receiving the number of bytes read (1..5).
    /// @return True if decoding succeeded without overflow or underflow; false otherwise.
    [[nodiscard]] static bool decodeLength(const std::uint8_t* data,
                                           std::size_t size,
                                           std::size_t& decodedLength,
                                           std::size_t& bytesConsumed) noexcept;

    /// @brief Computes the number of bytes required to encode a tag in BER-OID format.
    /// @param[in] tag Tag identifier.
    /// @return Number of bytes (1 for tag < 128, 2+ for tag >= 128).
    [[nodiscard]] static std::size_t encodedTagSize(std::uint32_t tag) noexcept;

    /// @brief Encodes a tag value using BER-OID format.
    /// @param[in] tag Tag identifier.
    /// @param[out] out Destination vector where BER-OID bytes will be appended.
    static void encodeTag(std::uint32_t tag, std::vector<std::uint8_t>& out);

    /// @brief Decodes a BER-OID tag from a memory buffer.
    /// @param[in] data Pointer to the start of the tag bytes.
    /// @param[in] size Remaining buffer size available in bytes.
    /// @param[out] decodedTag Output parameter receiving the decoded tag ID.
    /// @param[out] bytesConsumed Output parameter receiving the number of bytes read.
    /// @return True if decoding succeeded; false otherwise.
    [[nodiscard]] static bool decodeTag(const std::uint8_t* data,
                                        std::size_t size,
                                        std::uint32_t& decodedTag,
                                        std::size_t& bytesConsumed) noexcept;
};

} // namespace Klv
