#pragma once

/// @file PeakFitter.h
/// @brief Sub-step parabolic and Gaussian optical peak interpolation algorithms.

#include <cstddef>
#include <cstdint>

#include "AutoFocusTypes.h"
#include "LensTypes.h"

namespace LensControl {

/// @class PeakFitter
/// @brief Sub-step optical peak interpolator executing with zero dynamic heap allocations.
class PeakFitter final {
public:
    /// @brief Interpolates peak position using discrete 3-point parabolic fitting.
    /// @param p1 Left bracket sample point (z1 < z2).
    /// @param p2 Summit bracket sample point (z2, M2 >= M1 and M2 >= M3).
    /// @param p3 Right bracket sample point (z3 > z2).
    /// @return PeakFitResult with interpolated vertex and curvature.
    [[nodiscard]] static PeakFitResult fitParabolic(
        const SamplePoint& p1, const SamplePoint& p2, const SamplePoint& p3) noexcept;

    /// @brief Interpolates peak position using discrete 3-point Gaussian fitting.
    /// @param p1 Left bracket sample point.
    /// @param p2 Summit bracket sample point.
    /// @param p3 Right bracket sample point.
    /// @return PeakFitResult with Gaussian summit coordinate.
    [[nodiscard]] static PeakFitResult fitGaussian(
        const SamplePoint& p1, const SamplePoint& p2, const SamplePoint& p3) noexcept;

    /// @brief Finds the best peak across a pre-allocated array of search samples.
    /// @param samples Pointer to contiguous sample points.
    /// @param count Number of valid samples in array (minimum 3).
    /// @return Best PeakFitResult across all candidate local maxima.
    [[nodiscard]] static PeakFitResult fitMultiPoint(const SamplePoint* samples, std::size_t count) noexcept;
};

} // namespace LensControl
