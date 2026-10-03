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

} // namespace Klv
