#pragma once

/// @file GeoRegistrationEncoder.h
/// @brief MISB ST 1601.2 Geo-Registration Local Set encoder.

#include "GeoRegistrationTypes.h"
#include "KlvTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Klv {

/// @class GeoRegistrationEncoder
/// @brief Serializes GeoRegistrationLocalSet structures into MISB ST 1601 binary streams.
class GeoRegistrationEncoder {
public:
    /// @brief Encodes an embedded Geo-Registration Local Set (without 16-byte UL).
    /// @details Designed for embedding into MISB ST 0601 Amend Local Set (Tag 101, Sub-Tag 98).
    /// @param[in] set GeoRegistrationLocalSet to serialize.
    /// @param[out] out Destination vector to append encoded TLV bytes.
    /// @return KlvStatus::Success on success, or error status code on validation failure.
    [[nodiscard]] static KlvStatus encode(const GeoRegistrationLocalSet& set,
                                          std::vector<std::uint8_t>& out);

    /// @brief Encodes a standalone Geo-Registration Local Set packet (with 16-byte UL and length).
    /// @param[in] set GeoRegistrationLocalSet to serialize.
    /// @param[out] out Destination vector to append full KLV packet bytes.
    /// @return KlvStatus::Success on success, or error status code on validation failure.
    [[nodiscard]] static KlvStatus encodePacket(const GeoRegistrationLocalSet& set,
                                                std::vector<std::uint8_t>& out);
};

} // namespace Klv
