#pragma once

/// @file AfHeuristicsTypes.h
/// @brief Strongly-typed value objects, structures, and enums for intelligent autofocus heuristics.

#include <cstddef>
#include <cstdint>

#include <array>

#include "FocusEvaluator.h"
#include "LensTypes.h"

namespace LensControl {

/// @class AngularVelocityDegPerSec
/// @brief Strongly-typed angular velocity in degrees per second.
class AngularVelocityDegPerSec {
public:
    explicit constexpr AngularVelocityDegPerSec(float degPerSec = 0.0F) noexcept
        : m_degPerSec { degPerSec }
    {
    }

    [[nodiscard]] constexpr float value() const noexcept
    {
        return m_degPerSec;
    }

    [[nodiscard]] constexpr bool operator<(AngularVelocityDegPerSec o) const noexcept
    {
        return m_degPerSec < o.m_degPerSec;
    }

    [[nodiscard]] constexpr bool operator>(AngularVelocityDegPerSec o) const noexcept
    {
        return m_degPerSec > o.m_degPerSec;
    }

    [[nodiscard]] constexpr bool operator<=(AngularVelocityDegPerSec o) const noexcept
    {
        return m_degPerSec <= o.m_degPerSec;
    }

    [[nodiscard]] constexpr bool operator>=(AngularVelocityDegPerSec o) const noexcept
    {
        return m_degPerSec >= o.m_degPerSec;
    }

    [[nodiscard]] constexpr bool operator==(AngularVelocityDegPerSec o) const noexcept
    {
        return m_degPerSec == o.m_degPerSec;
    }

    [[nodiscard]] constexpr bool operator!=(AngularVelocityDegPerSec o) const noexcept
    {
        return m_degPerSec != o.m_degPerSec;
    }

private:
    float m_degPerSec { 0.0F };
};

/// @class DistanceMeters
/// @brief Strongly-typed distance in physical meters.
class DistanceMeters {
public:
    explicit constexpr DistanceMeters(float meters = 0.0F) noexcept
        : m_meters { meters }
    {
    }

    [[nodiscard]] constexpr float value() const noexcept
    {
        return m_meters;
    }

    [[nodiscard]] constexpr bool isInfinite() const noexcept
    {
        return m_meters >= 1.0e9F;
    }

    [[nodiscard]] static constexpr DistanceMeters infinity() noexcept
    {
        return DistanceMeters { 1.0e9F };
    }

    [[nodiscard]] constexpr bool operator<(DistanceMeters o) const noexcept
    {
        return m_meters < o.m_meters;
    }

    [[nodiscard]] constexpr bool operator>(DistanceMeters o) const noexcept
    {
        return m_meters > o.m_meters;
    }

    [[nodiscard]] constexpr bool operator<=(DistanceMeters o) const noexcept
    {
        return m_meters <= o.m_meters;
    }

    [[nodiscard]] constexpr bool operator>=(DistanceMeters o) const noexcept
    {
        return m_meters >= o.m_meters;
    }

    [[nodiscard]] constexpr bool operator==(DistanceMeters o) const noexcept
    {
        return m_meters == o.m_meters;
    }

    [[nodiscard]] constexpr bool operator!=(DistanceMeters o) const noexcept
    {
        return m_meters != o.m_meters;
    }

private:
    float m_meters { 0.0F };
};

/// @class FocalLengthMm
/// @brief Strongly-typed optical focal length in millimeters.
class FocalLengthMm {
public:
    explicit constexpr FocalLengthMm(float mm = 1.0F) noexcept
        : m_mm { mm >= 0.0F ? mm : 0.0F }
    {
    }

    [[nodiscard]] constexpr float value() const noexcept
    {
        return m_mm;
    }

    [[nodiscard]] constexpr bool operator<(FocalLengthMm o) const noexcept
    {
        return m_mm < o.m_mm;
    }

    [[nodiscard]] constexpr bool operator>(FocalLengthMm o) const noexcept
    {
        return m_mm > o.m_mm;
    }

    [[nodiscard]] constexpr bool operator<=(FocalLengthMm o) const noexcept
    {
        return m_mm <= o.m_mm;
    }

    [[nodiscard]] constexpr bool operator>=(FocalLengthMm o) const noexcept
    {
        return m_mm >= o.m_mm;
    }

    [[nodiscard]] constexpr bool operator==(FocalLengthMm o) const noexcept
    {
        return m_mm == o.m_mm;
    }

    [[nodiscard]] constexpr bool operator!=(FocalLengthMm o) const noexcept
    {
        return m_mm != o.m_mm;
    }

private:
    float m_mm { 1.0F };
};

/// @class FNumber
/// @brief Strongly-typed optical aperture F-number (N = f / D).
class FNumber {
public:
    explicit constexpr FNumber(float n = 1.6F) noexcept
        : m_n { n > 0.5F ? n : 1.6F }
    {
    }

    [[nodiscard]] constexpr float value() const noexcept
    {
        return m_n;
    }

    [[nodiscard]] constexpr bool operator<(FNumber o) const noexcept
    {
        return m_n < o.m_n;
    }

    [[nodiscard]] constexpr bool operator>(FNumber o) const noexcept
    {
        return m_n > o.m_n;
    }

    [[nodiscard]] constexpr bool operator<=(FNumber o) const noexcept
    {
        return m_n <= o.m_n;
    }

    [[nodiscard]] constexpr bool operator>=(FNumber o) const noexcept
    {
        return m_n >= o.m_n;
    }

    [[nodiscard]] constexpr bool operator==(FNumber o) const noexcept
    {
        return m_n == o.m_n;
    }

    [[nodiscard]] constexpr bool operator!=(FNumber o) const noexcept
    {
        return m_n != o.m_n;
    }

private:
    float m_n { 1.6F };
};

/// @class CircleOfConfusionMm
/// @brief Strongly-typed circle of confusion diameter in millimeters.
class CircleOfConfusionMm {
public:
    explicit constexpr CircleOfConfusionMm(float mm = 0.0069F) noexcept
        : m_mm { mm > 0.0F ? mm : 0.0069F }
    {
    }

    [[nodiscard]] constexpr float value() const noexcept
    {
        return m_mm;
    }

    [[nodiscard]] constexpr bool operator<(CircleOfConfusionMm o) const noexcept
    {
        return m_mm < o.m_mm;
    }

    [[nodiscard]] constexpr bool operator>(CircleOfConfusionMm o) const noexcept
    {
        return m_mm > o.m_mm;
    }

    [[nodiscard]] constexpr bool operator<=(CircleOfConfusionMm o) const noexcept
    {
        return m_mm <= o.m_mm;
    }

    [[nodiscard]] constexpr bool operator>=(CircleOfConfusionMm o) const noexcept
    {
        return m_mm >= o.m_mm;
    }

    [[nodiscard]] constexpr bool operator==(CircleOfConfusionMm o) const noexcept
    {
        return m_mm == o.m_mm;
    }

    [[nodiscard]] constexpr bool operator!=(CircleOfConfusionMm o) const noexcept
    {
        return m_mm != o.m_mm;
    }

private:
    float m_mm { 0.0069F };
};

/// @enum AfGatingState
/// @brief Operational state of platform motion-gated continuous autofocus.
enum class AfGatingState : std::uint8_t {
    Active = 0U, ///< Platform motion below threshold; CAF active
    Suppressed = 1U, ///< Slew rate exceeds threshold; CAF frozen
    SettlingHoldoff = 2U ///< Slew stopped; waiting for mechanical settling
};

/// @struct DofLimits
/// @brief Calculated physical Depth-of-Field boundaries and hyperfocal distance.
struct DofLimits {
    DistanceMeters nearLimit { 0.0F };
    DistanceMeters farLimit { 0.0F };
    DistanceMeters depthOfField { 0.0F };
    DistanceMeters hyperfocalDistance { 0.0F };
    bool isFarInfinite { false };
};

/// @struct AdaptiveStepConfig
/// @brief Dynamically scaled autofocus search step sizes based on optical DOF.
struct AdaptiveStepConfig {
    std::uint16_t coarseStep { 300U };
    std::uint16_t midStep { 100U };
    std::uint16_t fineStep { 20U };
    float scaleFactor { 1.0F };
};

/// @enum MultiZoneArbitrationMode
/// @brief Arbitration strategy across the 3x3 focus tile grid.
enum class MultiZoneArbitrationMode : std::uint8_t {
    CenterPriority = 0U, ///< Center tile (Zone 4) weighted highest
    ClosestSubject = 1U, ///< First verified peak from Near to Far wins
    MotionSaliency = 2U, ///< Zone with highest temporal luma delta prioritized
    TargetCentering = 3U, ///< Zone overlapping target bounding box prioritized
    FusedWeighted = 4U ///< Multi-factor weighted sum of all zones
};

/// @struct ZoneScore
/// @brief Focus evaluation and weighting details for a single tile.
struct ZoneScore {
    FocusMetricScore metric { };
    float motionSaliency { 0.0F };
    float targetWeight { 1.0F };
    float compositeScore { 0.0F };
    bool peakConfirmed { false };
    MotorPosition peakPosition { 0U };
};

/// @struct MultiZoneResult
/// @brief Composite arbitration result spanning the 3x3 spatial matrix.
struct MultiZoneResult {
    std::array<ZoneScore, 9U> zones { };
    std::size_t selectedZoneIndex { 4U }; ///< Default center (Zone 4)
    std::uint32_t compositeMetric { 0U };
    bool hasForegroundPeak { false };
    MotorPosition recommendedPosition { 0U };
};

} // namespace LensControl
