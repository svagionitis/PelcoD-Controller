/// @file MercatorProjection.cpp
/// @brief Implementation of Spherical Web Mercator (EPSG:3857) projection and tiling mathematics.

#include "MercatorProjection.h"

#include <cmath>

#include <algorithm>

#include "GeoTypes.h"
#include "KlvTypes.h"

namespace Mapping {

namespace {
    inline constexpr double kPi { 3.14159265358979323846 };
    inline constexpr double kDegToRad { kPi / 180.0 };
    inline constexpr double kRadToDeg { 180.0 / kPi };

    [[nodiscard]] double sanitizeZoom(double zoom) noexcept
    {
        if (!std::isfinite(zoom)) {
            return 0.0;
        }
        return std::clamp(
            zoom, static_cast<double>(MercatorProjection::kMinZoom), static_cast<double>(MercatorProjection::kMaxZoom));
    }
} // namespace

double MercatorProjection::normalizeLongitude(double lonDeg) noexcept
{
    if (!std::isfinite(lonDeg)) {
        return 0.0;
    }
    double lon { std::fmod(lonDeg + 180.0, 360.0) };
    if (lon < 0.0) {
        lon += 360.0;
    }
    return lon - 180.0;
}

ScreenPoint MercatorProjection::latLonToGlobalPixel(const Klv::GeoPoint2D& point, double zoom, int tileSize) noexcept
{
    if (tileSize <= 0) {
        return ScreenPoint { 0.0, 0.0 };
    }
    const double lat { clampLatitude(point.latitudeDeg) };
    const double lon { normalizeLongitude(point.longitudeDeg) };
    const double safeZoom { sanitizeZoom(zoom) };
    const double mapDimension { static_cast<double>(tileSize) * std::pow(2.0, safeZoom) };

    const double x { (lon + 180.0) / 360.0 * mapDimension };

    const double sinLat { std::sin(lat * kDegToRad) };
    // Standard Mercator y-pixel formula: [0.5 - ln((1 + sin) / (1 - sin)) / (4 * pi)] * mapDimension
    const double y { (0.5 - std::log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * kPi)) * mapDimension };

    return ScreenPoint { x, y };
}

Klv::GeoPoint2D MercatorProjection::globalPixelToLatLon(const ScreenPoint& pixel, double zoom, int tileSize) noexcept
{
    if (tileSize <= 0 || !std::isfinite(pixel.x) || !std::isfinite(pixel.y)) {
        return Klv::GeoPoint2D { 0.0, 0.0 };
    }
    const double safeZoom { sanitizeZoom(zoom) };
    const double mapDimension { static_cast<double>(tileSize) * std::pow(2.0, safeZoom) };
    if (mapDimension <= 0.0) {
        return Klv::GeoPoint2D { 0.0, 0.0 };
    }

    const double lon { (pixel.x / mapDimension) * 360.0 - 180.0 };
    const double n { kPi - (2.0 * kPi * pixel.y) / mapDimension };
    const double lat { kRadToDeg * std::atan(std::sinh(n)) };

    return Klv::GeoPoint2D { clampLatitude(lat), normalizeLongitude(lon) };
}

TileCoord MercatorProjection::latLonToTile(const Klv::GeoPoint2D& point, int zoom) noexcept
{
    const int safeZoom { clampZoom(zoom) };
    const int maxTiles { 1 << safeZoom };
    const double n { static_cast<double>(maxTiles) };

    const double lat { clampLatitude(point.latitudeDeg) };
    const double lon { normalizeLongitude(point.longitudeDeg) };

    const double rawX { std::floor((lon + 180.0) / 360.0 * n) };
    const double latRad { lat * kDegToRad };
    const double rawY { std::floor((1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / kPi) / 2.0 * n) };

    const int maxIndex { maxTiles - 1 };
    const double clampedX { std::clamp(rawX, 0.0, static_cast<double>(maxIndex)) };
    const double clampedY { std::clamp(rawY, 0.0, static_cast<double>(maxIndex)) };

    const int x { static_cast<int>(clampedX) };
    const int y { static_cast<int>(clampedY) };

    return TileCoord { x, y, safeZoom };
}

BoundingBox MercatorProjection::tileToBoundingBox(const TileCoord& coord) noexcept
{
    const int safeZoom { clampZoom(coord.zoom) };
    const int maxTiles { 1 << safeZoom };
    const int maxIndex { maxTiles - 1 };
    const int safeX { std::clamp(coord.x, 0, maxIndex) };
    const int safeY { std::clamp(coord.y, 0, maxIndex) };
    const double n { static_cast<double>(maxTiles) };

    const double west { static_cast<double>(safeX) / n * 360.0 - 180.0 };
    const double east { static_cast<double>(safeX + 1) / n * 360.0 - 180.0 };

    const double northRad { std::atan(std::sinh(kPi * (1.0 - 2.0 * static_cast<double>(safeY) / n))) };
    const double southRad { std::atan(std::sinh(kPi * (1.0 - 2.0 * static_cast<double>(safeY + 1) / n))) };

    return BoundingBox { clampLatitude(northRad * kRadToDeg), clampLatitude(southRad * kRadToDeg),
        normalizeLongitude(east), normalizeLongitude(west) };
}

double MercatorProjection::metersPerPixel(double latitudeDeg, double zoom, int tileSize) noexcept
{
    if (tileSize <= 0) {
        return 0.0;
    }
    const double latRad { clampLatitude(latitudeDeg) * kDegToRad };
    const double safeZoom { sanitizeZoom(zoom) };
    const double mapDimension { static_cast<double>(tileSize) * std::pow(2.0, safeZoom) };
    if (mapDimension <= 0.0) {
        return 0.0;
    }

    return (kEarthCircumferenceMeters * std::cos(latRad)) / mapDimension;
}

} // namespace Mapping
