#pragma once

/// @file SightlineKlvBuilder.h
/// @brief Serializer for Sightline KLV metadata and CoT broadcast packets (IDD KLV Metadata module).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__klv.html

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineKlvBuilder
/// @brief Encodes STANAG 4609 / MISB KLV platform telemetry, mission tags, rates, and Cursor-on-Target XML.
class SightlineKlvBuilder {
public:
    /// @brief Encodes platform position telemetry for KLV insertion (Message ID 0x13).
    /// @param[in] msg Metadata values.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMetadataValues(
        const MsgSetMetadataValues& msg);

    /// @brief Encodes static mission and classification metadata (Message ID 0x14).
    /// @param[in] msg Static metadata values.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildMetadataStaticValues(
        const MsgMetadataStaticValues& msg);

    /// @brief Encodes KLV transmission rate (Message ID 0x62).
    /// @param[in] msg Metadata rate parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMetadataRate(
        const MsgSetMetadataRate& msg);

    /// @brief Encodes Cursor-on-Target XML tactical broadcast (Message ID 0xB0).
    /// @param[in] msg CoT parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCursorOnTarget(
        const MsgCursorOnTarget& msg);

    /// @brief Encodes query for active metadata values (Message ID 0x28 query 0x13).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMetadataValues();

    /// @brief Encodes query for static metadata values (Message ID 0x28 query 0x14).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMetadataStaticValues();

    /// @brief Encodes query for metadata rate (Message ID 0x28 query 0x62).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMetadataRate();
};

} // namespace Sightline
