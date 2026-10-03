#pragma once

/// @file SightlineKlvParser.h
/// @brief Deserializer for Sightline KLV metadata responses (IDD KLV Metadata module).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__klv.html
/// @see https://knowledge.sightlineintelligence.com/wp-content/uploads/EAN-KLV-Metadata.pdf

#include "SightlineFraming.h"
#include "SightlineKlv.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineKlvParser
/// @brief Parses platform telemetry, MISB metadata values, and KLV responses.
class SightlineKlvParser {
public:
    /// @brief Parses KLV metadata values (Message ID 0x13 or 0x8B).
    /// @details Extracts 44-byte SLASetMetadataValues_t or 42-byte SLACurrentMetadataValues_t.
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized metadata values structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseMetadataValues(
        const std::vector<std::uint8_t>& packet, MsgSetMetadataValues& out);

    /// @brief Parses current KLV metadata values (Message ID 0x8B or 0x13).
    /// @details Deserializes SLACurrentMetadataValues_t (42-byte payload).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized current metadata values structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseCurrentValues(
        const std::vector<std::uint8_t>& packet, MsgCurrentMetadataValues& out);

    /// @brief Parses static metadata values reply (Message ID 0x14).
    /// @details Deserializes SLAMetadataStaticValues_t.
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized static metadata structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseMetadataStaticValues(
        const std::vector<std::uint8_t>& packet, MsgMetadataStaticValues& out);

    /// @brief Parses frame metadata values (Message ID 0x15 or 0x8C).
    /// @details Deserializes 49-byte SLASetMetadataFrameValues_t.
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized frame metadata structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseFrameValues(
        const std::vector<std::uint8_t>& packet, MsgSetMetadataFrameValues& out);

    /// @brief Parses current frame metadata values (Message ID 0x8C or 0x15).
    /// @details Deserializes 47-byte SLACurrentMetadataFrameValues_t.
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized current frame values structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseCurrentFrameValues(
        const std::vector<std::uint8_t>& packet, MsgCurrentMetadataFrameValues& out);

    /// @brief Parses metadata rate reply (Message ID 0x62 or 0x8D).
    /// @details Deserializes SLASetMetadataRate_t (11-byte payload).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized metadata rate structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseMetadataRate(
        const std::vector<std::uint8_t>& packet, MsgSetMetadataRate& out);

    /// @brief Parses current metadata rate query reply (Message ID 0x8D).
    /// @details Deserializes SLACurrentMetadataRate_t (4-byte payload).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized current metadata rate structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseCurrentRate(
        const std::vector<std::uint8_t>& packet, MsgCurrentMetadataRate& out);

    /// @brief Parses TagData message (Message ID 0x96).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized TagData structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseTagData(
        const std::vector<std::uint8_t>& packet, MsgTagData& out);

    /// @brief Parses TagDataRate message (Message ID 0x97).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized TagDataRate structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseTagDataRate(
        const std::vector<std::uint8_t>& packet, MsgTagDataRate& out);

    /// @brief Parses TagSourceSelector message (Message ID 0x98).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized TagSourceSelector structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseTagSourceSelector(
        const std::vector<std::uint8_t>& packet, MsgTagSourceSelector& out);

    /// @brief Parses CursorOnTarget configuration (Message ID 0xB0).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized CursorOnTarget structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseCursorOnTarget(
        const std::vector<std::uint8_t>& packet, MsgCursorOnTarget& out);

    /// @brief Parses VMTI chips configuration (Message ID 0xAD).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized VmtiChips structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseVmtiChips(
        const std::vector<std::uint8_t>& packet, MsgVmtiChips& out);

    /// @brief Parses VMTI fields configuration (Message ID 0xBF).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized VmtiFields structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseVmtiFields(
        const std::vector<std::uint8_t>& packet, MsgVmtiFields& out);

    /// @brief Parses Ancillary text metadata (Message ID 0xAC).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized AncillaryTextMetadata structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseAncillaryText(
        const std::vector<std::uint8_t>& packet, MsgAncillaryTextMetadata& out);

    /// @brief Parses appended user metadata (Message ID 0x89).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized AppendedMetadata structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseAppendedMetadata(
        const std::vector<std::uint8_t>& packet, MsgAppendedMetadata& out);
};

} // namespace Sightline
