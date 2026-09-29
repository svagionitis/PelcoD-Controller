#pragma once

/// @file ReplayTypes.h
/// @brief Telemetry types, state enumeration, and record descriptors for the Maritime Replay Engine.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace Nmea {

/// @enum ReplayState
/// @brief Current operational state of the maritime replay engine.
enum class ReplayState : std::uint8_t {
    Stopped = 0U, ///< Idle at start or stopped
    Playing = 1U, ///< Active playback thread pacing sentences
    Paused = 2U, ///< Suspended at current offset
    Finished = 3U ///< Reached end of log file (when not in loop mode)
};

/// @enum TimestampSource
/// @brief Source used to calculate sentence dispatch timing.
enum class TimestampSource : std::uint8_t {
    TagBlock = 0U, ///< IEC 61162-1 \c:<timestamp>*hh\ UNIX epoch tag
    LogPrefix = 1U, ///< Leading [YYYY-MM-DD hh:mm:ss.zzz] syslog / ISO timestamp
    SentenceUtc = 2U, ///< $--GGA, $--RMC, or $--ZDA UTC field
    Synthesized = 3U ///< Fallback fixed-interval pacing
};

/// @struct ReplayRecord
/// @brief Indexed log sentence with relative and absolute timing metadata.
struct ReplayRecord {
    std::chrono::microseconds offsetFromStart { 0 };
    std::optional<std::chrono::system_clock::time_point> absoluteTime {};
    TimestampSource timeSource { TimestampSource::Synthesized };
    std::string sentence {};
};

/// @struct ReplayProgress
/// @brief Real-time status telemetry of the replay playback progress.
struct ReplayProgress {
    ReplayState state { ReplayState::Stopped };
    std::chrono::microseconds currentOffset { 0 };
    std::chrono::microseconds totalDuration { 0 };
    std::size_t currentLine { 0U };
    std::size_t totalLines { 0U };
    double speedMultiplier { 1.0 };
    double progressRatio { 0.0 };
    bool isLooping { false };
};

} // namespace Nmea
