#pragma once

/// @file NmeaTypes.h
/// @brief Strongly-typed data models and enums for NMEA 0183 / IEC 61162-1 navigation and tracking sentences.

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace Nmea {

/// @enum NmeaSentenceId
/// @brief Supported standard and proprietary sentence identifiers.
enum class NmeaSentenceId : std::uint8_t {
    Unknown = 0,
    GGA, ///< GPS Fix Data (lat, lon, alt, quality)
    RMC, ///< Recommended Minimum Specific GNSS Data (lat, lon, sog, cog)
    HDT, ///< Heading - True (gyrocompass heading)
    THS, ///< True Heading and Status
    TTM, ///< Tracked Target Message (ARPA radar target)
    TLL, ///< Target Latitude and Longitude
    XDR, ///< Transducer Measurement (pitch, roll, environmental)
    VDM, ///< AIS VHF Data-link Message
    VDO, ///< AIS VHF Own-vessel Data-link Message
    PFEC ///< FLIR Marine proprietary PTZ camera dialect
};

/// @enum NmeaFixQuality
/// @brief GPS / GNSS operational fix quality reported in GGA sentences.
enum class NmeaFixQuality : std::uint8_t {
    Invalid = 0U,
    GpsFix = 1U,
    DgpsFix = 2U,
    PpsFix = 3U,
    RtkFixed = 4U,
    RtkFloat = 5U,
    Estimated = 6U,
    Manual = 7U,
    Simulation = 8U
};

/// @enum NmeaFaaMode
/// @brief FAA operational mode indicator defined in NMEA 2.3+.
enum class NmeaFaaMode : char {
    Autonomous = 'A',
    Differential = 'D',
    Estimated = 'E',
    Manual = 'M',
    Simulated = 'S',
    NotValid = 'N',
    Precise = 'P'
};

/// @enum TtmTargetStatus
/// @brief ARPA radar target tracking status reported in TTM sentences.
enum class TtmTargetStatus : char {
    Query = 'Q', ///< Target being initially evaluated / acquired
    Tracking = 'T', ///< Target actively tracked
    Lost = 'L' ///< Target lost / no radar echo
};

/// @enum TtmReference
/// @brief Reference frame for radar target bearing and speed.
enum class TtmReference : char {
    True = 'T', ///< Referenced to True North
    Relative = 'R' ///< Referenced to Own Ship's Head
};

/// @struct NmeaUtcTime
/// @brief Represents time of day in UTC parsed from hhmmss.ss format.
struct NmeaUtcTime {
    std::uint8_t hour { 0U };
    std::uint8_t minute { 0U };
    std::uint8_t second { 0U };
    std::uint16_t millisecond { 0U };
};

/// @struct NmeaDate
/// @brief Represents calendar date parsed from ddmmyy format.
struct NmeaDate {
    std::uint8_t day { 0U };
    std::uint8_t month { 0U };
    std::uint16_t year { 0U }; ///< 4-digit year (e.g. 2026)
};

/// @struct NmeaCoordinates
/// @brief Geodetic latitude and longitude coordinates in decimal degrees.
struct NmeaCoordinates {
    double latitudeDeg { 0.0 }; ///< Latitude [-90.0 .. +90.0], North positive, South negative
    double longitudeDeg { 0.0 }; ///< Longitude [-180.0 .. +180.0], East positive, West negative
};

/// @struct GgaData
/// @brief Telemetry unpacked from $--GGA Global Positioning System Fix Data sentence.
struct GgaData {
    NmeaUtcTime utcTime {};
    NmeaCoordinates coordinates {};
    NmeaFixQuality fixQuality { NmeaFixQuality::Invalid };
    std::uint8_t numSatellites { 0U };
    double hdop { 99.9 };
    double altitudeMeters { 0.0 };
    double geoidalSeparationMeters { 0.0 };
    double dgpsAgeSeconds { 0.0 };
    std::uint16_t dgpsStationId { 0U };
    bool valid { false };
};

/// @struct RmcData
/// @brief Telemetry unpacked from $--RMC Recommended Minimum Specific GNSS Data sentence.
struct RmcData {
    NmeaUtcTime utcTime {};
    bool statusActive { false }; ///< True if 'A' (Active/Valid), false if 'V' (Void/Warning)
    NmeaCoordinates coordinates {};
    double speedOverGroundKnots { 0.0 };
    double courseOverGroundDegrees { 0.0 }; ///< [0.0 .. 360.0)
    NmeaDate date {};
    double magneticVariationDegrees { 0.0 }; ///< Positive East, Negative West
    NmeaFaaMode faaMode { NmeaFaaMode::NotValid };
    bool valid { false };
};

/// @struct HdtData
/// @brief Telemetry unpacked from $--HDT Heading - True sentence.
struct HdtData {
    double headingDegrees { 0.0 }; ///< Gyrocompass true heading [0.0 .. 360.0)
    bool valid { false };
};

/// @struct ThsData
/// @brief Telemetry unpacked from $--THS True Heading and Status sentence.
struct ThsData {
    double headingDegrees { 0.0 }; ///< True heading [0.0 .. 360.0)
    NmeaFaaMode mode { NmeaFaaMode::NotValid };
    bool valid { false };
};

/// @struct TtmData
/// @brief Radar track telemetry unpacked from $--TTM Tracked Target Message sentence.
struct TtmData {
    std::uint32_t targetNumber { 0U }; ///< Target tracking number (00 to 99)
    double targetDistanceNmi { 0.0 }; ///< Distance to target in nautical miles (or km if unit specified)
    double bearingDegrees { 0.0 }; ///< Bearing to target [0.0 .. 360.0)
    TtmReference bearingReference { TtmReference::True };
    double targetSpeedKnots { 0.0 }; ///< Target speed in knots
    double targetCourseDegrees { 0.0 }; ///< Target course [0.0 .. 360.0)
    TtmReference courseReference { TtmReference::True };
    double distanceCpaNmi { 0.0 }; ///< Distance at closest point of approach
    double timeCpaMinutes { 0.0 }; ///< Time to CPA (minutes, negative indicates past)
    char speedDistanceUnits { 'K' }; ///< 'K'=Knots/NM, 'S'=Statute Miles, 'M'=km
    std::string targetName {}; ///< Target identifier name or label
    TtmTargetStatus status { TtmTargetStatus::Lost };
    bool referenceTarget { false }; ///< 'R' if reference target, 'T' otherwise
    NmeaUtcTime utcTimeTag {};
    char acquisitionType { 'A' }; ///< 'A'=Auto, 'M'=Manual
    bool valid { false };
};

/// @struct TllData
/// @brief Target geographic coordinate telemetry unpacked from $--TLL sentence.
struct TllData {
    std::uint32_t targetNumber { 0U };
    NmeaCoordinates coordinates {};
    std::string targetName {};
    NmeaUtcTime utcTimeTag {};
    TtmTargetStatus status { TtmTargetStatus::Lost };
    bool referenceTarget { false };
    bool valid { false };
};

/// @struct XdrTransducer
/// @brief Single sensor transducer entry in an $--XDR sentence.
struct XdrTransducer {
    char type { '\0' }; ///< 'A'=Angular, 'C'=Temperature, 'P'=Pressure, etc.
    double measurement { 0.0 }; ///< Sensor value
    char units { '\0' }; ///< 'D'=Degrees, 'C'=Celsius, 'B'=Bar, etc.
    std::string id {}; ///< Transducer name (e.g. "PITCH", "ROLL")
};

/// @struct XdrData
/// @brief Transducer telemetry unpacked from $--XDR sentence.
struct XdrData {
    std::vector<XdrTransducer> transducers {};
    bool valid { false };
};

/// @struct NmeaNavSnapshot
/// @brief Unified own-ship navigation snapshot synthesized from GPS, gyro, and transducer sentences.
struct NmeaNavSnapshot {
    NmeaCoordinates position {};
    double altitudeMeters { 0.0 };
    double trueHeadingDegrees { 0.0 };
    double sogKnots { 0.0 };
    double cogDegrees { 0.0 };
    double pitchDegrees { 0.0 };
    double rollDegrees { 0.0 };
    NmeaFixQuality fixQuality { NmeaFixQuality::Invalid };
    std::chrono::steady_clock::time_point timestamp {};
    bool hasPosition { false };
    bool hasHeading { false };
    bool hasAttitude { false };
};

} // namespace Nmea
