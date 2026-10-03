#pragma once

/// @file RvtParser.h
/// @brief MISB ST 0806 Remote Video Terminal (RVT) metadata parser.

#include "KlvTypes.h"
#include "RvtTypes.h"

#include <cstddef>
#include <cstdint>

namespace Klv {

/// @class RvtParser
/// @brief Deserializes binary KLV byte sequences into RVT Local Sets and Subordinate Sets.
class RvtParser {
public:
    /// @brief Checks whether the given buffer begins with a MISB ST 0806 Universal Label.
    /// @param[in] data Pointer to memory buffer.
    /// @param[in] size Size of the buffer in bytes.
    /// @return True if buffer starts with MISB ST 0806 UL; false otherwise.
    [[nodiscard]] static bool isRvt(const std::uint8_t* data, std::size_t size) noexcept;

    /// @brief Parses an RVT Local Set from binary data.
    /// @details Supports both standalone packets (with 16-byte UL and Tag 1 CRC-32) and
    ///          embedded packets (from MISB ST 0601 Tag 73).
    /// @param[in] data Pointer to raw KLV data.
    /// @param[in] size Number of bytes available.
    /// @param[out] rvt Decoded RVT Local Set structure.
    /// @param[in] verifyChecksum When true, validates Tag 1 CRC-32 on standalone packets.
    /// @return KlvStatus code indicating outcome of operation.
    [[nodiscard]] static KlvStatus parse(
        const std::uint8_t* data,
        std::size_t size,
        RvtLocalSet& rvt,
        bool verifyChecksum = false) noexcept;

    /// @brief Parses an individual Point of Interest (POI) Local Set.
    /// @param[in] data Pointer to POI KLV bytes.
    /// @param[in] size Length of POI bytes.
    /// @param[out] poi Decoded POI pack.
    /// @return True if mandatory tags were parsed successfully; false otherwise.
    [[nodiscard]] static bool parsePoi(
        const std::uint8_t* data,
        std::size_t size,
        PoiPack& poi) noexcept;

    /// @brief Parses an individual Area of Interest (AOI) Local Set.
    /// @param[in] data Pointer to AOI KLV bytes.
    /// @param[in] size Length of AOI bytes.
    /// @param[out] aoi Decoded AOI pack.
    /// @return True if mandatory tags were parsed successfully; false otherwise.
    [[nodiscard]] static bool parseAoi(
        const std::uint8_t* data,
        std::size_t size,
        AoiPack& aoi) noexcept;

    /// @brief Parses an individual User Defined Data Local Set.
    /// @param[in] data Pointer to User Defined KLV bytes.
    /// @param[in] size Length of User Defined bytes.
    /// @param[out] userDef Decoded User Defined pack.
    /// @return True if mandatory tags were parsed successfully; false otherwise.
    [[nodiscard]] static bool parseUserDefined(
        const std::uint8_t* data,
        std::size_t size,
        UserDefinedPack& userDef) noexcept;

    /// @brief Unscales a 32-bit signed integer to WGS-84 latitude in degrees [-90, +90].
    /// @param[in] rawVal 32-bit signed integer.
    /// @return Latitude in degrees.
    [[nodiscard]] static double unscaleLatitude(std::int32_t rawVal) noexcept;

    /// @brief Unscales a 32-bit signed integer to WGS-84 longitude in degrees [-180, +180].
    /// @param[in] rawVal 32-bit signed integer.
    /// @return Longitude in degrees.
    [[nodiscard]] static double unscaleLongitude(std::int32_t rawVal) noexcept;

    /// @brief Unscales a 16-bit unsigned integer to altitude MSL in meters [-900, +19000].
    /// @param[in] rawVal 16-bit unsigned integer.
    /// @return Altitude in meters.
    [[nodiscard]] static double unscaleAltitude(std::uint16_t rawVal) noexcept;
};

} // namespace Klv
