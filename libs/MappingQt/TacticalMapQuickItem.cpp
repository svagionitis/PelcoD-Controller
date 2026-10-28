/// @file TacticalMapQuickItem.cpp
/// @brief Implementation of TacticalMapQuickItem for interactive tactical 2D map display in QML.

#include "TacticalMapQuickItem.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <limits>
#include <memory>

#include <QPainterPath>
#include <QtQml>

#include "MercatorProjection.h"

namespace MappingQt {

namespace {

    constexpr double kCoordEpsilon { 1e-9 };

    [[nodiscard]] bool nearlyEqual(double a, double b, double eps = kCoordEpsilon) noexcept
    {
        return std::abs(a - b) <= eps;
    }

    [[nodiscard]] double normalizeHeading(double headingDeg) noexcept
    {
        if (!std::isfinite(headingDeg)) {
            return 0.0;
        }
        double norm { std::fmod(headingDeg, 360.0) };
        if (norm < 0.0) {
            norm += 360.0;
        }
        return norm;
    }

    [[nodiscard]] bool isValidFrustum(const Klv::FrustumCorners& frustum) noexcept
    {
        return std::isfinite(frustum.topLeft.latitudeDeg) && std::isfinite(frustum.topLeft.longitudeDeg)
            && std::isfinite(frustum.topRight.latitudeDeg) && std::isfinite(frustum.topRight.longitudeDeg)
            && std::isfinite(frustum.bottomRight.latitudeDeg) && std::isfinite(frustum.bottomRight.longitudeDeg)
            && std::isfinite(frustum.bottomLeft.latitudeDeg) && std::isfinite(frustum.bottomLeft.longitudeDeg);
    }

    [[nodiscard]] Klv::GeoPoint2D sanitizePoint(double lat, double lon) noexcept
    {
        return { Mapping::MercatorProjection::clampLatitude(lat),
            Mapping::MercatorProjection::normalizeLongitude(lon) };
    }

} // namespace

TacticalMapQuickItem::TacticalMapQuickItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , m_viewport({ 37.9838, 23.7275 }, 12.0, 800.0, 600.0)
    , m_diskCache { nullptr }
    , m_tileProvider { m_diskCache }
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);
    setAntialiasing(true);
}

double TacticalMapQuickItem::centerLatitude() const
{
    QMutexLocker locker(&m_mutex);
    return m_viewport.center().latitudeDeg;
}

void TacticalMapQuickItem::setCenterLatitude(double lat)
{
    if (!std::isfinite(lat)) {
        return;
    }
    const double clampedLat { Mapping::MercatorProjection::clampLatitude(lat) };
    {
        QMutexLocker locker(&m_mutex);
        auto c = m_viewport.center();
        if (nearlyEqual(c.latitudeDeg, clampedLat)) {
            return;
        }
        c.latitudeDeg = clampedLat;
        m_viewport.setCenter(c);
    }
    emit centerChanged();
    update();
}

double TacticalMapQuickItem::centerLongitude() const
{
    QMutexLocker locker(&m_mutex);
    return m_viewport.center().longitudeDeg;
}

void TacticalMapQuickItem::setCenterLongitude(double lon)
{
    if (!std::isfinite(lon)) {
        return;
    }
    const double normLon { Mapping::MercatorProjection::normalizeLongitude(lon) };
    {
        QMutexLocker locker(&m_mutex);
        auto c = m_viewport.center();
        if (nearlyEqual(c.longitudeDeg, normLon)) {
            return;
        }
        c.longitudeDeg = normLon;
        m_viewport.setCenter(c);
    }
    emit centerChanged();
    update();
}

void TacticalMapQuickItem::setCenter(double lat, double lon)
{
    if (!std::isfinite(lat) || !std::isfinite(lon)) {
        return;
    }
    const double clampedLat { Mapping::MercatorProjection::clampLatitude(lat) };
    const double normLon { Mapping::MercatorProjection::normalizeLongitude(lon) };
    {
        QMutexLocker locker(&m_mutex);
        const auto cur = m_viewport.center();
        if (nearlyEqual(cur.latitudeDeg, clampedLat) && nearlyEqual(cur.longitudeDeg, normLon)) {
            return;
        }
        m_viewport.setCenter({ clampedLat, normLon });
    }
    emit centerChanged();
    update();
}

double TacticalMapQuickItem::zoom() const
{
    QMutexLocker locker(&m_mutex);
    return m_viewport.zoom();
}

void TacticalMapQuickItem::setZoom(double z)
{
    if (!std::isfinite(z)) {
        return;
    }
    const double clampedZoom { std::clamp(z, 0.0, static_cast<double>(Mapping::MercatorProjection::kMaxZoom)) };
    {
        QMutexLocker locker(&m_mutex);
        if (nearlyEqual(m_viewport.zoom(), clampedZoom)) {
            return;
        }
        m_viewport.setZoom(clampedZoom);
    }
    emit zoomChanged();
    update();
}

bool TacticalMapQuickItem::isOfflineOnly() const
{
    QMutexLocker locker(&m_mutex);
    return m_tileProvider.isOfflineOnly();
}

void TacticalMapQuickItem::setOfflineOnly(bool offline)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_tileProvider.isOfflineOnly() == offline) {
            return;
        }
        m_tileProvider.setOfflineOnly(offline);
    }
    emit offlineOnlyChanged();
    update();
}

void TacticalMapQuickItem::setOfflineDirectory(const QString& directoryPath)
{
    {
        QMutexLocker locker(&m_mutex);
        m_diskCache = std::make_shared<Mapping::DiskTileCache>(directoryPath.toStdString());
        m_tileProvider.setDiskCache(m_diskCache);
    }
    update();
}

bool TacticalMapQuickItem::showFrustum() const
{
    QMutexLocker locker(&m_mutex);
    return m_showFrustum;
}

void TacticalMapQuickItem::setShowFrustum(bool show)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_showFrustum == show) {
            return;
        }
        m_showFrustum = show;
    }
    emit showFrustumChanged();
    update();
}

bool TacticalMapQuickItem::showHeading() const
{
    QMutexLocker locker(&m_mutex);
    return m_showHeading;
}

void TacticalMapQuickItem::setShowHeading(bool show)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_showHeading == show) {
            return;
        }
        m_showHeading = show;
    }
    emit showHeadingChanged();
    update();
}

bool TacticalMapQuickItem::showGrid() const
{
    QMutexLocker locker(&m_mutex);
    return m_showGrid;
}

void TacticalMapQuickItem::setShowGrid(bool show)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_showGrid == show) {
            return;
        }
        m_showGrid = show;
    }
    emit showGridChanged();
    update();
}

double TacticalMapQuickItem::platformLatitude() const
{
    QMutexLocker locker(&m_mutex);
    return m_platformLat;
}

void TacticalMapQuickItem::setPlatformLatitude(double lat)
{
    if (!std::isfinite(lat)) {
        return;
    }
    const double clampedLat { Mapping::MercatorProjection::clampLatitude(lat) };
    {
        QMutexLocker locker(&m_mutex);
        if (nearlyEqual(m_platformLat, clampedLat)) {
            return;
        }
        m_platformLat = clampedLat;
    }
    emit platformChanged();
    update();
}

double TacticalMapQuickItem::platformLongitude() const
{
    QMutexLocker locker(&m_mutex);
    return m_platformLon;
}

void TacticalMapQuickItem::setPlatformLongitude(double lon)
{
    if (!std::isfinite(lon)) {
        return;
    }
    const double normLon { Mapping::MercatorProjection::normalizeLongitude(lon) };
    {
        QMutexLocker locker(&m_mutex);
        if (nearlyEqual(m_platformLon, normLon)) {
            return;
        }
        m_platformLon = normLon;
    }
    emit platformChanged();
    update();
}

double TacticalMapQuickItem::platformHeading() const
{
    QMutexLocker locker(&m_mutex);
    return m_platformHeading;
}

void TacticalMapQuickItem::setPlatformHeading(double headingDeg)
{
    if (!std::isfinite(headingDeg)) {
        return;
    }
    const double normHeading { normalizeHeading(headingDeg) };
    {
        QMutexLocker locker(&m_mutex);
        if (nearlyEqual(m_platformHeading, normHeading)) {
            return;
        }
        m_platformHeading = normHeading;
    }
    emit platformChanged();
    update();
}

void TacticalMapQuickItem::setPlatformPosition(double lat, double lon, double headingDeg)
{
    if (!std::isfinite(lat) || !std::isfinite(lon) || !std::isfinite(headingDeg)) {
        return;
    }
    const double clampedLat { Mapping::MercatorProjection::clampLatitude(lat) };
    const double normLon { Mapping::MercatorProjection::normalizeLongitude(lon) };
    const double normHeading { normalizeHeading(headingDeg) };
    {
        QMutexLocker locker(&m_mutex);
        if (nearlyEqual(m_platformLat, clampedLat) && nearlyEqual(m_platformLon, normLon)
            && nearlyEqual(m_platformHeading, normHeading)) {
            return;
        }
        m_platformLat = clampedLat;
        m_platformLon = normLon;
        m_platformHeading = normHeading;
    }
    emit platformChanged();
    update();
}

void TacticalMapQuickItem::setFrustum(
    double tlLat, double tlLon, double trLat, double trLon, double brLat, double brLon, double blLat, double blLon)
{
    Klv::FrustumCorners corners { };
    corners.topLeft = { tlLat, tlLon };
    corners.topRight = { trLat, trLon };
    corners.bottomRight = { brLat, brLon };
    corners.bottomLeft = { blLat, blLon };
    setFrustum(corners);
}

void TacticalMapQuickItem::setFrustum(const Klv::FrustumCorners& frustum)
{
    if (!isValidFrustum(frustum)) {
        return;
    }
    Klv::FrustumCorners sanitized { };
    sanitized.topLeft = sanitizePoint(frustum.topLeft.latitudeDeg, frustum.topLeft.longitudeDeg);
    sanitized.topRight = sanitizePoint(frustum.topRight.latitudeDeg, frustum.topRight.longitudeDeg);
    sanitized.bottomRight = sanitizePoint(frustum.bottomRight.latitudeDeg, frustum.bottomRight.longitudeDeg);
    sanitized.bottomLeft = sanitizePoint(frustum.bottomLeft.latitudeDeg, frustum.bottomLeft.longitudeDeg);

    {
        QMutexLocker locker(&m_mutex);
        m_frustum = sanitized;
        m_hasFrustum = true;
    }
    update();
}

void TacticalMapQuickItem::clearFrustum()
{
    {
        QMutexLocker locker(&m_mutex);
        if (!m_hasFrustum) {
            return;
        }
        m_hasFrustum = false;
    }
    update();
}

QColor TacticalMapQuickItem::themeColor() const
{
    QMutexLocker locker(&m_mutex);
    return m_themeColor;
}

void TacticalMapQuickItem::setThemeColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    {
        QMutexLocker locker(&m_mutex);
        if (m_themeColor == color) {
            return;
        }
        m_themeColor = color;
    }
    emit themeColorChanged();
    update();
}

QPointF TacticalMapQuickItem::geoToScreen(double lat, double lon) const
{
    if (!std::isfinite(lat) || !std::isfinite(lon)) {
        return QPointF(0.0, 0.0);
    }
    QMutexLocker locker(&m_mutex);
    const auto sp = m_viewport.geoToScreen({ lat, lon });
    return QPointF(sp.x, sp.y);
}

QPointF TacticalMapQuickItem::screenToGeo(double x, double y) const
{
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return QPointF(0.0, 0.0);
    }
    QMutexLocker locker(&m_mutex);
    const auto geo = m_viewport.screenToGeo({ x, y });
    return QPointF(geo.latitudeDeg, geo.longitudeDeg);
}

void TacticalMapQuickItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    {
        QMutexLocker locker(&m_mutex);
        m_viewport.setSize(newGeometry.width(), newGeometry.height());
    }
    update();
}

void TacticalMapQuickItem::mousePressEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }
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

void TacticalMapQuickItem::mouseMoveEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }
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

void TacticalMapQuickItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
    } else {
        QQuickPaintedItem::mouseReleaseEvent(event);
    }
}

void TacticalMapQuickItem::wheelEvent(QWheelEvent* event)
{
    if (event == nullptr) {
        return;
    }
    const double delta { (static_cast<double>(event->angleDelta().y()) / 120.0) * 0.5 };
    {
        QMutexLocker locker(&m_mutex);
        m_viewport.zoomBy(delta, { event->position().x(), event->position().y() });
    }
    emit zoomChanged();
    emit centerChanged();
    update();
    event->accept();
}

void TacticalMapQuickItem::paint(QPainter* painter)
{
    if (painter == nullptr) {
        return;
    }

    struct RenderSnapshot {
        Mapping::MapViewport viewport;
        bool showFrustum { true };
        bool hasFrustum { false };
        Klv::FrustumCorners frustum { };
        bool showHeading { true };
        double platformLat { 0.0 };
        double platformLon { 0.0 };
        double platformHeading { 0.0 };
        QColor themeColor { };
    };

    RenderSnapshot snap { Mapping::MapViewport({ 0.0, 0.0 }, 1.0, 1.0, 1.0) };
    {
        QMutexLocker locker(&m_mutex);
        snap.viewport = m_viewport;
        snap.showFrustum = m_showFrustum;
        snap.hasFrustum = m_hasFrustum;
        snap.frustum = m_frustum;
        snap.showHeading = m_showHeading;
        snap.platformLat = m_platformLat;
        snap.platformLon = m_platformLon;
        snap.platformHeading = m_platformHeading;
        snap.themeColor = m_themeColor;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 1. Draw base tactical background
    painter->fillRect(boundingRect(), QColor(14, 18, 24));

    // 2. Render visible tiles (m_tileProvider is internally thread-safe)
    const auto visibleTiles = snap.viewport.calculateVisibleTiles(1);
    constexpr auto kMaxIntSize { static_cast<std::size_t>(std::numeric_limits<int>::max()) };

    for (const auto& vt : visibleTiles) {
        const auto tileData = m_tileProvider.getTile(vt.coord);
        if (tileData && !tileData->bytes.empty() && tileData->bytes.size() <= kMaxIntSize) {
            QImage img { };
            if (img.loadFromData(tileData->bytes.data(), static_cast<int>(tileData->bytes.size()))) {
                const QRectF destRect(vt.screenRect.x, vt.screenRect.y, vt.screenRect.width, vt.screenRect.height);
                painter->drawImage(destRect, img);
            }
        }
    }

    // 3. Render tactical optical footprint frustum
    if (snap.showFrustum && snap.hasFrustum) {
        const auto sf = Mapping::TacticalOverlay::projectFrustum(snap.viewport, snap.frustum);
        QPolygonF poly { };
        for (std::size_t i { 0U }; i < 4U; ++i) {
            poly << QPointF(sf.corners[i].x, sf.corners[i].y);
        }

        // Semi-transparent glowing tactical polygon
        QColor fillColor { snap.themeColor };
        fillColor.setAlpha(45);
        painter->setBrush(QBrush(fillColor));

        const QPen borderPen(snap.themeColor, 2.0);
        painter->setPen(borderPen);
        painter->drawPolygon(poly);

        // Corner reticles
        painter->setPen(QPen(Qt::white, 1.5));
        for (std::size_t i { 0U }; i < 4U; ++i) {
            const QPointF pt(sf.corners[i].x, sf.corners[i].y);
            painter->drawLine(QPointF(pt.x() - 4.0, pt.y()), QPointF(pt.x() + 4.0, pt.y()));
            painter->drawLine(QPointF(pt.x(), pt.y() - 4.0), QPointF(pt.x(), pt.y() + 4.0));
        }
    }

    // 4. Render platform marker and heading arrow
    if (snap.showHeading) {
        const auto sv = Mapping::TacticalOverlay::projectHeadingVector(
            snap.viewport, { snap.platformLat, snap.platformLon }, snap.platformHeading, 35.0);

        const QPointF origin(sv.origin.x, sv.origin.y);
        const QPointF tip(sv.tip.x, sv.tip.y);

        // Platform circular reticle
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(snap.themeColor, 2.0));
        painter->drawEllipse(origin, 7.0, 7.0);

        painter->setPen(QPen(snap.themeColor, 1.5, Qt::SolidLine));
        painter->drawLine(origin, tip);

        // Arrow head (guarded against zero-length vector per CERT FLP32-C)
        const double dx { tip.x() - origin.x() };
        const double dy { tip.y() - origin.y() };
        if ((dx * dx + dy * dy) > 1e-12) {
            const double angle { std::atan2(dy, dx) };
            constexpr double kHeadLen { 8.0 };
            constexpr double kHeadAngle { 0.5 };

            const QPointF arrowP1(
                tip.x() - kHeadLen * std::cos(angle - kHeadAngle), tip.y() - kHeadLen * std::sin(angle - kHeadAngle));
            const QPointF arrowP2(
                tip.x() - kHeadLen * std::cos(angle + kHeadAngle), tip.y() - kHeadLen * std::sin(angle + kHeadAngle));

            QPolygonF arrowHead { };
            arrowHead << tip << arrowP1 << arrowP2;
            painter->setBrush(QBrush(snap.themeColor));
            painter->drawPolygon(arrowHead);
        }
    }
}

void registerQmlTypes()
{
    qmlRegisterType<TacticalMapQuickItem>("PelcoD.Mapping", 1, 0, "TacticalMap");
    qmlRegisterType<TacticalMapQuickItem>("PelcoD.Mapping", 1, 0, "TacticalMapQuickItem");
    qmlRegisterType<TacticalMapQuickItem>("MappingQt", 1, 0, "TacticalMap");
    qmlRegisterType<TacticalMapQuickItem>("MappingQt", 1, 0, "TacticalMapQuickItem");
}

} // namespace MappingQt
