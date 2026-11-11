/// @file VideoStabilizerEngine.h
/// @brief Top-level clean-room real-time digital video stabilizer engine.
#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <atomic>

#include "FeatureDetector.h"
#include "FrameBuffer.h"
#include "FramePyramid.h"
#include "FrameView.h"
#include "MotionModelEstimator.h"
#include "OpticalFlowTracker.h"
#include "StabilizerArena.h"
#include "StabilizerTypes.h"
#include "TrajectoryFilter.h"
#include "WarpEngine.h"

namespace Stabilizer {

/// @brief Operational state of the stabilizer supervisory engine.
enum class EngineState : std::uint8_t {
    Uninitialized = 0U, ///< Engine not configured or buffers unallocated.
    Initializing, ///< Capturing initial reference frame and corners.
    Tracking, ///< Nominal closed-loop motion tracking and warping.
    Degraded, ///< Insufficient inliers; executing fallback policy.
    Resetting ///< Reset sequence in progress.
};

/// @brief Fallback behavior policy when feature tracking fails.
enum class FallbackPolicy : std::uint8_t {
    IdentityHold = 0U, ///< Assume zero inter-frame motion (H = Identity).
    VelocityExtrapolate, ///< Extrapolate motion using damped filter velocity.
    BypassWarp ///< Pass raw input directly to output without warping.
};

/// @brief Comprehensive configuration container for the stabilization engine.
struct VideoStabilizerConfig {
    FrameDimensions dimensions { 1920U, 1080U };
    StabilizationMode mode { StabilizationMode::Affine };
    FallbackPolicy fallback { FallbackPolicy::VelocityExtrapolate };

    FeatureDetectorConfig detector { };
    OpticalFlowConfig tracker { };
    MotionEstimatorConfig estimator { };
    TrajectoryConfig filter { };
    WarpConfig warp { };

    std::uint32_t minTrackedFeatures { 20U };
    std::uint32_t replenishIntervalFrames { 15U };
    std::uint32_t maxDegradedFrames { 30U };
    float minInlierRatio { 0.25F };

    /// @brief Check whether configuration parameters are valid.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return dimensions.isValid() && detector.isValid() && tracker.isValid() && estimator.isValid()
            && filter.isValid() && warp.isValid() && (minTrackedFeatures >= 5U) && (minInlierRatio > 0.0F)
            && (minInlierRatio <= 1.0F);
    }
};

/// @brief Microsecond execution latency profiling for each pipeline stage.
struct StageTiming {
    std::uint32_t pyramidGenUs { 0U };
    std::uint32_t featureDetectUs { 0U };
    std::uint32_t opticalFlowUs { 0U };
    std::uint32_t motionModelUs { 0U };
    std::uint32_t trajectoryFilterUs { 0U };
    std::uint32_t warpSynthesisUs { 0U };
    std::uint32_t totalFrameUs { 0U };
};

/// @brief Real-time runtime diagnostic metrics and telemetry snapshot.
struct StabilizerDiagnostics {
    EngineState state { EngineState::Uninitialized };
    std::uint32_t frameIndex { 0U };
    std::uint32_t detectedFeatures { 0U };
    std::uint32_t trackedFeatures { 0U };
    std::uint32_t inlierCount { 0U };
    float inlierRatio { 0.0F };
    AffineTransform2D rawTransform { AffineTransform2D::identity() };
    AffineTransform2D smoothedTransform { AffineTransform2D::identity() };
    AffineTransform2D correctionTransform { AffineTransform2D::identity() };
    StageTiming latencyUs { };
    bool isPtzFeedforwardActive { false };
};

/// @brief Zero-heap, real-time digital video stabilizer orchestrator.
class VideoStabilizerEngine {
public:
    static constexpr std::size_t MaxFeatures { 500U };

    VideoStabilizerEngine() noexcept = default;
    ~VideoStabilizerEngine() = default;

    VideoStabilizerEngine(const VideoStabilizerEngine&) = delete;
    VideoStabilizerEngine& operator=(const VideoStabilizerEngine&) = delete;
    VideoStabilizerEngine(VideoStabilizerEngine&&) = delete;
    VideoStabilizerEngine& operator=(VideoStabilizerEngine&&) = delete;

    /// @brief Initialize and pre-allocate memory buffers and pyramids.
    /// @param config Master engine configuration.
    /// @return true if initialized cleanly; false on parameter error.
    [[nodiscard]] bool configure(const VideoStabilizerConfig& config) noexcept;

    /// @brief Ingest external PTZ gimbal velocity telemetry sample.
    /// @param sample Calibrated pan/tilt angular velocity and optical zoom.
    void updateTelemetry(const PtzTelemetrySample& sample) noexcept;

    /// @brief Process an incoming video frame and synthesize stabilized output.
    /// @param input Raw incoming video frame view.
    /// @param output Destination frame view for stabilized pixels.
    /// @param dt Frame interval in seconds (default 1/30 s).
    /// @return true on successful frame processing.
    [[nodiscard]] bool processFrame(const FrameView& input, FrameView& output, float dt = 0.033333F) noexcept;

    /// @brief Process planar YUV420p video frame.
    /// @param inY Input luma plane view.
    /// @param inU Input chroma U plane view.
    /// @param inV Input chroma V plane view.
    /// @param outY Output luma plane view.
    /// @param outU Output chroma U plane view.
    /// @param outV Output chroma V plane view.
    /// @param dt Frame interval in seconds.
    /// @return true on successful processing.
    [[nodiscard]] bool processYuv420p(const FrameView& inY, const FrameView& inU, const FrameView& inV, FrameView& outY,
        FrameView& outU, FrameView& outV, float dt = 0.033333F) noexcept;

    /// @brief Reset internal trajectory history, state machine, and pyramids.
    void reset() noexcept;

    /// @brief Retrieve snapshot of current runtime diagnostics and timings.
    [[nodiscard]] StabilizerDiagnostics diagnostics() const noexcept;

    /// @brief Retrieve current engine state.
    [[nodiscard]] EngineState state() const noexcept;

    /// @brief Check if engine is configured and ready to process frames.
    [[nodiscard]] bool isConfigured() const noexcept;

    /// @brief Retrieve current active configuration.
    [[nodiscard]] const VideoStabilizerConfig& config() const noexcept;

private:
    void swapPyramids() noexcept;
    void executeDegraded(AffineTransform2D& outTransform) noexcept;
    void replenishFeatures(const FramePyramid& pyramid) noexcept;
    void extractInliers(
        const TrackedCorrespondence* matches, std::size_t numMatches, const MotionEstimationResult& estResult) noexcept;

    VideoStabilizerConfig config_ { };
    EngineState state_ { EngineState::Uninitialized };
    std::uint32_t frameCount_ { 0U };
    std::uint32_t degradedCount_ { 0U };
    std::uint32_t framesSinceDetect_ { 0U };

    FeatureDetector detector_ { };
    OpticalFlowTracker tracker_ { };
    MotionModelEstimator estimator_ { };
    TrajectoryFilter filter_ { TrajectoryConfig { } };
    WarpEngine warper_ { };

    StabilizerArena arena_ { };
    FrameBuffer storage0A_ { };
    FrameBuffer storage0B_ { };
    FrameBuffer* currStorage0_ { &storage0A_ };
    FrameBuffer* prevStorage0_ { &storage0B_ };
    FramePyramid pyramidA_ { };
    FramePyramid pyramidB_ { };
    FramePyramid* currPyramid_ { &pyramidA_ };
    FramePyramid* prevPyramid_ { &pyramidB_ };

    std::array<FeaturePoint, MaxFeatures> featureStorage_ { };
    std::array<TrackedCorrespondence, MaxFeatures> matchStorage_ { };
    std::size_t activePointCount_ { 0U };

    std::atomic<bool> hasPendingTelemetry_ { false };
    PtzTelemetrySample latestTelemetry_ { };

    StabilizerDiagnostics lastDiagnostics_ { };
};

} // namespace Stabilizer
