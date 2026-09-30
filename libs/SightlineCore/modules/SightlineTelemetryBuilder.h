#pragma once

/// @file SightlineTelemetryBuilder.h
/// @brief Serializer for Sightline platform metadata, KLV insertion, and telemetry streaming commands.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineTelemetryBuilder
/// @brief Encodes MISB KLV platform metadata, mission tags, telemetry rates, and Cursor-on-Target XML.
class SightlineTelemetryBuilder {
public:
    /// @brief Encodes platform position telemetry for KLV insertion (Message ID 0x13).
    /// @param[in] msg Metadata values.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMetadataValues(
        const MsgSetMetadataValues& msg);

    /// @brief Encodes static mission and classification metadata (Message ID 0x14).
    /// @param[in] msg Static metadata values.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildMetadataStaticValues(
        const MsgMetadataStaticValues& msg);

    /// @brief Encodes KLV transmission rate (Message ID 0x62).
    /// @param[in] msg Metadata rate parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMetadataRate(
        const MsgSetMetadataRate& msg);

    /// @brief Encodes external telemetry destination IP/port (Message ID 0x64).
    /// @param[in] msg Destination parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTelemetryDest(
        const MsgSetTelemetryDestination& msg);

    /// @brief Encodes coordinate reporting mode configuration (Message ID 0x0B).
    /// @param[in] msg Reporting parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetReportingMode(
        const MsgCoordinateReportingMode& msg);

    /// @brief Encodes Cursor-on-Target XML tactical broadcast (Message ID 0xB0).
    /// @param[in] msg CoT parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCursorOnTarget(
        const MsgCursorOnTarget& msg);

    /// @brief Encodes query for coordinate reporting mode (Message ID 0x28 query 0x0B).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCoordReportingMode(
        std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for telemetry destination (Message ID 0x28 query 0x64).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTelemetryDest();
};

} // namespace Sightline
