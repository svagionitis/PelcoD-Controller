#pragma once

/// @file SightlineTelemetry.h
/// @brief Sightline SLA Telemetry Module (Reporting rate, telemetry destination routing).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__telemetry.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgCoordinateReportingMode
/// @brief Configure rate and telemetry contents for tracking coordinates (Message ID 0x0B).
struct MsgCoordinateReportingMode {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t framePeriod { 1U }; // 1 = every frame (30/60 Hz), 2 = every 2nd frame
    std::uint8_t reportingFlags { 0x03U }; // Primary + All
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
