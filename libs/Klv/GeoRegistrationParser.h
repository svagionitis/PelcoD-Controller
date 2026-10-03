#pragma once

/// @file GeoRegistrationParser.h
/// @brief MISB ST 1601.2 Geo-Registration Local Set parser.

#include "GeoRegistrationTypes.h"
#include "KlvTypes.h"

#include <cstddef>
#include <cstdint>

namespace Klv {

/// @class GeoRegistrationParser
/// @brief Parses MISB ST 1601 Geo-Registration Local Set packets and embedded sets.
class GeoRegistrationParser {
public:
    /// @brief Checks whether the given buffer begins with the MISB ST 1601 Universal Label.
    /// @param[in] data Pointer to memory buffer.
    /// @param[in] size Size of the buffer in bytes.
    /// @return True if buffer starts with MISB ST 1601 UL; false otherwise.
    [[nodiscard]] static bool isGeoRegistration(const std::uint8_t* data, std::size_t size) noexcept;

    /// @brief Parses an embedded Geo-Registration Local Set (without 16-byte UL).
    /// @details Designed for parsing Tag 98 within MISB ST 0601 Amend Local Set (Tag 101).
    /// @param[in] data Pointer to local set TLV payload.
    /// @param[in] size Number of bytes in payload.
    /// @param[out] outSet Decoded GeoRegistrationLocalSet structure.
    /// @return KlvStatus code indicating outcome of operation.
    [[nodiscard]] static KlvStatus parse(const std::uint8_t* data,
                                         std::size_t size,
                                         GeoRegistrationLocalSet& outSet) noexcept;

    /// @brief Parses a standalone Geo-Registration Local Set packet (with 16-byte UL).
    /// @param[in] data Pointer to raw KLV packet starting with 16-byte UL.
    /// @param[in] size Number of bytes available.
    /// @param[out] outSet Decoded GeoRegistrationLocalSet structure.
    /// @return KlvStatus code indicating outcome of operation.
    [[nodiscard]] static KlvStatus parsePacket(const std::uint8_t* data,
                                               std::size_t size,
                                               GeoRegistrationLocalSet& outSet) noexcept;
};

} // namespace Klv
