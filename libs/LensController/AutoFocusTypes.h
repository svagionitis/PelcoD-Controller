#pragma once

/// @file AutoFocusTypes.h
/// @brief Strong domain types, state definitions, and configuration for autofocus algorithms.

#include <cstddef>
#include <cstdint>

#include "LensTypes.h"

namespace LensControl {

/// @enum AutoFocusState
/// @brief Discrete operational state machine phases for autofocus execution.
enum class AutoFocusState : std::uint8_t {
    Idle = 0U,
    Initializing = 1U,
    CoarseSweep = 2U,
    MidSweep = 3U,
    FineSweep = 4U,
    Reversing = 5U,
    SlewToPeak = 6U,
    PeakConfirmed = 7U,
    Failed = 8U
};

/// @struct AutoFocusConfig
/// @brief Calibrated parameters for closed-loop autofocus execution matching Sightline 0xB2/0x55.
struct AutoFocusConfig {
    MotorSpeed coarseSpeed { MotorSpeed { 180U } };
    MotorSpeed midSpeed { MotorSpeed { 100U } };
    MotorSpeed fineSpeed { MotorSpeed { 40U } };
    float coarseThresh { 0.65F }; ///< Ratio M / M_max to enter Mid Sweep
    float fineThresh { 0.15F }; ///< Derivative dM / M to enter Fine Sweep
    std::uint8_t cdownMax { 3U }; ///< Consecutive declines to confirm peak
    std::uint16_t coarseStep { 300U }; ///< Motor steps per coarse tick
    std::uint16_t midStep { 100U }; ///< Motor steps per mid tick
    std::uint16_t fineStep { 20U }; ///< Motor steps per fine tick
    std::uint16_t backlashSteps { 50U }; ///< Actuator gear mechanical backlash
    std::uint8_t frameOffsetMult8 { 16U }; ///< Video pipeline delay in 1/8 frames (2.0 frames)
    float rateAdjust { 1.0F }; ///< Dynamic zoom speed scaling factor
    float cafDefocusFrac { 0.25F }; ///< Sharpness drop fraction triggering CAF refocus
    std::uint32_t timeoutMs { 10000U }; ///< Maximum search duration before timeout failure
    std::uint16_t searchMin { 0U }; ///< Lower search boundary
    std::uint16_t searchMax { 65535U }; ///< Upper search boundary
    std::uint16_t gssTolerance { 10U }; ///< Golden section search step tolerance
};

/// @struct SamplePoint
/// @brief Sampled optical position and sharpness evaluation pair.
struct SamplePoint {
    MotorPosition pos { 0U };
    FocusMetricScore score { };
    std::uint64_t timestampNs { 0U };
};

/// @struct PeakFitResult
/// @brief Mathematical interpolation result from PeakFitter.
struct PeakFitResult {
    MotorPosition vertex { 0U };
    float subStepOffset { 0.0F }; ///< Sub-encoder offset in [-h/2, +h/2]
    float peakScore { 0.0F }; ///< Interpolated summit sharpness score
    float curvature { 0.0F }; ///< Parabolic quadratic curvature term (a < 0)
    float rSquared { 1.0F }; ///< Goodness-of-fit coefficient of determination
    bool isValid { false }; ///< True if strictly convex summit identified
};

} // namespace LensControl
