#pragma once

/// @file DjiTelemetrySource.h
/// @brief Facade: DJI MP4 file -> time-aligned MISB ST 0601 messages.

#include "Klv/KlvTypes.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace Dji {

/// @struct TimedTelemetry
/// @brief One telemetry message with its presentation time in the video.
struct TimedTelemetry {
    double timeSec { 0.0 }; ///< Presentation time from the text track (seconds).
    Klv::UasDatalinkMessage message {}; ///< Mapped ST 0601 telemetry.
};

/// @struct DjiLoadOptions
/// @brief Options for loadDjiTrack().
struct DjiLoadOptions {
    std::optional<std::int32_t> utcOffsetMin {}; ///< Manual local-UTC offset; unset = auto-derive.
    bool absAltIsHae { false }; ///< Map abs_alt to Tag 75 instead of Tag 15.
    bool deriveFov { true }; ///< Derive Tags 16/17 when aspect is known.
};

/// @brief Returns true if the file starts with an ISO-BMFF 'ftyp' box.
/// @param[in] path File system path.
/// @return True if bytes 4..7 are "ftyp".
[[nodiscard]] bool isMp4File(std::string_view path);

/// @brief Loads every DJI telemetry sample from an MP4 file's tx3g track.
/// @details Reads only the moov box and the text samples (about 220 bytes per frame), never
///          the video payload. Sample times come from the track's stts/elst tables, so entries
///          line up exactly with video frames. When @c opts.utcOffsetMin is unset, the offset
///          is derived from the first sample's local time and the mvhd creation time (UTC),
///          falling back to 0. The video track aspect ratio feeds FOV derivation and the
///          ilst encoder string feeds Tag 10. Samples that fail to parse are skipped.
/// @param[in] path File system path.
/// @param[out] out Cleared, then filled with entries sorted by time.
/// @param[in] opts Conversion options.
/// @return True if at least one sample was produced.
[[nodiscard]] bool loadDjiTrack(
    std::string_view path, std::vector<TimedTelemetry>& out, const DjiLoadOptions& opts = DjiLoadOptions {});

} // namespace Dji
