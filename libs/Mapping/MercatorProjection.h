#pragma once

/// @file MercatorProjection.h
/// @brief Spherical Web Mercator (EPSG:3857) projection and Slippy Map tiling mathematics.

#include "GeoTypes.h"
#include <cmath>

namespace Mapping {

/// @class MercatorProjection
/// @brief High-precision conversion engine between WGS-84 geodetic coordinates,
///        Web Mercator global pixels, and discrete Slippy Map / TMS tile coordinates.
class MercatorProjection {
public:
    /// @brief Maximum representable latitude in Web Mercator (+85.0511287798 degrees).
    static constexpr double kMaxLatitude { 85.05112877980659 };

    /// @brief Minimum representable latitude in Web Mercator (-85.0511287798 degrees).
    static constexpr double kMinLatitude { -85.05112877980659 };

    /// @brief Standard raster tile dimension in pixels (256x256).
    static constexpr int kDefaultTileSize { 256 };

    /// @brief WGS-84 equatorial circumference in meters (2 * PI * 6,378,137.0 m).
    static constexpr double kEarthCircumferenceMeters { 40075016.68557849 };

    /// @brief Clamps latitude into the valid Web Mercator range [-85.051129, +85.051129].
    /// @param[in] latDeg Input latitude in degrees.
    /// @return Clamped latitude in degrees.
    [[nodiscard]] static constexpr double clampLatitude(double latDeg) noexcept {
        if (latDeg > kMaxLatitude) return kMaxLatitude;
        if (latDeg < kMinLatitude) return kMinLatitude;
        return latDeg;
    }

    /// @brief Normalizes longitude into the [-180.0, +180.0) range.
    /// @param[in] lonDeg Input longitude in degrees.
    /// @return Normalized longitude in degrees.
    [[nodiscard]] static double normalizeLongitude(double lonDeg) noexcept;

    /// @brief Projects geographic coordinates to continuous global pixel coordinates at a given zoom.
    /// @param[in] point Geographic position (lat, lon) in degrees.
    /// @param[in] zoom Fractional or integer zoom level (typically 0 to 22).
    /// @param[in] tileSize Raster tile size in pixels (default 256).
    /// @return Absolute global pixel coordinates from the world top-left origin (0, 0).
    [[nodiscard]] static ScreenPoint latLonToGlobalPixel(const Klv::GeoPoint2D& point,
                                                         double zoom,
                                                         int tileSize = kDefaultTileSize) noexcept;

    /// @brief Inverse projects global pixel coordinates back to geographic (lat, lon) coordinates.
    /// @param[in] pixel Global pixel coordinate at the specified zoom level.
    /// @param[in] zoom Zoom level associated with the pixel coordinate.
    /// @param[in] tileSize Raster tile size in pixels (default 256).
    /// @return Geographic coordinate (lat, lon) in degrees.
    [[nodiscard]] static Klv::GeoPoint2D globalPixelToLatLon(const ScreenPoint& pixel,
                                                             double zoom,
                                                             int tileSize = kDefaultTileSize) noexcept;

    /// @brief Computes the discrete Slippy Map tile coordinate (X, Y) containing a geographic point.
    /// @param[in] point Geographic position (lat, lon) in degrees.
    /// @param[in] zoom Integer zoom level.
    /// @return Discrete TileCoord (x, y, zoom).
    [[nodiscard]] static TileCoord latLonToTile(const Klv::GeoPoint2D& point, int zoom) noexcept;

    /// @brief Computes the geographic bounding box of a discrete tile.
    /// @param[in] coord Discrete TileCoord (x, y, zoom).
    /// @return BoundingBox (north, south, east, west) in degrees.
    [[nodiscard]] static BoundingBox tileToBoundingBox(const TileCoord& coord) noexcept;

    /// @brief Converts standard Slippy Map (OSM) Y-coordinate to TMS (MBTiles) Y-coordinate.
    /// @param[in] slippyY Standard Slippy Map row index (0 is top/north).
    /// @param[in] zoom Zoom level.
    /// @return TMS row index (0 is bottom/south).
    [[nodiscard]] static constexpr int slippyToTmsY(int slippyY, int zoom) noexcept {
        return (1 << zoom) - 1 - slippyY;
    }

    /// @brief Converts TMS (MBTiles) Y-coordinate to standard Slippy Map (OSM) Y-coordinate.
    /// @param[in] tmsY TMS row index (0 is bottom/south).
    /// @param[in] zoom Zoom level.
    /// @return Slippy Map row index (0 is top/north).
    [[nodiscard]] static constexpr int tmsToSlippyY(int tmsY, int zoom) noexcept {
        return (1 << zoom) - 1 - tmsY;
    }

    /// @brief Computes the ground resolution in meters per pixel at a specific latitude and zoom.
    /// @param[in] latitudeDeg Latitude in degrees.
    /// @param[in] zoom Zoom level.
    /// @param[in] tileSize Raster tile size in pixels (default 256).
    /// @return Ground resolution in meters / pixel.
    [[nodiscard]] static double metersPerPixel(double latitudeDeg,
                                               double zoom,
                                               int tileSize = kDefaultTileSize) noexcept;
};

} // namespace Mapping
