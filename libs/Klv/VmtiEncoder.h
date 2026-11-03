#pragma once

/// @file VmtiEncoder.h
/// @brief MISB ST 0903 Video Moving Target Indicator (VMTI) metadata encoder.

#include <cstdint>

#include <vector>

#include "VmtiTypes.h"

namespace Klv {

/// @class VmtiEncoder
/// @brief Serializes VMTI Local Sets and VTarget Packs into binary KLV byte sequences.
class VmtiEncoder {
public:
    /// @brief Encodes a VmtiLocalSet into binary KLV format.
    /// @details When standalone is true, prepends the 16-byte Universal Label, length,
    ///          and appends Tag 1 CRC-16 checksum. When standalone is false (embedded in ST 0601),
    ///          encodes raw TLV items without UL or checksum.
    /// @param[in] vmti The VMTI Local Set data to encode.
    /// @param[in] standalone True for standalone VMTI packet; false for embedded in ST 0601 Tag 74.
    /// @return Byte vector containing the encoded KLV packet.
    [[nodiscard]] static std::vector<std::uint8_t> encode(const VmtiLocalSet& vmti, bool standalone = false);

    /// @brief Scales latitude or longitude offset in degrees to IMAPB(-19.2, 19.2, 3) 24-bit unsigned integer.
    /// @param[in] offsetDeg Geographic angle offset in degrees [-19.2, +19.2].
    /// @return 24-bit unsigned integer representation.
    [[nodiscard]] static std::uint32_t scaleOffset(double offsetDeg) noexcept;

    /// @brief Scales height above ellipsoid in meters to IMAPB(-900, 19000, 2) 16-bit unsigned integer.
    /// @param[in] haeM Height in meters [-900.0, +19000.0].
    /// @return 16-bit unsigned integer representation.
    [[nodiscard]] static std::uint16_t scaleHae(double haeM) noexcept;

    /// @brief Encodes an individual VTargetPack into its binary representation.
    /// @param[in] target Target pack to encode.
    /// @param[in] frameWidth Width of the video frame in pixels.
    /// @param[in] frameHeight Height of the video frame in pixels.
    /// @param[out] out Destination vector to append encoded bytes.
    static void encodeTarget(
        const VTargetPack& target, std::uint32_t frameWidth, std::uint32_t frameHeight, std::vector<std::uint8_t>& out);

    /// @brief Encodes a series of VTargetPacks into Tag 101 series format.
    /// @param[in] targets Vector of target packs.
    /// @param[in] frameWidth Width of the video frame in pixels.
    /// @param[in] frameHeight Height of the video frame in pixels.
    /// @param[out] out Destination vector to append Tag 101 TLV.
    static void encodeSeries(const std::vector<VTargetPack>& targets, std::uint32_t frameWidth,
        std::uint32_t frameHeight, std::vector<std::uint8_t>& out);

    /// @brief Scales velocity in m/s to IMAPB(-327.68, 327.67, 3) 24-bit unsigned integer.
    [[nodiscard]] static std::uint32_t scaleVelocity(double vMps) noexcept;

    /// @brief Scales vertical velocity in m/s to IMAPB(-100, 100, 2) 16-bit unsigned integer.
    [[nodiscard]] static std::uint16_t scaleVelocityUp(double vMps) noexcept;

    /// @brief Scales acceleration in m/s^2 to IMAPB(-50, 50, 2) 16-bit unsigned integer.
    [[nodiscard]] static std::uint16_t scaleAcceleration(double aMps2) noexcept;

    /// @brief Scales ground course heading in degrees to 16-bit unsigned integer [0, 360) deg.
    [[nodiscard]] static std::uint16_t scaleCourse(double courseDeg) noexcept;

    /// @brief Scales ground speed in m/s to 16-bit unsigned integer [0, 200] m/s.
    [[nodiscard]] static std::uint16_t scaleSpeed(double speedMps) noexcept;

    /// @brief Encodes an individual VTrackPack into binary bytes.
    static void encodeTrackPack(const VTrackPack& track, std::uint32_t frameWidth, std::vector<std::uint8_t>& out);

    /// @brief Encodes a series of VTrackPacks into Tag 102 series format.
    static void encodeTrackSeries(
        const std::vector<VTrackPack>& tracks, std::uint32_t frameWidth, std::vector<std::uint8_t>& out);

    /// @brief Encodes an individual OntologyLocalSet into binary bytes.
    static void encodeOntologySet(const OntologyLocalSet& ontology, std::vector<std::uint8_t>& out);

    /// @brief Encodes a series of OntologyLocalSets into Tag 103 series format.
    static void encodeOntologySeries(const std::vector<OntologyLocalSet>& ontologies, std::vector<std::uint8_t>& out);
};

} // namespace Klv
