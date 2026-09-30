#pragma once

/// @file SightlineTelemetryParser.h
/// @brief Parser deserializing raw Sightline SLA platform telemetry and KLV metadata frames.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineTelemetryParser
/// @brief Deserializes platform telemetry and KLV metadata values.
class SightlineTelemetryParser {
public:
    /// @brief Parses active platform telemetry metadata (Message ID 0x8B).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized metadata values.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseMetadataValues(
        const std::vector<std::uint8_t>& packet, MsgSetMetadataValues& out);

    /// @brief Parses coordinate reporting mode configuration (Message ID 0x0B).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized reporting mode.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCoordReportingMode(
        const std::vector<std::uint8_t>& packet, MsgCoordinateReportingMode& out);

    /// @brief Parses telemetry destination parameters (Message ID 0x64).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized destination parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseTelemetryDestination(
        const std::vector<std::uint8_t>& packet, MsgSetTelemetryDestination& out);
};

} // namespace Sightline
