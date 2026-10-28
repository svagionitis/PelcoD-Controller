#pragma once

/// @file MercatorProjection.h
/// @brief Spherical Web Mercator (EPSG:3857) projection and Slippy Map tiling mathematics.

#include "GeoTypes.h"

namespace Mapping {

/// @class MercatorProjection
/// @brief High-precision conversion engine between WGS-84 geodetic coordinates,
///        Web Mercator global pixels, and discrete Slippy Map / TMS tile coordinates.
/// @details All static projection helpers are thread-safe, noexcept, and sanitize non-finite
///          or out-of-range inputs per SEI CERT FLP34-C and INT34-C.
class MercatorProjection {
public:
    /// @brief Maximum representable latitude in Web Mercator (+85.0511287798 degrees).
    static constexpr double kMaxLatitude { 85.05112877980659 };

    /// @brief Minimum representable latitude in Web Mercator (-85.0511287798 degrees).
    static constexpr double kMinLatitude { -85.05112877980659 };

    /// @brief Minimum supported discrete zoom level.
    static constexpr int kMinZoom { 0 };

    /// @brief Maximum supported discrete zoom level.
    static constexpr int kMaxZoom { 22 };

    /// @brief Standard raster tile dimension in pixels (256x256).
    static constexpr int kDefaultTileSize { 256 };

    /// @brief WGS-84 equatorial circumference in meters (2 * PI * 6,378,137.0 m).
    static constexpr double kEarthCircumferenceMeters { 40075016.68557849 };

    /// @brief Clamps an integer zoom level into the safe [kMinZoom, kMaxZoom] range.
    /// @details Prevents undefined behavior on signed bit shifts (1 << zoom) per SEI CERT INT34-C.
    /// @param[in] zoom Input discrete zoom level.
    /// @return Clamped zoom level in [0, 22].
    [[nodiscard]] static constexpr int clampZoom(int zoom) noexcept
    {
        if (zoom < kMinZoom) {
            return kMinZoom;
        }
        if (zoom > kMaxZoom) {
            return kMaxZoom;
        }
        return zoom;
    }

    /// @brief Clamps latitude into the valid Web Mercator range [-85.051129, +85.051129].
    /// @details Guards against NaN inputs (returning 0.0) before bounding to Web Mercator limits.
    /// @param[in] latDeg Input latitude in degrees.
    /// @return Clamped latitude in degrees (or 0.0 if input is NaN).
    [[nodiscard]] static constexpr double clampLatitude(double latDeg) noexcept
    {
        if (latDeg != latDeg) {
            return 0.0; // NaN guard (SEI CERT FLP34-C)
        }
        if (latDeg > kMaxLatitude) {
            return kMaxLatitude;
        }
        if (latDeg < kMinLatitude) {
            return kMinLatitude;
        }
        return latDeg;
    }

    /// @brief Normalizes longitude into the [-180.0, +180.0) range.
    /// @details Wraps multi-revolution longitudes via modulo arithmetic and returns 0.0 for non-finite values.
    /// @param[in] lonDeg Input longitude in degrees.
    /// @return Normalized longitude in degrees (or 0.0 if input is non-finite).
    [[nodiscard]] static double normalizeLongitude(double lonDeg) noexcept;

    /// @brief Projects geographic coordinates to continuous global pixel coordinates at a given zoom.
    /// @details Applies EPSG:3857 spherical Mercator formulas scaled by tileSize * 2^zoom.
    /// @param[in] point Geographic position (lat, lon) in degrees.
    /// @param[in] zoom Fractional or integer zoom level (typically 0 to 22).
    /// @param[in] tileSize Raster tile size in pixels (default 256).
    /// @return Absolute global pixel coordinates from the world top-left origin (0, 0).
    [[nodiscard]] static ScreenPoint latLonToGlobalPixel(
        const Klv::GeoPoint2D& point, double zoom, int tileSize = kDefaultTileSize) noexcept;

    /// @brief Inverse projects global pixel coordinates back to geographic (lat, lon) coordinates.
    /// @details Inverts EPSG:3857 global pixel coordinates into clamped WGS-84 latitude and normalized longitude.
    /// @param[in] pixel Global pixel coordinate at the specified zoom level.
    /// @param[in] zoom Zoom level associated with the pixel coordinate.
    /// @param[in] tileSize Raster tile size in pixels (default 256).
    /// @return Geographic coordinate (lat, lon) in degrees.
    [[nodiscard]] static Klv::GeoPoint2D globalPixelToLatLon(
        const ScreenPoint& pixel, double zoom, int tileSize = kDefaultTileSize) noexcept;

    /// @brief Computes the discrete Slippy Map tile coordinate (X, Y) containing a geographic point.
    /// @details Clamps zoom to [0, 22] and bounds output tile indices to [0, 2^zoom - 1].
    /// @param[in] point Geographic position (lat, lon) in degrees.
    /// @param[in] zoom Integer zoom level.
    /// @return Discrete TileCoord (x, y, zoom).
    [[nodiscard]] static TileCoord latLonToTile(const Klv::GeoPoint2D& point, int zoom) noexcept;

    /// @brief Computes the geographic bounding box of a discrete tile.
    /// @details Projects the tile's north-west and south-east corners back to WGS-84 degrees.
    /// @param[in] coord Discrete TileCoord (x, y, zoom).
    /// @return BoundingBox (north, south, east, west) in degrees.
    [[nodiscard]] static BoundingBox tileToBoundingBox(const TileCoord& coord) noexcept;

    /// @brief Converts standard Slippy Map (OSM) Y-coordinate to TMS (MBTiles) Y-coordinate.
    /// @details Flips the vertical tile index via (2^zoom - 1 - slippyY) with clamped zoom.
    /// @param[in] slippyY Standard Slippy Map row index (0 is top/north).
    /// @param[in] zoom Zoom level (clamped to [0, 22]).
    /// @return TMS row index (0 is bottom/south).
    [[nodiscard]] static constexpr int slippyToTmsY(int slippyY, int zoom) noexcept
    {
        const int safeZoom { clampZoom(zoom) };
        return (1 << safeZoom) - 1 - slippyY;
    }

    /// @brief Converts TMS (MBTiles) Y-coordinate to standard Slippy Map (OSM) Y-coordinate.
    /// @details Flips the vertical tile index via (2^zoom - 1 - tmsY) with clamped zoom.
    /// @param[in] tmsY TMS row index (0 is bottom/south).
    /// @param[in] zoom Zoom level (clamped to [0, 22]).
    /// @return Slippy Map row index (0 is top/north).
    [[nodiscard]] static constexpr int tmsToSlippyY(int tmsY, int zoom) noexcept
    {
        const int safeZoom { clampZoom(zoom) };
        return (1 << safeZoom) - 1 - tmsY;
    }

    /// @brief Computes the ground resolution in meters per pixel at a specific latitude and zoom.
    /// @details Scales the equatorial circumference by cos(latitude) divided by (tileSize * 2^zoom).
    /// @param[in] latitudeDeg Latitude in degrees.
    /// @param[in] zoom Zoom level.
    /// @param[in] tileSize Raster tile size in pixels (default 256).
    /// @return Ground resolution in meters / pixel.
    [[nodiscard]] static double metersPerPixel(
        double latitudeDeg, double zoom, int tileSize = kDefaultTileSize) noexcept;
};

} // namespace Mapping
