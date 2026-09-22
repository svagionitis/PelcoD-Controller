#pragma once

/// @file GeodesyUtils.h
/// @brief Mathematical geodetic and coordinate transformation utilities for ONVIF PTZ GeoMove.

#include "OnvifTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Onvif::Geodesy {

/// @brief Mean Earth radius in meters according to WGS84 ellipsoid spherical approximation.
inline constexpr double EarthRadiusMeters = 6371000.0;

/// @brief Mathematical constant Pi.
inline constexpr double Pi = 3.14159265358979323846;

/// @brief Converts degrees to radians.
[[nodiscard]] inline constexpr double degToRad(double deg) noexcept
{
    return deg * (Pi / 180.0);
}

/// @brief Converts radians to degrees.
[[nodiscard]] inline constexpr double radToDeg(double rad) noexcept
{
    return rad * (180.0 / Pi);
}

/// @brief Normalizes an azimuth angle into the range [0.0, 360.0).
[[nodiscard]] inline double normalizeAngle360(double deg) noexcept
{
    double result = std::fmod(deg, 360.0);
    if (result < 0.0) {
        result += 360.0;
    }
    return result;
}

/// @brief Clamps an elevation/tilt angle into the range [-90.0, +90.0].
[[nodiscard]] inline double clampElevation90(double deg) noexcept
{
    return std::clamp(deg, -90.0, 90.0);
}

/// @brief Calculates true bearing, surface distance, slant range, and camera-relative pan/tilt.
/// @param[in] cameraLoc Camera WGS84 geographic coordinate (lat, lon, elevation).
/// @param[in] cameraOrient Camera 3-axis mounting orientation (yaw/heading, pitch, roll).
/// @param[in] targetLoc Target WGS84 geographic coordinate.
/// @param[out] outPanDeg Computed camera-relative pan angle in degrees [0.0, 360.0).
/// @param[out] outTiltDeg Computed camera-relative tilt angle in degrees [-90.0, +90.0].
/// @param[out] outSlantRangeMeters Slant line-of-sight distance in meters.
/// @return True if computation was mathematically well-defined.
inline bool computeTargetAzimuthElevation(const GeoLocation& cameraLoc, const GeoOrientation& cameraOrient,
    const GeoLocation& targetLoc, double& outPanDeg, double& outTiltDeg, double& outSlantRangeMeters) noexcept
{
    const double phi1 = degToRad(cameraLoc.latitude);
    const double lam1 = degToRad(cameraLoc.longitude);
    const double phi2 = degToRad(targetLoc.latitude);
    const double lam2 = degToRad(targetLoc.longitude);

    const double deltaLam = lam2 - lam1;
    const double deltaPhi = phi2 - phi1;

    // 1. Forward azimuth (initial bearing)
    const double y = std::sin(deltaLam) * std::cos(phi2);
    const double x = std::cos(phi1) * std::sin(phi2) - std::sin(phi1) * std::cos(phi2) * std::cos(deltaLam);
    const double trueAzimuth = normalizeAngle360(radToDeg(std::atan2(y, x)));

    // 2. Great-circle surface distance using Haversine formula
    const double sinHalfPhi = std::sin(deltaPhi / 2.0);
    const double sinHalfLam = std::sin(deltaLam / 2.0);
    const double a = sinHalfPhi * sinHalfPhi + std::cos(phi1) * std::cos(phi2) * sinHalfLam * sinHalfLam;
    const double c = 2.0 * std::atan2(std::sqrt(std::max(0.0, a)), std::sqrt(std::max(0.0, 1.0 - a)));
    const double surfaceDistance = EarthRadiusMeters * c;

    // 3. Elevation difference and slant range
    const double deltaElevation = targetLoc.elevation - cameraLoc.elevation;
    outSlantRangeMeters = std::sqrt(surfaceDistance * surfaceDistance + deltaElevation * deltaElevation);

    // 4. Elevation angle above horizon (positive = looking up, negative = looking down)
    double trueElevation = 0.0;
    if (surfaceDistance > 1e-4) {
        trueElevation = radToDeg(std::atan2(deltaElevation, surfaceDistance));
    } else if (deltaElevation > 0.0) {
        trueElevation = 90.0;
    } else if (deltaElevation < 0.0) {
        trueElevation = -90.0;
    }

    // 5. Account for camera mounting yaw (compass heading) and pitch
    outPanDeg = normalizeAngle360(trueAzimuth - cameraOrient.yaw);
    outTiltDeg = clampElevation90(trueElevation - cameraOrient.pitch);

    return true;
}

/// @brief Estimates required optical zoom ratio [0.0, 1.0] to frame a target area of specified dimension.
/// @param[in] targetSpanMeters Width or height of the target area in meters.
/// @param[in] slantRangeMeters Distance to target in meters.
/// @param[in] hfovWideDeg Horizontal field of view at wide zoom (default: 60.0°).
/// @param[in] hfovTeleDeg Horizontal field of view at full tele zoom (default: 2.0°).
/// @return Normalized zoom position [0.0 (wide) to 1.0 (tele)].
[[nodiscard]] inline double computeZoomFromTargetArea(
    double targetSpanMeters, double slantRangeMeters, double hfovWideDeg = 60.0, double hfovTeleDeg = 2.0) noexcept
{
    if (slantRangeMeters <= 1e-3 || targetSpanMeters <= 1e-3) {
        return 0.0;
    }

    // Required Field of View angle
    const double requiredFov = radToDeg(2.0 * std::atan2(targetSpanMeters / 2.0, slantRangeMeters));

    // Linear interpolation between wide (zoom 0.0) and tele (zoom 1.0)
    const double zoom = (hfovWideDeg - requiredFov) / (hfovWideDeg - hfovTeleDeg);
    return std::clamp(zoom, 0.0, 1.0);
}

/// @brief Converts continuous pan/tilt angles in degrees to Pelco-D centidegrees (0.01°).
/// @param[in] panDeg Azimuth pan angle [0.0, 360.0).
/// @param[in] tiltDeg Elevation tilt angle [-90.0, +90.0].
/// @param[out] outPanCdeg Centidegrees for pan [0, 35999].
/// @param[out] outTiltCdeg Centidegrees for tilt [0, 35999].
inline void anglesToPelcoCentidegrees(
    double panDeg, double tiltDeg, std::uint16_t& outPanCdeg, std::uint16_t& outTiltCdeg) noexcept
{
    const double normPan = normalizeAngle360(panDeg);
    outPanCdeg = static_cast<std::uint16_t>(std::clamp(normPan * 100.0, 0.0, 35999.0));

    // For tilt: Pelco-D convention where 0° is horizontal, 0..90° is up (0..9000),
    // and negative angles (depression) are represented as 360.0 + tiltDeg (e.g. -10° -> 350.0° -> 35000)
    double normTilt = tiltDeg;
    if (normTilt < 0.0) {
        normTilt = 360.0 + normTilt;
    }
    normTilt = normalizeAngle360(normTilt);
    outTiltCdeg = static_cast<std::uint16_t>(std::clamp(normTilt * 100.0, 0.0, 35999.0));
}

} // namespace Onvif::Geodesy
