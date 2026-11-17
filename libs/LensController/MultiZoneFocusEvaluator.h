#pragma once

/// @file MultiZoneFocusEvaluator.h
/// @brief Zero-heap 3x3 spatial matrix focus evaluator with saliency and target arbitration.

#include <cstddef>
#include <cstdint>

#include <array>

#include "AfHeuristicsTypes.h"
#include "FocusEvaluator.h"
#include "LensTypes.h"

namespace LensControl {

/// @class MultiZoneFocusEvaluator
/// @brief Evaluates 9 spatial zones spanning the frame with motion and foreground arbitration.
class MultiZoneFocusEvaluator final {
public:
    static constexpr std::size_t kNumZones = 9U;
    static constexpr std::size_t kDownsampleGrid = 32U; ///< 32x32 temporal subsampling

    explicit MultiZoneFocusEvaluator(FocusEvaluator& evaluator) noexcept;
    ~MultiZoneFocusEvaluator() noexcept = default;

    MultiZoneFocusEvaluator(const MultiZoneFocusEvaluator&) = delete;
    MultiZoneFocusEvaluator& operator=(const MultiZoneFocusEvaluator&) = delete;
    MultiZoneFocusEvaluator(MultiZoneFocusEvaluator&&) = delete;
    MultiZoneFocusEvaluator& operator=(MultiZoneFocusEvaluator&&) = delete;

    /// @brief Evaluates the 3x3 spatial focus matrix across an 8-bit luma frame.
    /// @param grayPixels Pointer to contiguous 8-bit grayscale pixel data.
    /// @param width Frame width in pixels.
    /// @param height Frame height in pixels.
    /// @param stride Row stride in bytes (if 0, assumes width).
    /// @param currentFocusPos Current focus motor position during sweep.
    /// @return MultiZoneResult containing all zone scores and arbitrated winner.
    [[nodiscard]] MultiZoneResult evaluateGrid(const std::uint8_t* grayPixels, std::size_t width, std::size_t height,
        std::size_t stride = 0U, MotorPosition currentFocusPos = MotorPosition { 0U }) noexcept;

    /// @brief Updates active target bounding box for target-centering prior.
    /// @param normX Target center X [0.0..1.0].
    /// @param normY Target center Y [0.0..1.0].
    /// @param normW Target width [0.0..1.0].
    /// @param normH Target height [0.0..1.0].
    void setTargetRoi(float normX, float normY, float normW, float normH) noexcept;

    /// @brief Clears active target bounding box.
    void clearTargetRoi() noexcept;

    /// @brief Sets the active arbitration strategy.
    void setArbitrationMode(MultiZoneArbitrationMode mode) noexcept;

    /// @brief Returns the active arbitration strategy.
    [[nodiscard]] MultiZoneArbitrationMode arbitrationMode() const noexcept;

    /// @brief Resets temporal inter-frame history and peak trackers.
    void reset() noexcept;

    /// @brief Computes the bounding pixel rectangle for a given zone index [0..8].
    [[nodiscard]] static PixelRoi zonePixelRoi(
        std::size_t zoneIdx, std::size_t frameWidth, std::size_t frameHeight) noexcept;

    /// @brief Computes the normalized fractional ROI for a given zone index [0..8].
    [[nodiscard]] static NormalizedRoi zoneNormalizedRoi(std::size_t zoneIdx) noexcept;

private:
    FocusEvaluator& m_evaluator;
    MultiZoneArbitrationMode m_mode { MultiZoneArbitrationMode::CenterPriority };
    bool m_hasTarget { false };
    float m_targetX { 0.5F };
    float m_targetY { 0.5F };
    float m_targetW { 0.2F };
    float m_targetH { 0.2F };

    // Low-resolution temporal subsample buffer for zero-heap motion saliency
    std::array<std::uint8_t, kDownsampleGrid * kDownsampleGrid> m_prevSubsample { };
    bool m_hasPrevFrame { false };

    // Tracking peaks per zone for closest-subject arbitration
    std::array<std::uint32_t, kNumZones> m_prevZoneMetric { };
    std::array<std::uint32_t, kNumZones> m_maxZoneMetric { };
    std::array<MotorPosition, kNumZones> m_prevPositions { };
    std::array<MotorPosition, kNumZones> m_peakPositions { };
    std::array<bool, kNumZones> m_peakConfirmed { };

    void computeSaliency(const std::uint8_t* grayPixels, std::size_t width, std::size_t height, std::size_t stride,
        std::array<float, kNumZones>& outSaliency) noexcept;

    [[nodiscard]] float computeTargetWeight(std::size_t zoneIdx) const noexcept;
};

} // namespace LensControl
