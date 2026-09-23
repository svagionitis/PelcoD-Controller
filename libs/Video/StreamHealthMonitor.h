#pragma once

/// @file StreamHealthMonitor.h
/// @brief Video stream health, frozen frame, signal loss, blackout, and whiteout diagnostic monitor.

#include "DecoderTypes.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace Video {

/// @enum StreamHealthState
/// @brief Categorizes operational health and failure modes of an active video feed.
enum class StreamHealthState : std::uint8_t {
    Healthy, ///< Active stream with dynamic content and normal frame rate.
    Degraded, ///< Stream is receiving frames, but frame rate has dropped significantly below nominal.
    Frozen, ///< Video content has stalled on identical frames for longer than freeze duration threshold.
    SignalLoss, ///< No frames have been received within signal loss timeout interval.
    Blackout, ///< Mean frame luminance is near zero (covered lens, capped optics, or sensor power failure).
    Whiteout ///< Mean frame luminance is near 255 with extreme saturation (direct laser glare or overexposure).
};

/// @struct StreamHealthConfig
/// @brief Thresholds and tuning parameters for stream health analysis.
struct StreamHealthConfig {
    double nominalFps { 30.0 }; ///< Expected stream frame rate in frames per second.
    double freezeDurationThresholdSec { 2.5 }; ///< Consecutive duration of identical frames before declaring Frozen.
    double signalLossTimeoutSec { 1.5 }; ///< Elapsed silence since last frame before declaring SignalLoss.
    double blackoutLuminanceThreshold { 6.0 }; ///< Maximum average luminance [0.0, 255.0] to trigger Blackout.
    double blackoutVarianceThreshold { 4.0 }; ///< Maximum luminance variance to trigger Blackout.
    double whiteoutLuminanceThreshold { 248.0 }; ///< Minimum average luminance [0.0, 255.0] to trigger Whiteout.
    double whiteoutSaturationRatio { 0.90 }; ///< Minimum fraction of pixels saturated (>= 250) to trigger Whiteout.
    double freezeDifferenceThreshold {
        0.5
    }; ///< Maximum mean pixel absolute difference per pixel to consider identical.
    double degradedFpsRatio { 0.5 }; ///< Fraction of nominalFps below which stream is flagged as Degraded.
    std::size_t sampleGridStep { 8U }; ///< Sub-sampling stride in pixels for lightweight frame analysis.
    bool autoReconnectOnFailure { false }; ///< Automatically invoke reconnect callback when failure states persist.
};

/// @struct StreamHealthMetrics
/// @brief Real-time diagnostic telemetry describing stream content and delivery health.
struct StreamHealthMetrics {
    StreamHealthState state { StreamHealthState::Healthy }; ///< Current diagnosed health state.
    double measuredFps { 0.0 }; ///< Estimated effective frame rate over recent history.
    double meanLuminance { 0.0 }; ///< Average pixel luminance [0.0, 255.0].
    double luminanceVariance { 0.0 }; ///< Variance of pixel luminance across the frame.
    double frameDifference { 0.0 }; ///< Mean absolute difference against the previous frame fingerprint.
    double secondsSinceLastFrame { 0.0 }; ///< Seconds elapsed since the most recent frame arrived.
    double frozenDurationSec { 0.0 }; ///< Cumulative seconds current video content has remained frozen.
    std::uint64_t totalFramesAnalyzed { 0U }; ///< Cumulative count of frames ingested and inspected.
    std::uint64_t freezeCount { 0U }; ///< Cumulative count of freeze incidents detected.
    std::uint64_t signalLossCount { 0U }; ///< Cumulative count of signal loss incidents detected.
    std::uint64_t blackoutCount { 0U }; ///< Cumulative count of blackout incidents detected.
    std::uint64_t whiteoutCount { 0U }; ///< Cumulative count of whiteout incidents detected.
};

/// @class StreamHealthMonitor
/// @brief Diagnostic monitor detecting frozen frames, packet starvation, blackouts, and sensor whiteout.
/// @details Implements IFrameProcessor for seamless in-line attachment to decoders and Qt stream workers.
///          Sub-samples pixel grids for ultra-fast, sub-millisecond evaluation with minimal CPU overhead.
///          Thread-safe for concurrent frame processing and metric polling.
class StreamHealthMonitor : public IFrameProcessor {
public:
    /// @brief Callback signature invoked upon health state transitions.
    /// @param oldState Previous health state.
    /// @param newState Current newly diagnosed health state.
    /// @param metrics Snapshot of current diagnostic metrics.
    using HealthCallback = std::function<void(
        StreamHealthState oldState, StreamHealthState newState, const StreamHealthMetrics& metrics)>;

    /// @brief Callback signature to trigger stream reconnection.
    /// @return True if reconnection was initiated successfully.
    using ReconnectCallback = std::function<bool()>;

    /// @brief Constructs a StreamHealthMonitor with given configuration.
    /// @param[in] config Diagnostic thresholds and tuning parameters.
    explicit StreamHealthMonitor(StreamHealthConfig config = {});

    /// @brief Destructor.
    ~StreamHealthMonitor() override = default;

    /// @brief IFrameProcessor interface entry point for in-line pipeline processing.
    /// @param[in,out] data Contiguous frame buffer (not modified by monitor).
    /// @param[in] width Frame width in pixels.
    /// @param[in] height Frame height in pixels.
    /// @param[in] format Pixel format of frame.
    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    /// @brief Ingests a frame explicitly with optional presentation timestamp.
    /// @param[in] data Pixel buffer.
    /// @param[in] width Frame width in pixels.
    /// @param[in] height Frame height in pixels.
    /// @param[in] format Pixel format.
    /// @param[in] timestampSec Presentation timestamp in seconds (-1.0 to use steady clock).
    void ingestFrame(const std::uint8_t* data, int width, int height, PixelFormat format, double timestampSec = -1.0);

    /// @brief Periodically checks for frame delivery timeouts (SignalLoss).
    /// @details Call this from a timer or GUI thread to detect signal loss when no frames arrive.
    /// @param[in] nowSec Monotonic time in seconds (-1.0 to use steady clock).
    void checkTimeout(double nowSec = -1.0);

    /// @brief Returns the current stream health state.
    [[nodiscard]] StreamHealthState getState() const;

    /// @brief Returns a snapshot of all diagnostic metrics.
    [[nodiscard]] StreamHealthMetrics getMetrics() const;

    /// @brief Updates active configuration parameters.
    /// @param[in] config New configuration settings.
    void setConfig(const StreamHealthConfig& config);

    /// @brief Retrieves the active configuration.
    [[nodiscard]] StreamHealthConfig getConfig() const;

    /// @brief Registers a callback for state transition notifications.
    /// @param[in] callback State change callback function.
    void setHealthCallback(HealthCallback callback);

    /// @brief Registers a callback to trigger proactive decoder reconnection.
    /// @param[in] callback Reconnection action callback.
    void setReconnectCallback(ReconnectCallback callback);

    /// @brief Resets all internal statistics, timers, and state to Healthy.
    void reset();

    /// @brief Converts a StreamHealthState enum to a human-readable string.
    /// @param[in] state Health state enum value.
    /// @return String representation (e.g. "Healthy", "Frozen", "SignalLoss").
    [[nodiscard]] static std::string stateToString(StreamHealthState state) noexcept;

private:
    void evaluateStateTransitionLocked(StreamHealthState candidateState, double nowSec);

    mutable std::mutex m_mutex;
    StreamHealthConfig m_config {};
    StreamHealthMetrics m_metrics {};

    HealthCallback m_healthCallback { nullptr };
    ReconnectCallback m_reconnectCallback { nullptr };

    std::vector<std::uint8_t> m_previousFingerprint {};
    int m_previousFingerprintWidth { 0 };
    int m_previousFingerprintHeight { 0 };

    double m_lastFrameTimeSec { 0.0 };
    double m_freezeStartTimeSec { 0.0 };
    bool m_hasReceivedFirstFrame { false };
    bool m_isCandidateFrozen { false };

    // Rolling frame timestamp window for FPS calculation
    std::vector<double> m_recentFrameTimestamps {};
    static constexpr std::size_t kFpsHistoryCapacity { 30U };
};

} // namespace Video
