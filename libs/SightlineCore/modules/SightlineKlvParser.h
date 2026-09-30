#pragma once

/// @file SightlineKlvParser.h
/// @brief Deserializer for Sightline KLV metadata responses (IDD KLV Metadata module).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__klv.html

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineKlvParser
/// @brief Parses platform telemetry, MISB metadata values, and KLV responses.
class SightlineKlvParser {
public:
    /// @brief Parses current KLV metadata values reply (Message ID 0x13 / 0x8B).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized metadata values structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseMetadataValues(
        const std::vector<std::uint8_t>& packet, MsgSetMetadataValues& out);

    /// @brief Parses static metadata values reply (Message ID 0x14).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized static metadata structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseMetadataStaticValues(
        const std::vector<std::uint8_t>& packet, MsgMetadataStaticValues& out);

    /// @brief Parses metadata rate reply (Message ID 0x62 / 0x8D).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized metadata rate structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseMetadataRate(
        const std::vector<std::uint8_t>& packet, MsgSetMetadataRate& out);
};

} // namespace Sightline
