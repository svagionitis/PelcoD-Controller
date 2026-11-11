/// @file VideoStabilizerEngine.cpp
/// @brief Implementation of top-level real-time digital video stabilizer orchestrator.
#include "VideoStabilizerEngine.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <chrono>
#include <utility>

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

bool VideoStabilizerEngine::configure(const VideoStabilizerConfig& config) noexcept
{
    if (!config.isValid()) {
        return false;
    }

    config_ = config;

    storage0A_ = FrameBuffer(config_.dimensions);
    storage0B_ = FrameBuffer(config_.dimensions);
    currStorage0_ = &storage0A_;
    prevStorage0_ = &storage0B_;

    pyramidA_ = FramePyramid(config_.dimensions);
    pyramidB_ = FramePyramid(config_.dimensions);
    currPyramid_ = &pyramidA_;
    prevPyramid_ = &pyramidB_;

    const std::size_t pixelCount
        = static_cast<std::size_t>(config_.dimensions.width()) * static_cast<std::size_t>(config_.dimensions.height());
    const std::size_t arenaSize = std::max<std::size_t>(16ULL * 1024ULL * 1024ULL, pixelCount * 16ULL);
    arena_ = StabilizerArena(arenaSize);

    detector_.setConfig(config_.detector);
    tracker_.setConfig(config_.tracker);
    estimator_.setConfig(config_.estimator);

    TrajectoryConfig filterCfg = config_.filter;
    filterCfg.frameDimensions = config_.dimensions;
    if (!filter_.setConfig(filterCfg)) {
        return false;
    }

    warper_.setConfig(config_.warp);

    activePointCount_ = 0U;
    frameCount_ = 0U;
    degradedCount_ = 0U;
    framesSinceDetect_ = 0U;
    hasPendingTelemetry_.store(false, std::memory_order_relaxed);
    latestTelemetry_ = PtzTelemetrySample { };

    lastDiagnostics_ = StabilizerDiagnostics { };
    state_ = EngineState::Initializing;

    return true;
}

void VideoStabilizerEngine::updateTelemetry(const PtzTelemetrySample& sample) noexcept
{
    latestTelemetry_ = sample;
    hasPendingTelemetry_.store(true, std::memory_order_release);
}

void VideoStabilizerEngine::reset() noexcept
{
    filter_.reset();
    activePointCount_ = 0U;
    degradedCount_ = 0U;
    framesSinceDetect_ = 0U;
    hasPendingTelemetry_.store(false, std::memory_order_relaxed);
    latestTelemetry_ = PtzTelemetrySample { };
    lastDiagnostics_ = StabilizerDiagnostics { };

    if (isConfigured()) {
        state_ = EngineState::Initializing;
    } else {
        state_ = EngineState::Uninitialized;
    }
}

StabilizerDiagnostics VideoStabilizerEngine::diagnostics() const noexcept
{
    return lastDiagnostics_;
}

EngineState VideoStabilizerEngine::state() const noexcept
{
    return state_;
}

bool VideoStabilizerEngine::isConfigured() const noexcept
{
    return (state_ != EngineState::Uninitialized) && config_.isValid();
}

const VideoStabilizerConfig& VideoStabilizerEngine::config() const noexcept
{
    return config_;
}

void VideoStabilizerEngine::swapPyramids() noexcept
{
    std::swap(currPyramid_, prevPyramid_);
    std::swap(currStorage0_, prevStorage0_);
}

void VideoStabilizerEngine::executeDegraded(AffineTransform2D& outTransform) noexcept
{
    if (config_.fallback == FallbackPolicy::VelocityExtrapolate) {
        const float decay = (degradedCount_ > 1U) ? 0.40F : 0.80F;
        outTransform = AffineTransform2D::translation(
            lastDiagnostics_.rawTransform.tx * decay, lastDiagnostics_.rawTransform.ty * decay);
    } else {
        outTransform = AffineTransform2D::identity();
    }
}

void VideoStabilizerEngine::replenishFeatures(const FramePyramid& pyramid) noexcept
{
    activePointCount_ = detector_.detectFeatures(pyramid.level(0U), arena_, featureStorage_.data(), MaxFeatures);
}

void VideoStabilizerEngine::extractInliers(
    const TrackedCorrespondence* matches, std::size_t numMatches, const MotionEstimationResult& estResult) noexcept
{
    if ((matches == nullptr) || (numMatches == 0U)) {
        activePointCount_ = 0U;
        return;
    }

    const float inlierThreshSq = config_.estimator.inlierThreshold.squared();
    std::size_t inlierIdx = 0U;

    for (std::size_t i = 0U; (i < numMatches) && (inlierIdx < MaxFeatures); ++i) {
        if (!matches[i].isTracked()) {
            continue;
        }

        const SubPixelPoint projected = estResult.transform.transform(matches[i].feature.pt);
        const float distSq = projected.distanceSquared(matches[i].currentPt);

        if (distSq <= inlierThreshSq) {
            featureStorage_[inlierIdx].pt = matches[i].currentPt;
            featureStorage_[inlierIdx].score = matches[i].feature.score;
            featureStorage_[inlierIdx].cellId = matches[i].feature.cellId;
            ++inlierIdx;
        }
    }

    activePointCount_ = inlierIdx;
}

bool VideoStabilizerEngine::processFrame(const FrameView& input, FrameView& output, float dt) noexcept
{
    const auto tStart = std::chrono::steady_clock::now();
    StageTiming timing { };

    if (!isConfigured() || input.isEmpty() || output.isEmpty()) {
        return false;
    }

    if ((input.width() != config_.dimensions.width()) || (input.height() != config_.dimensions.height())
        || (output.width() != config_.dimensions.width()) || (output.height() != config_.dimensions.height())) {
        return false;
    }

    arena_.reset();

    for (std::uint32_t y = 0U; y < input.height(); ++y) {
        std::memcpy(currStorage0_->rowPtr(y), input.rowPtr(y), input.width());
    }

    // Stage 1: Build pyramid for current input frame
    const auto tPyr0 = std::chrono::steady_clock::now();
    if (!currPyramid_->build(currStorage0_->view())) {
        return false;
    }
    const auto tPyr1 = std::chrono::steady_clock::now();
    timing.pyramidGenUs
        = static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(tPyr1 - tPyr0).count());

    // Stage 2: Initial Reference Frame Acquisition
    if (state_ == EngineState::Initializing) {
        replenishFeatures(*currPyramid_);
        if (!warper_.warpFrame(input, output, AffineTransform2D::identity())) {
            return false;
        }
        swapPyramids();

        state_ = EngineState::Tracking;
        frameCount_ = 1U;

        lastDiagnostics_.state = state_;
        lastDiagnostics_.frameIndex = frameCount_;
        lastDiagnostics_.detectedFeatures = static_cast<std::uint32_t>(activePointCount_);
        lastDiagnostics_.trackedFeatures = static_cast<std::uint32_t>(activePointCount_);
        lastDiagnostics_.inlierCount = static_cast<std::uint32_t>(activePointCount_);
        lastDiagnostics_.inlierRatio = 1.0F;
        lastDiagnostics_.rawTransform = AffineTransform2D::identity();
        lastDiagnostics_.smoothedTransform = AffineTransform2D::identity();
        lastDiagnostics_.correctionTransform = AffineTransform2D::identity();

        const auto tEnd = std::chrono::steady_clock::now();
        timing.totalFrameUs
            = static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count());
        lastDiagnostics_.latencyUs = timing;

        return true;
    }

    // Stage 3: Feature Replenishment Check
    const auto tDetect0 = std::chrono::steady_clock::now();
    if ((activePointCount_ < config_.minTrackedFeatures) || (framesSinceDetect_ >= config_.replenishIntervalFrames)) {
        replenishFeatures(*prevPyramid_);
        framesSinceDetect_ = 0U;
    } else {
        ++framesSinceDetect_;
    }
    const auto tDetect1 = std::chrono::steady_clock::now();
    timing.featureDetectUs = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(tDetect1 - tDetect0).count());

    // Stage 4: Pyramidal Lucas-Kanade Optical Flow Tracking
    const auto tFlow0 = std::chrono::steady_clock::now();
    std::size_t trackedCount = 0U;
    if (activePointCount_ > 0U) {
        trackedCount = tracker_.trackFeatures(
            *prevPyramid_, *currPyramid_, featureStorage_.data(), activePointCount_, matchStorage_.data(), arena_);
    }
    const auto tFlow1 = std::chrono::steady_clock::now();
    timing.opticalFlowUs
        = static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(tFlow1 - tFlow0).count());

    // Stage 5: Parametric Motion Model Estimation (MSAC)
    const auto tModel0 = std::chrono::steady_clock::now();
    AffineTransform2D rawH = AffineTransform2D::identity();
    MotionEstimationResult estResult { };
    bool modelValid = false;

    if (trackedCount >= 4U) {
        const bool estSuccess = estimator_.estimateMotion(matchStorage_.data(), trackedCount, estResult, &arena_);
        modelValid = estSuccess && estResult.isSuccess && (estResult.inlierRatio >= config_.minInlierRatio);
    }
    const auto tModel1 = std::chrono::steady_clock::now();
    timing.motionModelUs
        = static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(tModel1 - tModel0).count());

    // Stage 6: Supervisory Health Check & Degraded Mode Handling
    if (modelValid) {
        rawH = estResult.transform;
        extractInliers(matchStorage_.data(), trackedCount, estResult);
        degradedCount_ = 0U;
        state_ = EngineState::Tracking;
    } else {
        ++degradedCount_;
        if (degradedCount_ >= config_.maxDegradedFrames) {
            state_ = EngineState::Resetting;
            reset();
            return false;
        }
        state_ = EngineState::Degraded;
        executeDegraded(rawH);
        activePointCount_ = 0U; // Force replenishment on next frame
    }

    // Stage 7: Telemetry Ingestion & Trajectory Filter Update
    const auto tFilter0 = std::chrono::steady_clock::now();
    const bool ptzActive = hasPendingTelemetry_.exchange(false, std::memory_order_acq_rel);
    const PtzTelemetrySample ptzSample = latestTelemetry_;

    TrajectoryState trajState { };
    const bool filterSuccess = filter_.update(rawH, ptzSample, dt, trajState);
    if (!filterSuccess) {
        trajState.correctionTransform = AffineTransform2D::identity();
    }
    const auto tFilter1 = std::chrono::steady_clock::now();
    timing.trajectoryFilterUs = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(tFilter1 - tFilter0).count());

    // Stage 8: SIMD Geometric Warp Synthesis
    const auto tWarp0 = std::chrono::steady_clock::now();
    bool warpOk = false;
    if ((config_.fallback == FallbackPolicy::BypassWarp) && (state_ == EngineState::Degraded)) {
        warpOk = warper_.warpFrame(input, output, AffineTransform2D::identity());
    } else if (config_.warp.enableRollingShutter) {
        warpOk
            = warper_.warpRollingShutter(input, output, trajState.correctionTransform, trajState.correctionTransform);
    } else {
        warpOk = warper_.warpFrame(input, output, trajState.correctionTransform);
    }
    if (!warpOk) {
        return false;
    }
    const auto tWarp1 = std::chrono::steady_clock::now();
    timing.warpSynthesisUs
        = static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(tWarp1 - tWarp0).count());

    // Finalize Cycle: Swap pyramids and advance frame index
    swapPyramids();
    ++frameCount_;

    const auto tEnd = std::chrono::steady_clock::now();
    timing.totalFrameUs
        = static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count());

    // Record Diagnostics
    lastDiagnostics_.state = state_;
    lastDiagnostics_.frameIndex = frameCount_;
    lastDiagnostics_.detectedFeatures = static_cast<std::uint32_t>(activePointCount_);
    lastDiagnostics_.trackedFeatures = static_cast<std::uint32_t>(trackedCount);
    lastDiagnostics_.inlierCount = estResult.inlierCount;
    lastDiagnostics_.inlierRatio = estResult.inlierRatio;
    lastDiagnostics_.rawTransform = rawH;
    lastDiagnostics_.smoothedTransform = trajState.smoothedPose;
    lastDiagnostics_.correctionTransform = trajState.correctionTransform;
    lastDiagnostics_.latencyUs = timing;
    lastDiagnostics_.isPtzFeedforwardActive = ptzActive;

    return true;
}

bool VideoStabilizerEngine::processYuv420p(const FrameView& inY, const FrameView& inU, const FrameView& inV,
    FrameView& outY, FrameView& outU, FrameView& outV, float dt) noexcept
{
    const bool lumaSuccess = processFrame(inY, outY, dt);
    if (!lumaSuccess) {
        return false;
    }

    return warper_.warpYuv420p(inY, inU, inV, outY, outU, outV, lastDiagnostics_.correctionTransform);
}

} // namespace Stabilizer
