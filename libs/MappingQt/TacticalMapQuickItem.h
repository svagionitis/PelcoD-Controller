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
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QRectF>
#include <QString>

#include "CompositeTileProvider.h"
#include "DiskTileCache.h"
#include "GeoTypes.h"
#include "KlvTypes.h"
#include "MapViewport.h"
#include "VmtiTypes.h"

class QMouseEvent;
class QPainter;
class QWheelEvent;

namespace MappingQt {

/// @class TacticalMapQuickItem
/// @brief High-performance Qt Quick map item for tactical situational awareness,
///        supporting interactive pan/zoom, offline tile stores, camera optical footprint frustums,
///        platform heading pointers, target line-of-sight, MISB ST 0903 VMTI tracks,
///        and click-to-PTZ coordinate reporting.
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
    explicit TacticalMapQuickItem(QQuickItem* parent = nullptr);
    ~TacticalMapQuickItem() override = default;

    // Center coordinates
    [[nodiscard]] double centerLatitude() const;
    void setCenterLatitude(double lat);

    [[nodiscard]] double centerLongitude() const;
    void setCenterLongitude(double lon);

    Q_INVOKABLE void setCenter(double lat, double lon);

    // Zoom level
    [[nodiscard]] double zoom() const;
    void setZoom(double z);

    // Offline configuration
    [[nodiscard]] bool isOfflineOnly() const;
    void setOfflineOnly(bool offline);

    Q_INVOKABLE void setOfflineDirectory(const QString& directoryPath);
    Q_INVOKABLE void addOfflineSearchDirectory(const QString& directoryPath);

    // Tactical overlay controls
    [[nodiscard]] bool showFrustum() const;
    void setShowFrustum(bool show);

    [[nodiscard]] bool showHeading() const;
    void setShowHeading(bool show);

    [[nodiscard]] bool showGrid() const;
    void setShowGrid(bool show);

    // Platform telemetry
    [[nodiscard]] double platformLatitude() const;
    void setPlatformLatitude(double lat);

    [[nodiscard]] double platformLongitude() const;
    void setPlatformLongitude(double lon);

    [[nodiscard]] double platformHeading() const;
    void setPlatformHeading(double headingDeg);

    Q_INVOKABLE void setPlatformPosition(double lat, double lon, double headingDeg);

    // Target line-of-sight telemetry
    [[nodiscard]] bool hasTarget() const;
    [[nodiscard]] double targetLatitude() const;
    [[nodiscard]] double targetLongitude() const;
    Q_INVOKABLE void setTargetPosition(double lat, double lon);
    Q_INVOKABLE void clearTarget();

    // Sensor footprint frustum
    Q_INVOKABLE void setFrustum(
        double tlLat, double tlLon, double trLat, double trLon, double brLat, double brLon, double blLat, double blLon);

    void setFrustum(const Klv::FrustumCorners& frustum);
    void clearFrustum();

    // MISB ST 0903 VMTI moving target overlays
    void setVmtiTargets(const Klv::VmtiLocalSet& vmti);
    Q_INVOKABLE void clearVmtiTargets();
    [[nodiscard]] std::size_t vmtiTargetCount() const;

    // Decoded tile cache inspection
    [[nodiscard]] std::size_t decodedTileCacheSize() const;

    // Color theme
    [[nodiscard]] QColor themeColor() const;
    void setThemeColor(const QColor& color);

    // Geographic <-> Screen coordinate conversions
    [[nodiscard]] Q_INVOKABLE QPointF geoToScreen(double lat, double lon) const;
    [[nodiscard]] Q_INVOKABLE QPointF screenToGeo(double x, double y) const;

    // QQuickPaintedItem overrides
    void paint(QPainter* painter) override;

    /// @brief Registers this item type into QML under URIs "PelcoD.Mapping 1.0" and "MappingQt 1.0".
    static void registerQmlTypes();

signals:
    void centerChanged();
    void zoomChanged();
    void offlineOnlyChanged();
    void showFrustumChanged();
    void showHeadingChanged();
    void showGridChanged();
    void platformChanged();
    void targetChanged();
    void themeColorChanged();
    void coordinateClicked(double latitude, double longitude);

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    static constexpr std::size_t kMaxDecodedTiles { 128U };

    void clearDecodedTileCacheLocked();
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

/// @brief Registers all MappingQt QML types with the Qt QML type system.
void registerQmlTypes();

} // namespace MappingQt
