#pragma once

/// @file RvtEncoder.h
/// @brief MISB ST 0806 Remote Video Terminal (RVT) metadata encoder.

#include "RvtTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Klv {

/// @class RvtEncoder
/// @brief Serializes RVT Local Sets, POI Packs, and AOI Packs into binary KLV byte sequences.
class RvtEncoder {
public:
    /// @brief Encodes an RvtLocalSet into binary KLV format.
    /// @details When standalone is true, prepends the 16-byte Universal Label, length,
    ///          ensures Tag 2 is first, and appends Tag 1 CRC-32 (ISO/IEC 13818-1) checksum last.
    ///          When standalone is false (embedded in ST 0601 Tag 73), encodes raw TLV items.
    /// @param[in] rvt The RVT Local Set data to encode.
    /// @param[in] standalone True for standalone RVT packet; false for embedded in ST 0601 Tag 73.
    /// @return Byte vector containing the encoded KLV packet.
    [[nodiscard]] static std::vector<std::uint8_t> encode(
        const RvtLocalSet& rvt,
        bool standalone = false);

    /// @brief Encodes an individual Point of Interest (POI) Local Set into TLV bytes.
    /// @param[in] poi POI pack to encode.
    /// @param[out] out Destination vector to append encoded bytes.
    static void encodePoi(
        const PoiPack& poi,
        std::vector<std::uint8_t>& out);

    /// @brief Encodes an individual Area of Interest (AOI) Local Set into TLV bytes.
    /// @param[in] aoi AOI pack to encode.
    /// @param[out] out Destination vector to append encoded bytes.
    static void encodeAoi(
        const AoiPack& aoi,
        std::vector<std::uint8_t>& out);

    /// @brief Encodes an individual User Defined Data Local Set into TLV bytes.
    /// @param[in] userDef User Defined pack to encode.
    /// @param[out] out Destination vector to append encoded bytes.
    static void encodeUserDefined(
        const UserDefinedPack& userDef,
        std::vector<std::uint8_t>& out);

    /// @brief Scales latitude in degrees [-90, +90] to 32-bit signed integer.
    /// @param[in] latDeg Latitude in degrees.
    /// @return 32-bit signed integer representation.
    [[nodiscard]] static std::int32_t scaleLatitude(double latDeg) noexcept;

    /// @brief Scales longitude in degrees [-180, +180] to 32-bit signed integer.
    /// @param[in] lonDeg Longitude in degrees.
    /// @return 32-bit signed integer representation.
    [[nodiscard]] static std::int32_t scaleLongitude(double lonDeg) noexcept;

    /// @brief Scales altitude MSL in meters [-900, +19000] to 16-bit unsigned integer.
    /// @param[in] altM Altitude in meters.
    /// @return 16-bit unsigned integer representation.
    [[nodiscard]] static std::uint16_t scaleAltitude(double altM) noexcept;
};

} // namespace Klv
