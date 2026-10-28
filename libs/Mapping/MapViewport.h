#pragma once

/// @file MapViewport.h
/// @brief Dynamic 2D camera viewport state, coordinate transforms, and visible tile calculations.

#include <vector>

#include "GeoTypes.h"
#include "MercatorProjection.h"

namespace Mapping {

/// @struct VisibleTile
/// @brief Describes a single tile to render and its destination bounding rectangle on the viewport.
/// @details Pairs a discrete integer tile coordinate with its scaled floating-point screen destination rectangle.
struct VisibleTile {
    TileCoord coord { }; ///< Discrete tile coordinate (X, Y, integer Zoom)
    ScreenRect screenRect { }; ///< Pixel position and scaled dimension on viewport
};

/// @class MapViewport
/// @brief Manages the 2D pan/zoom viewport for map visualization, coordinate projections,
///        and calculation of visible tile coverage grids.
/// @details Maintains sanitized center coordinates, zoom level in [0.0, 22.0], and positive viewport dimensions.
class MapViewport {
public:
    /// @brief Constructs a default MapViewport centered at (0, 0), zoom 2.0, and 800x600 dimensions.
    /// @details Initializes all viewport parameters to safe default values.
    MapViewport() = default;

    /// @brief Constructs a MapViewport with specific geographic center, zoom, and pixel dimensions.
    /// @details Sanitizes and clamps all input parameters via setCenter, setZoom, and setSize.
    /// @param[in] center Geographic location (lat, lon) at the center of the viewport.
    /// @param[in] zoom Fractional zoom level [0.0, 22.0].
    /// @param[in] width Viewport width in pixels.
    /// @param[in] height Viewport height in pixels.
    MapViewport(const Klv::GeoPoint2D& center, double zoom, double width, double height) noexcept;

    /// @brief Sets the geographic center point of the viewport.
    /// @details Clamps latitude to Web Mercator bounds and normalizes longitude to [-180.0, +180.0).
    /// @param[in] center Geographic location (lat, lon).
    void setCenter(const Klv::GeoPoint2D& center) noexcept;

    /// @brief Returns the current geographic center point of the viewport.
    /// @details Guaranteed to hold finite, normalized WGS-84 coordinates.
    /// @return Geographic center coordinate.
    [[nodiscard]] Klv::GeoPoint2D center() const noexcept;

    /// @brief Sets the fractional zoom level, clamped between 0.0 and 22.0.
    /// @details Ignores non-finite (NaN or Inf) zoom inputs.
    /// @param[in] zoom Fractional zoom level.
    void setZoom(double zoom) noexcept;

    /// @brief Returns the current fractional zoom level.
    /// @details Always lies within the closed range [0.0, 22.0].
    /// @return Fractional zoom level.
    [[nodiscard]] double zoom() const noexcept;

    /// @brief Sets the viewport dimensions in pixels.
    /// @details Clamps finite width and height values to at least 1.0 pixel and ignores NaN/Inf.
    /// @param[in] width Viewport width in pixels.
    /// @param[in] height Viewport height in pixels.
    void setSize(double width, double height) noexcept;

    /// @brief Returns the viewport width in pixels.
    /// @details Guaranteed to be >= 1.0.
    /// @return Viewport width in pixels.
    [[nodiscard]] double width() const noexcept;

    /// @brief Returns the viewport height in pixels.
    /// @details Guaranteed to be >= 1.0.
    /// @return Viewport height in pixels.
    [[nodiscard]] double height() const noexcept;

    /// @brief Sets the base tile dimension (default 256).
    /// @details Ignores non-positive tile sizes.
    /// @param[in] tileSize Tile size in pixels (> 0).
    void setTileSize(int tileSize) noexcept;

    /// @brief Returns the base tile dimension.
    /// @details Defaults to MercatorProjection::kDefaultTileSize (256).
    /// @return Tile dimension in pixels.
    [[nodiscard]] int tileSize() const noexcept;

    /// @brief Pans the viewport by the specified delta in screen pixels.
    /// @details Shifts the geographic center inversely to the screen drag vector; ignores non-finite deltas.
    /// @param[in] deltaPixelsX Horizontal shift (positive moves view right, ground moves left).
    /// @param[in] deltaPixelsY Vertical shift (positive moves view down, ground moves up).
    void pan(double deltaPixelsX, double deltaPixelsY) noexcept;

    /// @brief Zooms the viewport by deltaZoom while keeping the geographic coordinate under pivot fixed.
    /// @details Projects the pivot screen position to WGS-84 prior to zooming and pans to keep it anchored.
    /// @param[in] deltaZoom Change in zoom level.
    /// @param[in] pivot Screen position (x, y) around which to zoom.
    void zoomBy(double deltaZoom, const ScreenPoint& pivot) noexcept;

    /// @brief Transforms a geographic point (lat, lon) to viewport screen coordinates (x, y).
    /// @details Unrolls horizontal global pixel deltas across the +-180 deg anti-meridian relative to center.
    /// @param[in] geo Geographic point.
    /// @return 2D screen coordinate in pixels relative to viewport top-left.
    [[nodiscard]] ScreenPoint geoToScreen(const Klv::GeoPoint2D& geo) const noexcept;

    /// @brief Transforms a viewport screen coordinate (x, y) to geographic (lat, lon).
    /// @details Offsets the global pixel center by the relative screen position and inverse-projects to WGS-84.
    /// @param[in] screen Screen coordinate in pixels relative to viewport top-left.
    /// @return Geographic coordinate (lat, lon).
    [[nodiscard]] Klv::GeoPoint2D screenToGeo(const ScreenPoint& screen) const noexcept;

    /// @brief Computes the visible geographic bounding box currently covered by the viewport.
    /// @details Clamps longitude span to [-180.0, 180.0] when the viewport width spans the entire world.
    /// @return Geographic BoundingBox of the viewport.
    [[nodiscard]] BoundingBox visibleBoundingBox() const noexcept;

    /// @brief Calculates the exact set of visible tiles and their destination screen rects.
    /// @details Supports fractional zoom scaling and wraps horizontal tile indices across the anti-meridian.
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
