#pragma once

/// @file ZoomTrackTypes.h
/// @brief Value objects, limits, and domain structures for parfocal zoom-to-focus curves.

#include <cstddef>
#include <cstdint>

#include <array>

#include "LensTypes.h"

namespace LensControl {

/// @enum InterpolationMode
/// @brief Mathematical interpolation algorithm for zoom-to-focus evaluation.
enum class InterpolationMode : std::uint8_t {
    PchipSpline = 0U, ///< Monotone Cubic Hermite Spline (zero overshoot)
    PiecewiseLinear = 1U ///< Piecewise linear interpolation fallback
};

/// @enum TrackCurveError
/// @brief Status codes for parfocal curve parsing, serialization, and evaluation.
enum class TrackCurveError : std::uint8_t {
    None = 0U,
    EmptyInput = 1U,
    InvalidHeader = 2U,
    CountMismatch = 3U,
    NonMonotonicZoom = 4U,
    CapacityExceeded = 5U,
    FileNotFound = 6U,
    InsufficientKnots = 7U,
    WriteError = 8U
};

/// @struct TrackKnot
/// @brief Calibrated coordinate knot with precomputed derivative slope.
struct TrackKnot {
    MotorPosition zoom { 0U };
    MotorPosition focus { 0U };
    float slope { 0.0F }; ///< Precalculated PCHIP derivative (df/dz)
};

/// @struct TrackCurve
/// @brief Fixed-capacity calibrated curve representing zoom-to-focus profile at a distance.
struct TrackCurve {
    static constexpr std::size_t kMaxKnots = 64U;

    std::array<TrackKnot, kMaxKnots> knots { };
    std::size_t knotCount { 0U };
    float distanceMeters { 0.0F }; ///< 0.0F denotes infinity

    /// @brief Validates that knots are sufficient and zoom positions strictly increase.
    /// @return True if curve has at least 2 knots and strict zoom monotonicity.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        if (knotCount < 2U || knotCount > kMaxKnots) {
            return false;
        }
        for (std::size_t i = 1U; i < knotCount; ++i) {
            if (knots[i].zoom.value() <= knots[i - 1U].zoom.value()) {
                return false;
            }
        }
        return true;
    }
};

/// @struct TrackCurveSet
/// @brief Multi-distance container holding up to 8 discrete distance curves.
struct TrackCurveSet {
    static constexpr std::size_t kMaxCurves = 8U;

    std::array<TrackCurve, kMaxCurves> curves { };
    std::size_t curveCount { 0U };
    std::size_t activeIndex { 0U };
};

} // namespace LensControl
