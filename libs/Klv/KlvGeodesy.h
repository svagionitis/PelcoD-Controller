#pragma once

/// @file KlvGeodesy.h
/// @brief Analytical geodetic projection engine for camera telemetry, slant range, and footprint frustums.
/// @details Supports MISB ST 0601, ST 0807, and ST 1206 full 3D platform attitude (yaw, pitch, roll),
///          sensor orientation (azimuth, elevation, optical roll), and Earth curvature horizon clipping.

#include "KlvTypes.h"
#include <optional>

namespace Klv {

/// @struct PlatformAttitude
/// @brief Platform 3D orientation (Yaw, Pitch, Roll) per MISB ST 0601 Section 7.2.2.
struct PlatformAttitude {
    double headingDeg { 0.0 }; ///< Tag 5: Platform Heading Angle [0, 360) deg
    double pitchDeg { 0.0 };   ///< Tag 6 / 90: Platform Pitch Angle [-90, +90] deg (positive = nose up)
    double rollDeg { 0.0 };    ///< Tag 7 / 91: Platform Roll Angle [-180, +180] deg (positive = starboard down)
};

/// @struct CameraOrientation
/// @brief Gimbal / camera 3D relative orientation per MISB ST 0601 Section 7.2.3.
struct CameraOrientation {
    double azimuthDeg { 0.0 };   ///< Tag 18: Sensor Relative Azimuth [0, 360) deg
    double elevationDeg { 0.0 }; ///< Tag 19: Sensor Relative Elevation [-180, +180] deg (negative = downwards)
    double rollDeg { 0.0 };      ///< Tag 20 / Tag 118: Sensor Relative Roll / Optical Roll [0, 360) deg
};

/// @struct Vector3D
/// @brief 3D Cartesian vector.
struct Vector3D {
    double x { 0.0 };
    double y { 0.0 };
    double z { 0.0 };
};

/// @class KlvGeodesy
/// @brief Provides WGS-84 / spherical Earth calculations to compute target coordinates and ATAK sensor footprint polygons.
class KlvGeodesy {
public:
    /// @brief Mean Earth radius in meters (WGS-84 volumetric mean radius).
    static constexpr double kEarthRadiusMeters { 6371008.8 };

    /// @brief Computes the direct destination coordinate given an origin, bearing, and distance.
    /// @param[in] origin Starting 2D geographic point (lat, lon).
    /// @param[in] bearingDeg Compass direction [0, 360) in degrees.
    /// @param[in] distanceMeters Ground travel distance in meters.
    /// @return Destination 2D geographic point (lat, lon).
    [[nodiscard]] static GeoPoint2D directGeodetic(const GeoPoint2D& origin,
                                                   double bearingDeg,
                                                   double distanceMeters) noexcept;

    /// @brief Computes great-circle surface distance between two coordinates in meters.
    /// @param[in] p1 First coordinate point.
    /// @param[in] p2 Second coordinate point.
    /// @return Surface distance in meters.
    [[nodiscard]] static double distanceMeters(const GeoPoint2D& p1, const GeoPoint2D& p2) noexcept;

    /// @brief Computes the initial compass bearing from point 1 towards point 2.
    /// @param[in] from Origin coordinate.
    /// @param[in] to Destination coordinate.
    /// @return Initial bearing in degrees [0, 360).
    [[nodiscard]] static double bearingDeg(const GeoPoint2D& from, const GeoPoint2D& to) noexcept;

    /// @brief Computes geometric horizon distance in meters from given platform altitude.
    /// @param[in] altitudeM Platform altitude above MSL or HAE in meters.
    /// @param[in] groundElevationM Ground elevation MSL in meters.
    /// @return Line-of-sight distance to horizon in meters.
    [[nodiscard]] static double horizonDistance(double altitudeM,
                                                double groundElevationM = 0.0) noexcept;

    /// @brief Computes the unit line-of-sight vector in local NED coordinates.
    /// @param[in] attitude Platform 3D attitude (heading, pitch, roll).
    /// @param[in] sensor Sensor 3D orientation (azimuth, elevation, roll).
    /// @param[in] opticalOffsetX Horizontal tangent angle offset (tan(hfov/2) * normalized_x).
    /// @param[in] opticalOffsetY Vertical tangent angle offset (tan(vfov/2) * normalized_y).
    /// @return Unit 3-vector in local North-East-Down (NED) frame.
    [[nodiscard]] static Vector3D computeRayNed(const PlatformAttitude& attitude,
                                                const CameraOrientation& sensor,
                                                double opticalOffsetX = 0.0,
                                                double opticalOffsetY = 0.0) noexcept;

    /// @brief Computes ray intersection with the spherical Earth taking into account horizon clipping.
    /// @param[in] platformPos Platform 3D location (latitude, longitude, altitude MSL in meters).
    /// @param[in] rayNed Unit line-of-sight vector in local NED coordinate frame.
    /// @param[in] groundElevationM Ground elevation MSL in meters.
    /// @param[in] clipToHorizon If true and ray points above horizon, clips ray to horizon limit.
    /// @return Projected ground coordinate, or std::nullopt if looking above horizon and clipToHorizon is false.
    [[nodiscard]] static std::optional<GeoPoint2D> intersectRayEarth(const GeoPoint3D& platformPos,
                                                                     const Vector3D& rayNed,
                                                                     double groundElevationM = 0.0,
                                                                     bool clipToHorizon = false) noexcept;

    /// @brief Computes ground intersection (Frame Center) using full 3D platform attitude and sensor orientation.
    /// @param[in] platformPos Platform 3D location (latitude, longitude, altitude MSL in meters).
    /// @param[in] attitude Platform 3D attitude (heading, pitch, roll).
    /// @param[in] sensor Sensor 3D orientation (azimuth, elevation, roll).
    /// @param[in] groundElevationM Ground elevation MSL in meters.
    /// @return Projected ground center point (lat, lon), or std::nullopt if pointing above the horizon.
    [[nodiscard]] static std::optional<GeoPoint2D> computeFrameCenter(const GeoPoint3D& platformPos,
                                                                      const PlatformAttitude& attitude,
                                                                      const CameraOrientation& sensor,
                                                                      double groundElevationM = 0.0) noexcept;

    /// @brief Computes the ground intersection (Frame Center) of the camera's optical line-of-sight (legacy overload).
    /// @param[in] platformPos Platform 3D location (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform heading [0, 360) in degrees.
    /// @param[in] sensorAzimuthDeg Sensor relative azimuth [0, 360) in degrees.
    /// @param[in] sensorElevationDeg Sensor elevation [-90, +90] in degrees (negative = looking downwards).
    /// @param[in] groundElevationM Ground elevation MSL in meters (default 0.0 m).
    /// @return Projected ground center point (lat, lon), or std::nullopt if looking above the horizon.
    [[nodiscard]] static std::optional<GeoPoint2D> computeFrameCenter(const GeoPoint3D& platformPos,
                                                                      double platformHeadingDeg,
                                                                      double sensorAzimuthDeg,
                                                                      double sensorElevationDeg,
                                                                      double groundElevationM = 0.0) noexcept;

    /// @brief Computes the slant range from the camera platform to the ground target.
    /// @param[in] platformAltitudeM Platform altitude above MSL in meters.
    /// @param[in] sensorElevationDeg Sensor elevation [-90, +90] in degrees (negative = downwards).
    /// @param[in] groundElevationM Ground elevation MSL in meters (default 0.0 m).
    /// @return Slant range in meters, or std::nullopt if looking at or above horizontal.
    [[nodiscard]] static std::optional<double> computeSlantRange(double platformAltitudeM,
                                                                 double sensorElevationDeg,
                                                                 double groundElevationM = 0.0) noexcept;

    /// @brief Computes 4-corner ground footprint frustum using full 3D attitude, sensor orientation, and horizon clipping.
    /// @param[in] platformPos Platform 3D location (latitude, longitude, altitude MSL in meters).
    /// @param[in] attitude Platform 3D attitude (heading, pitch, roll).
    /// @param[in] sensor Sensor 3D orientation (azimuth, elevation, roll).
    /// @param[in] hfovDeg Sensor horizontal field of view in degrees.
    /// @param[in] vfovDeg Sensor vertical field of view in degrees.
    /// @param[in] groundElevationM Ground elevation MSL in meters.
    /// @return FrustumCorners containing Top-Left, Top-Right, Bottom-Right, and Bottom-Left coordinates.
    [[nodiscard]] static std::optional<FrustumCorners> computeFrustum(const GeoPoint3D& platformPos,
                                                                      const PlatformAttitude& attitude,
                                                                      const CameraOrientation& sensor,
                                                                      double hfovDeg,
                                                                      double vfovDeg,
                                                                      double groundElevationM = 0.0) noexcept;

    /// @brief Computes the 4-corner ground footprint frustum (legacy overload).
    /// @param[in] platformPos Platform 3D location (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform heading [0, 360) in degrees.
    /// @param[in] sensorAzimuthDeg Sensor relative azimuth [0, 360) in degrees.
    /// @param[in] sensorElevationDeg Sensor elevation in degrees (negative = downwards).
    /// @param[in] hfovDeg Sensor horizontal field of view in degrees.
    /// @param[in] vfovDeg Sensor vertical field of view in degrees.
    /// @param[in] groundElevationM Ground elevation MSL in meters.
    /// @return FrustumCorners containing Top-Left, Top-Right, Bottom-Right, and Bottom-Left coordinates.
    [[nodiscard]] static std::optional<FrustumCorners> computeFrustum(const GeoPoint3D& platformPos,
                                                                      double platformHeadingDeg,
                                                                      double sensorAzimuthDeg,
                                                                      double sensorElevationDeg,
                                                                      double hfovDeg,
                                                                      double vfovDeg,
                                                                      double groundElevationM = 0.0) noexcept;
};

} // namespace Klv
