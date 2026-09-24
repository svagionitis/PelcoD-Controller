#include "MercatorProjection.h"

#include <algorithm>
#include <cmath>

namespace Mapping {

namespace {
inline constexpr double kPi { 3.14159265358979323846 };
inline constexpr double kDegToRad { kPi / 180.0 };
inline constexpr double kRadToDeg { 180.0 / kPi };
} // namespace

double MercatorProjection::normalizeLongitude(double lonDeg) noexcept {
    double lon = std::fmod(lonDeg + 180.0, 360.0);
    if (lon < 0.0) {
        lon += 360.0;
    }
    return lon - 180.0;
}

ScreenPoint MercatorProjection::latLonToGlobalPixel(const Klv::GeoPoint2D& point,
                                                   double zoom,
                                                   int tileSize) noexcept {
    const double lat = clampLatitude(point.latitudeDeg);
    const double lon = normalizeLongitude(point.longitudeDeg);
    const double mapDimension = static_cast<double>(tileSize) * std::pow(2.0, zoom);

    const double x = (lon + 180.0) / 360.0 * mapDimension;

    const double sinLat = std::sin(lat * kDegToRad);
    // Standard Mercator y-pixel formula: [0.5 - ln((1 + sin) / (1 - sin)) / (4 * pi)] * mapDimension
    const double y = (0.5 - std::log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * kPi)) * mapDimension;

    return ScreenPoint { x, y };
}

Klv::GeoPoint2D MercatorProjection::globalPixelToLatLon(const ScreenPoint& pixel,
                                                       double zoom,
                                                       int tileSize) noexcept {
    const double mapDimension = static_cast<double>(tileSize) * std::pow(2.0, zoom);
    if (mapDimension <= 0.0) {
        return Klv::GeoPoint2D { 0.0, 0.0 };
    }

    const double lon = (pixel.x / mapDimension) * 360.0 - 180.0;
    const double n = kPi - (2.0 * kPi * pixel.y) / mapDimension;
    const double lat = kRadToDeg * std::atan(std::sinh(n));

    return Klv::GeoPoint2D { clampLatitude(lat), normalizeLongitude(lon) };
}

TileCoord MercatorProjection::latLonToTile(const Klv::GeoPoint2D& point, int zoom) noexcept {
    if (zoom < 0) zoom = 0;
    const double n = std::pow(2.0, zoom);

    const double lat = clampLatitude(point.latitudeDeg);
    const double lon = normalizeLongitude(point.longitudeDeg);

    int x = static_cast<int>(std::floor((lon + 180.0) / 360.0 * n));

    const double latRad = lat * kDegToRad;
    int y = static_cast<int>(std::floor(
        (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / kPi) / 2.0 * n));

    // Clamp coordinates to valid tile grid [0, 2^zoom - 1]
    const int maxIndex = static_cast<int>(n) - 1;
    x = std::clamp(x, 0, maxIndex);
    y = std::clamp(y, 0, maxIndex);

    return TileCoord { x, y, zoom };
}

BoundingBox MercatorProjection::tileToBoundingBox(const TileCoord& coord) noexcept {
    const int zoom = std::max(0, coord.zoom);
    const double n = std::pow(2.0, zoom);

    const double west = static_cast<double>(coord.x) / n * 360.0 - 180.0;
    const double east = static_cast<double>(coord.x + 1) / n * 360.0 - 180.0;

    const double northRad = std::atan(std::sinh(kPi * (1.0 - 2.0 * static_cast<double>(coord.y) / n)));
    const double southRad = std::atan(std::sinh(kPi * (1.0 - 2.0 * static_cast<double>(coord.y + 1) / n)));

    return BoundingBox {
        clampLatitude(northRad * kRadToDeg),
        clampLatitude(southRad * kRadToDeg),
        normalizeLongitude(east),
        normalizeLongitude(west)
    };
}

double MercatorProjection::metersPerPixel(double latitudeDeg,
                                         double zoom,
                                         int tileSize) noexcept {
    const double latRad = clampLatitude(latitudeDeg) * kDegToRad;
    const double mapDimension = static_cast<double>(tileSize) * std::pow(2.0, zoom);
    if (mapDimension <= 0.0) return 0.0;

    return (kEarthCircumferenceMeters * std::cos(latRad)) / mapDimension;
}

} // namespace Mapping
