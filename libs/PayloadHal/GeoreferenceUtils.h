#pragma once

/// @file GeoreferenceUtils.h
/// @brief Geodetic coordinate calculations, target projection, and click-to-slew geometry.

#include "Klv/KlvTypes.h"
#include <optional>

namespace PayloadHal {

/// @struct GimbalLookAngles
/// @brief Result of inverse geodetic calculation: angles and range required to slew gimbal to a ground target.
struct GimbalLookAngles {
    double panAngleDeg { 0.0 };      ///< Gimbal azimuth relative to platform heading [-180.0, +180.0] deg
    double tiltAngleDeg { 0.0 };     ///< Gimbal elevation [-90.0, +90.0] deg (negative = downwards)
    double slantRangeMeters { 0.0 }; ///< Straight-line line-of-sight distance in meters
    double trueBearingDeg { 0.0 };   ///< True geographic bearing from platform to target [0.0, 360.0) deg
};

/// @class GeoreferenceUtils
/// @brief Analytical helper functions for target coordinate projection, terrain intersection,
///        and coordinate-to-gimbal look angle computation.
class GeoreferenceUtils {
public:
    /// @brief Projects 3D target coordinates given platform position, platform heading,
    ///        gimbal pan/tilt orientation, and measured LRF slant range.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true compass heading in degrees [0.0, 360.0).
    /// @param[in] gimbalPanDeg Gimbal azimuth relative to platform in degrees.
    /// @param[in] gimbalTiltDeg Gimbal elevation in degrees (positive = up, negative = down).
    /// @param[in] slantRangeMeters Measured straight-line distance in meters (> 0.0).
    /// @return Geodetic 3D coordinate of target, or std::nullopt if slantRangeMeters <= 0.
    [[nodiscard]] static std::optional<Klv::GeoPoint3D> computeTargetFromSlantRange(
        const Klv::GeoPoint3D& platformPos,
        double platformHeadingDeg,
        double gimbalPanDeg,
        double gimbalTiltDeg,
        double slantRangeMeters) noexcept;

    /// @brief Computes 3D target intersection coordinate with ground plane / terrain elevation
    ///        when LRF is unavailable or unequipped, assuming optical line-of-sight points downwards.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true heading in degrees [0.0, 360.0).
    /// @param[in] gimbalPanDeg Gimbal azimuth relative to platform in degrees.
    /// @param[in] gimbalTiltDeg Gimbal elevation in degrees (must be < 0 for ground intersection).
    /// @param[in] groundElevationM Target ground elevation MSL in meters (default 0.0).
    /// @return Geodetic 3D coordinate of ground intersection, or std::nullopt if looking above horizon.
    [[nodiscard]] static std::optional<Klv::GeoPoint3D> computeTargetFromGroundIntersection(
        const Klv::GeoPoint3D& platformPos,
        double platformHeadingDeg,
        double gimbalPanDeg,
        double gimbalTiltDeg,
        double groundElevationM = 0.0) noexcept;

    /// @brief Calculates the gimbal pan and tilt look angles required to point at a geographic target.
    /// @param[in] platformPos Platform 3D position (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true compass heading in degrees [0.0, 360.0).
    /// @param[in] targetPos Target 3D position (latitude, longitude, altitude MSL in meters).
    /// @return GimbalLookAngles structure containing relative pan, tilt, bearing, and slant range.
    [[nodiscard]] static GimbalLookAngles computeLookAnglesToTarget(
        const Klv::GeoPoint3D& platformPos,
        double platformHeadingDeg,
        const Klv::GeoPoint3D& targetPos) noexcept;
};

} // namespace PayloadHal
