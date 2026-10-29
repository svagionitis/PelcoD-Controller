#pragma once

/// @file StanagScrubController.h
/// @brief Master playback and timeline scrubbing controller linking video and telemetry.

#include "StanagScrubTypes.h"
#include "TelemetryTimeIndex.h"

#include <cstdint>
#include <functional>
#include <mutex>

namespace Klv {

/// @class StanagScrubController
/// @brief Handles random-access timeline scrubbing, frame stepping, and sub-50ms KLV pairing.
class StanagScrubController {
public:
    /// @brief Callback signature for synchronized scrub frames.
    using ScrubFrameCallback = std::function<void(const SynchronizedScrubFrame& frame)>;

    /// @brief Callback signature for external video seek requests.
    /// @param[in] seconds Target timeline seek position in seconds.
    /// @param[in] keyframeOnly True to seek only to nearest preceding keyframe; false for exact frame.
    /// @return True if seek succeeded.
    using SeekSecondsCallback = std::function<bool(double seconds, bool keyframeOnly)>;

    StanagScrubController();
    ~StanagScrubController() = default;

    /// @brief Registers external seek target callback.
    /// @param[in] callback Callback invoked during seeking operations.
    void setSeekCallback(SeekSecondsCallback callback);

    /// @brief Loads or sets the timeline index.
    /// @param[in] index Finalized TelemetryTimeIndex.
    void setTimeIndex(TelemetryTimeIndex index);

    /// @brief Returns a snapshot copy of the active TelemetryTimeIndex.
    [[nodiscard]] TelemetryTimeIndex timeIndex() const;

    /// @brief Registers output callback for synchronized scrub events.
    /// @param[in] callback Consumer callback.
    void setScrubCallback(ScrubFrameCallback callback);

    /// @brief Configures scrubbing seek strategy.
    /// @param[in] mode Seek mode (KeyframeOnly, Exact, or Adaptive).
    void setSeekMode(ScrubSeekMode mode);

    /// @brief Gets active seek mode.
    [[nodiscard]] ScrubSeekMode seekMode() const;

    /// @brief Enables or disables continuous telemetry interpolation between packets.
    /// @param[in] enable True to interpolate telemetry smoothly; false to snap to nearest packet.
    void setInterpolate(bool enable);

    /// @brief Checks whether telemetry interpolation is enabled.
    [[nodiscard]] bool isInterpolateEnabled() const;

    /// @brief Sets maximum allowable video-telemetry synchronization tolerance in ms.
    /// @param[in] toleranceMs Tolerance in milliseconds (default 50 ms per STANAG 4609).
    void setSyncTolerance(std::uint32_t toleranceMs);

    /// @brief Performs an interactive scrub to an elapsed timeline position in seconds.
    /// @param[in] seconds Target position in seconds.
    /// @return True if scrub succeeded.
    [[nodiscard]] bool scrubToSeconds(double seconds);

    /// @brief Performs a scrub to a 90 kHz presentation timestamp.
    /// @param[in] pts 90 kHz PTS ticks.
    /// @return True if scrub succeeded.
    [[nodiscard]] bool scrubToPts(std::uint64_t pts);

    /// @brief Steps playback forward or backward by a discrete number of frames.
    /// @param[in] frameDelta Delta in frames (-1 for step back, +1 for step forward).
    /// @return True if step succeeded.
    [[nodiscard]] bool stepFrames(int frameDelta);

    /// @brief Retrieves the current synchronized scrub frame.
    [[nodiscard]] SynchronizedScrubFrame currentFrame() const;

    /// @brief Gets active scrubbing statistics and diagnostics.
    [[nodiscard]] ScrubStats stats() const;

private:
    mutable std::mutex m_mutex {};
    TelemetryTimeIndex m_timeIndex {};
    SeekSecondsCallback m_seekCallback {};
    ScrubFrameCallback m_callback {};

    ScrubSeekMode m_seekMode { ScrubSeekMode::AdaptiveScrub };
    bool m_interpolateTelemetry { true };
    std::uint32_t m_syncToleranceMs { 50U };
    SynchronizedScrubFrame m_currentFrame {};
    ScrubStats m_stats {};
    std::uint32_t m_currentFrameIndex { 0U };

    bool executeSeekLocked(std::uint64_t targetPts);
};

} // namespace Klv
