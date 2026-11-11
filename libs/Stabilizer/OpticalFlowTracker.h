/// @file OpticalFlowTracker.h
/// @brief Multi-scale Pyramidal Lucas-Kanade optical flow tracker with bidirectional validation.
#pragma once

#include <cstddef>
#include <cstdint>

#include "FeatureDetector.h"
#include "FramePyramid.h"
#include "FrameView.h"
#include "StabilizerArena.h"
#include "StabilizerTypes.h"

namespace Stabilizer {

/// @brief Tracking outcome status for an individual feature.
enum class TrackingStatus : std::uint8_t {
    Tracked = 0U, ///< Successfully tracked with sub-pixel precision.
    LostOutOfBounds, ///< Feature window drifted outside valid frame boundaries.
    LostIllConditioned, ///< Spatial gradient tensor is singular / insufficient texture.
    LostConvergence, ///< Newton-Raphson iterations diverged or exceeded max limit.
    LostForwardBackward ///< Failed bidirectional forward-backward consistency check.
};

/// @brief Invariant-enforced integration window half-width [2 .. 16].
/// Total window dimension is (2 * halfWidth + 1).
class FlowWindowSize {
public:
    constexpr FlowWindowSize() noexcept = default;

    /// @brief Construct window size with validated half-width.
    /// @param halfWidth Half-window extent in pixels (e.g. 5 -> 11x11, 7 -> 15x15).
    explicit constexpr FlowWindowSize(std::uint16_t halfWidth) noexcept
        : halfWidth_ { static_cast<std::uint16_t>((halfWidth < 2U) ? 2U : ((halfWidth > 16U) ? 16U : halfWidth)) }
    {
    }

    /// @brief Half-width extent (w).
    [[nodiscard]] constexpr std::uint16_t halfWidth() const noexcept
    {
        return halfWidth_;
    }

    /// @brief Full window dimension (2w + 1).
    [[nodiscard]] constexpr std::uint16_t fullWidth() const noexcept
    {
        return static_cast<std::uint16_t>((2U * halfWidth_) + 1U);
    }

    /// @brief Total number of pixels within integration window.
    [[nodiscard]] constexpr std::size_t totalPixels() const noexcept
    {
        const auto side = static_cast<std::size_t>(fullWidth());
        return side * side;
    }

private:
    std::uint16_t halfWidth_ { 5U }; // Default 5 -> 11x11 window
};

/// @brief Tracked correspondence pairing reference feature with current location.
struct TrackedCorrespondence {
    FeaturePoint feature { }; ///< Source feature landmark from reference frame.
    SubPixelPoint currentPt { }; ///< Estimated sub-pixel position in current frame.
    MotionVector displacement { }; ///< Inter-frame motion vector (dx, dy).
    float forwardBackwardError { 0.0F }; ///< Bidirectional loop-closure error in pixels.
    float minEigenvalue { 0.0F }; ///< Minimum eigenvalue of structure tensor.
    TrackingStatus status { TrackingStatus::LostConvergence }; ///< Tracking outcome.

    /// @brief Check if feature was cleanly tracked.
    [[nodiscard]] constexpr bool isTracked() const noexcept
    {
        return status == TrackingStatus::Tracked;
    }
};

/// @brief Configuration parameters for Pyramidal Lucas-Kanade OpticalFlowTracker.
struct OpticalFlowConfig {
    FlowWindowSize windowSize { }; ///< Integration patch window half-width.
    std::uint8_t numPyramidLevels { 3U }; ///< Number of pyramid levels to traverse (1 to 3).
    std::uint16_t maxIterations { 25U }; ///< Maximum Newton-Raphson iterations per level.
    float minEpsilon { 0.01F }; ///< Termination displacement threshold in pixels.
    float minEigenvalue { 0.001F }; ///< Minimum eigenvalue threshold for well-conditioned G.
    float maxForwardBackwardError { 0.50F }; ///< Maximum allowable bidirectional error in pixels.
    bool enableBidirectional { true }; ///< Enable forward-backward consistency check.

    /// @brief Validate configuration invariant bounds.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (numPyramidLevels >= 1U) && (numPyramidLevels <= FramePyramid::MaxPyramidLevels) && (maxIterations >= 1U)
            && (maxIterations <= 100U) && (minEpsilon >= 0.0001F) && (minEpsilon <= 0.50F)
            && (minEigenvalue >= 0.00001F) && (minEigenvalue <= 10.0F) && (maxForwardBackwardError >= 0.05F)
            && (maxForwardBackwardError <= 5.0F);
    }
};

/// @brief Clean-room, zero-heap multi-scale Pyramidal Lucas-Kanade optical flow tracker.
class OpticalFlowTracker {
public:
    OpticalFlowTracker() noexcept = default;
    explicit OpticalFlowTracker(const OpticalFlowConfig& config) noexcept;

    /// @brief Track an array of salient features from previous pyramid to current pyramid.
    /// @param pyrPrev Multi-scale pyramid of reference frame I(t-1).
    /// @param pyrCurr Multi-scale pyramid of current frame I(t).
    /// @param inFeatures Input array of detected feature points in reference frame.
    /// @param numFeatures Number of features in inFeatures array.
    /// @param outMatches Output array receiving tracked correspondences.
    /// @param arena Monotonic scratchpad allocator for temporary working memory.
    /// @return Number of successfully tracked features (status == Tracked).
    [[nodiscard]] std::size_t trackFeatures(const FramePyramid& pyrPrev, const FramePyramid& pyrCurr,
        const FeaturePoint* inFeatures, std::size_t numFeatures, TrackedCorrespondence* outMatches,
        StabilizerArena& arena) noexcept;

    /// @brief Active tracker configuration.
    [[nodiscard]] const OpticalFlowConfig& config() const noexcept
    {
        return config_;
    }

    /// @brief Update tracker configuration.
    /// @param config New configuration to apply if valid.
    /// @return true if updated; false if configuration was rejected.
    bool setConfig(const OpticalFlowConfig& config) noexcept;

private:
    [[nodiscard]] bool trackSingle(const FramePyramid& pyrPrev, const FramePyramid& pyrCurr,
        const SubPixelPoint& startPt, SubPixelPoint& outTargetPt, float& outMinEigen, TrackingStatus& outStatus,
        float* patchI, float* patchIx, float* patchIy) const noexcept;

    [[nodiscard]] bool trackLevel(const FrameView& viewI, const FrameView& viewJ, const SubPixelPoint& ptI,
        const SubPixelPoint& initialGuessJ, SubPixelPoint& outRefinedJ, float& outMinEigen, TrackingStatus& outStatus,
        float* patchI, float* patchIx, float* patchIy) const noexcept;

    [[nodiscard]] static float sampleBilinear(const FrameView& view, float x, float y) noexcept;

    OpticalFlowConfig config_ { };
};

} // namespace Stabilizer
