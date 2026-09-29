#pragma once

/// @file NmeaJsonSerializer.h
/// @brief Fast, zero-external-dependency JSON serializer and parser for marine telemetry and commands.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Nmea::Network {

/// @struct SlewCommand
/// @brief Parsed slew-to-cue or PTZ command received from web dashboard.
struct SlewCommand {
    std::string commandType {}; ///< "slewToCue", "ptzMove", "zoom", "stop"
    double targetLat { 0.0 };
    double targetLon { 0.0 };
    double panAngle { 0.0 };
    double tiltAngle { 0.0 };
    double zoomLevel { 1.0 };
};

/// @class NmeaJsonSerializer
/// @brief Fast JSON serialization utility for streaming vessel and camera telemetry to HTML5 dashboards.
class NmeaJsonSerializer {
public:
    /// @brief Serializes vessel navigation telemetry into a JSON payload.
    [[nodiscard]] static std::string serializeVessel(double lat, double lon, double sog, double cog,
                                                     double hdg, double pitch = 0.0, double roll = 0.0,
                                                     double depth = 0.0);

    /// @brief Serializes PTZ camera gimbal telemetry into a JSON payload.
    [[nodiscard]] static std::string serializeGimbal(double pan, double tilt, double zoom, double hfov,
                                                     bool tracking = false, int targetId = -1);

    /// @brief Serializes a radar/AIS tracking target into a JSON payload.
    [[nodiscard]] static std::string serializeTarget(int id, double bearing, double range,
                                                     double cpa, double tcpa, std::string_view threat);

    /// @brief Serializes a BAM alert record into a JSON payload.
    [[nodiscard]] static std::string serializeAlert(std::uint32_t alertId, std::uint32_t instance,
                                                    std::string_view type, std::string_view category,
                                                    std::string_view state, std::string_view description);

    /// @brief Parses an inbound JSON command from a web dashboard.
    /// @param[in] jsonText Valid JSON string.
    /// @return SlewCommand struct if parsed, nullopt otherwise.
    [[nodiscard]] static std::optional<SlewCommand> parseCommand(std::string_view jsonText);
};

} // namespace Nmea::Network
