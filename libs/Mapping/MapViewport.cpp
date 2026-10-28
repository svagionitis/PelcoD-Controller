/// @file MapViewport.cpp
/// @brief Implementation of 2D camera viewport state, coordinate transforms, and visible tile calculations.

#include "MapViewport.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <vector>

namespace Mapping {

MapViewport::MapViewport(const Klv::GeoPoint2D& center, double zoom, double width, double height) noexcept
{
    setCenter(center);
    setZoom(zoom);
    setSize(width, height);
}

void MapViewport::setCenter(const Klv::GeoPoint2D& center) noexcept
{
    m_center.latitudeDeg = MercatorProjection::clampLatitude(center.latitudeDeg);
    m_center.longitudeDeg = MercatorProjection::normalizeLongitude(center.longitudeDeg);
}

Klv::GeoPoint2D MapViewport::center() const noexcept
{
    return m_center;
}

void MapViewport::setZoom(double zoom) noexcept
{
    if (!std::isfinite(zoom)) {
        return;
    }
    m_zoom = std::clamp(
        zoom, static_cast<double>(MercatorProjection::kMinZoom), static_cast<double>(MercatorProjection::kMaxZoom));
}

double MapViewport::zoom() const noexcept
{
    return m_zoom;
}

void MapViewport::setSize(double width, double height) noexcept
{
    if (std::isfinite(width)) {
        m_width = std::max(1.0, width);
    }
    if (std::isfinite(height)) {
        m_height = std::max(1.0, height);
    }
}

double MapViewport::width() const noexcept
{
    return m_width;
}

double MapViewport::height() const noexcept
{
    return m_height;
}

void MapViewport::setTileSize(int tileSize) noexcept
{
    if (tileSize > 0) {
        m_tileSize = tileSize;
    }
}

int MapViewport::tileSize() const noexcept
{
    return m_tileSize;
}

ScreenPoint MapViewport::geoToScreen(const Klv::GeoPoint2D& geo) const noexcept
{
    const ScreenPoint centerGlobal { MercatorProjection::latLonToGlobalPixel(m_center, m_zoom, m_tileSize) };
    const ScreenPoint targetGlobal { MercatorProjection::latLonToGlobalPixel(geo, m_zoom, m_tileSize) };

    const double mapDimension { static_cast<double>(m_tileSize) * std::pow(2.0, m_zoom) };

    // Handle anti-meridian wrapping relative to center
    double dx { targetGlobal.x - centerGlobal.x };
    if (mapDimension > 0.0) {
        if (dx > mapDimension * 0.5) {
            dx -= mapDimension;
        } else if (dx < -mapDimension * 0.5) {
            dx += mapDimension;
        }
    }

    const double screenX { (m_width * 0.5) + dx };
    const double screenY { (m_height * 0.5) + (targetGlobal.y - centerGlobal.y) };

    return ScreenPoint { screenX, screenY };
}

Klv::GeoPoint2D MapViewport::screenToGeo(const ScreenPoint& screen) const noexcept
{
    const ScreenPoint centerGlobal { MercatorProjection::latLonToGlobalPixel(m_center, m_zoom, m_tileSize) };

    const double globalX { centerGlobal.x + (screen.x - (m_width * 0.5)) };
    const double globalY { centerGlobal.y + (screen.y - (m_height * 0.5)) };

    return MercatorProjection::globalPixelToLatLon(ScreenPoint { globalX, globalY }, m_zoom, m_tileSize);
}

void MapViewport::pan(double deltaPixelsX, double deltaPixelsY) noexcept
{
    if (!std::isfinite(deltaPixelsX) || !std::isfinite(deltaPixelsY)) {
        return;
    }
    // Shifting view by deltaPixels moves center in opposite direction in screen space
    const ScreenPoint targetScreen { (m_width * 0.5) - deltaPixelsX, (m_height * 0.5) - deltaPixelsY };
    setCenter(screenToGeo(targetScreen));
}

void MapViewport::zoomBy(double deltaZoom, const ScreenPoint& pivot) noexcept
{
    if (!std::isfinite(deltaZoom) || !std::isfinite(pivot.x) || !std::isfinite(pivot.y)) {
        return;
    }
    // Record geographical coordinate anchored under the pivot
    const Klv::GeoPoint2D pivotGeo { screenToGeo(pivot) };

    // Apply new zoom
    setZoom(m_zoom + deltaZoom);

    // Calculate where pivotGeo landed after zoom change
    const ScreenPoint newScreen { geoToScreen(pivotGeo) };

    // Pan to restore pivotGeo back to the original pivot location
    pan(pivot.x - newScreen.x, pivot.y - newScreen.y);
}

BoundingBox MapViewport::visibleBoundingBox() const noexcept
{
    const Klv::GeoPoint2D topLeft { screenToGeo(ScreenPoint { 0.0, 0.0 }) };
    const Klv::GeoPoint2D bottomRight { screenToGeo(ScreenPoint { m_width, m_height }) };

    const double mapDimension { static_cast<double>(m_tileSize) * std::pow(2.0, m_zoom) };
    const bool spansFullWorld { mapDimension > 0.0 && m_width >= mapDimension };

    return BoundingBox {
        topLeft.latitudeDeg, // North
        bottomRight.latitudeDeg, // South
        spansFullWorld ? 180.0 : bottomRight.longitudeDeg, // East
        spansFullWorld ? -180.0 : topLeft.longitudeDeg // West
    };
}

std::vector<VisibleTile> MapViewport::calculateVisibleTiles(int paddingTiles) const
{
    std::vector<VisibleTile> tiles { };

    const int safePadding { std::clamp(paddingTiles, 0, 8) };
    const int baseZoom { MercatorProjection::clampZoom(static_cast<int>(std::floor(m_zoom))) };
    const double scale { std::pow(2.0, m_zoom - static_cast<double>(baseZoom)) };
    const double scaledTileSize { static_cast<double>(m_tileSize) * scale };
    if (scaledTileSize <= 0.0) {
        return tiles;
    }
    const int totalTiles { 1 << baseZoom };

    const ScreenPoint centerGlobal { MercatorProjection::latLonToGlobalPixel(m_center, m_zoom, m_tileSize) };
    const double tlGlobalX { centerGlobal.x - (m_width * 0.5) };
    const double tlGlobalY { centerGlobal.y - (m_height * 0.5) };

    const int minTileX { static_cast<int>(std::floor(tlGlobalX / scaledTileSize)) - safePadding };
    const int maxTileX { static_cast<int>(std::floor((tlGlobalX + m_width) / scaledTileSize)) + safePadding };

    int minTileY { static_cast<int>(std::floor(tlGlobalY / scaledTileSize)) - safePadding };
    int maxTileY { static_cast<int>(std::floor((tlGlobalY + m_height) / scaledTileSize)) + safePadding };

    // Clamp Y to valid world tile bounds [0, 2^zoom - 1]
    minTileY = std::clamp(minTileY, 0, totalTiles - 1);
    maxTileY = std::clamp(maxTileY, 0, totalTiles - 1);

    if (maxTileX >= minTileX && maxTileY >= minTileY) {
        const std::size_t countX { static_cast<std::size_t>(maxTileX - minTileX + 1) };
        const std::size_t countY { static_cast<std::size_t>(maxTileY - minTileY + 1) };
        tiles.reserve(countX * countY);
    }

    for (int y { minTileY }; y <= maxTileY; ++y) {
        for (int x { minTileX }; x <= maxTileX; ++x) {
            // Handle horizontal wrap-around for X
            const int wrappedX { ((x % totalTiles) + totalTiles) % totalTiles };

            const double tileScreenX { static_cast<double>(x) * scaledTileSize - tlGlobalX };
            const double tileScreenY { static_cast<double>(y) * scaledTileSize - tlGlobalY };

            tiles.push_back(VisibleTile { TileCoord { wrappedX, y, baseZoom },
                ScreenRect { tileScreenX, tileScreenY, scaledTileSize, scaledTileSize } });
        }
    }

    return tiles;
}

} // namespace Mapping
