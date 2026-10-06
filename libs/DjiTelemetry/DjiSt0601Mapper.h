#pragma once

/// @file DjiSt0601Mapper.h
/// @brief Maps DJI subtitle telemetry onto the MISB ST 0601 UAS Datalink Local Set model.

#include "DjiSubtitleParser.h"
#include "Klv/KlvTypes.h"

#include <cstdint>
#include <optional>
#include <string>

namespace Dji {

/// @struct DjiMapConfig
/// @brief Conversion options for mapToSt0601().
struct DjiMapConfig {
    std::int32_t utcOffsetMin { 0 }; ///< Local time minus UTC, minutes (e.g. +180 for EEST).
    bool absAltIsHae { false }; ///< True: abs_alt -> Tag 75 (HAE); false: Tag 15 (MSL).
    bool deriveFov { false }; ///< Derive Tags 16/17 from focal_len and dzoom_ratio.
    double sensorAspect { 0.0 }; ///< Image width/height; FOV derivation needs > 0.
    std::string platform {}; ///< Tag 10 platform designation; empty = unset.
};

/// @brief Converts one DJI telemetry sample into a UasDatalinkMessage.
/// @details Mapping:
///          - local time - utcOffsetMin -> Tag 2 (precisionTimeStampUs)
///          - latitude / longitude -> Tags 13 / 14 (dropped if out of range or 0/0 "no fix")
///          - abs_alt -> Tag 15 or Tag 75 per @c absAltIsHae (range [-900, 19000] m)
///          - gb_yaw / gb_pitch / gb_roll -> Tags 18 / 19 / 20
///          - focal_len and dzoom_ratio -> Tags 16 / 17 when @c deriveFov is set
///
///          DJI gimbal angles are absolute (north / horizon), whereas Tags 18-20 are relative
///          to the platform. Aircraft attitude is not in the subtitle track, so Tags 5-7 are
///          left unset and Tags 18-20 carry the absolute angles (platform attitude assumed 0).
/// @param[in] sample Parsed DJI telemetry.
/// @param[in] cfg Conversion options.
/// @return Populated message; fields with missing or out-of-range inputs are left empty.
/// @note Pure function; thread-safe.
[[nodiscard]] Klv::UasDatalinkMessage mapToSt0601(const DjiTelemetrySample& sample, const DjiMapConfig& cfg);

/// @brief Derives the local-time UTC offset from a local and a UTC timestamp.
/// @details The difference is rounded to the nearest 15 minutes, which tolerates up to
///          ±7.5 minutes of skew between the two clocks.
/// @param[in] localUs Local wall-clock time, µs since 1970 (no TZ).
/// @param[in] utcUs Matching UTC time, µs since 1970.
/// @return Offset in minutes (local - UTC).
/// @retval std::nullopt Offset outside ±14 hours.
[[nodiscard]] std::optional<std::int32_t> deriveUtcOffset(std::int64_t localUs, std::uint64_t utcUs) noexcept;

/// @brief Wraps an angle into [0, 360).
/// @param[in] deg Angle in degrees (finite).
/// @return Equivalent angle in [0, 360).
[[nodiscard]] double wrapDegrees360(double deg) noexcept;

} // namespace Dji
