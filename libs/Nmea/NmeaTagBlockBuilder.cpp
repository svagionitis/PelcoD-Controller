/// @file NmeaTagBlockBuilder.cpp
/// @brief Implementation of NMEA Tag Block builder.

#include "NmeaTagBlockBuilder.h"
#include "NmeaChecksum.h"

#include <iomanip>
#include <sstream>

namespace Nmea {

std::string NmeaTagBlockBuilder::build(const NmeaTagBlock& block)
{
    std::string payload {};
    payload.reserve(64U);

    bool first = true;
    auto appendTag = [&](std::string_view key, std::string_view val) {
        if (val.empty()) {
            return;
        }
        if (!first) {
            payload.push_back(',');
        }
        payload.append(key);
        payload.push_back(':');
        payload.append(val);
        first = false;
    };

    if (!block.sourceId.empty()) {
        appendTag("s", block.sourceId);
    }
    if (block.timestampEpochSec > 0ULL) {
        appendTag("c", std::to_string(block.timestampEpochSec));
    }
    if (!block.grouping.empty()) {
        appendTag("g", block.grouping);
    }
    if (block.lineCount > 0U) {
        appendTag("n", std::to_string(block.lineCount));
    }
    if (!block.destinationId.empty()) {
        appendTag("d", block.destinationId);
    }
    if (!block.text.empty()) {
        appendTag("t", block.text);
    }

    if (payload.empty()) {
        return {};
    }

    std::uint8_t crc { 0U };
    for (const char c : payload) {
        crc ^= static_cast<std::uint8_t>(c);
    }

    std::string result {};
    result.reserve(payload.size() + 6U);
    result.push_back('\\');
    result.append(payload);
    result.push_back('*');
    result.append(NmeaChecksum::toHexString(crc));
    result.push_back('\\');

    return result;
}

std::string NmeaTagBlockBuilder::wrapSentence(const NmeaTagBlock& block, std::string_view sentence)
{
    const std::string tb = build(block);
    if (tb.empty()) {
        return std::string(sentence);
    }
    std::string result = tb;
    result.append(sentence);
    return result;
}

} // namespace Nmea
