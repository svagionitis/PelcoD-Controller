#pragma once

/// @file KlvTypes.h
/// @brief Core data structures, enumerations, and constants for STANAG 4609 / MISB ST 0601 KLV metadata.

#include "GeoTypes.h"
#include "MiisCoreId.h"
#include "RvtTypes.h"
#include "St1607Types.h"
#include "VmtiTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Klv {

/// @brief Universal Label (UL) 16-byte key size defined by SMPTE ST 336.
inline constexpr std::size_t kUniversalLabelSize { 16U };

/// @brief Standard MISB ST 0601 16-byte Universal Label key (UAS Datalink Local Set).
/// @details 06 0E 2B 34 02 0B 01 01 0E 01 03 01 01 00 00 00
inline constexpr std::array<std::uint8_t, kUniversalLabelSize> kMisb0601UniversalLabel {
    0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01, 0x0E, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00
};

/// @brief Standard prefix length for MISB ST 0601 Universal Label matching (first 12 bytes).
inline constexpr std::size_t kMisb0601PrefixSize { 12U };

/// @enum Tag
/// @brief MISB ST 0601 Local Set Tag identifiers.
enum class Tag : std::uint32_t {
    Checksum = 1U,              ///< CRC-16-CCITT packet checksum (2 bytes)
    PrecisionTimeStamp = 2U,    ///< Microseconds since UNIX epoch (8 bytes)
    MissionId = 3U,             ///< Mission identification string (variable)
    PlatformTailNumber = 4U,    ///< Platform tail / callsign string (variable)
    PlatformHeading = 5U,       ///< Platform heading angle [0, 360) deg (2 bytes)
    PlatformPitch = 6U,         ///< Platform pitch angle [-20, +20] deg (2 bytes)
    PlatformRoll = 7U,          ///< Platform roll angle [-50, +50] deg (2 bytes)
    PlatformDesignation = 10U,  ///< Platform designation string (variable)
    ImageSourceSensor = 11U,    ///< Image sensor description (variable)
    ImageCoordinateSystem = 12U,///< Image coordinate system (variable)
    SensorLatitude = 13U,       ///< Sensor WGS-84 latitude [-90, +90] deg (4 bytes)
    SensorLongitude = 14U,      ///< Sensor WGS-84 longitude [-180, +180] deg (4 bytes)
    SensorTrueAltitude = 15U,   ///< Sensor altitude above MSL [-900, +19000] m (2 bytes)
    SensorHFOV = 16U,           ///< Horizontal field of view [0, 180] deg (2 bytes)
    SensorVFOV = 17U,           ///< Vertical field of view [0, 180] deg (2 bytes)
    SensorRelAzimuth = 18U,     ///< Sensor relative azimuth angle [0, 360) deg (4 bytes)
    SensorRelElevation = 19U,   ///< Sensor relative elevation angle [-180, +180] deg (4 bytes)
    SensorRelRoll = 20U,        ///< Sensor relative roll angle [0, 360) deg (4 bytes)
    SlantRange = 21U,           ///< Slant range to target [0, 5000000] m (4 bytes)
    TargetWidth = 22U,          ///< Target width in meters [0, 10000] m (2 bytes)
    FrameCenterLat = 23U,       ///< Frame center WGS-84 latitude [-90, +90] deg (4 bytes)
    FrameCenterLon = 24U,       ///< Frame center WGS-84 longitude [-180, +180] deg (4 bytes)
    FrameCenterElev = 25U,      ///< Frame center elevation above MSL [-900, +19000] m (2 bytes)
    OffsetCornerLat1 = 26U,     ///< Tag 26: Offset Corner 1 latitude [-0.075, +0.075] deg (2 bytes)
    OffsetCornerLon1 = 27U,     ///< Tag 27: Offset Corner 1 longitude [-0.075, +0.075] deg (2 bytes)
    OffsetCornerLat2 = 28U,     ///< Tag 28: Offset Corner 2 latitude [-0.075, +0.075] deg (2 bytes)
    OffsetCornerLon2 = 29U,     ///< Tag 29: Offset Corner 2 longitude [-0.075, +0.075] deg (2 bytes)
    OffsetCornerLat3 = 30U,     ///< Tag 30: Offset Corner 3 latitude [-0.075, +0.075] deg (2 bytes)
    OffsetCornerLon3 = 31U,     ///< Tag 31: Offset Corner 3 longitude [-0.075, +0.075] deg (2 bytes)
    OffsetCornerLat4 = 32U,     ///< Tag 32: Offset Corner 4 latitude [-0.075, +0.075] deg (2 bytes)
    OffsetCornerLon4 = 33U,     ///< Tag 33: Offset Corner 4 longitude [-0.075, +0.075] deg (2 bytes)
    CornerLat1 = 26U,           ///< Backward-compatible alias for OffsetCornerLat1
    CornerLon1 = 27U,           ///< Backward-compatible alias for OffsetCornerLon1
    CornerLat2 = 28U,           ///< Backward-compatible alias for OffsetCornerLat2
    CornerLon2 = 29U,           ///< Backward-compatible alias for OffsetCornerLon2
    CornerLat3 = 30U,           ///< Backward-compatible alias for OffsetCornerLat3
    CornerLon3 = 31U,           ///< Backward-compatible alias for OffsetCornerLon3
    CornerLat4 = 32U,           ///< Backward-compatible alias for OffsetCornerLat4
    CornerLon4 = 33U,           ///< Backward-compatible alias for OffsetCornerLon4
    TargetErrorCe90 = 45U,      ///< Tag 45: Target Location Error CE90 in meters (2 bytes)
    TargetErrorLe90 = 46U,      ///< Tag 46: Target Location Error LE90 in meters (2 bytes)
    SecurityLocalSet = 48U,     ///< Tag 48: MISB ST 0102 Security Classification Local Set (nested)
    UasLsVersion = 65U,         ///< Tag 65: UAS Datalink LS version number (1 byte)
    RvtLocalSet = 73U,          ///< Tag 73: MISB ST 0806 RVT Local Set (nested)
    VmtiLocalSet = 74U,         ///< Tag 74: MISB ST 0903 VMTI Local Set (nested)
    SensorAltitudeHae = 75U,    ///< Tag 75: Sensor true altitude HAE [-900, +19000] m (2 bytes)
    FrameCenterElevHae = 78U,   ///< Tag 78: Frame center elevation HAE [-900, +19000] m (2 bytes)
    CornerLat1Full = 82U,       ///< Tag 82: Corner 1 Latitude Full [-90, +90] deg (4 bytes)
    CornerLon1Full = 83U,       ///< Tag 83: Corner 1 Longitude Full [-180, +180] deg (4 bytes)
    CornerLat2Full = 84U,       ///< Tag 84: Corner 2 Latitude Full [-90, +90] deg (4 bytes)
    CornerLon2Full = 85U,       ///< Tag 85: Corner 2 Longitude Full [-180, +180] deg (4 bytes)
    CornerLat3Full = 86U,       ///< Tag 86: Corner 3 Latitude Full [-90, +90] deg (4 bytes)
    CornerLon3Full = 87U,       ///< Tag 87: Corner 3 Longitude Full [-180, +180] deg (4 bytes)
    CornerLat4Full = 88U,       ///< Tag 88: Corner 4 Latitude Full [-90, +90] deg (4 bytes)
    CornerLon4Full = 89U,       ///< Tag 89: Corner 4 Longitude Full [-180, +180] deg (4 bytes)
    MiisCoreId = 94U,           ///< Tag 94: MISB ST 1204 Core Identifier
    WavelengthBands = 95U,      ///< Tag 95: Wavelength band bitmask (1 byte)
    GeoRegistration = 98U,      ///< Tag 98: MISB ST 1601 Geo-Registration Local Set
    SegmentLocalSet = 100U,     ///< Tag 100: MISB ST 1607 Segment Local Set
    AmendLocalSet = 101U,       ///< Tag 101: MISB ST 1607 Amend Local Set
    SensorRollAngle = 118U,     ///< Tag 118: Sensor roll angle [0, 360) deg (4 bytes)
    Msid = 143U                 ///< Tag 143: MISB ST 0601 Metadata Substream ID Pack
};

/// @enum KlvStatus
/// @brief Status codes returned by KLV encoding and decoding operations.
enum class KlvStatus {
    Success,               ///< Operation succeeded
    BufferTooSmall,        ///< Output buffer capacity is insufficient
    InvalidUniversalLabel, ///< Universal label prefix did not match MISB ST 0601
    CrcMismatch,           ///< Packet CRC-16 checksum verification failed
    MalformedBerLength,    ///< Basic Encoding Rules (BER) length is corrupt or out of bounds
    BufferUnderflow,       ///< Reached end of buffer unexpectedly while parsing
    TagError               ///< Unexpected or malformed tag structure
};




/// @enum SecurityClassification
/// @brief MISB ST 0102 Security Classification levels.
enum class SecurityClassification : std::uint8_t {
    Unclassified = 1U,   ///< UNCLASSIFIED
    Restricted = 2U,     ///< RESTRICTED
    Confidential = 3U,   ///< CONFIDENTIAL
    Secret = 4U,         ///< SECRET
    TopSecret = 5U       ///< TOP SECRET
};

/// @struct SecurityMetadata
/// @brief Conforms to MISB ST 0102.13 Security Classification Local Set (ST 0601 Tag 48).
struct SecurityMetadata {
    SecurityClassification classification { SecurityClassification::Unclassified }; ///< Sub-Tag 1
    std::uint8_t countryCodingMethod { 1U }; ///< Sub-Tag 2: 1 = ISO-3166 Two-Letter
    std::string classifyingCountry { "US" }; ///< Sub-Tag 3: Country / Authority code (e.g. "US", "NATO")
    std::string sciShiInfo {};               ///< Sub-Tag 4: Security caveats / SCI / SHI
    std::string caveats {};                  ///< Sub-Tag 5: Handling caveats
    std::string releasingInstructions {};    ///< Sub-Tag 6: Releasing instructions
    std::uint8_t objectCountryCodingMethod { 1U }; ///< Sub-Tag 12: Object country coding method
    std::string objectCountryCodes {};       ///< Sub-Tag 13: Object country codes
    std::uint8_t version { 13U };            ///< Sub-Tag 22: ST 0102 version (default 13)
};

/// @struct UasDatalinkMessage
/// @brief Strongly-typed representation of a MISB ST 0601 UAS Datalink Local Set message.
/// @details Each field is wrapped in std::optional to support sparse telemetry updates.
struct UasDatalinkMessage {
    std::optional<std::uint64_t> precisionTimeStampUs; ///< Tag 2: Microseconds since epoch
    std::optional<std::string> missionId;             ///< Tag 3: Mission ID
    std::optional<std::string> platformTailNumber;    ///< Tag 4: Tail number / callsign
    std::optional<double> platformHeadingDeg;         ///< Tag 5: [0, 360) deg
    std::optional<double> platformPitchDeg;           ///< Tag 6: [-20, +20] deg
    std::optional<double> platformRollDeg;            ///< Tag 7: [-50, +50] deg
    std::optional<std::string> platformDesignation;   ///< Tag 10: Platform model
    std::optional<std::string> imageSourceSensor;     ///< Tag 11: Sensor payload model
    std::optional<std::string> imageCoordinateSystem; ///< Tag 12: Coordinate system
    std::optional<double> sensorLatitudeDeg;          ///< Tag 13: [-90, +90] deg
    std::optional<double> sensorLongitudeDeg;         ///< Tag 14: [-180, +180] deg
    std::optional<double> sensorTrueAltitudeM;        ///< Tag 15: [-900, +19000] m
    std::optional<double> sensorHfovDeg;              ///< Tag 16: [0, 180] deg
    std::optional<double> sensorVfovDeg;              ///< Tag 17: [0, 180] deg
    std::optional<double> sensorRelAzimuthDeg;        ///< Tag 18: [0, 360) deg
    std::optional<double> sensorRelElevationDeg;      ///< Tag 19: [-180, +180] deg
    std::optional<double> sensorRelRollDeg;           ///< Tag 20: [0, 360) deg
    std::optional<double> slantRangeM;                ///< Tag 21: [0, 5000000] m
    std::optional<double> targetWidthM;               ///< Tag 22: [0, 10000] m
    std::optional<double> frameCenterLatDeg;          ///< Tag 23: [-90, +90] deg
    std::optional<double> frameCenterLonDeg;          ///< Tag 24: [-180, +180] deg
    std::optional<double> frameCenterElevM;           ///< Tag 25: [-900, +19000] m
    std::optional<FrustumCorners> cornerCoordinates;  ///< Decoded from Tags 26..33 or 82..89
    std::optional<double> targetErrorCe90M;           ///< Tag 45: CE90 in meters
    std::optional<double> targetErrorLe90M;           ///< Tag 46: LE90 in meters
    std::optional<SecurityMetadata> security;         ///< Tag 48: Security Local Set
    std::optional<std::uint8_t> uasLsVersion;         ///< Tag 65: Version number (e.g. 1..19)
    std::optional<RvtLocalSet> rvt;                   ///< Tag 73: MISB ST 0806 RVT Local Set
    std::optional<VmtiLocalSet> vmti;                 ///< Tag 74: MISB ST 0903 VMTI Local Set
    std::optional<double> sensorAltitudeHaeM;         ///< Tag 75: Sensor HAE altitude in meters
    std::optional<double> frameCenterElevHaeM;        ///< Tag 78: Frame center HAE elevation in meters
    std::optional<MiisCoreId> miisCoreId {};          ///< Tag 94: MISB ST 1204 MIIS Core Identifier
    std::optional<double> sensorRollAngleDeg;         ///< Tag 118: Sensor roll angle [0, 360) deg
    std::vector<SegmentLocalSet> segments {};         ///< Tag 100: MISB ST 1607 Segment Local Sets
    std::vector<AmendLocalSet> amends {};             ///< Tag 101: MISB ST 1607 Amend Local Sets
};

} // namespace Klv
