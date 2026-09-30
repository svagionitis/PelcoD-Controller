#pragma once

/// @file SightlineTelemetry.h
/// @brief Sightline SLA Telemetry Module (Reporting rate, telemetry destination routing).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__telemetry.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgCoordinateReportingMode
/// @brief Configure rate and telemetry contents for tracking coordinates (Message ID 0x0B).
/// @details Official Sightline SLACoordinateReportingMode_t: framePeriod(u8), flags(u16 LE), cameraIndex(u8).
struct MsgCoordinateReportingMode {
    std::uint8_t framePeriod { 1U }; ///< 1 = every frame (30/60 Hz), 2 = every 2nd frame
    std::uint16_t flags { 0x0003U }; ///< Reporting bits: 0x0001 primary, 0x0002 all tracks
    std::uint8_t cameraIndex { 0U }; ///< Camera index
    std::uint8_t reportingFlags { 0x03U }; ///< Backward compatibility alias for flags
};

/// @struct MsgSetTelemetryDestination
/// @brief Register external client IP/port for dedicated telemetry stream (Message ID 0x64).
struct MsgSetTelemetryDestination {
    std::uint8_t clientIndex { 0U }; // 0 to 3 (up to 4 clients)
    std::uint32_t clientIpAddress { 0U };
    std::uint16_t clientPort { 14002U };
    std::uint8_t flags { 0x01U }; // 0x01: Enable telemetry
};

} // namespace Sightline
