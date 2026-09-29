#pragma once

/// @file NmeaTagBlockParser.h
/// @brief Zero-allocation parser for NMEA 0183 v4.00+ / IEC 61162-1 Tag Blocks.

#include "NmeaTypes.h"
#include <string_view>

namespace Nmea {

/// @class NmeaTagBlockParser
/// @brief Validates and unpacks metadata parameters from tag block headers.
class NmeaTagBlockParser {
public:
    /// @brief Checks if a raw string view starts with a tag block opening delimiter ('\\').
    /// @param[in] raw Input string view.
    /// @return True if the string starts with '\\'.
    [[nodiscard]] static bool hasTagBlock(std::string_view raw) noexcept;

    /// @brief Parses an optional tag block from the beginning of an NMEA raw sentence.
    /// @param[in] raw Raw sentence potentially starting with \\s:...,c:...*hh\\...
    /// @param[out] outBlock Parsed metadata fields.
    /// @param[out] outSentenceRemainder Substring containing the remainder of the sentence ($... or !...).
    /// @return True if a valid tag block was parsed and validated against its checksum.
    [[nodiscard]] static bool parse(
        std::string_view raw, NmeaTagBlock& outBlock, std::string_view& outSentenceRemainder) noexcept;
};

} // namespace Nmea
