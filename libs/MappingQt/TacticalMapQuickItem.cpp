#include "TacticalMapQuickItem.h"

#include <QPainterPath>
#include <QtQml>

namespace MappingQt {

TacticalMapQuickItem::TacticalMapQuickItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , m_viewport({ 37.9838, 23.7275 }, 12.0, 800.0, 600.0)
    , m_diskCache(nullptr)
    , m_tileProvider(m_diskCache) {
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);
    setAntialiasing(true);
}

double TacticalMapQuickItem::centerLatitude() const noexcept {
    QMutexLocker locker(&m_mutex);
    return m_viewport.center().latitudeDeg;
}

void TacticalMapQuickItem::setCenterLatitude(double lat) noexcept {
    {
        QMutexLocker locker(&m_mutex);
        auto c = m_viewport.center();
        c.latitudeDeg = lat;
        m_viewport.setCenter(c);
    }
    emit centerChanged();
    update();
}

double TacticalMapQuickItem::centerLongitude() const noexcept {
    QMutexLocker locker(&m_mutex);
    return m_viewport.center().longitudeDeg;
}

void TacticalMapQuickItem::setCenterLongitude(double lon) noexcept {
    {
        QMutexLocker locker(&m_mutex);
        auto c = m_viewport.center();
        c.longitudeDeg = lon;
        m_viewport.setCenter(c);
    }
    emit centerChanged();
    update();
}

void TacticalMapQuickItem::setCenter(double lat, double lon) noexcept {
    {
        QMutexLocker locker(&m_mutex);
        m_viewport.setCenter({ lat, lon });
    }
    emit centerChanged();
    update();
}

double TacticalMapQuickItem::zoom() const noexcept {
    QMutexLocker locker(&m_mutex);
    return m_viewport.zoom();
}

void TacticalMapQuickItem::setZoom(double z) noexcept {
    {
        QMutexLocker locker(&m_mutex);
        m_viewport.setZoom(z);
    }
    emit zoomChanged();
    update();
}

bool TacticalMapQuickItem::isOfflineOnly() const noexcept {
    QMutexLocker locker(&m_mutex);
    return m_tileProvider.isOfflineOnly();
}

void TacticalMapQuickItem::setOfflineOnly(bool offline) noexcept {
    {
        QMutexLocker locker(&m_mutex);
        m_tileProvider.setOfflineOnly(offline);
    }
    emit offlineOnlyChanged();
    update();
}

void TacticalMapQuickItem::setOfflineDirectory(const QString& directoryPath) {
    QMutexLocker locker(&m_mutex);
    m_diskCache = std::make_shared<Mapping::DiskTileCache>(directoryPath.toStdString());
    m_tileProvider.setDiskCache(m_diskCache);
    update();
}

bool TacticalMapQuickItem::showFrustum() const noexcept {
    return m_showFrustum;
}

void TacticalMapQuickItem::setShowFrustum(bool show) noexcept {
    if (m_showFrustum != show) {
        m_showFrustum = show;
        emit showFrustumChanged();
        update();
    }
}

bool TacticalMapQuickItem::showHeading() const noexcept {
    return m_showHeading;
}

void TacticalMapQuickItem::setShowHeading(bool show) noexcept {
    if (m_showHeading != show) {
        m_showHeading = show;
        emit showHeadingChanged();
        update();
    }
}

bool TacticalMapQuickItem::showGrid() const noexcept {
    return m_showGrid;
}

void TacticalMapQuickItem::setShowGrid(bool show) noexcept {
    if (m_showGrid != show) {
        m_showGrid = show;
        emit showGridChanged();
        update();
    }
}

double TacticalMapQuickItem::platformLatitude() const noexcept {
    return m_platformLat;
}

void TacticalMapQuickItem::setPlatformLatitude(double lat) noexcept {
    if (m_platformLat != lat) {
        m_platformLat = lat;
        emit platformChanged();
        update();
    }
}

double TacticalMapQuickItem::platformLongitude() const noexcept {
    return m_platformLon;
}

void TacticalMapQuickItem::setPlatformLongitude(double lon) noexcept {
    if (m_platformLon != lon) {
        m_platformLon = lon;
        emit platformChanged();
        update();
    }
}

double TacticalMapQuickItem::platformHeading() const noexcept {
    return m_platformHeading;
}

void TacticalMapQuickItem::setPlatformHeading(double headingDeg) noexcept {
    if (m_platformHeading != headingDeg) {
        m_platformHeading = headingDeg;
        emit platformChanged();
        update();
    }
}

void TacticalMapQuickItem::setPlatformPosition(double lat, double lon, double headingDeg) noexcept {
    m_platformLat = lat;
    m_platformLon = lon;
    m_platformHeading = headingDeg;
    emit platformChanged();
    update();
}

void TacticalMapQuickItem::setFrustum(double tlLat, double tlLon,
                                     double trLat, double trLon,
                                     double brLat, double brLon,
                                     double blLat, double blLon) noexcept {
    m_frustum.topLeft = { tlLat, tlLon };
    m_frustum.topRight = { trLat, trLon };
    m_frustum.bottomRight = { brLat, brLon };
    m_frustum.bottomLeft = { blLat, blLon };
    m_hasFrustum = true;
    update();
}

void TacticalMapQuickItem::setFrustum(const Klv::FrustumCorners& frustum) noexcept {
    m_frustum = frustum;
    m_hasFrustum = true;
    update();
}

void TacticalMapQuickItem::clearFrustum() noexcept {
    m_hasFrustum = false;
    update();
}

QColor TacticalMapQuickItem::themeColor() const noexcept {
    return m_themeColor;
}

void TacticalMapQuickItem::setThemeColor(const QColor& color) noexcept {
    if (m_themeColor != color) {
        m_themeColor = color;
        emit themeColorChanged();
        update();
    }
}

QPointF TacticalMapQuickItem::geoToScreen(double lat, double lon) const noexcept {
    QMutexLocker locker(&m_mutex);
    const auto sp = m_viewport.geoToScreen({ lat, lon });
    return QPointF(sp.x, sp.y);
}

QPointF TacticalMapQuickItem::screenToGeo(double x, double y) const noexcept {
    QMutexLocker locker(&m_mutex);
    const auto geo = m_viewport.screenToGeo({ x, y });
    return QPointF(geo.latitudeDeg, geo.longitudeDeg);
}

void TacticalMapQuickItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    QMutexLocker locker(&m_mutex);
    m_viewport.setSize(newGeometry.width(), newGeometry.height());
    update();
}

void TacticalMapQuickItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_lastMousePos = event->position();

        const auto geo = screenToGeo(event->position().x(), event->position().y());
        emit coordinateClicked(geo.x(), geo.y());
        event->accept();
    } else {
        QQuickPaintedItem::mousePressEvent(event);
    }
}

void TacticalMapQuickItem::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        const QPointF delta = event->position() - m_lastMousePos;
        m_lastMousePos = event->position();

        {
            QMutexLocker locker(&m_mutex);
            m_viewport.pan(delta.x(), delta.y());
        }
        emit centerChanged();
        update();
        event->accept();
    } else {
        QQuickPaintedItem::mouseMoveEvent(event);
    }
}

void TacticalMapQuickItem::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
    } else {
        QQuickPaintedItem::mouseReleaseEvent(event);
    }
}

void TacticalMapQuickItem::wheelEvent(QWheelEvent* event) {
    const double delta = (static_cast<double>(event->angleDelta().y()) / 120.0) * 0.5;
    {
        QMutexLocker locker(&m_mutex);
        m_viewport.zoomBy(delta, { event->position().x(), event->position().y() });
    }
    emit zoomChanged();
    emit centerChanged();
    update();
    event->accept();
}

void TacticalMapQuickItem::paint(QPainter* painter) {
    QMutexLocker locker(&m_mutex);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 1. Draw base tactical background
    painter->fillRect(boundingRect(), QColor(14, 18, 24));

    // 2. Render visible tiles
    const auto visibleTiles = m_viewport.calculateVisibleTiles(1);
    for (const auto& vt : visibleTiles) {
        const auto tileData = m_tileProvider.getTile(vt.coord);
        if (tileData && !tileData->bytes.empty()) {
            QImage img;
            if (img.loadFromData(tileData->bytes.data(), static_cast<int>(tileData->bytes.size()))) {
                const QRectF destRect(vt.screenRect.x, vt.screenRect.y, vt.screenRect.width, vt.screenRect.height);
                painter->drawImage(destRect, img);
            }
        }
    }

    // 3. Render tactical optical footprint frustum
    if (m_showFrustum && m_hasFrustum) {
        const auto sf = Mapping::TacticalOverlay::projectFrustum(m_viewport, m_frustum);
        QPolygonF poly;
        for (int i = 0; i < 4; ++i) {
            poly << QPointF(sf.corners[static_cast<std::size_t>(i)].x,
                            sf.corners[static_cast<std::size_t>(i)].y);
        }

        // Semi-transparent glowing tactical polygon
        QColor fillColor = m_themeColor;
        fillColor.setAlpha(45);
        painter->setBrush(QBrush(fillColor));

        QPen borderPen(m_themeColor, 2.0);
        painter->setPen(borderPen);
        painter->drawPolygon(poly);

        // Corner reticles
        painter->setPen(QPen(Qt::white, 1.5));
        for (int i = 0; i < 4; ++i) {
            const QPointF pt(sf.corners[static_cast<std::size_t>(i)].x,
                             sf.corners[static_cast<std::size_t>(i)].y);
            painter->drawLine(QPointF(pt.x() - 4, pt.y()), QPointF(pt.x() + 4, pt.y()));
            painter->drawLine(QPointF(pt.x(), pt.y() - 4), QPointF(pt.x(), pt.y() + 4));
        }
    }

    // 4. Render platform marker and heading arrow
    if (m_showHeading) {
        const auto sv = Mapping::TacticalOverlay::projectHeadingVector(
            m_viewport, { m_platformLat, m_platformLon }, m_platformHeading, 35.0);

        const QPointF origin(sv.origin.x, sv.origin.y);
        const QPointF tip(sv.tip.x, sv.tip.y);

        // Platform circular reticle
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(m_themeColor, 2.0));
        painter->drawEllipse(origin, 7.0, 7.0);

        painter->setPen(QPen(m_themeColor, 1.5, Qt::SolidLine));
        painter->drawLine(origin, tip);

        // Arrow head
        const double angle = std::atan2(tip.y() - origin.y(), tip.x() - origin.x());
        constexpr double kHeadLen = 8.0;
        constexpr double kHeadAngle = 0.5;

        const QPointF arrowP1(tip.x() - kHeadLen * std::cos(angle - kHeadAngle),
                              tip.y() - kHeadLen * std::sin(angle - kHeadAngle));
        const QPointF arrowP2(tip.x() - kHeadLen * std::cos(angle + kHeadAngle),
                              tip.y() - kHeadLen * std::sin(angle + kHeadAngle));

        QPolygonF arrowHead;
        arrowHead << tip << arrowP1 << arrowP2;
        painter->setBrush(QBrush(m_themeColor));
        painter->drawPolygon(arrowHead);
    }
}

void TacticalMapQuickItem::registerQmlTypes() {
    qmlRegisterType<TacticalMapQuickItem>("MappingQt", 1, 0, "TacticalMapQuickItem");
}

} // namespace MappingQt
