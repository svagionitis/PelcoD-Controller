/// @file FeatureDetector.h
/// @brief Shi-Tomasi corner detector with uniform spatial grid bucketing for video stabilization.
#pragma once

#include <cstddef>
#include <cstdint>

#include "FrameView.h"
#include "StabilizerArena.h"
#include "StabilizerTypes.h"

namespace Stabilizer {

/// @brief Detected feature landmark with sub-pixel location, score, and cell ID.
struct FeaturePoint {
    SubPixelPoint pt { };
    float score { 0.0F };
    std::uint16_t cellId { 0U };

    /// @brief Check if feature coordinates and score are valid.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return pt.isValid() && (score > 0.0F);
    }
};

/// @brief Invariant-enforced quality threshold ratio [0.0001 .. 0.50].
class QualityThreshold {
public:
    constexpr QualityThreshold() noexcept = default;

    /// @brief Construct quality threshold clamped to allowable bounds.
    /// @param val Relative quality fraction of peak corner score.
    explicit constexpr QualityThreshold(float val) noexcept
        : value_ { (val < 0.0001F) ? 0.0001F : ((val > 0.50F) ? 0.50F : val) }
    {
    }

    /// @brief Underlying float threshold value.
    [[nodiscard]] constexpr float value() const noexcept
    {
        return value_;
    }

private:
    float value_ { 0.01F }; // Default 1% of peak score
};

/// @brief Spatial grid bucketing configuration parameters.
struct GridBucketingConfig {
    std::uint16_t gridCols { 8U };
    std::uint16_t gridRows { 6U };
    std::uint16_t maxFeaturesPerCell { 8U };
    float minDistance { 8.0F };

    /// @brief Check whether configuration parameters are within allowable bounds.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (gridCols >= 1U) && (gridCols <= 32U) && (gridRows >= 1U) && (gridRows <= 32U)
            && (maxFeaturesPerCell >= 1U) && (maxFeaturesPerCell <= 64U) && (minDistance >= 0.0F)
            && (minDistance <= 128.0F);
    }

    /// @brief Total number of cells across the grid.
    [[nodiscard]] constexpr std::uint32_t totalBuckets() const noexcept
    {
        return static_cast<std::uint32_t>(gridCols) * static_cast<std::uint32_t>(gridRows);
    }

    /// @brief Maximum capacity across all cells.
    [[nodiscard]] constexpr std::uint32_t maxCapacity() const noexcept
    {
        return totalBuckets() * static_cast<std::uint32_t>(maxFeaturesPerCell);
    }
};

/// @brief Aggregate configuration parameters for FeatureDetector.
struct FeatureDetectorConfig {
    QualityThreshold quality { };
    GridBucketingConfig bucketing { };
    std::uint16_t borderMargin { 8U };
    std::uint32_t maxTotalFeatures { 300U };

    /// @brief Check whether entire detector configuration is valid.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return bucketing.isValid() && (borderMargin >= 3U) && (borderMargin <= 64U) && (maxTotalFeatures >= 10U)
            && (maxTotalFeatures <= 4096U);
    }
};

/// @brief Clean-room, zero-heap Shi-Tomasi feature detector with spatial grid bucketing.
class FeatureDetector {
public:
    FeatureDetector() noexcept = default;
    explicit FeatureDetector(const FeatureDetectorConfig& config) noexcept;

    /// @brief Detect salient features across a frame view using monotonic scratchpad arena.
    /// @param frame Input 8-bit luma frame view.
    /// @param arena Monotonic scratchpad allocator for temporary buffers.
    /// @param outFeatures Output destination array receiving detected features.
    /// @param maxFeatures Maximum capacity of outFeatures array.
    /// @return Number of features populated into outFeatures.
    [[nodiscard]] std::size_t detectFeatures(
        const FrameView& frame, StabilizerArena& arena, FeaturePoint* outFeatures, std::size_t maxFeatures) noexcept;

    /// @brief Retrieve active detector configuration.
    [[nodiscard]] const FeatureDetectorConfig& config() const noexcept
    {
        return config_;
    }

    /// @brief Update detector configuration.
    /// @param config New configuration to apply if valid.
    /// @return true if updated; false if configuration was rejected.
    bool setConfig(const FeatureDetectorConfig& config) noexcept;

private:
    void computeGradients(const FrameView& frame, std::int16_t* gradX, std::int16_t* gradY) noexcept;

    void computeCornerScores(const FrameView& frame, const std::int16_t* gradX, const std::int16_t* gradY,
        float* outScores, float& outMaxScore) noexcept;

    void suppressNonMax(
        const FrameDimensions& dims, const float* scores, float minThreshold, float* suppressedScores) noexcept;

    std::size_t bucketFeatures(const FrameDimensions& dims, const float* suppressedScores, FeaturePoint* outFeatures,
        std::size_t maxFeatures, StabilizerArena& arena) noexcept;

    FeatureDetectorConfig config_ { };
};

} // namespace Stabilizer
