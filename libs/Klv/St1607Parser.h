#pragma once

/// @file St1607Parser.h
/// @brief MISB ST 1607.2 Amend and Segment Local Set parser.

#include "KlvTypes.h"
#include "St1607Types.h"

#include <cstddef>
#include <cstdint>

namespace Klv {

/// @class St1607Parser
/// @brief Parses MISB ST 1607 Amend and Segment Local Sets and MSID packs.
class St1607Parser {
public:
    /// @brief Checks whether buffer starts with Amend Local Set Universal Label.
    /// @param[in] data Pointer to memory buffer.
    /// @param[in] size Size of buffer in bytes.
    /// @return True if buffer starts with Amend Local Set UL; false otherwise.
    [[nodiscard]] static bool isAmendLocalSet(const std::uint8_t* data, std::size_t size) noexcept;

    /// @brief Checks whether buffer starts with Segment Local Set Universal Label.
    /// @param[in] data Pointer to memory buffer.
    /// @param[in] size Size of buffer in bytes.
    /// @return True if buffer starts with Segment Local Set UL; false otherwise.
    [[nodiscard]] static bool isSegmentLocalSet(const std::uint8_t* data, std::size_t size) noexcept;

    /// @brief Parses a Metadata Substream ID (Item 143) pack.
    /// @param[in] data Pointer to MSID pack data.
    /// @param[in] size Size of buffer in bytes.
    /// @param[out] outMsid Decoded MetadataSubstreamId.
    /// @param[out] bytesRead Number of bytes consumed.
    /// @return True on success, false on malformed data.
    [[nodiscard]] static bool parseMsid(const std::uint8_t* data,
                                        std::size_t size,
                                        MetadataSubstreamId& outMsid,
                                        std::size_t& bytesRead) noexcept;

    /// @brief Parses an embedded Amend Local Set (Tag 101 payload).
    /// @param[in] data Pointer to TLV payload.
    /// @param[in] size Payload length in bytes.
    /// @param[out] outSet Decoded AmendLocalSet structure.
    /// @return KlvStatus code indicating outcome of operation.
    [[nodiscard]] static KlvStatus parseAmend(const std::uint8_t* data,
                                             std::size_t size,
                                             AmendLocalSet& outSet) noexcept;

    /// @brief Parses an embedded Segment Local Set (Tag 100 payload).
    /// @param[in] data Pointer to TLV payload.
    /// @param[in] size Payload length in bytes.
    /// @param[out] outSet Decoded SegmentLocalSet structure.
    /// @return KlvStatus code indicating outcome of operation.
    [[nodiscard]] static KlvStatus parseSegment(const std::uint8_t* data,
                                               std::size_t size,
                                               SegmentLocalSet& outSet) noexcept;

    /// @brief Parses a standalone Amend Local Set packet (with 16-byte UL and length).
    /// @param[in] data Pointer to raw KLV packet buffer.
    /// @param[in] size Available buffer size in bytes.
    /// @param[out] outSet Decoded AmendLocalSet structure.
    /// @return KlvStatus code indicating outcome of operation.
    [[nodiscard]] static KlvStatus parsePacket(const std::uint8_t* data,
                                               std::size_t size,
                                               AmendLocalSet& outSet) noexcept;

    /// @brief Parses a standalone Segment Local Set packet (with 16-byte UL and length).
    /// @param[in] data Pointer to raw KLV packet buffer.
    /// @param[in] size Available buffer size in bytes.
    /// @param[out] outSet Decoded SegmentLocalSet structure.
    /// @return KlvStatus code indicating outcome of operation.
    [[nodiscard]] static KlvStatus parsePacket(const std::uint8_t* data,
                                               std::size_t size,
                                               SegmentLocalSet& outSet) noexcept;
};

} // namespace Klv
