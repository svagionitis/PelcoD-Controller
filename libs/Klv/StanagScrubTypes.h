#pragma once

/// @file StanagScrubTypes.h
/// @brief Core types and structures for STANAG 4609 telemetry scrubbing and time indexing.

#include "KlvTypes.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Klv {

/// @enum ScrubSeekMode
/// @brief Strategy for seeking during user scrubbing actions.
enum class ScrubSeekMode : std::uint8_t {
    FastKeyframeOnly = 0U,   ///< Seeks only to nearest IDR/keyframe (ultra-fast preview)
    ExactFrameAccurate = 1U, ///< Decodes from nearest keyframe up to target frame (exact)
    AdaptiveScrub = 2U       ///< Keyframe-only while slider dragged, exact when stopped
};

/// @struct VideoIndexEntry
/// @brief Metadata indexing an individual video frame within the MPEG-TS container.
struct VideoIndexEntry {
    std::uint64_t ptsTicks { 0U };        ///< 90 kHz Presentation Timestamp (modulo 2^33)
    std::uint64_t dtsTicks { 0U };        ///< 90 kHz Decode Timestamp
    std::uint64_t fileByteOffset { 0U };  ///< Byte position of TS packet in container
    std::uint32_t packetSizeBytes { 0U }; ///< PES payload size in bytes
    std::uint32_t frameIndex { 0U };      ///< Monotonic frame ordinal index
    bool isKeyframe { false };            ///< True if frame contains IDR / SPS / I-slice
};

/// @struct KlvIndexEntry
/// @brief Metadata indexing an extracted KLV packet within the MPEG-TS container.
struct KlvIndexEntry {
    std::uint64_t ptsTicks { 0U };        ///< 90 kHz PES Presentation Timestamp
    std::uint64_t utcTimestampUs { 0U };  ///< MISB ST 0601 Tag 2 Precision Time Stamp
    std::uint64_t fileByteOffset { 0U };  ///< Byte position in file
    std::uint32_t packetSizeBytes { 0U }; ///< KLV byte payload size
    std::uint32_t messageIndex { 0U };    ///< Index into decoded message table
    std::optional<UasDatalinkMessage> message {}; ///< Cached decoded telemetry message
};

/// @struct SynchronizedScrubFrame
/// @brief Coordinated video frame presentation timestamp and decoded KLV telemetry state.
struct SynchronizedScrubFrame {
    std::uint64_t ptsTicks { 0U };            ///< Synchronized 90 kHz PTS
    double timeSeconds { 0.0 };               ///< Presentation time in seconds from origin
    std::uint64_t utcTimestampUs { 0U };      ///< Microsecond UTC epoch
    UasDatalinkMessage telemetry {};          ///< Decoded or interpolated telemetry message
    std::int64_t skewDeltaUs { 0 };           ///< Skew between video and telemetry (<= 50,000 us)
    bool isKeyframe { false };                ///< Whether current frame is an IDR/keyframe
    bool isInterpolated { false };            ///< True if telemetry was synthesized via interpolation
};

/// @struct ScrubStats
/// @brief Runtime diagnostics and indexing telemetry.
struct ScrubStats {
    std::size_t totalVideoFrames { 0U };
    std::size_t totalKeyframes { 0U };
    std::size_t totalKlvPackets { 0U };
    double indexDurationSec { 0.0 };
    double averageGopLengthMs { 0.0 };
    double averageKlvRateHz { 0.0 };
    std::size_t cacheHitCount { 0U };
    std::size_t cacheMissCount { 0U };
};

} // namespace Klv
