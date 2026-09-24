#pragma once

/// @file TacticalMapQuickItem.h
/// @brief Custom QQuickPaintedItem providing interactive tactical 2D map display in QML.

#include "CompositeTileProvider.h"
#include "DiskTileCache.h"
#include "GeoTypes.h"
#include "KlvTypes.h"
#include "MapViewport.h"
#include "TacticalOverlay.h"

#include <QColor>
#include <QImage>
#include <QMouseEvent>
#include <QMutex>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QWheelEvent>

#include <memory>
#include <optional>
#include <string>

namespace MappingQt {

/// @class TacticalMapQuickItem
/// @brief High-performance Qt Quick map item for tactical situational awareness,
///        supporting interactive pan/zoom, offline tile stores, camera optical footprint frustums,
///        platform heading pointers, and click-to-PTZ coordinate reporting.
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

    Q_PROPERTY(QColor themeColor READ themeColor WRITE setThemeColor NOTIFY themeColorChanged)

public:
    explicit TacticalMapQuickItem(QQuickItem* parent = nullptr);
    ~TacticalMapQuickItem() override = default;

    // Center coordinates
    [[nodiscard]] double centerLatitude() const noexcept;
    void setCenterLatitude(double lat) noexcept;

    [[nodiscard]] double centerLongitude() const noexcept;
    void setCenterLongitude(double lon) noexcept;

    Q_INVOKABLE void setCenter(double lat, double lon) noexcept;

    // Zoom level
    [[nodiscard]] double zoom() const noexcept;
    void setZoom(double z) noexcept;

    // Offline configuration
    [[nodiscard]] bool isOfflineOnly() const noexcept;
    void setOfflineOnly(bool offline) noexcept;

    Q_INVOKABLE void setOfflineDirectory(const QString& directoryPath);

    // Tactical overlay controls
    [[nodiscard]] bool showFrustum() const noexcept;
    void setShowFrustum(bool show) noexcept;

    [[nodiscard]] bool showHeading() const noexcept;
    void setShowHeading(bool show) noexcept;

    [[nodiscard]] bool showGrid() const noexcept;
    void setShowGrid(bool show) noexcept;

    // Platform telemetry
    [[nodiscard]] double platformLatitude() const noexcept;
    void setPlatformLatitude(double lat) noexcept;

    [[nodiscard]] double platformLongitude() const noexcept;
    void setPlatformLongitude(double lon) noexcept;

    [[nodiscard]] double platformHeading() const noexcept;
    void setPlatformHeading(double headingDeg) noexcept;

    Q_INVOKABLE void setPlatformPosition(double lat, double lon, double headingDeg) noexcept;

    // Sensor footprint frustum
    Q_INVOKABLE void setFrustum(double tlLat, double tlLon,
                                double trLat, double trLon,
                                double brLat, double brLon,
                                double blLat, double blLon) noexcept;

    void setFrustum(const Klv::FrustumCorners& frustum) noexcept;
    void clearFrustum() noexcept;

    // Color theme
    [[nodiscard]] QColor themeColor() const noexcept;
    void setThemeColor(const QColor& color) noexcept;

    // Geographic <-> Screen coordinate conversions
    Q_INVOKABLE QPointF geoToScreen(double lat, double lon) const noexcept;
    Q_INVOKABLE QPointF screenToGeo(double x, double y) const noexcept;

    // QQuickPaintedItem overrides
    void paint(QPainter* painter) override;

    /// @brief Registers this item type into QML under uri "MappingQt 1.0".
    static void registerQmlTypes();

signals:
    void centerChanged();
    void zoomChanged();
    void offlineOnlyChanged();
    void showFrustumChanged();
    void showHeadingChanged();
    void showGridChanged();
    void platformChanged();
    void themeColorChanged();
    void coordinateClicked(double latitude, double longitude);

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    mutable QMutex m_mutex;
    Mapping::MapViewport m_viewport;
    std::shared_ptr<Mapping::DiskTileCache> m_diskCache;
    Mapping::CompositeTileProvider m_tileProvider;

    bool m_showFrustum { true };
    bool m_showHeading { true };
    bool m_showGrid { true };

    double m_platformLat { 0.0 };
    double m_platformLon { 0.0 };
    double m_platformHeading { 0.0 };

    Klv::FrustumCorners m_frustum {};
    bool m_hasFrustum { false };

    QColor m_themeColor { 0, 229, 255 }; // Electric Cyan default

    // Mouse interaction tracking
    bool m_isDragging { false };
    QPointF m_lastMousePos {};
};

/// @brief Registers all MappingQt QML types with the Qt QML type system.
void registerQmlTypes();

} // namespace MappingQt
