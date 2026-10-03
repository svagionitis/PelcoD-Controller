#pragma once

/// @file VmtiParser.h
/// @brief MISB ST 0903 Video Moving Target Indicator (VMTI) metadata parser.

#include "KlvTypes.h"
#include "VmtiTypes.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Klv {

/// @class VmtiParser
/// @brief Deserializes binary KLV byte sequences into VMTI Local Sets and VTarget Packs.
class VmtiParser {
public:
    /// @brief Checks whether the given buffer begins with a MISB ST 0903 Universal Label.
    /// @param[in] data Pointer to memory buffer.
    /// @param[in] size Size of the buffer in bytes.
    /// @return True if buffer starts with MISB ST 0903 UL; false otherwise.
    [[nodiscard]] static bool isVmti(const std::uint8_t* data, std::size_t size) noexcept;

    /// @brief Parses a VMTI Local Set from binary data.
    /// @details Supports both standalone packets (with 16-byte UL and CRC) and embedded packets
    ///          (from MISB ST 0601 Tag 74).
    /// @param[in] data Pointer to raw KLV data.
    /// @param[in] size Number of bytes available.
    /// @param[out] vmti Decoded VMTI Local Set structure.
    /// @param[in] verifyChecksum When true, validates Tag 1 CRC checksum on standalone packets.
    /// @return KlvStatus code indicating outcome of operation.
    [[nodiscard]] static KlvStatus parse(
        const std::uint8_t* data,
        std::size_t size,
        VmtiLocalSet& vmti,
        bool verifyChecksum = false) noexcept;

    /// @brief Unscales an IMAPB(-19.2, 19.2, 3) 24-bit unsigned integer to degrees.
    /// @param[in] rawVal 24-bit unsigned integer value.
    /// @return Geographic angle offset in degrees [-19.2, +19.2].
    [[nodiscard]] static double unscaleOffset(std::uint32_t rawVal) noexcept;

    /// @brief Unscales an IMAPB(-900, 19000, 2) 16-bit unsigned integer to meters HAE.
    /// @param[in] rawVal 16-bit unsigned integer value.
    /// @return Height above ellipsoid in meters.
    [[nodiscard]] static double unscaleHae(std::uint16_t rawVal) noexcept;

    /// @brief Parses an individual VTargetPack from a memory buffer.
    /// @param[in] data Pointer to pack bytes starting with BER-OID targetId.
    /// @param[in] size Length of pack in bytes.
    /// @param[out] target Decoded target pack.
    /// @param[in] frameWidth Video frame width for pixel conversions.
    /// @param[in] frameHeight Video frame height for pixel conversions.
    /// @return True if parsing succeeded; false otherwise.
    [[nodiscard]] static bool parseTarget(
        const std::uint8_t* data,
        std::size_t size,
        VTargetPack& target,
        std::uint32_t frameWidth,
        std::uint32_t frameHeight) noexcept;

    /// @brief Parses a Tag 101 VTargetSeries into target packs.
    /// @param[in] data Pointer to series value bytes.
    /// @param[in] size Length of series in bytes.
    /// @param[out] targets Destination vector of decoded target packs.
    /// @param[in] frameWidth Video frame width.
    /// @param[in] frameHeight Video frame height.
    /// @return True if parsing succeeded; false otherwise.
    [[nodiscard]] static bool parseSeries(
        const std::uint8_t* data,
        std::size_t size,
        std::vector<VTargetPack>& targets,
        std::uint32_t frameWidth,
        std::uint32_t frameHeight) noexcept;
};

} // namespace Klv
