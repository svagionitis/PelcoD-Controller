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
    SafetyBroadcast14 = 14U,
    ClassBPosition18 = 18U,
    ClassBExtendedPosition19 = 19U,
    ClassBStatic24 = 24U
};

/// @enum AisBeaconType
/// @brief Classification of AIS transponder based on ITU-R M.585-7 MMSI numbering formats.
enum class AisBeaconType : std::uint8_t {
    None = 0U,
    StandardVessel = 1U,
    AisSart = 2U, ///< 970 prefix (Search and Rescue Transponder)
    AisMob = 3U, ///< 972 prefix (Man Overboard Device)
    EpirbAis = 4U, ///< 974 prefix (Float-Free EPIRB with AIS)
    SarAircraft = 5U ///< 111 prefix (Search and Rescue Aircraft)
};

/// @brief Classifies an MMSI into an emergency beacon or standard vessel type.
/// @param[in] mmsi 9-digit Maritime Mobile Service Identity.
/// @return Classified AisBeaconType.
[[nodiscard]] constexpr AisBeaconType classifyAisMmsi(std::uint32_t mmsi) noexcept
{
    if (mmsi == 0U) {
        return AisBeaconType::None;
    }
    const std::uint32_t prefix = mmsi / 1000000U;
    if (prefix == 970U) {
        return AisBeaconType::AisSart;
    }
    if (prefix == 972U) {
        return AisBeaconType::AisMob;
    }
    if (prefix == 974U) {
        return AisBeaconType::EpirbAis;
    }
    if (prefix == 111U) {
        return AisBeaconType::SarAircraft;
    }
    return AisBeaconType::StandardVessel;
}

/// @brief Checks if an MMSI belongs to an active life-saving emergency distress beacon (SART, MOB, EPIRB).
/// @param[in] mmsi 9-digit Maritime Mobile Service Identity.
/// @return True if SART, MOB, or EPIRB.
[[nodiscard]] constexpr bool isAisEmergencyBeacon(std::uint32_t mmsi) noexcept
{
    const auto type = classifyAisMmsi(mmsi);
    return type == AisBeaconType::AisSart || type == AisBeaconType::AisMob || type == AisBeaconType::EpirbAis
        || type == AisBeaconType::SarAircraft;
}

/// @struct AisSafetyBroadcast
/// @brief Safety-related broadcast message unpacked from AIS Message 14.
struct AisSafetyBroadcast {
    std::uint32_t mmsi { 0U };
    std::string text {};
    bool isTestMode { false };
    std::chrono::steady_clock::time_point timestamp { std::chrono::steady_clock::now() };
};

/// @struct AisEmergencyAlert
/// @brief Telemetry snapshot for an activated AIS-SART, AIS-MOB, or EPIRB distress device.
struct AisEmergencyAlert {
    std::uint32_t mmsi { 0U };
    AisBeaconType beaconType { AisBeaconType::StandardVessel };
    NmeaCoordinates coordinates {};
    bool positionValid { false };
    double speedOverGroundKnots { 0.0 };
    double courseOverGroundDegrees { 0.0 };
    bool isTestMode { false };
    std::string messageText {};
    std::chrono::steady_clock::time_point timestamp { std::chrono::steady_clock::now() };
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
    bool staticDataValid { false }; ///< True if static voyage data has been decoded
    // Safety & Emergency Broadcasts (Type 14, SART, MOB, EPIRB)
    std::string safetyText {}; ///< Safety-related broadcast message text
    AisBeaconType beaconType { AisBeaconType::StandardVessel }; ///< Transponder classification
    bool isEmergencyBeacon { false }; ///< True if MMSI indicates SART, MOB, EPIRB or SART Active status
    bool isEmergencyTestMode { false }; ///< True if emergency burst is test-mode

    std::chrono::steady_clock::time_point lastUpdate { std::chrono::steady_clock::now() };

    /// @brief Converts vessel snapshot into an AisEmergencyAlert structure.
    [[nodiscard]] AisEmergencyAlert toEmergencyAlert() const noexcept
    {
        AisEmergencyAlert alert {};
        alert.mmsi = mmsi;
        alert.beaconType = beaconType;
        alert.coordinates = coordinates;
        alert.positionValid = positionValid;
        alert.speedOverGroundKnots = speedOverGroundKnots;
        alert.courseOverGroundDegrees = courseOverGroundDegrees;
        alert.isTestMode = isEmergencyTestMode;
        alert.messageText = safetyText;
        alert.timestamp = lastUpdate;
        return alert;
    }
};

} // namespace Nmea
