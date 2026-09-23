#pragma once

/// @file KlvTypes.h
/// @brief Core data structures, enumerations, and constants for STANAG 4609 / MISB ST 0601 KLV metadata.

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
    CornerLat1 = 26U,           ///< Corner 1 (Top-Left) latitude [-90, +90] deg (4 bytes)
    CornerLon1 = 27U,           ///< Corner 1 (Top-Left) longitude [-180, +180] deg (4 bytes)
    CornerLat2 = 28U,           ///< Corner 2 (Top-Right) latitude [-90, +90] deg (4 bytes)
    CornerLon2 = 29U,           ///< Corner 2 (Top-Right) longitude [-180, +180] deg (4 bytes)
    CornerLat3 = 30U,           ///< Corner 3 (Bottom-Right) latitude [-90, +90] deg (4 bytes)
    CornerLon3 = 31U,           ///< Corner 3 (Bottom-Right) longitude [-180, +180] deg (4 bytes)
    CornerLat4 = 32U,           ///< Corner 4 (Bottom-Left) latitude [-90, +90] deg (4 bytes)
    CornerLon4 = 33U,           ///< Corner 4 (Bottom-Left) longitude [-180, +180] deg (4 bytes)
    SecurityLocalSet = 48U,     ///< MISB ST 0102 Security Classification Local Set (nested)
    UasLsVersion = 65U          ///< UAS Datalink LS version number (1 byte)
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

/// @struct GeoPoint2D
/// @brief WGS-84 2D geodetic position (latitude and longitude in degrees).
struct GeoPoint2D {
    double latitudeDeg { 0.0 };  ///< Latitude [-90.0, +90.0] degrees
    double longitudeDeg { 0.0 }; ///< Longitude [-180.0, +180.0] degrees
};

/// @struct GeoPoint3D
/// @brief WGS-84 3D geodetic position (latitude, longitude, altitude).
struct GeoPoint3D {
    double latitudeDeg { 0.0 };  ///< Latitude [-90.0, +90.0] degrees
    double longitudeDeg { 0.0 }; ///< Longitude [-180.0, +180.0] degrees
    double altitudeM { 0.0 };    ///< Height above Mean Sea Level (MSL) in meters
};

/// @struct FrustumCorners
/// @brief Optical footprint 4-corner ground projection coordinates on the WGS-84 ellipsoid.
struct FrustumCorners {
    GeoPoint2D topLeft {};     ///< Corner 1 (Top-Left)
    GeoPoint2D topRight {};    ///< Corner 2 (Top-Right)
    GeoPoint2D bottomRight {}; ///< Corner 3 (Bottom-Right)
    GeoPoint2D bottomLeft {};  ///< Corner 4 (Bottom-Left)
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
/// @brief Basic metadata for MISB ST 0102 Security Classification Local Set (Tag 48).
struct SecurityMetadata {
    SecurityClassification classification { SecurityClassification::Unclassified }; ///< Tag 1
    std::string classifyingCountry; ///< Tag 2: Country / Authority code (e.g. "US", "NATO")
    std::string sciShiInfo;         ///< Tag 3: Security caveats / SCI / SHI
    std::string caveats;            ///< Tag 4: Handling caveats
    std::string releasingInstructions; ///< Tag 5: Releasing instructions
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
    std::optional<FrustumCorners> cornerCoordinates;  ///< Tags 26..33: 4 Footprint corners
    std::optional<SecurityMetadata> security;         ///< Tag 48: Security Local Set
    std::optional<std::uint8_t> uasLsVersion;         ///< Tag 65: Version number (e.g. 1..16)
};

} // namespace Klv
