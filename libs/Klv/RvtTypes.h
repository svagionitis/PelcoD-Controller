#pragma once

/// @file RvtTypes.h
/// @brief Core data structures, enumerations, and constants for MISB ST 0806 RVT Local Set.
/// @see MISB ST 0806.4 "Remote Video Terminal Metadata Set"

#include "GeoTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Klv {

/// @brief Universal Label (UL) 16-byte key size defined by SMPTE ST 336.
inline constexpr std::size_t kRvtUniversalLabelSize { 16U };

/// @brief Standard MISB ST 0806 16-byte Universal Label key (RVT Local Set).
/// @details 06 0E 2B 34 02 0B 01 01 0E 01 03 01 02 00 00 00
inline constexpr std::array<std::uint8_t, kRvtUniversalLabelSize> kMisb0806UniversalLabel {
    0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01, 0x0E, 0x01, 0x03, 0x01, 0x02, 0x00, 0x00, 0x00
};

/// @brief Universal Label key for Point of Interest (POI) Local Set.
/// @details 06 0E 2B 34 02 0B 01 01 0E 01 03 01 0C 00 00 00
inline constexpr std::array<std::uint8_t, kRvtUniversalLabelSize> kPoiUniversalLabel {
    0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01, 0x0E, 0x01, 0x03, 0x01, 0x0C, 0x00, 0x00, 0x00
};

/// @brief Universal Label key for Area of Interest (AOI) Local Set.
/// @details 06 0E 2B 34 02 0B 01 01 0E 01 03 01 0D 00 00 00
inline constexpr std::array<std::uint8_t, kRvtUniversalLabelSize> kAoiUniversalLabel {
    0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01, 0x0E, 0x01, 0x03, 0x01, 0x0D, 0x00, 0x00, 0x00
};

/// @brief Universal Label key for User Defined Local Set.
/// @details 06 0E 2B 34 02 0B 01 01 0E 01 03 01 0F 00 00 00
inline constexpr std::array<std::uint8_t, kRvtUniversalLabelSize> kUserDefinedUniversalLabel {
    0x06, 0x0E, 0x2B, 0x34, 0x02, 0x0B, 0x01, 0x01, 0x0E, 0x01, 0x03, 0x01, 0x0F, 0x00, 0x00, 0x00
};

/// @brief Standard prefix length for MISB ST 0806 Universal Label matching (first 12 bytes).
inline constexpr std::size_t kMisb0806PrefixSize { 12U };

/// @enum RvtTag
/// @brief MISB ST 0806 top-level RVT Local Set Tag identifiers.
enum class RvtTag : std::uint32_t {
    Checksum = 1U,                    ///< CRC-32 packet checksum (4 bytes, MPEG-2)
    PrecisionTimeStamp = 2U,          ///< Microseconds since UNIX epoch (8 bytes, UTC)
    PlatformTrueAirspeed = 3U,        ///< Platform true airspeed in m/s (2 bytes)
    PlatformIndicatedAirspeed = 4U,   ///< Platform indicated airspeed in m/s (2 bytes)
    TelemetryAccuracyIndicator = 5U,  ///< Reserved for future use (1 byte)
    FragCircleRadius = 6U,            ///< Fragmentation circle radius in meters (2 bytes)
    FrameCode = 7U,                   ///< 60 Hz frame counter (4 bytes)
    UasLsVersion = 8U,                ///< RVT LS version number, 2 for ST 0806.4 (1 byte)
    VideoDataRate = 9U,               ///< Video data rate in bps or carrier in Hz (4 bytes)
    DigitalVideoFileFormat = 10U,      ///< Video compression string <= 127 bytes
    UserDefinedLs = 11U,              ///< Nested User Defined Local Set
    PointOfInterestLs = 12U,          ///< Nested Point of Interest (POI) Local Set
    AreaOfInterestLs = 13U,           ///< Nested Area of Interest (AOI) Local Set
    AircraftMgrsZone = 14U,           ///< Aircraft MGRS UTM Zone 1..60 (1 byte)
    AircraftMgrsLatBandSquare = 15U,  ///< Aircraft MGRS Lat Band + 100km Grid Square (3 bytes)
    AircraftMgrsEasting = 16U,        ///< Aircraft MGRS Easting in meters (3 bytes uint24)
    AircraftMgrsNorthing = 17U,       ///< Aircraft MGRS Northing in meters (3 bytes uint24)
    FrameCenterMgrsZone = 18U,        ///< Frame Center MGRS UTM Zone 1..60 (1 byte)
    FrameCenterMgrsLatBandSquare = 19U,///< Frame Center MGRS Lat Band + 100km Grid Square (3 bytes)
    FrameCenterMgrsEasting = 20U,     ///< Frame Center MGRS Easting in meters (3 bytes uint24)
    FrameCenterMgrsNorthing = 21U     ///< Frame Center MGRS Northing in meters (3 bytes uint24)
};

/// @enum PoiTag
/// @brief MISB ST 0806 Point of Interest (POI) Local Set element tags.
enum class PoiTag : std::uint32_t {
    PoiNumber = 1U,       ///< Unique POI number (2 bytes uint16, mandatory)
    PoiLatitude = 2U,     ///< POI Latitude in WGS-84 (4 bytes int32, mandatory)
    PoiLongitude = 3U,    ///< POI Longitude in WGS-84 (4 bytes int32, mandatory)
    PoiAltitude = 4U,     ///< POI Altitude MSL in meters (2 bytes uint16)
    PoiType = 5U,         ///< Target identifier (1 byte int8: 1=Friendly, 2=Hostile, 3=Target, 4=Unknown)
    PoiText = 6U,         ///< User defined text string (max 2048 bytes)
    PoiSourceIcon = 7U,   ///< MIL-STD-2525B symbol icon string (max 127 bytes)
    PoiSourceId = 8U,     ///< Source identifier string (max 255 bytes)
    PoiLabel = 9U,        ///< Label string (16 bytes)
    PoiOperationId = 10U  ///< Operation ID string (max 127 bytes)
};

/// @enum AoiTag
/// @brief MISB ST 0806 Area of Interest (AOI) Local Set element tags.
enum class AoiTag : std::uint32_t {
    AoiNumber = 1U,       ///< Unique AOI number (2 bytes uint16, mandatory)
    CornerLat1 = 2U,      ///< NW Corner 1 Latitude in WGS-84 (4 bytes int32, mandatory)
    CornerLon1 = 3U,      ///< NW Corner 1 Longitude in WGS-84 (4 bytes int32, mandatory)
    CornerLat3 = 4U,      ///< SE Corner 3 Latitude in WGS-84 (4 bytes int32, mandatory)
    CornerLon3 = 5U,      ///< SE Corner 3 Longitude in WGS-84 (4 bytes int32, mandatory)
    AoiType = 6U,         ///< Target identifier (1 byte int8: 1=Friendly, 2=Hostile, 3=Reserved, 4=Unknown)
    AoiText = 7U,         ///< User defined text string (max 2048 bytes)
    AoiSourceId = 8U,     ///< Source identifier string (max 255 bytes)
    AoiLabel = 9U,        ///< Label string (16 bytes)
    AoiOperationId = 10U  ///< Operation ID string (max 127 bytes)
};

/// @enum UserDefinedTag
/// @brief MISB ST 0806 User Defined Data Local Set element tags.
enum class UserDefinedTag : std::uint32_t {
    NumericIdType = 1U, ///< Numeric identifier and data type (1 byte uint8, mandatory)
    UserData = 2U       ///< Raw user payload (variable length, mandatory)
};

/// @enum RvtTargetType
/// @brief MISB ST 0806 Target Identifier classification.
enum class RvtTargetType : std::int8_t {
    Friendly = 1,  ///< Friendly entity
    Hostile = 2,   ///< Hostile entity
    Target = 3,    ///< Identified target (or Reserved in AOI)
    Unknown = 4    ///< Unknown entity
};

/// @enum RvtUserDataType
/// @brief MISB ST 0806 User Defined Data type specified in bits 7-8 of Tag 1.
enum class RvtUserDataType : std::uint8_t {
    String = 0U,       ///< 00: String data
    Int = 1U,          ///< 01: Signed integer data
    Uint = 2U,         ///< 10: Unsigned integer data
    Experimental = 3U  ///< 11: Experimental data
};

/// @struct RvtMgrsCoord
/// @brief MGRS coordinates encoded in MISB ST 0806 (Tags 14-17 / 18-21).
struct RvtMgrsCoord {
    std::uint8_t zone { 0U };               ///< UTM zone 1..60
    std::string bandAndGridSquare {};      ///< 3-char band and 100km grid square (e.g. "SMC")
    std::uint32_t eastingM { 0U };          ///< 5-digit Easting in meters [0, 99999]
    std::uint32_t northingM { 0U };         ///< 5-digit Northing in meters [0, 99999]
};

/// @struct PoiPack
/// @brief Strongly-typed representation of a MISB ST 0806 Point of Interest (POI) Pack.
struct PoiPack {
    std::uint16_t poiNumber { 0U };                   ///< Tag 1: Unique POI number
    double latitudeDeg { 0.0 };                      ///< Tag 2: POI latitude [-90.0, +90.0] deg
    double longitudeDeg { 0.0 };                     ///< Tag 3: POI longitude [-180.0, +180.0] deg
    std::optional<double> altitudeMslM {};           ///< Tag 4: Altitude MSL in meters [-900.0, 19000.0]
    std::optional<RvtTargetType> type {};            ///< Tag 5: Entity type
    std::optional<std::string> text {};              ///< Tag 6: Description text
    std::optional<std::string> sourceIcon {};        ///< Tag 7: MIL-STD-2525B icon string
    std::optional<std::string> sourceId {};          ///< Tag 8: Source identifier
    std::optional<std::string> label {};             ///< Tag 9: 16-char label
    std::optional<std::string> operationId {};       ///< Tag 10: Operation ID
};

/// @struct AoiPack
/// @brief Strongly-typed representation of a MISB ST 0806 Area of Interest (AOI) Pack.
struct AoiPack {
    std::uint16_t aoiNumber { 0U };                   ///< Tag 1: Unique AOI number
    GeoPoint2D corner1Nw {};                         ///< Tags 2 & 3: Top-left / NW corner (lat, lon)
    GeoPoint2D corner3Se {};                         ///< Tags 4 & 5: Bottom-right / SE corner (lat, lon)
    RvtTargetType type { RvtTargetType::Unknown };   ///< Tag 6: Entity type
    std::optional<std::string> text {};              ///< Tag 7: Description text
    std::optional<std::string> sourceId {};          ///< Tag 8: Source identifier
    std::optional<std::string> label {};             ///< Tag 9: 16-char label
    std::optional<std::string> operationId {};       ///< Tag 10: Operation ID
};

/// @struct UserDefinedPack
/// @brief Strongly-typed representation of a MISB ST 0806 User Defined Data Pack.
struct UserDefinedPack {
    std::uint8_t numericId { 0U };                               ///< Data item ID [0, 63]
    RvtUserDataType dataType { RvtUserDataType::String };        ///< Data format classification
    std::vector<std::uint8_t> data {};                           ///< Raw data payload
};

/// @struct RvtLocalSet
/// @brief Strongly-typed representation of a MISB ST 0806 RVT Local Set.
struct RvtLocalSet {
    std::optional<std::uint64_t> precisionTimeStampUs {};         ///< Tag 2: Timestamp (UTC microseconds)
    std::optional<std::uint16_t> platformTrueAirspeedMps {};      ///< Tag 3: Platform TAS in m/s
    std::optional<std::uint16_t> platformIndicatedAirspeedMps {}; ///< Tag 4: Platform IAS in m/s
    std::optional<std::uint8_t> telemetryAccuracy {};             ///< Tag 5: Reserved indicator
    std::optional<std::uint16_t> fragCircleRadiusM {};            ///< Tag 6: Fragmentation circle in meters
    std::optional<std::uint32_t> frameCode {};                    ///< Tag 7: 60 Hz frame counter
    std::uint8_t version { 2U };                                  ///< Tag 8: Version (2 for ST 0806.4)
    std::optional<std::uint32_t> videoDataRate {};                ///< Tag 9: Video data rate in bps / Hz
    std::optional<std::string> digitalVideoFileFormat {};         ///< Tag 10: Compression string
    std::optional<RvtMgrsCoord> aircraftMgrs {};                  ///< Tags 14..17: Aircraft MGRS position
    std::optional<RvtMgrsCoord> frameCenterMgrs {};               ///< Tags 18..21: Frame center MGRS position
    std::vector<PoiPack> pois {};                                 ///< Tag 12: Points of Interest
    std::vector<AoiPack> aois {};                                 ///< Tag 13: Areas of Interest
    std::vector<UserDefinedPack> userDefined {};                  ///< Tag 11: User Defined data items
};

} // namespace Klv
