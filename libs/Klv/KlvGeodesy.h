#pragma once

/// @file KlvGeodesy.h
/// @brief Analytical geodetic projection engine for camera telemetry, slant range, and footprint frustums.

#include "KlvTypes.h"
#include <optional>

namespace Klv {

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

    /// @brief Computes the ground intersection (Frame Center) of the camera's optical line-of-sight.
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

    /// @brief Computes the 4-corner ground footprint frustum for ATAK / WinTAK moving map display.
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
