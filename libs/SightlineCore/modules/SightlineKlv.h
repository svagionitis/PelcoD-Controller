#pragma once

/// @file SightlineKlv.h
/// @brief Sightline SLA KLV Metadata Module (STANAG 4609 / MISB metadata embedding and CoT).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__klv.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <string>

namespace Sightline {

/// @struct MsgSetMetadataValues
/// @brief Dynamic platform and target telemetry for KLV insertion (Message ID 0x13).
struct MsgSetMetadataValues {
    double platformLatitudeDeg { 0.0 };
    double platformLongitudeDeg { 0.0 };
    double platformAltitudeMeters { 0.0 };
    double platformHeadingDeg { 0.0 };
    double platformPitchDeg { 0.0 };
    double platformRollDeg { 0.0 };
    double sensorHorizontalFovDeg { 0.0 };
    double sensorVerticalFovDeg { 0.0 };
};

/// @struct MsgMetadataStaticValues
/// @brief Static mission identifiers and classification markings (Message ID 0x14).
struct MsgMetadataStaticValues {
    std::string missionId {};
    std::string platformTailNumber {};
    std::string securityClassification { "UNCLASSIFIED" };
};

/// @struct MsgSetMetadataRate
/// @brief Telemetry output rate and MISB packet frequency (Message ID 0x62).
struct MsgSetMetadataRate {
    std::uint8_t metadataType { 0U }; // 0: Synchronous KLV, 1: Asynchronous
    std::uint8_t ratePeriod { 1U }; // Rate divisor
};

/// @struct MsgCursorOnTarget
/// @brief Cursor-on-Target (CoT) XML tactical message broadcast (Message ID 0xB0).
struct MsgCursorOnTarget {
    std::uint8_t enable { 1U };
    std::uint16_t broadcastPort { 1870U };
    std::string uid {};
    std::string cotType { "a-f-G-E-V-C" }; // Military vehicle symbol
};

} // namespace Sightline
