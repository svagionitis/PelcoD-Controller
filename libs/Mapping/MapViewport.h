#pragma once

/// @file MapViewport.h
/// @brief Dynamic 2D camera viewport state, coordinate transforms, and visible tile calculations.

#include "GeoTypes.h"
#include "MercatorProjection.h"

#include <optional>
#include <vector>

namespace Mapping {

/// @struct VisibleTile
/// @brief Describes a single tile to render and its destination bounding rectangle on the viewport.
struct VisibleTile {
    TileCoord coord;       ///< Discrete tile coordinate (X, Y, integer Zoom)
    ScreenRect screenRect; ///< Pixel position and scaled dimension on viewport
};

/// @class MapViewport
/// @brief Manages the 2D pan/zoom viewport for map visualization, coordinate projections,
///        and calculation of visible tile coverage grids.
class MapViewport {
public:
    MapViewport() = default;

    /// @brief Constructs a MapViewport with specific geographic center, zoom, and pixel dimensions.
    /// @param[in] center Geographic location (lat, lon) at the center of the viewport.
    /// @param[in] zoom Fractional zoom level [0.0, 22.0].
    /// @param[in] width Viewport width in pixels.
    /// @param[in] height Viewport height in pixels.
    MapViewport(const Klv::GeoPoint2D& center, double zoom, double width, double height) noexcept;

    /// @brief Sets the geographic center point of the viewport.
    void setCenter(const Klv::GeoPoint2D& center) noexcept;

    /// @brief Returns the current geographic center point of the viewport.
    [[nodiscard]] Klv::GeoPoint2D center() const noexcept;

    /// @brief Sets the fractional zoom level, clamped between 0.0 and 22.0.
    void setZoom(double zoom) noexcept;

    /// @brief Returns the current fractional zoom level.
    [[nodiscard]] double zoom() const noexcept;

    /// @brief Sets the viewport dimensions in pixels.
    void setSize(double width, double height) noexcept;

    /// @brief Returns the viewport width in pixels.
    [[nodiscard]] double width() const noexcept;

    /// @brief Returns the viewport height in pixels.
    [[nodiscard]] double height() const noexcept;

    /// @brief Sets the base tile dimension (default 256).
    void setTileSize(int tileSize) noexcept;

    /// @brief Returns the base tile dimension.
    [[nodiscard]] int tileSize() const noexcept;

    /// @brief Pans the viewport by the specified delta in screen pixels.
    /// @param[in] deltaPixelsX Horizontal shift (positive moves view right, ground moves left).
    /// @param[in] deltaPixelsY Vertical shift (positive moves view down, ground moves up).
    void pan(double deltaPixelsX, double deltaPixelsY) noexcept;

    /// @brief Zooms the viewport by deltaZoom while keeping the geographic coordinate under pivot fixed.
    /// @param[in] deltaZoom Change in zoom level.
    /// @param[in] pivot Screen position (x, y) around which to zoom.
    void zoomBy(double deltaZoom, const ScreenPoint& pivot) noexcept;

    /// @brief Transforms a geographic point (lat, lon) to viewport screen coordinates (x, y).
    /// @param[in] geo Geographic point.
    /// @return 2D screen coordinate in pixels relative to viewport top-left.
    [[nodiscard]] ScreenPoint geoToScreen(const Klv::GeoPoint2D& geo) const noexcept;

    /// @brief Transforms a viewport screen coordinate (x, y) to geographic (lat, lon).
    /// @param[in] screen Screen coordinate in pixels relative to viewport top-left.
    /// @return Geographic coordinate (lat, lon).
    [[nodiscard]] Klv::GeoPoint2D screenToGeo(const ScreenPoint& screen) const noexcept;

    /// @brief Computes the visible geographic bounding box currently covered by the viewport.
    [[nodiscard]] BoundingBox visibleBoundingBox() const noexcept;

    /// @brief Calculates the exact set of visible tiles and their destination screen rects.
    /// @param[in] paddingTiles Number of extra boundary tiles to include around the viewport edge.
    /// @return Vector of VisibleTile items ready for rendering.
    [[nodiscard]] std::vector<VisibleTile> calculateVisibleTiles(int paddingTiles = 0) const;

private:
    Klv::GeoPoint2D m_center { 0.0, 0.0 };
    double m_zoom { 2.0 };
    double m_width { 800.0 };
    double m_height { 600.0 };
    int m_tileSize { MercatorProjection::kDefaultTileSize };
};

} // namespace Mapping
