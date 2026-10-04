#pragma once

/// @file St1607Types.h
/// @brief MISB ST 1607.2 Constructs to Amend/Segment KLV Metadata data structures.

#include "GeoRegistrationTypes.h"
#include "GeoTypes.h"
#include "MiisCoreId.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Klv {

// Forward declaration
struct UasDatalinkMessage;

/// @brief 16-byte Universal Label for MISB ST 1607 Amend Local Set.
/// @details 06.0E.2B.34.02.0B.01.01.0E.01.03.03.03.01.00.00 (CRC 17182).
inline constexpr std::array<std::uint8_t, 16> AmendLocalSetUl = {
    0x06U, 0x0EU, 0x2BU, 0x34U, 0x02U, 0x0BU, 0x01U, 0x01U,
    0x0EU, 0x01U, 0x03U, 0x03U, 0x03U, 0x01U, 0x00U, 0x00U
};

/// @brief 16-byte Universal Label for MISB ST 1607 Segment Local Set.
/// @details 06.0E.2B.34.02.0B.01.01.0E.01.03.03.03.00.00.00 (CRC 29742).
inline constexpr std::array<std::uint8_t, 16> SegmentLocalSetUl = {
    0x06U, 0x0EU, 0x2BU, 0x34U, 0x02U, 0x0BU, 0x01U, 0x01U,
    0x0EU, 0x01U, 0x03U, 0x03U, 0x03U, 0x00U, 0x00U, 0x00U
};

/// @struct MetadataSubstreamId
/// @brief MISB ST 0601 Item 143: Metadata Substream Identifier Pack (MSID).
/// @details Two-element truncation pack: local identifier (BER-OID) or 16-byte UUID.
struct MetadataSubstreamId {
    std::uint32_t localId { 0U };                                   ///< Local ID (BER-OID; 0 indicates universal ID)
    std::optional<std::array<std::uint8_t, 16>> universalId {};    ///< UUID when localId == 0
};

/// @struct AmendLocalSet
/// @brief MISB ST 1607.2 Amend Local Set (ST 0601 Item 101).
/// @details Enables child/branch metadata corrections while preserving root metadata.
struct AmendLocalSet {
    MetadataSubstreamId msid {};                                    ///< Item 143: Mandatory MSID pack
    std::optional<double> platformHeadingDeg {};                    ///< Tag 5: Heading [0, 360) deg
    std::optional<double> platformPitchDeg {};                      ///< Tag 6: Pitch [-20, +20] deg
    std::optional<double> platformRollDeg {};                       ///< Tag 7: Roll [-50, +50] deg
    std::optional<std::string> imageSourceSensor {};                ///< Tag 11: Sensor model
    std::optional<std::string> imageCoordinateSystem {};            ///< Tag 12: Coordinate system
    std::optional<double> sensorLatitudeDeg {};                     ///< Tag 13: Latitude [-90, +90] deg
    std::optional<double> sensorLongitudeDeg {};                    ///< Tag 14: Longitude [-180, +180] deg
    std::optional<double> sensorTrueAltitudeM {};                   ///< Tag 15: True altitude [-900, +19000] m
    std::optional<double> sensorHfovDeg {};                         ///< Tag 16: HFOV [0, 180] deg
    std::optional<double> sensorVfovDeg {};                         ///< Tag 17: VFOV [0, 180] deg
    std::optional<double> sensorRelAzimuthDeg {};                   ///< Tag 18: Relative azimuth [0, 360) deg
    std::optional<double> sensorRelElevationDeg {};                 ///< Tag 19: Relative elevation [-180, +180] deg
    std::optional<double> sensorRelRollDeg {};                      ///< Tag 20: Relative roll [0, 360) deg
    std::optional<double> slantRangeM {};                           ///< Tag 21: Slant range [0, 5000000] m
    std::optional<double> targetWidthM {};                          ///< Tag 22: Target width [0, 10000] m
    std::optional<double> frameCenterLatDeg {};                     ///< Tag 23: Frame center lat [-90, +90] deg
    std::optional<double> frameCenterLonDeg {};                     ///< Tag 24: Frame center lon [-180, +180] deg
    std::optional<double> frameCenterElevM {};                      ///< Tag 25: Frame center elev [-900, +19000] m
    std::optional<FrustumCorners> cornerCoordinates {};             ///< Corner coords (Tags 26..33 or 82..89)
    std::optional<double> targetErrorCe90M {};                      ///< Tag 45: CE90 in meters
    std::optional<double> targetErrorLe90M {};                      ///< Tag 46: LE90 in meters
    std::optional<double> sensorAltitudeHaeM {};                    ///< Tag 75: Sensor HAE altitude in meters
    std::optional<double> frameCenterElevHaeM {};                   ///< Tag 78: Frame center HAE elevation in meters
    std::optional<double> sensorRollAngleDeg {};                    ///< Tag 118: Sensor roll angle [0, 360) deg
    std::optional<GeoRegistrationLocalSet> geoRegistration {};      ///< Tag 98: MISB ST 1601 Geo-Registration LS
    std::optional<std::uint8_t> securityCountryCodingMethod {};     ///< Tag 48 Item 12 (ST 1607.2-09)
    std::optional<std::string> securityObjectCountryCodes {};       ///< Tag 48 Item 13 (ST 1607.2-09)
    std::vector<AmendLocalSet> childAmends {};                      ///< Nested child Amend LS instances

    /// @brief Validates conformance with MISB ST 1607.2 requirements.
    /// @return True if valid, false if non-conforming.
    [[nodiscard]] bool validate() const noexcept;

    /// @brief Applies this Amend Local Set onto a base message ("union and override").
    /// @param[in,out] target Target message to receive amended values.
    void applyTo(UasDatalinkMessage& target) const noexcept;
};

/// @struct SegmentLocalSet
/// @brief MISB ST 1607.2 Segment Local Set (ST 0601 Item 100).
/// @details Enables sharing common parent metadata while partitioning independent attributes.
struct SegmentLocalSet {
    MetadataSubstreamId msid {};                                    ///< Item 143: Mandatory MSID pack
    std::optional<std::string> imageSourceSensor {};                ///< Tag 11: Sensor model
    std::optional<std::string> imageCoordinateSystem {};            ///< Tag 12: Coordinate system
    std::optional<double> sensorLatitudeDeg {};                     ///< Tag 13: Latitude [-90, +90] deg
    std::optional<double> sensorLongitudeDeg {};                    ///< Tag 14: Longitude [-180, +180] deg
    std::optional<double> sensorTrueAltitudeM {};                   ///< Tag 15: True altitude [-900, +19000] m
    std::optional<double> sensorHfovDeg {};                         ///< Tag 16: HFOV [0, 180] deg
    std::optional<double> sensorVfovDeg {};                         ///< Tag 17: VFOV [0, 180] deg
    std::optional<double> sensorRelAzimuthDeg {};                   ///< Tag 18: Relative azimuth [0, 360) deg
    std::optional<double> sensorRelElevationDeg {};                 ///< Tag 19: Relative elevation [-180, +180] deg
    std::optional<double> sensorRelRollDeg {};                      ///< Tag 20: Relative roll [0, 360) deg
    std::optional<double> slantRangeM {};                           ///< Tag 21: Slant range [0, 5000000] m
    std::optional<double> targetWidthM {};                          ///< Tag 22: Target width [0, 10000] m
    std::optional<double> frameCenterLatDeg {};                     ///< Tag 23: Frame center lat [-90, +90] deg
    std::optional<double> frameCenterLonDeg {};                     ///< Tag 24: Frame center lon [-180, +180] deg
    std::optional<double> frameCenterElevM {};                      ///< Tag 25: Frame center elev [-900, +19000] m
    std::optional<FrustumCorners> cornerCoordinates {};             ///< Corner coords (Tags 26..33 or 82..89)
    std::optional<double> sensorAltitudeHaeM {};                    ///< Tag 75: Sensor HAE altitude in meters
    std::optional<double> frameCenterElevHaeM {};                   ///< Tag 78: Frame center HAE elevation in meters
    std::optional<MiisCoreId> miisCoreId {};                       ///< Tag 94: MISB ST 1204 Core Identifier
    std::optional<double> sensorRollAngleDeg {};                    ///< Tag 118: Sensor roll angle [0, 360) deg
    std::optional<std::uint8_t> securityCountryCodingMethod {};     ///< Tag 48 Item 12 (ST 1607-04)
    std::optional<std::string> securityObjectCountryCodes {};       ///< Tag 48 Item 13 (ST 1607-04)
    std::vector<SegmentLocalSet> childSegments {};                  ///< Nested child Segment LS instances
    std::vector<AmendLocalSet> childAmends {};                      ///< Nested child Amend LS instances

    /// @brief Validates conformance with MISB ST 1607.2 requirements.
    /// @return True if valid, false if non-conforming.
    [[nodiscard]] bool validate() const noexcept;

    /// @brief Applies this Segment Local Set onto a base message ("union and override").
    /// @param[in,out] target Target message to receive segmented values.
    void applyTo(UasDatalinkMessage& target) const noexcept;
};

} // namespace Klv
