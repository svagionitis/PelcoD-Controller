#pragma once

/// @file TacticalMapQuickItem.h
/// @brief Custom QQuickPaintedItem providing interactive tactical 2D map display in QML.

#include <cstddef>

#include <deque>
#include <memory>
#include <unordered_map>

#include <QColor>
#include <QImage>
#include <QMutex>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QRectF>
#include <QString>

#include "CompositeTileProvider.h"
#include "GeoTypes.h"
#include "KlvTypes.h"
#include "MapViewport.h"
#include "VmtiTypes.h"

class QMouseEvent;
class QPainter;
class QQuickItem;
class QWheelEvent;

namespace Mapping {
class DiskTileCache;
} // namespace Mapping

namespace MappingQt {

/// @class TacticalMapQuickItem
/// @brief High-performance Qt Quick map item for tactical situational awareness.
/// @details Supports interactive mouse pan and cursor-centered wheel zoom, offline tile stores,
///          decoded tile caching, camera optical footprint frustums, platform heading indicators,
///          target line-of-sight rays, MISB ST 0903 VMTI moving target overlays, and click-to-slew
///          coordinate reporting. All shared state between the GUI thread and the Qt Quick scene
///          graph render thread is synchronized via an internal mutex.
class TacticalMapQuickItem : public QQuickPaintedItem {
    Q_OBJECT

    Q_PROPERTY(double centerLatitude READ centerLatitude WRITE setCenterLatitude NOTIFY centerChanged)
    Q_PROPERTY(double centerLongitude READ centerLongitude WRITE setCenterLongitude NOTIFY centerChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(bool offlineOnly READ isOfflineOnly WRITE setOfflineOnly NOTIFY offlineOnlyChanged)
    Q_PROPERTY(bool showFrustum READ showFrustum WRITE setShowFrustum NOTIFY showFrustumChanged)
    Q_PROPERTY(bool showHeading READ showHeading WRITE setShowHeading NOTIFY showHeadingChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)

    Q_PROPERTY(double platformLatitude READ platformLatitude WRITE setPlatformLatitude NOTIFY platformChanged)
    Q_PROPERTY(double platformLongitude READ platformLongitude WRITE setPlatformLongitude NOTIFY platformChanged)
    Q_PROPERTY(double platformHeading READ platformHeading WRITE setPlatformHeading NOTIFY platformChanged)

    Q_PROPERTY(bool hasTarget READ hasTarget NOTIFY targetChanged)
    Q_PROPERTY(double targetLatitude READ targetLatitude NOTIFY targetChanged)
    Q_PROPERTY(double targetLongitude READ targetLongitude NOTIFY targetChanged)

    Q_PROPERTY(QColor themeColor READ themeColor WRITE setThemeColor NOTIFY themeColorChanged)

public:
    /// @brief Constructs a TacticalMapQuickItem with default viewport and tactical overlay settings.
    /// @details Initializes the viewport centered at `(37.9838, 23.7275)` at zoom `12.0`, enables
    ///          left/right mouse buttons, hover events, antialiasing, and procedural grid fallback.
    /// @param[in] parent Optional parent QQuickItem in the Qt Quick scene graph hierarchy.
    explicit TacticalMapQuickItem(QQuickItem* parent = nullptr);

    /// @brief Destroys the TacticalMapQuickItem instance and releases cached resources.
    /// @details Defaulted out-of-line in the implementation unit so forward-declared types
    ///          managed by smart pointers are destroyed with complete type definitions.
    ~TacticalMapQuickItem() override;

    /// @brief Returns the current geodetic latitude at the center of the map viewport.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return Viewport center latitude in degrees within `[-85.05112878, 85.05112878]`.
    [[nodiscard]] double centerLatitude() const;

    /// @brief Sets the geodetic latitude at the center of the map viewport.
    /// @details Ignores non-finite values (`NaN`/`Inf`), clamps latitude to Web Mercator limits,
    ///          and emits `centerChanged()` and schedules a repaint only if the value changed.
    /// @param[in] lat Target center latitude in degrees.
    void setCenterLatitude(double lat);

    /// @brief Returns the current geodetic longitude at the center of the map viewport.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return Viewport center longitude in degrees within `[-180.0, 180.0]`.
    [[nodiscard]] double centerLongitude() const;

    /// @brief Sets the geodetic longitude at the center of the map viewport.
    /// @details Ignores non-finite values (`NaN`/`Inf`), normalizes longitude to `[-180.0, 180.0]`,
    ///          and emits `centerChanged()` and schedules a repaint only if the value changed.
    /// @param[in] lon Target center longitude in degrees.
    void setCenterLongitude(double lon);

    /// @brief Atomically sets both the center latitude and longitude of the map viewport.
    /// @details Ignores non-finite inputs, clamps latitude, normalizes longitude, and emits a
    ///          single `centerChanged()` notification if either coordinate component changed.
    /// @param[in] lat Target center latitude in degrees.
    /// @param[in] lon Target center longitude in degrees.
    Q_INVOKABLE void setCenter(double lat, double lon);

    /// @brief Returns the current Web Mercator zoom level of the map viewport.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return Continuous zoom level within `[0.0, 22.0]`.
    [[nodiscard]] double zoom() const;

    /// @brief Sets the Web Mercator zoom level of the map viewport.
    /// @details Ignores non-finite values, clamps zoom to `[0.0, MercatorProjection::kMaxZoom]`,
    ///          and emits `zoomChanged()` and schedules a repaint only if the zoom level changed.
    /// @param[in] z Target zoom level.
    void setZoom(double z);

    /// @brief Indicates whether the tile provider operates strictly in offline mode.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return `true` if network tile fetching is disabled; `false` otherwise.
    [[nodiscard]] bool isOfflineOnly() const;

    /// @brief Enables or disables strict offline-only tile retrieval mode.
    /// @details Invalidates the decoded `QImage` tile cache, emits `offlineOnlyChanged()`, and
    ///          schedules a repaint when the offline mode state changes.
    /// @param[in] offline `true` to restrict tile loading to local disk/procedural providers.
    void setOfflineOnly(bool offline);

    /// @brief Configures the primary local disk directory used for XYZ raster tile caching.
    /// @details Passing an empty or whitespace-only path detaches the primary `DiskTileCache`.
    ///          Clears both the provider's memory cache and the decoded `QImage` cache before
    ///          scheduling a repaint.
    /// @param[in] directoryPath Filesystem path to the root XYZ tile directory (`{z}/{x}/{y}.png`).
    Q_INVOKABLE void setOfflineDirectory(const QString& directoryPath);

    /// @brief Registers an additional read-only offline tile directory for fallback lookups.
    /// @details Ignores empty paths. Attaches the directory to the existing primary disk cache
    ///          or adds a secondary `DiskTileCache` provider, then invalidates cached tiles.
    /// @param[in] directoryPath Filesystem path to the secondary XYZ tile directory.
    Q_INVOKABLE void addOfflineSearchDirectory(const QString& directoryPath);

    /// @brief Returns whether the camera optical footprint frustum overlay is enabled.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return `true` if the sensor footprint polygon is rendered when valid corners are set.
    [[nodiscard]] bool showFrustum() const;

    /// @brief Enables or disables rendering of the camera optical footprint frustum overlay.
    /// @details Emits `showFrustumChanged()` and schedules a repaint if the flag changed.
    /// @param[in] show `true` to render the sensor footprint polygon; `false` to hide it.
    void setShowFrustum(bool show);

    /// @brief Returns whether the platform position reticle and heading vector are enabled.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return `true` if the platform marker and heading arrow are rendered.
    [[nodiscard]] bool showHeading() const;

    /// @brief Enables or disables rendering of the platform reticle and heading vector.
    /// @details Emits `showHeadingChanged()` and schedules a repaint if the flag changed.
    /// @param[in] show `true` to render the platform marker and heading vector; `false` to hide them.
    void setShowHeading(bool show);

    /// @brief Returns whether the procedural fallback grid and coordinate grid overlay are enabled.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return `true` if procedural grid tiles and tactical grid lines are enabled.
    [[nodiscard]] bool showGrid() const;

    /// @brief Enables or disables procedural fallback grid tiles and tactical grid line rendering.
    /// @details Updates `CompositeTileProvider::setProceduralFallbackEnabled()`, clears the memory
    ///          and decoded `QImage` tile caches, emits `showGridChanged()`, and schedules a repaint.
    /// @param[in] show `true` to enable procedural fallback tiles and grid overlay; `false` to disable.
    void setShowGrid(bool show);

    /// @brief Returns the current platform geodetic latitude in degrees.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return Platform latitude in degrees.
    [[nodiscard]] double platformLatitude() const;

    /// @brief Updates the platform geodetic latitude in degrees.
    /// @details Rejects non-finite values, clamps latitude to Web Mercator limits, and emits
    ///          `platformChanged()` and schedules a repaint if the value changed.
    /// @param[in] lat Platform latitude in degrees.
    void setPlatformLatitude(double lat);

    /// @brief Returns the current platform geodetic longitude in degrees.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return Platform longitude in degrees.
    [[nodiscard]] double platformLongitude() const;

    /// @brief Updates the platform geodetic longitude in degrees.
    /// @details Rejects non-finite values, normalizes longitude to `[-180.0, 180.0]`, and emits
    ///          `platformChanged()` and schedules a repaint if the value changed.
    /// @param[in] lon Platform longitude in degrees.
    void setPlatformLongitude(double lon);

    /// @brief Returns the current platform true heading angle in degrees.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return Platform heading in degrees within `[0.0, 360.0)` (`0 = North`, `90 = East`).
    [[nodiscard]] double platformHeading() const;

    /// @brief Updates the platform true heading angle in degrees.
    /// @details Rejects non-finite values, normalizes the heading into `[0.0, 360.0)`, and emits
    ///          `platformChanged()` and schedules a repaint if the value changed.
    /// @param[in] headingDeg Platform heading in degrees clockwise from true North.
    void setPlatformHeading(double headingDeg);

    /// @brief Atomically updates the platform latitude, longitude, and true heading.
    /// @details Rejects updates if any parameter is non-finite. Clamps latitude, normalizes
    ///          longitude and heading, and emits a single `platformChanged()` signal on change.
    /// @param[in] lat Platform geodetic latitude in degrees.
    /// @param[in] lon Platform geodetic longitude in degrees.
    /// @param[in] headingDeg Platform true heading in degrees clockwise from North.
    Q_INVOKABLE void setPlatformPosition(double lat, double lon, double headingDeg);

    /// @brief Returns whether an active target ground position is currently set.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return `true` if a valid target position is active for line-of-sight rendering.
    [[nodiscard]] bool hasTarget() const;

    /// @brief Returns the geodetic latitude of the active target position.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return Target latitude in degrees.
    [[nodiscard]] double targetLatitude() const;

    /// @brief Returns the geodetic longitude of the active target position.
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return Target longitude in degrees.
    [[nodiscard]] double targetLongitude() const;

    /// @brief Sets the active target ground coordinate for optical line-of-sight rendering.
    /// @details Rejects non-finite inputs, clamps latitude, normalizes longitude, activates
    ///          target rendering, and emits `targetChanged()` when the target state changes.
    /// @param[in] lat Target geodetic latitude in degrees.
    /// @param[in] lon Target geodetic longitude in degrees.
    Q_INVOKABLE void setTargetPosition(double lat, double lon);

    /// @brief Clears the active target position and hides the line-of-sight overlay.
    /// @details Emits `targetChanged()` and schedules a repaint if a target was previously active.
    Q_INVOKABLE void clearTarget();

    /// @brief Sets the four geographic corners of the sensor optical footprint frustum from QML.
    /// @details Delegates to `setFrustum(const Klv::FrustumCorners&)` after packing the 8 coordinates.
    /// @param[in] tlLat Top-left corner latitude in degrees.
    /// @param[in] tlLon Top-left corner longitude in degrees.
    /// @param[in] trLat Top-right corner latitude in degrees.
    /// @param[in] trLon Top-right corner longitude in degrees.
    /// @param[in] brLat Bottom-right corner latitude in degrees.
    /// @param[in] brLon Bottom-right corner longitude in degrees.
    /// @param[in] blLat Bottom-left corner latitude in degrees.
    /// @param[in] blLon Bottom-left corner longitude in degrees.
    Q_INVOKABLE void setFrustum(
        double tlLat, double tlLon, double trLat, double trLon, double brLat, double brLon, double blLat, double blLon);

    /// @brief Sets the sensor optical footprint frustum corners from a `Klv::FrustumCorners` packet.
    /// @details Rejects the update if any corner coordinate is non-finite. Clamps latitudes,
    ///          normalizes longitudes, and schedules a repaint only if the footprint changed.
    /// @param[in] frustum Geographic coordinates of the four sensor footprint corners.
    void setFrustum(const Klv::FrustumCorners& frustum);

    /// @brief Clears the active sensor footprint frustum and schedules a repaint if one was active.
    /// @details Thread-safe mutator synchronized under the internal mutex.
    void clearFrustum();

    /// @brief Updates the active MISB ST 0903 VMTI moving target set for tactical overlay rendering.
    /// @details Copies the target pack series under lock and schedules a scene-graph repaint.
    /// @param[in] vmti Parsed MISB ST 0903 VMTI local set containing moving target tracks.
    void setVmtiTargets(const Klv::VmtiLocalSet& vmti);

    /// @brief Clears all active MISB ST 0903 VMTI moving target tracks.
    /// @details Empties the stored VMTI target vector under lock and schedules a repaint if non-empty.
    Q_INVOKABLE void clearVmtiTargets();

    /// @brief Returns the number of currently stored MISB ST 0903 VMTI target tracks.
    /// @details Thread-safe query synchronized under the internal mutex.
    /// @return Count of active VMTI target tracks.
    [[nodiscard]] std::size_t vmtiTargetCount() const;

    /// @brief Returns the number of decoded `QImage` tiles currently held in the render cache.
    /// @details Thread-safe diagnostic query synchronized under the internal mutex.
    /// @return Number of cached `QImage` tile entries (bounded by `kMaxDecodedTiles`).
    [[nodiscard]] std::size_t decodedTileCacheSize() const;

    /// @brief Returns the primary accent color used for tactical overlays (frustum, LOS, reticle).
    /// @details Thread-safe getter synchronized under the internal mutex.
    /// @return Current tactical theme `QColor`.
    [[nodiscard]] QColor themeColor() const;

    /// @brief Sets the primary accent color used for tactical overlays.
    /// @details Ignores invalid `QColor` instances and emits `themeColorChanged()` on change.
    /// @param[in] color New valid tactical accent `QColor`.
    void setThemeColor(const QColor& color);

    /// @brief Projects a geographic WGS-84 coordinate to item-local screen pixels.
    /// @details Converts the given latitude and longitude into 2D screen coordinates
    ///          relative to the current viewport center, zoom level, and item dimensions.
    ///          Returns `(0.0, 0.0)` if either input is non-finite (`NaN` or `Inf`).
    /// @param[in] lat Geodetic latitude in degrees.
    /// @param[in] lon Geodetic longitude in degrees.
    /// @return Screen position as `QPointF(xPixels, yPixels)`.
    [[nodiscard]] Q_INVOKABLE QPointF geoToScreen(double lat, double lon) const;

    /// @brief Unprojects item-local screen pixel coordinates to WGS-84 geographic coordinates.
    /// @details Converts `(x, y)` pixel coordinates within the item into geodetic latitude
    ///          and longitude. Returns `(0.0, 0.0)` if either input is non-finite (`NaN` or `Inf`).
    /// @param[in] x Horizontal screen coordinate in pixels from the item's left edge.
    /// @param[in] y Vertical screen coordinate in pixels from the item's top edge.
    /// @return Geographic coordinate encoded as `QPointF(latitudeDeg, longitudeDeg)`,
    ///         where `QPointF::x()` is latitude in degrees and `QPointF::y()` is longitude in degrees.
    [[nodiscard]] Q_INVOKABLE QPointF screenToGeo(double x, double y) const;

    /// @brief Renders the map tiles, coordinate grid, frustum, LOS, VMTI tracks, and platform heading.
    /// @details Invoked by the Qt Quick scene graph (potentially on the dedicated render thread).
    ///          Copies a consistent `RenderSnapshot` under a brief lock before performing tile
    ///          lookup/decompression and `QPainter` rasterization without holding `m_mutex`.
    /// @param[in] painter Target `QPainter` provided by `QQuickPaintedItem` (guarded against `nullptr`).
    void paint(QPainter* painter) override;

    /// @brief Registers `TacticalMapQuickItem` into the QML type system under `"PelcoD.Mapping"` and `"MappingQt"`.
    /// @details Delegates to `MappingQt::registerQmlTypes()`.
    static void registerQmlTypes();

signals:
    /// @brief Emitted whenever the viewport center latitude or longitude changes.
    void centerChanged();

    /// @brief Emitted whenever the viewport zoom level changes.
    void zoomChanged();

    /// @brief Emitted whenever the offline-only tile provider mode changes.
    void offlineOnlyChanged();

    /// @brief Emitted whenever the sensor footprint frustum visibility flag changes.
    void showFrustumChanged();

    /// @brief Emitted whenever the platform heading overlay visibility flag changes.
    void showHeadingChanged();

    /// @brief Emitted whenever the procedural fallback and coordinate grid flag changes.
    void showGridChanged();

    /// @brief Emitted whenever the platform latitude, longitude, or heading changes.
    void platformChanged();

    /// @brief Emitted whenever the target position is updated or cleared.
    void targetChanged();

    /// @brief Emitted whenever the tactical overlay accent color changes.
    void themeColorChanged();

    /// @brief Emitted when the operator performs a stationary left-click on the map (within the 4 px deadband).
    /// @param[in] latitude Clicked geodetic latitude in degrees.
    /// @param[in] longitude Clicked geodetic longitude in degrees.
    void coordinateClicked(double latitude, double longitude);

protected:
    /// @brief Synchronizes the internal `Mapping::MapViewport` dimensions when the QML item resizes.
    /// @details Updates the viewport width and height under lock and schedules a repaint.
    /// @param[in] newGeometry Updated item bounding geometry in scene coordinates.
    /// @param[in] oldGeometry Previous item bounding geometry in scene coordinates.
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

    /// @brief Initiates left-button click/drag tracking on mouse press.
    /// @details Records the initial press position (`m_pressMousePos`) and resets the drag deadband
    ///          state without prematurely emitting `coordinateClicked()`.
    /// @param[in] event Pointer to the incoming mouse press event (guarded against `nullptr`).
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief Pans the map viewport during left-button mouse drag gestures.
    /// @details Marks `m_dragExceededThreshold = true` once displacement from `m_pressMousePos`
    ///          exceeds `4.0` pixels, pans the viewport by the incremental delta, and emits `centerChanged()`.
    /// @param[in] event Pointer to the incoming mouse move event (guarded against `nullptr`).
    void mouseMoveEvent(QMouseEvent* event) override;

    /// @brief Completes left-button interaction and emits `coordinateClicked()` if not dragged.
    /// @details Emits `coordinateClicked(latitude, longitude)` if and only if the cursor remained
    ///          within the `4.0` pixel Euclidean click deadband throughout the press-release cycle.
    /// @param[in] event Pointer to the incoming mouse release event (guarded against `nullptr`).
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// @brief Zooms the map viewport centered around the cursor position on mouse wheel rotation.
    /// @details Computes a fractional zoom delta from `event->angleDelta().y()`, pivots around
    ///          `event->position()`, and emits `zoomChanged()` and `centerChanged()`.
    /// @param[in] event Pointer to the incoming wheel event (guarded against `nullptr`).
    void wheelEvent(QWheelEvent* event) override;

private:
    static constexpr std::size_t kMaxDecodedTiles { 128U };

    /// @brief Clears all entries from the bounded decoded `QImage` tile cache.
    /// @details Caller must hold `m_mutex` before invoking this helper.
    void clearDecodedTileCacheLocked();

    /// @brief Retrieves a decoded `QImage` for `coord` from cache, or decodes and caches it on miss.
    /// @details Performs compressed tile retrieval and `QImage::loadFromData()` outside `m_mutex`
    ///          to avoid blocking the GUI thread during raster decompression.
    /// @param[in] coord Web Mercator tile coordinate (`z`, `x`, `y`).
    /// @return Decoded `QImage`, or a null `QImage` if no valid tile data is available.
    [[nodiscard]] QImage findOrDecodeTile(const Mapping::TileCoord& coord);

    mutable QMutex m_mutex { };
    Mapping::MapViewport m_viewport;
    std::shared_ptr<Mapping::DiskTileCache> m_diskCache { nullptr };
    Mapping::CompositeTileProvider m_tileProvider;

    std::unordered_map<Mapping::TileCoord, QImage> m_decodedTileCache { };
    std::deque<Mapping::TileCoord> m_decodedTileOrder { };

    bool m_showFrustum { true };
    bool m_showHeading { true };
    bool m_showGrid { true };

    double m_platformLat { 0.0 };
    double m_platformLon { 0.0 };
    double m_platformHeading { 0.0 };

    double m_targetLat { 0.0 };
    double m_targetLon { 0.0 };
    bool m_hasTarget { false };

    Klv::FrustumCorners m_frustum { };
    bool m_hasFrustum { false };

    Klv::VmtiLocalSet m_vmtiSet { };
    bool m_hasVmti { false };

    QColor m_themeColor { 0, 229, 255 }; // Electric Cyan default

    // Mouse interaction tracking
    bool m_isDragging { false };
    bool m_dragExceededThreshold { false };
    QPointF m_pressMousePos { };
    QPointF m_lastMousePos { };
};

/// @brief Registers all `MappingQt` QML types (`TacticalMap` and `TacticalMapQuickItem`) with the Qt QML engine.
/// @details Registers both `"PelcoD.Mapping"` (`1.0`) and `"MappingQt"` (`1.0`) QML import namespaces.
void registerQmlTypes();

} // namespace MappingQt
