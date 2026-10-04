#pragma once

/// @file PtsSyncTypes.h
/// @brief Configuration, enumerations, and statistics for STANAG 4609 PTS synchronization.

#include "KlvTypes.h"
#include "VideoTypes.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Klv {

/// @enum TimeBaseMode
/// @brief Master time base reference mode.
enum class TimeBaseMode : std::uint8_t {
    SystemEpoch = 0U,       ///< System UTC clock (synchronized via NTP / PTP)
    MonotonicOffset = 1U,   ///< Steady monotonic clock with calibrated UTC offset
    ExternalClock = 2U      ///< User-supplied external PTP/IRIG-B hardware clock
};

/// @enum PtsSyncPolicy
/// @brief Strategy for reconciling video and telemetry timestamps exceeding 50 ms skew.
enum class PtsSyncPolicy : std::uint8_t {
    StrictClamp = 0U,       ///< Clamps metadata PTS to within [videoPts - 50ms, videoPts + 50ms]
    NearestFrame = 1U,      ///< Snaps metadata PTS directly to the closest video access unit
    DropStale = 2U          ///< Discards metadata packets lagging beyond allowable window
};

/// @struct PtsSyncConfig
/// @brief Configuration settings for PtsSyncManager.
struct PtsSyncConfig {
    TimeBaseMode timeBaseMode { TimeBaseMode::SystemEpoch };
    PtsSyncPolicy syncPolicy { PtsSyncPolicy::NearestFrame };
    std::uint32_t maxSyncDeltaMs { 50U };      ///< Max allowed delta per STANAG 4609 (default 50 ms)
    std::uint32_t jitterBufferDepthMs { 50U }; ///< Sliding buffer window depth in milliseconds
    bool preserveSensorTimestamp { true };     ///< Keep original Tag 2 timestamp in ST 0601
};

/// @struct PtsSyncStats
/// @brief Real-time synchronization metrics and compliance diagnostics.
struct PtsSyncStats {
    double currentSkewMs { 0.0 };         ///< Instantaneous skew (t_video - t_klv)
    double averageSkewMs { 0.0 };         ///< Exponential moving average of skew
    double maxSkewMs { 0.0 };             ///< Peak observed absolute skew
    std::uint64_t totalFrames { 0U };     ///< Total video frames processed
    std::uint64_t totalKlvPackets { 0U }; ///< Total KLV packets processed
    std::uint64_t clampedPackets { 0U };  ///< Count of packets clamped to <= 50 ms
    std::uint64_t droppedPackets { 0U };  ///< Count of discarded stale packets
    bool isCompliant { true };            ///< True if recent packets adhere to <= 50 ms
};

/// @struct SynchronizedAccessUnit
/// @brief Fully synchronized pair of a Video Access Unit and corresponding metadata.
struct SynchronizedAccessUnit {
    VideoAccessUnit videoAu {};
    std::optional<UasDatalinkMessage> metadataMessage {};
    std::vector<std::uint8_t> rawKlvPacket {};
    std::int64_t skewDeltaUs { 0 };       ///< Observed skew in microseconds
    bool wasClamped { false };            ///< True if metadata PTS was modified by policy
};

} // namespace Klv
