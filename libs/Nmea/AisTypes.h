#pragma once

/// @file AisTypes.h
/// @brief Strongly-typed data models and enums for AIS (ITU-R M.1371 / IEC 62320-1) vessel telemetry.

#include "NmeaTypes.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace Nmea {

/// @enum AisNavStatus
/// @brief Vessel navigation status reported in Class A position reports.
enum class AisNavStatus : std::uint8_t {
    UnderWayUsingEngine = 0U,
    AtAnchor = 1U,
    NotUnderCommand = 2U,
    RestrictedManoeuvrability = 3U,
    ConstrainedByDraught = 4U,
    Moored = 5U,
    Aground = 6U,
    EngagedInFishing = 7U,
    UnderWaySailing = 8U,
    ReservedHsc = 9U,
    ReservedWig = 10U,
    Reserved11 = 11U,
    Reserved12 = 12U,
    Reserved13 = 13U,
    AisSartActive = 14U,
    NotDefined = 15U
};

/// @enum AisMessageType
/// @brief Supported ITU-R M.1371 message types.
enum class AisMessageType : std::uint8_t {
    Unknown = 0U,
    ClassAPosition1 = 1U,
    ClassAPosition2 = 2U,
    ClassAPosition3 = 3U,
    ClassAStaticVoyage5 = 5U,
    ClassBPosition18 = 18U,
    ClassBExtendedPosition19 = 19U,
    ClassBStatic24 = 24U
};

/// @struct AisDimensions
/// @brief Physical vessel dimensions derived from distance to reference antenna.
struct AisDimensions {
    std::uint16_t toBow { 0U }; ///< Distance from reference point to bow (meters)
    std::uint16_t toStern { 0U }; ///< Distance from reference point to stern (meters)
    std::uint8_t toPort { 0U }; ///< Distance from reference point to port (meters)
    std::uint8_t toStarboard { 0U }; ///< Distance from reference point to starboard (meters)

    /// @brief Total calculated vessel length in meters.
    [[nodiscard]] constexpr std::uint16_t lengthMeters() const noexcept
    {
        return static_cast<std::uint16_t>(toBow + toStern);
    }

    /// @brief Total calculated vessel beam (width) in meters.
    [[nodiscard]] constexpr std::uint8_t beamMeters() const noexcept
    {
        return static_cast<std::uint8_t>(toPort + toStarboard);
    }
};

/// @struct AisVesselTarget
/// @brief Comprehensive state snapshot for an AIS-tracked maritime vessel.
struct AisVesselTarget {
    std::uint32_t mmsi { 0U }; ///< 9-digit Maritime Mobile Service Identity
    AisMessageType messageType { AisMessageType::Unknown }; ///< Type of most recent message received
    std::uint8_t repeatIndicator { 0U }; ///< Broadcast repeat count (0-3)

    // Position & Kinematics (Types 1, 2, 3, 18, 19)
    AisNavStatus navStatus { AisNavStatus::NotDefined };
    NmeaCoordinates coordinates {}; ///< Geodetic position in decimal degrees
    double speedOverGroundKnots { 0.0 }; ///< Speed over ground in knots (0.0 to 102.2)
    double courseOverGroundDegrees { 0.0 }; ///< COG [0.0 .. 360.0)
    double trueHeadingDegrees { 0.0 }; ///< True heading [0.0 .. 360.0)
    double rateOfTurnDegPerMin { 0.0 }; ///< Rate of turn in degrees/minute (+ right, - left)
    std::uint8_t utcSecond { 60U }; ///< Timestamp second (0-59, 60=not available)
    bool positionAccuracyHigh { false }; ///< True if GNSS DGPS accuracy < 10m
    bool positionValid { false }; ///< True if valid coordinates were unpacked

    // Static & Voyage Metadata (Types 5, 19, 24)
    std::string vesselName {}; ///< Cleaned vessel name
    std::string callSign {}; ///< Radio call sign
    std::uint32_t imoNumber { 0U }; ///< IMO ship number
    std::uint8_t shipType { 0U }; ///< Cargo / vessel type code
    AisDimensions dimensions {}; ///< Hull dimensions in meters
    double draughtMeters { 0.0 }; ///< Maximum static draught in meters
    std::string destination {}; ///< Voyage destination port / label
    bool staticDataValid { false }; ///< True if static data has been received

    std::chrono::steady_clock::time_point lastUpdate { std::chrono::steady_clock::now() };
};

} // namespace Nmea
