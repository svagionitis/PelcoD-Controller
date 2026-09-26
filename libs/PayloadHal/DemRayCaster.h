#pragma once

/// @file DemRayCaster.h
/// @brief Numerical ray-casting and root-finding engine against Digital Elevation Models.

#include "IDemProvider.h"
#include "Klv/KlvTypes.h"
#include <optional>

namespace PayloadHal {

/// @struct DemRayConfig
/// @brief Algorithmic parameters controlling terrain ray-marching resolution and convergence.
struct DemRayConfig {
    double initialStepMeters { 50.0 };      ///< Step size along ray during search (default 50m)
    double minStepMeters { 5.0 };           ///< Minimum adaptive step size near surface
    double maxSlantRangeMeters { 50000.0 };  ///< Maximum slant range cutoff (default 50km)
    double toleranceMeters { 0.1 };         ///< Convergence height difference tolerance in meters (10cm)
    int maxRootIterations { 12 };           ///< Maximum bisection / secant refinement iterations
    double fallbackGroundElevationM { 0.0 };///< Fallback elevation if outside DEM coverage
};

/// @struct DemIntersectionResult
/// @brief Complete result of terrain line-of-sight ray intersection.
struct DemIntersectionResult {
    Klv::GeoPoint3D targetPosition {};      ///< 3D intersection coordinate (lat, lon, alt MSL)
    double slantRangeMeters { 0.0 };        ///< Straight-line slant range from platform to terrain
    double rayElevationAtTargetM { 0.0 };   ///< Computed ray height at intersection point
    double demElevationAtTargetM { 0.0 };   ///< Sampled DEM ground elevation at intersection point
    bool isOccludedByForeground { false };  ///< True if intersected elevated terrain/ridge above baseline
    int stepsEvaluated { 0 };               ///< Total ray steps evaluated
};

/// @class DemRayCaster
/// @brief Analytical ray-marching engine for terrain surface intersection.
class DemRayCaster {
public:
    /// @brief Intersects a line-of-sight ray with an elevation model using forward ray-marching and root refinement.
    /// @param[in] dem Reference to digital elevation model.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true compass heading [0 .. 360) degrees.
    /// @param[in] gimbalPanDeg Gimbal azimuth relative to platform in degrees.
    /// @param[in] gimbalTiltDeg Gimbal elevation in degrees (must be < 0 for downward ground intersection).
    /// @param[in] config Numerical parameters for ray search.
    /// @return DemIntersectionResult if intersection is found, std::nullopt otherwise.
    [[nodiscard]] static std::optional<DemIntersectionResult> intersect(
        const IDemProvider& dem,
        const Klv::GeoPoint3D& platformPos,
        double platformHeadingDeg,
        double gimbalPanDeg,
        double gimbalTiltDeg,
        const DemRayConfig& config = {}) noexcept;

    /// @brief Evaluates the exact 3D ray point and altitude at distance s along the line of sight.
    /// @param[in] platformPos Platform 3D coordinate.
    /// @param[in] trueBearingDeg True compass azimuth bearing [0 .. 360) degrees.
    /// @param[in] tiltDeg Elevation angle in degrees (negative = downwards).
    /// @param[in] slantDistanceM Distance along ray in meters.
    /// @return GeoPoint3D containing 2D ground coordinate and exact curved-Earth ray altitude.
    [[nodiscard]] static Klv::GeoPoint3D evaluateRayPoint(
        const Klv::GeoPoint3D& platformPos,
        double trueBearingDeg,
        double tiltDeg,
        double slantDistanceM) noexcept;
};

} // namespace PayloadHal
