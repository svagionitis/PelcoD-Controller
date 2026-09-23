#pragma once

/// @file KlvParser.h
/// @brief STANAG 4609 / MISB ST 0601 KLV metadata parser and deserializer.

#include "KlvTypes.h"
#include <cstddef>
#include <cstdint>

namespace Klv {

/// @class KlvParser
/// @brief Validates and unpacks SMPTE ST 336 / MISB ST 0601 byte packets into strongly-typed telemetry structures.
class KlvParser {
public:
    /// @brief Checks if a buffer starts with a valid MISB ST 0601 16-byte Universal Label.
    /// @param[in] data Pointer to the buffer.
    /// @param[in] size Available buffer size in bytes.
    /// @return True if the buffer starts with the MISB ST 0601 prefix; false otherwise.
    [[nodiscard]] static bool isMisb0601(const std::uint8_t* data, std::size_t size) noexcept;

    /// @brief Parses a complete STANAG 4609 / MISB ST 0601 KLV packet into a UasDatalinkMessage.
    /// @param[in] data Pointer to the start of the KLV packet (starting with the 16-byte Universal Label).
    /// @param[in] size Total size of the buffer in bytes.
    /// @param[out] message Output struct where parsed telemetry values will be stored.
    /// @param[in] verifyChecksum If true, verifies the Tag 1 CRC-16 checksum before parsing.
    /// @return KlvStatus::Success if parsed correctly, or an error status code.
    [[nodiscard]] static KlvStatus parse(const std::uint8_t* data,
                                         std::size_t size,
                                         UasDatalinkMessage& message,
                                         bool verifyChecksum = true);

    /// @brief Parses the nested MISB ST 0102 Security Classification Local Set (Tag 48).
    /// @param[in] data Pointer to the start of the inner Tag 48 payload.
    /// @param[in] size Length of the inner payload in bytes.
    /// @param[out] security Output security metadata struct.
    /// @return KlvStatus::Success if parsed successfully; error otherwise.
    [[nodiscard]] static KlvStatus parseSecurityLocalSet(const std::uint8_t* data,
                                                         std::size_t size,
                                                         SecurityMetadata& security);
};

} // namespace Klv
