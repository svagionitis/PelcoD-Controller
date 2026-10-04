#pragma once

/// @file GeoTypes.h
/// @brief Fundamental 2D and 3D geodetic coordinate types for KLV and VMTI.

namespace Klv {

/// @struct GeoPoint2D
/// @brief WGS-84 2D geodetic position (latitude and longitude in degrees).
struct GeoPoint2D {
    double latitudeDeg { 0.0 };  ///< Latitude [-90.0, +90.0] degrees
    double longitudeDeg { 0.0 }; ///< Longitude [-180.0, +180.0] degrees
};

/// @struct GeoPoint3D
/// @brief WGS-84 3D geodetic position (latitude, longitude, altitude).
struct GeoPoint3D {
    double latitudeDeg { 0.0 };  ///< Latitude [-90.0, +90.0] degrees
    double longitudeDeg { 0.0 }; ///< Longitude [-180.0, +180.0] degrees
    double altitudeM { 0.0 };    ///< Height above Mean Sea Level (MSL) or HAE in meters
};

/// @struct FrustumCorners
/// @brief Optical footprint 4-corner ground projection coordinates on the WGS-84 ellipsoid.
struct FrustumCorners {
    GeoPoint2D topLeft {};     ///< Corner 1 (Top-Left)
    GeoPoint2D topRight {};    ///< Corner 2 (Top-Right)
    GeoPoint2D bottomRight {}; ///< Corner 3 (Bottom-Right)
    GeoPoint2D bottomLeft {};  ///< Corner 4 (Bottom-Left)
};

} // namespace Klv
