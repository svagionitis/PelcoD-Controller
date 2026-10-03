#pragma once

/// @file SightlineKlvBuilder.h
/// @brief Serializer for Sightline KLV metadata and CoT broadcast packets (IDD KLV Metadata module).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__klv.html
/// @see https://knowledge.sightlineintelligence.com/wp-content/uploads/EAN-KLV-Metadata.pdf

#include "SightlineFraming.h"
#include "SightlineKlv.h"
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

    /// @brief Encodes frame data values and OLS terrain modes (Message ID 0x15).
    /// @param[in] msg Frame metadata values.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMetadataFrameValues(
        const MsgSetMetadataFrameValues& msg);

    /// @brief Encodes custom KLV blob to inject directly into stream (Message ID 0x61).
    /// @param[in] msg KLV blob container.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetKlvData(
        const MsgSetKlvData& msg);

    /// @brief Encodes legacy KLV transmission rate (Message ID 0x62).
    /// @param[in] msg Metadata rate parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMetadataRate(
        const MsgSetMetadataRate& msg);

    /// @brief Encodes external targets to inject into VMTI Local Set Tag 74 (Message ID 0x84).
    /// @param[in] msg VMTI target container.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVmti(
        const MsgSetVmti& msg);

    /// @brief Encodes application-specific appended metadata Tag 100 (Message ID 0x89).
    /// @param[in] msg Appended metadata.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildAppendedMetadata(
        const MsgAppendedMetadata& msg);

    /// @brief Alias for buildAppendedMetadata.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetAppendedMetadata(
        const MsgAppendedMetadata& msg)
    {
        return buildAppendedMetadata(msg);
    }

    /// @brief Encodes custom MISB ST 0601 Tag value override (Message ID 0x96).
    /// @param[in] msg Tag data container.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildTagData(
        const MsgTagData& msg);

    /// @brief Encodes periodic broadcast rate for a Tag or Tag range (Message ID 0x97).
    /// @param[in] msg Tag data rate parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTagDataRate(
        const MsgTagDataRate& msg);

    /// @brief Alias for buildSetTagDataRate.
    [[nodiscard]] static std::vector<std::uint8_t> buildTagDataRate(
        const MsgTagDataRate& msg)
    {
        return buildSetTagDataRate(msg);
    }

    /// @brief Encodes query for Tag data rate (Message ID 0x28 query 0x97).
    /// @param[in] tagId Target Tag identifier.
    /// @param[in] displayId Network Display ID.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTagDataRate(
        std::uint8_t tagId, std::uint16_t displayId = 0x0002U);

    /// @brief Encodes source selector multiplexer for a Tag or range (Message ID 0x98).
    /// @param[in] msg Tag source selector parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTagSourceSelector(
        const MsgTagSourceSelector& msg);

    /// @brief Encodes query for Tag source selector (Message ID 0x28 query 0x98).
    /// @param[in] tagId Target Tag identifier.
    /// @param[in] displayId Network Display ID.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTagSourceSelector(
        std::uint8_t tagId, std::uint16_t displayId = 0x0002U);

    /// @brief Encodes MISB ST 0808 ancillary text metadata insertion (Message ID 0xAC).
    /// @param[in] msg Ancillary text message parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildAncillaryTextMetadata(
        const MsgAncillaryTextMetadata& msg);

    /// @brief Alias for buildAncillaryTextMetadata.
    [[nodiscard]] static std::vector<std::uint8_t> buildAncillaryText(
        const MsgAncillaryTextMetadata& msg)
    {
        return buildAncillaryTextMetadata(msg);
    }

    /// @brief Encodes VMTI target image chips configuration (Message ID 0xAD).
    /// @param[in] msg VMTI chips parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildVmtiChips(
        const MsgVmtiChips& msg);

    /// @brief Encodes Cursor-on-Target XML tactical broadcast configuration (Message ID 0xB0).
    /// @param[in] msg CoT parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCursorOnTarget(
        const MsgCursorOnTarget& msg);

    /// @brief Encodes active VMTI fields and ontology refresh rate (Message ID 0xBF).
    /// @param[in] msg VMTI fields parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildVmtiFields(
        const MsgVmtiFields& msg);

    /// @brief Encodes query for active metadata values (Message ID 0x28 query 0x13).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMetadataValues();

    /// @brief Encodes query for static metadata values (Message ID 0x28 query 0x14).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMetadataStaticValues();

    /// @brief Encodes query for frame metadata values (Message ID 0x28 query 0x15).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMetadataFrame();

    /// @brief Encodes query for metadata rate (Message ID 0x28 query 0x62).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMetadataRate();

    /// @brief Encodes query for VMTI chips configuration (Message ID 0x28 query 0xAD).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVmtiChips();

    /// @brief Encodes query for Cursor-on-Target configuration (Message ID 0x28 query 0xB0).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCursorOnTarget();

    /// @brief Encodes query for VMTI fields (Message ID 0x28 query 0xBF).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVmtiFields();

    /// @brief Encodes query for Appended Metadata configuration (Message ID 0x28 query 0x89).
    /// @param[in] cameraIndex Target camera channel index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetAppendedMetadata(
        std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
