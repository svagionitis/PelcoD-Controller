/// @file NmeaTagBlockParser.cpp
/// @brief Implementation of NMEA 0183 / IEC 61162-1 Tag Block parser.

#include "NmeaTagBlockParser.h"
#include "NmeaChecksum.h"

#include <charconv>

namespace Nmea {

bool NmeaTagBlockParser::hasTagBlock(std::string_view raw) noexcept
{
    return !raw.empty() && raw.front() == '\\';
}

bool NmeaTagBlockParser::parse(
    std::string_view raw, NmeaTagBlock& outBlock, std::string_view& outSentenceRemainder) noexcept
{
    outBlock = {};
    outSentenceRemainder = {};

    if (!hasTagBlock(raw)) {
        return false;
    }

    // Must find closing backslash after the checksum asterisk
    const auto secondBackslashPos = raw.find('\\', 1U);
    if (secondBackslashPos == std::string_view::npos) {
        return false;
    }

    const auto blockContent = raw.substr(1U, secondBackslashPos - 1U);
    const auto starPos = blockContent.rfind('*');
    if (starPos == std::string_view::npos || starPos + 3U != blockContent.size()) {
        return false; // Asterisk missing or not followed by exactly 2 hex chars
    }

    const auto payload = blockContent.substr(0U, starPos);
    const char hi = blockContent[starPos + 1U];
    const char lo = blockContent[starPos + 2U];

    std::uint8_t expectedCrc { 0U };
    if (!NmeaChecksum::parseHexByte(hi, lo, expectedCrc)) {
        return false;
    }

    std::uint8_t calculatedCrc { 0U };
    for (const char c : payload) {
        calculatedCrc ^= static_cast<std::uint8_t>(c);
    }

    if (calculatedCrc != expectedCrc) {
        return false;
    }

    // Parse comma-separated key:value pairs inside payload
    std::size_t start { 0U };
    while (start < payload.size()) {
        auto nextComma = payload.find(',', start);
        if (nextComma == std::string_view::npos) {
            nextComma = payload.size();
        }

        const auto token = payload.substr(start, nextComma - start);
        start = nextComma + 1U;

        const auto colonPos = token.find(':');
        if (colonPos == std::string_view::npos || colonPos == 0U) {
            continue;
        }

        const auto key = token.substr(0U, colonPos);
        const auto val = token.substr(colonPos + 1U);

        if (key == "s") {
            outBlock.sourceId = std::string(val);
        } else if (key == "c") {
            std::uint64_t epoch { 0ULL };
            const auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), epoch);
            if (ec == std::errc()) {
                outBlock.timestampEpochSec = epoch;
            }
        } else if (key == "g") {
            outBlock.grouping = std::string(val);
        } else if (key == "n") {
            std::uint32_t line { 0U };
            const auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), line);
            if (ec == std::errc()) {
                outBlock.lineCount = line;
            }
        } else if (key == "d") {
            outBlock.destinationId = std::string(val);
        } else if (key == "t") {
            outBlock.text = std::string(val);
        }
    }

    outBlock.valid = true;
    outSentenceRemainder = raw.substr(secondBackslashPos + 1U);
    while (!outSentenceRemainder.empty()
        && (outSentenceRemainder.front() == ' ' || outSentenceRemainder.front() == '\t')) {
        outSentenceRemainder.remove_prefix(1U);
    }

    return true;
}

} // namespace Nmea
