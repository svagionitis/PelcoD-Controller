/// @file NmeaJsonSerializer.cpp
/// @brief Implementation of fast JSON serialization and command parsing for marine telemetry.

#include "NmeaJsonSerializer.h"

#include <cstdio>
#include <sstream>

namespace Nmea::Network {

namespace {

std::string escapeJson(std::string_view s)
{
    std::string out {};
    out.reserve(s.size() + 8U);
    for (const char c : s) {
        if (c == '"') {
            out += "\\\"";
        } else if (c == '\\') {
            out += "\\\\";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::optional<std::string> extractJsonString(std::string_view json, std::string_view key)
{
    std::string needle { "\"" };
    needle.append(key);
    needle.append("\":\"");
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    const auto start = pos + needle.size();
    const auto end = json.find('"', start);
    if (end == std::string_view::npos) {
        return std::nullopt;
    }
    return std::string { json.substr(start, end - start) };
}

std::optional<double> extractJsonNumber(std::string_view json, std::string_view key)
{
    std::string needle { "\"" };
    needle.append(key);
    needle.append("\":");
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    auto start = pos + needle.size();
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) {
        ++start;
    }
    auto end = start;
    while (end < json.size() && (json[end] == '-' || json[end] == '+' || json[end] == '.' ||
                                 (json[end] >= '0' && json[end] <= '9') || json[end] == 'e' || json[end] == 'E')) {
        ++end;
    }
    if (end <= start) {
        return std::nullopt;
    }
    try {
        return std::stod(std::string { json.substr(start, end - start) });
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

std::string NmeaJsonSerializer::serializeVessel(double lat, double lon, double sog, double cog,
                                                double hdg, double pitch, double roll, double depth)
{
    char buf[256] {};
    std::snprintf(buf, sizeof(buf),
                  "{\"type\":\"vessel\",\"lat\":%.6f,\"lon\":%.6f,\"sog\":%.2f,\"cog\":%.2f,\"hdg\":%.2f,"
                  "\"pitch\":%.2f,\"roll\":%.2f,\"depth\":%.2f}",
                  lat, lon, sog, cog, hdg, pitch, roll, depth);
    return std::string { buf };
}

std::string NmeaJsonSerializer::serializeGimbal(double pan, double tilt, double zoom, double hfov,
                                                bool tracking, int targetId)
{
    char buf[256] {};
    std::snprintf(buf, sizeof(buf),
                  "{\"type\":\"gimbal\",\"pan\":%.2f,\"tilt\":%.2f,\"zoom\":%.2f,\"hfov\":%.2f,"
                  "\"tracking\":%s,\"targetId\":%d}",
                  pan, tilt, zoom, hfov, tracking ? "true" : "false", targetId);
    return std::string { buf };
}

std::string NmeaJsonSerializer::serializeTarget(int id, double bearing, double range,
                                                double cpa, double tcpa, std::string_view threat)
{
    char buf[256] {};
    std::snprintf(buf, sizeof(buf),
                  "{\"type\":\"target\",\"id\":%d,\"bearing\":%.2f,\"range\":%.2f,"
                  "\"cpa\":%.2f,\"tcpa\":%.2f,\"threat\":\"%s\"}",
                  id, bearing, range, cpa, tcpa, escapeJson(threat).c_str());
    return std::string { buf };
}

std::string NmeaJsonSerializer::serializeAlert(std::uint32_t alertId, std::uint32_t instance,
                                               std::string_view type, std::string_view category,
                                               std::string_view state, std::string_view description)
{
    std::ostringstream oss;
    oss << "{\"type\":\"alert\",\"alertId\":" << alertId
        << ",\"instance\":" << instance
        << ",\"alertType\":\"" << escapeJson(type) << "\""
        << ",\"category\":\"" << escapeJson(category) << "\""
        << ",\"state\":\"" << escapeJson(state) << "\""
        << ",\"desc\":\"" << escapeJson(description) << "\"}";
    return oss.str();
}

std::optional<SlewCommand> NmeaJsonSerializer::parseCommand(std::string_view jsonText)
{
    const auto cmdOpt = extractJsonString(jsonText, "cmd");
    if (!cmdOpt.has_value()) {
        return std::nullopt;
    }

    SlewCommand cmd {};
    cmd.commandType = *cmdOpt;

    if (const auto lat = extractJsonNumber(jsonText, "lat")) {
        cmd.targetLat = *lat;
    }
    if (const auto lon = extractJsonNumber(jsonText, "lon")) {
        cmd.targetLon = *lon;
    }
    if (const auto pan = extractJsonNumber(jsonText, "pan")) {
        cmd.panAngle = *pan;
    }
    if (const auto tilt = extractJsonNumber(jsonText, "tilt")) {
        cmd.tiltAngle = *tilt;
    }
    if (const auto zoom = extractJsonNumber(jsonText, "zoom")) {
        cmd.zoomLevel = *zoom;
    }

    return cmd;
}

} // namespace Nmea::Network
