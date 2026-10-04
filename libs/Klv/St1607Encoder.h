#pragma once

/// @file St1607Encoder.h
/// @brief MISB ST 1607.2 Amend and Segment Local Set serializer.

#include "KlvTypes.h"
#include "St1607Types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Klv {

/// @class St1607Encoder
/// @brief Serializes MISB ST 1607 Amend and Segment Local Sets and MSID packs.
class St1607Encoder {
public:
    /// @brief Encodes a Metadata Substream ID (Item 143) pack.
    /// @param[in] msid MetadataSubstreamId to encode.
    /// @param[out] out Destination vector to append MSID pack bytes.
    /// @return True on success, false on invalid data.
    [[nodiscard]] static bool encodeMsid(const MetadataSubstreamId& msid,
                                        std::vector<std::uint8_t>& out);

    /// @brief Encodes an embedded Amend Local Set (Tag 101 payload).
    /// @param[in] set AmendLocalSet to serialize.
    /// @param[out] out Destination vector to append TLV bytes.
    /// @return KlvStatus::Success on success.
    [[nodiscard]] static KlvStatus encodeAmend(const AmendLocalSet& set,
                                              std::vector<std::uint8_t>& out);

    /// @brief Encodes an embedded Segment Local Set (Tag 100 payload).
    /// @param[in] set SegmentLocalSet to serialize.
    /// @param[out] out Destination vector to append TLV bytes.
    /// @return KlvStatus::Success on success.
    [[nodiscard]] static KlvStatus encodeSegment(const SegmentLocalSet& set,
                                                std::vector<std::uint8_t>& out);

    /// @brief Encodes a standalone Amend Local Set packet (with 16-byte UL and length).
    /// @param[in] set AmendLocalSet to serialize.
    /// @param[out] out Destination vector to append full KLV packet bytes.
    /// @return KlvStatus::Success on success.
    [[nodiscard]] static KlvStatus encodePacket(const AmendLocalSet& set,
                                                std::vector<std::uint8_t>& out);

    /// @brief Encodes a standalone Segment Local Set packet (with 16-byte UL and length).
    /// @param[in] set SegmentLocalSet to serialize.
    /// @param[out] out Destination vector to append full KLV packet bytes.
    /// @return KlvStatus::Success on success.
    [[nodiscard]] static KlvStatus encodePacket(const SegmentLocalSet& set,
                                                std::vector<std::uint8_t>& out);
};

} // namespace Klv
