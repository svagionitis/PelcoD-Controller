#pragma once

/// @file GeoreferenceUtils.h
/// @brief Geodetic coordinate calculations, target projection, and click-to-slew geometry.

#include "DemRayCaster.h"
#include "IDemProvider.h"
#include "Klv/KlvTypes.h"
#include <functional>
#include <optional>

namespace PayloadHal {

/// @struct GimbalLookAngles
/// @brief Result of inverse geodetic calculation: angles and range required to slew gimbal to a ground target.
struct GimbalLookAngles {
    double panAngleDeg { 0.0 }; ///< Gimbal azimuth relative to platform heading [-180.0, +180.0] deg
    double tiltAngleDeg { 0.0 }; ///< Gimbal elevation [-90.0, +90.0] deg (negative = downwards)
    double slantRangeMeters { 0.0 }; ///< Straight-line line-of-sight distance in meters
    double trueBearingDeg { 0.0 }; ///< True geographic bearing from platform to target [0.0, 360.0) deg
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
    [[nodiscard]] static std::optional<Klv::GeoPoint3D> computeTargetFromSlantRange(const Klv::GeoPoint3D& platformPos,
        double platformHeadingDeg, double gimbalPanDeg, double gimbalTiltDeg, double slantRangeMeters) noexcept;

    /// @brief Elevation lookup callback signature querying ground elevation in meters MSL at (lat, lon).
    using ElevationLookupFn = std::function<double(double latDeg, double lonDeg)>;

    /// @brief Computes 3D target intersection coordinate with ground plane / terrain elevation
    ///        when LRF is unavailable or unequipped, assuming optical line-of-sight points downwards.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true heading in degrees [0.0, 360.0).
    /// @param[in] gimbalPanDeg Gimbal azimuth relative to platform in degrees.
    /// @param[in] gimbalTiltDeg Gimbal elevation in degrees (must be < 0 for ground intersection).
    /// @param[in] groundElevationM Target ground elevation MSL in meters (default 0.0).
    /// @return Geodetic 3D coordinate of ground intersection, or std::nullopt if looking above horizon.
    [[nodiscard]] static std::optional<Klv::GeoPoint3D> computeTargetFromGroundIntersection(
        const Klv::GeoPoint3D& platformPos, double platformHeadingDeg, double gimbalPanDeg, double gimbalTiltDeg,
        double groundElevationM = 0.0) noexcept;

    /// @brief Computes 3D target intersection coordinate with optional Digital Elevation Model (DEM)
    ///        ray-marching convergence against ground elevation lookup.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true heading in degrees [0.0, 360.0).
    /// @param[in] gimbalPanDeg Gimbal azimuth relative to platform in degrees.
    /// @param[in] gimbalTiltDeg Gimbal elevation in degrees (must be < 0 for ground intersection).
    /// @param[in] elevationLookup Optional elevation callback (or DEM grid sampler).
    /// @param[in] fallbackGroundElevationM Fallback ground elevation MSL in meters (default 0.0).
    /// @return Geodetic 3D coordinate of ground intersection, or std::nullopt if looking above horizon.
    [[nodiscard]] static std::optional<Klv::GeoPoint3D> computeTargetFromGroundIntersection(
        const Klv::GeoPoint3D& platformPos, double platformHeadingDeg, double gimbalPanDeg, double gimbalTiltDeg,
        const ElevationLookupFn& elevationLookup, double fallbackGroundElevationM = 0.0) noexcept;

    /// @brief Computes 3D target intersection coordinate with a Digital Elevation Model (DEM)
    ///        using numerical 2-phase ray-marching with foreground occlusion handling.
    /// @param[in] dem Reference to digital elevation model provider.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true heading in degrees [0.0, 360.0).
    /// @param[in] gimbalPanDeg Gimbal azimuth relative to platform in degrees.
    /// @param[in] gimbalTiltDeg Gimbal elevation in degrees (must be < 0 for ground intersection).
    /// @param[in] config Numerical parameters for ray search.
    /// @return Geodetic 3D coordinate of ground intersection, or std::nullopt if looking above horizon.
    [[nodiscard]] static std::optional<Klv::GeoPoint3D> computeTargetFromDem(
        const IDemProvider& dem, const Klv::GeoPoint3D& platformPos, double platformHeadingDeg,
        double gimbalPanDeg, double gimbalTiltDeg, const DemRayConfig& config = {}) noexcept;

    /// @brief Computes the 4-corner ground projection footprint frustum polygon on the WGS-84 ellipsoid.
    /// @details If vfovDeg is <= 0.0, vertical FOV is automatically derived from hfovDeg assuming standard 16:9 aspect
    /// ratio. If gimbalRollDeg is non-zero, the projection is rotated by the roll angle around the optical axis.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true compass heading in degrees [0.0, 360.0).
    /// @param[in] gimbalPanDeg Gimbal azimuth relative to platform in degrees.
    /// @param[in] gimbalTiltDeg Gimbal elevation in degrees (negative = downwards).
    /// @param[in] hfovDeg Horizontal field of view in degrees (> 0.0).
    /// @param[in] vfovDeg Vertical field of view in degrees (if <= 0.0, derived from 16:9 aspect ratio).
    /// @param[in] groundElevationM Target ground elevation MSL in meters (default 0.0).
    /// @param[in] gimbalRollDeg Optical roll angle in degrees (default 0.0).
    /// @return FrustumCorners containing 4 geodetic coordinates, or std::nullopt if looking at or above horizon.
    [[nodiscard]] static std::optional<Klv::FrustumCorners> computeFrustumCorners(const Klv::GeoPoint3D& platformPos,
        double platformHeadingDeg, double gimbalPanDeg, double gimbalTiltDeg, double hfovDeg, double vfovDeg = 0.0,
        double groundElevationM = 0.0, double gimbalRollDeg = 0.0) noexcept;

    /// @brief Computes the 4-corner ground footprint frustum polygon projected onto a Digital Elevation Model.
    /// @param[in] dem Reference to digital elevation model.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true compass heading in degrees [0.0, 360.0).
    /// @param[in] gimbalPanDeg Gimbal azimuth relative to platform in degrees.
    /// @param[in] gimbalTiltDeg Gimbal elevation in degrees (negative = downwards).
    /// @param[in] hfovDeg Horizontal field of view in degrees (> 0.0).
    /// @param[in] vfovDeg Vertical field of view in degrees (if <= 0.0, derived from 16:9 aspect ratio).
    /// @param[in] gimbalRollDeg Optical roll angle in degrees (default 0.0).
    /// @param[in] config Ray-casting configuration parameters.
    /// @return FrustumCorners containing 4 geodetic coordinates, or std::nullopt if any corner looks at or above horizon.
    [[nodiscard]] static std::optional<Klv::FrustumCorners> computeFrustumCorners(
        const IDemProvider& dem, const Klv::GeoPoint3D& platformPos,
        double platformHeadingDeg, double gimbalPanDeg, double gimbalTiltDeg,
        double hfovDeg, double vfovDeg = 0.0, double gimbalRollDeg = 0.0,
        const DemRayConfig& config = {}) noexcept;

    /// @brief Computes the required gimbal roll angle to cancel platform roll/pitch attitude
    ///        and maintain the optical image frame level with the true geodetic horizon.
    /// @param[in] platformRollDeg Host platform roll / bank angle in degrees (positive = Right Wing Down).
    /// @param[in] platformPitchDeg Host platform pitch angle in degrees (positive = Nose Up).
    /// @param[in] gimbalPanDeg Gimbal azimuth relative to platform in degrees.
    /// @param[in] gimbalTiltDeg Gimbal elevation relative to platform in degrees.
    /// @return Counter-roll angle in degrees [-180.0, +180.0].
    [[nodiscard]] static double computeLevelingRoll(double platformRollDeg, double platformPitchDeg,
        double gimbalPanDeg, double gimbalTiltDeg) noexcept;

    /// @brief Calculates the gimbal pan and tilt look angles required to point at a geographic target.
    /// @param[in] platformPos Platform 3D position (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true compass heading in degrees [0.0, 360.0).
    /// @param[in] targetPos Target 3D position (latitude, longitude, altitude MSL in meters).
    /// @return GimbalLookAngles structure containing relative pan, tilt, bearing, and slant range.
    [[nodiscard]] static GimbalLookAngles computeLookAnglesToTarget(
        const Klv::GeoPoint3D& platformPos, double platformHeadingDeg, const Klv::GeoPoint3D& targetPos) noexcept;
};

} // namespace PayloadHal
