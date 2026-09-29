#pragma once

/// @file NmeaTagBlockBuilder.h
/// @brief Formatter for NMEA 0183 v4.00+ / IEC 61162-1 Tag Blocks.

#include "NmeaTypes.h"

#include <string>
#include <string_view>

namespace Nmea {

/// @class NmeaTagBlockBuilder
/// @brief Serializes NmeaTagBlock metadata into standard NMEA tag block strings with XOR checksums.
class NmeaTagBlockBuilder {
public:
    /// @brief Builds a standalone tag block string including delimiters and checksum.
    /// @param[in] block Tag block metadata fields.
    /// @return Formatted tag block string (e.g. "\\s:station,c:1609459200*3A\\").
    [[nodiscard]] static std::string build(const NmeaTagBlock& block);

    /// @brief Wraps an existing NMEA sentence with a tag block header.
    /// @param[in] block Tag block metadata fields.
    /// @param[in] sentence Standard NMEA sentence ($... or !...).
    /// @return Combined sentence string (e.g. "\\s:station*3A\\$GPGGA,...").
    [[nodiscard]] static std::string wrapSentence(const NmeaTagBlock& block, std::string_view sentence);
};

} // namespace Nmea
