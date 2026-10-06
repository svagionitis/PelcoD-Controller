#pragma once

/// @file DjiSubtitleParser.h
/// @brief Parser for DJI per-frame telemetry text (MP4 tx3g samples / SRT cues).

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace Dji {

/// @struct DjiTelemetrySample
/// @brief Telemetry values decoded from one DJI subtitle sample.
/// @details All numeric fields are optional because DJI firmware varies by aircraft, camera
///          and version. Values are stored exactly as written by the aircraft (no range
///          clamping); validation happens in the ST 0601 mapper.
struct DjiTelemetrySample {
    std::optional<std::uint64_t> frameCount {}; ///< "FrameCnt" counter.
    std::optional<std::int64_t> localTimeUs {}; ///< Local wall clock as µs since 1970 (no TZ).
    std::optional<double> focalLenMm {}; ///< "focal_len" (35 mm-equivalent, mm).
    std::optional<double> digitalZoom {}; ///< "dzoom_ratio".
    std::optional<double> latitudeDeg {}; ///< "latitude" (WGS-84 deg).
    std::optional<double> longitudeDeg {}; ///< "longitude" / "longtitude" (WGS-84 deg).
    std::optional<double> relAltM {}; ///< "rel_alt" (m above take-off point).
    std::optional<double> absAltM {}; ///< "abs_alt" (m).
    std::optional<double> gimbalYawDeg {}; ///< "gb_yaw" (absolute, deg).
    std::optional<double> gimbalPitchDeg {}; ///< "gb_pitch" (absolute, deg).
    std::optional<double> gimbalRollDeg {}; ///< "gb_roll" (deg).
    std::map<std::string, std::string> extra {}; ///< Unrecognised keys, raw string values.
};

/// @brief Parses one DJI telemetry text sample.
/// @details Accepts the bracketed "key: value" layout used by DJI enterprise and consumer
///          aircraft, e.g.
///          @code
///          FrameCnt: 0 2026-09-24 15:20:55.713
///          [focal_len: 52.70] [latitude: 38.375988] [longitude: 23.257121]
///          [rel_alt: 20.160 abs_alt: 169.523] [gb_yaw: -77.4 gb_pitch: 7.4 gb_roll: 0.0]
///          @endcode
///          Text before the first '[' is scanned for "FrameCnt:" and a "YYYY-MM-DD HH:MM:SS[.f]"
///          timestamp. Each bracket may hold several pairs; "key : value" (space before the
///          colon) is also accepted. Numbers use std::from_chars (locale-independent, no
///          exceptions). Malformed values are skipped individually.
/// @param[in] text Sample text (UTF-8, LF or CRLF line endings).
/// @return Parsed sample.
/// @retval std::nullopt Neither latitude nor longitude could be parsed.
/// @note Pure function; thread-safe.
[[nodiscard]] std::optional<DjiTelemetrySample> parseDjiText(std::string_view text);

} // namespace Dji
