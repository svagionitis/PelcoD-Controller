/// @file MotionModelEstimator.h
/// @brief Robust parametric motion model estimator using MSAC with least-squares refinement.
#pragma once

#include <cstddef>
#include <cstdint>

#include "OpticalFlowTracker.h"
#include "StabilizerArena.h"
#include "StabilizerTypes.h"

namespace Stabilizer {

/// @brief Strongly-typed inlier reprojection distance threshold [0.1 .. 20.0] pixels.
class InlierThreshold {
public:
    constexpr InlierThreshold() noexcept = default;

    /// @brief Construct threshold clamped to allowable bounds.
    /// @param val Maximum Euclidean reprojection error in pixels.
    explicit constexpr InlierThreshold(float val) noexcept
        : value_ { (val < 0.10F) ? 0.10F : ((val > 20.0F) ? 20.0F : val) }
    {
    }

    /// @brief Inlier distance threshold value in pixels.
    [[nodiscard]] constexpr float value() const noexcept
    {
        return value_;
    }

    /// @brief Squared inlier distance threshold value (T^2).
    [[nodiscard]] constexpr float squared() const noexcept
    {
        return value_ * value_;
    }

private:
    float value_ { 2.0F }; // Default 2.0 pixels
};

/// @brief Configuration parameters for MSAC MotionModelEstimator.
struct MotionEstimatorConfig {
    StabilizationMode mode { StabilizationMode::Affine }; ///< Desired degrees of freedom.
    InlierThreshold inlierThreshold { }; ///< MSAC inlier distance threshold.
    std::uint32_t maxIterations { 200U }; ///< Maximum consensus sampling iterations.
    std::uint32_t minIterations { 20U }; ///< Minimum iterations before adaptive stop.
    float minInlierRatio { 0.25F }; ///< Minimum fraction of consensus inliers required.
    float confidence { 0.99F }; ///< Target consensus confidence [0.80 .. 0.999].
    std::uint32_t randomSeed { 0x12345678U }; ///< Deterministic PRNG seed.
    bool enableRefinement { true }; ///< Enable linear least-squares consensus refinement.

    /// @brief Validate configuration parameters.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (mode != StabilizationMode::Disabled) && (maxIterations >= 10U) && (maxIterations <= 1000U)
            && (minIterations >= 5U) && (minIterations <= maxIterations) && (minInlierRatio >= 0.05F)
            && (minInlierRatio <= 0.95F) && (confidence >= 0.80F) && (confidence <= 0.999F);
    }
};

/// @brief Telemetry and quantitative metrics from motion model estimation.
struct MotionEstimationResult {
    AffineTransform2D transform { AffineTransform2D::identity() }; ///< Estimated 2D transform matrix.
    std::uint32_t totalPoints { 0U }; ///< Total valid correspondences evaluated.
    std::uint32_t inlierCount { 0U }; ///< Number of consensus inlier points.
    float inlierRatio { 0.0F }; ///< Ratio of inliers to total points.
    float meanInlierError { 0.0F }; ///< Mean Euclidean reprojection error of inliers.
    float msacCost { 0.0F }; ///< Final bounded MSAC objective cost.
    std::uint32_t iterationsUsed { 0U }; ///< Consensus sampling iterations executed.
    bool isDegenerate { false }; ///< Flag set if geometry was ill-conditioned.
    bool isSuccess { false }; ///< Flag set if model met consensus criteria.
};

/// @brief Deterministic, zero-heap MSAC parametric motion estimator.
class MotionModelEstimator {
public:
    MotionModelEstimator() noexcept = default;

    /// @brief Construct estimator with explicit configuration.
    /// @param config Motion estimator parameters.
    explicit MotionModelEstimator(const MotionEstimatorConfig& config) noexcept;

    /// @brief Estimate global motion transform from tracked feature correspondences.
    /// @param matches Input array of tracked correspondences from OpticalFlowTracker.
    /// @param numMatches Number of correspondences in matches array.
    /// @param outResult Receives the estimated transform and telemetry.
    /// @param arena Optional scratchpad arena for temporary inlier indices.
    /// @return true if estimation converged on a valid model; false on degeneracy.
    [[nodiscard]] bool estimateMotion(const TrackedCorrespondence* matches, std::size_t numMatches,
        MotionEstimationResult& outResult, StabilizerArena* arena = nullptr) noexcept;

    /// @brief Current estimator configuration.
    [[nodiscard]] const MotionEstimatorConfig& config() const noexcept
    {
        return config_;
    }

    /// @brief Update estimator configuration.
    /// @param config New configuration parameters.
    /// @return true if valid; false if rejected.
    bool setConfig(const MotionEstimatorConfig& config) noexcept;

private:
    /// @brief Minimal sample solver for Translation (2-DOF, 1 point).
    static void solveTranslation(
        const SubPixelPoint& p, const SubPixelPoint& pPrime, AffineTransform2D& outTransform) noexcept;

    /// @brief Minimal sample solver for Euclidean / Rigid (3-DOF, 2 points).
    [[nodiscard]] static bool solveEuclidean(const SubPixelPoint& p0, const SubPixelPoint& p0Prime,
        const SubPixelPoint& p1, const SubPixelPoint& p1Prime, AffineTransform2D& outTransform) noexcept;

    /// @brief Minimal sample solver for Similarity (4-DOF, 2 points).
    [[nodiscard]] static bool solveSimilarity(const SubPixelPoint& p0, const SubPixelPoint& p0Prime,
        const SubPixelPoint& p1, const SubPixelPoint& p1Prime, AffineTransform2D& outTransform) noexcept;

    /// @brief Minimal sample solver for Affine (6-DOF, 3 points).
    [[nodiscard]] static bool solveAffine(
        const SubPixelPoint* p, const SubPixelPoint* pPrime, AffineTransform2D& outTransform) noexcept;

    /// @brief Evaluate MSAC bounded cost across all correspondences.
    static void evaluateModel(const AffineTransform2D& model, const SubPixelPoint* p, const SubPixelPoint* pPrime,
        std::size_t count, float thresholdSquared, float& outCost, std::uint32_t& outInliers) noexcept;

    /// @brief Refine transformation using all consensus inliers via normal equations.
    [[nodiscard]] static bool refineInliers(const SubPixelPoint* p, const SubPixelPoint* pPrime, const bool* inlierMask,
        std::size_t count, StabilizationMode mode, AffineTransform2D& outTransform) noexcept;

    /// @brief Check whether transformation violates physical sanity bounds.
    [[nodiscard]] static bool checkTransform(const AffineTransform2D& t) noexcept;

    /// @brief Check whether sample points form a degenerate configuration.
    [[nodiscard]] static bool checkDegenerate(
        const SubPixelPoint* p, std::size_t count, StabilizationMode mode) noexcept;

    /// @brief Minimal sample size for a given stabilization mode.
    [[nodiscard]] static constexpr std::uint32_t sampleSize(StabilizationMode mode) noexcept
    {
        switch (mode) {
        case StabilizationMode::TranslationOnly:
            return 1U;
        case StabilizationMode::TranslationRotation:
        case StabilizationMode::Similarity:
            return 2U;
        case StabilizationMode::Affine:
        case StabilizationMode::RollingShutterComp:
        default:
            return 3U;
        }
    }

    MotionEstimatorConfig config_ { };
};

} // namespace Stabilizer
