#pragma once

/// @file TelemetryInterpolator.h
/// @brief Geodetic, attitude, and sensor parameter interpolation between KLV packets.

#include "KlvTypes.h"
#include <cstdint>

namespace Klv {

/// @class TelemetryInterpolator
/// @brief Reconstructs intermediate telemetry states adhering to STANAG 4609 dynamics.
class TelemetryInterpolator {
public:
    TelemetryInterpolator() = default;
    ~TelemetryInterpolator() = default;

    /// @brief Interpolates a complete UasDatalinkMessage at target PTS between two packets.
    /// @param[in] m1 Starting telemetry state.
    /// @param[in] pts1 Timestamp of m1 in 90 kHz ticks.
    /// @param[in] m2 Ending telemetry state.
    /// @param[in] pts2 Timestamp of m2 in 90 kHz ticks.
    /// @param[in] targetPts Target timestamp.
    /// @return Interpolated UasDatalinkMessage with updated timestamp.
    [[nodiscard]] static UasDatalinkMessage interpolate(const UasDatalinkMessage& m1,
                                                       std::uint64_t pts1,
                                                       const UasDatalinkMessage& m2,
                                                       std::uint64_t pts2,
                                                       std::uint64_t targetPts) noexcept;

    /// @brief Interpolates spherical angles [0, 360) with shortest-arc wrapping.
    /// @param[in] angle1Deg Start angle in degrees.
    /// @param[in] angle2Deg End angle in degrees.
    /// @param[in] factor Normalized factor [0.0, 1.0].
    /// @return Interpolated angle in degrees [0, 360).
    [[nodiscard]] static double interpolateAngle(double angle1Deg,
                                                 double angle2Deg,
                                                 double factor) noexcept;

    /// @brief Interpolates longitude coordinates [-180, +180] with anti-meridian wrapping.
    /// @param[in] lon1Deg Start longitude in degrees.
    /// @param[in] lon2Deg End longitude in degrees.
    /// @param[in] factor Normalized factor [0.0, 1.0].
    /// @return Interpolated longitude in degrees [-180, +180].
    [[nodiscard]] static double interpolateLongitude(double lon1Deg,
                                                    double lon2Deg,
                                                    double factor) noexcept;

    /// @brief Performs linear interpolation of double scalar value.
    /// @param[in] v1 Start value.
    /// @param[in] v2 End value.
    /// @param[in] factor Normalized factor [0.0, 1.0].
    /// @return Interpolated scalar.
    [[nodiscard]] static double lerp(double v1, double v2, double factor) noexcept;
};

} // namespace Klv
