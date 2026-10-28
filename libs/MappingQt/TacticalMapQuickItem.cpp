/// @file TacticalMapQuickItem.cpp
/// @brief Implementation of TacticalMapQuickItem for interactive tactical 2D map display in QML.

#include "TacticalMapQuickItem.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include <QBrush>
#include <QColor>
#include <QImage>
#include <QMouseEvent>
#include <QMutexLocker>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QWheelEvent>
#include <qqml.h>

#include "DiskTileCache.h"
#include "GeoTypes.h"
#include "KlvTypes.h"
#include "MercatorProjection.h"
#include "TacticalOverlay.h"
#include "VmtiOverlay.h"
#include "VmtiTypes.h"

namespace MappingQt {

namespace {

    constexpr double kCoordEpsilon { 1e-9 };
    constexpr double kClickDeadbandSq { 16.0 }; // 4.0 px Euclidean click-vs-drag radius

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

    [[nodiscard]] bool nearlyEqualPoint(const Klv::GeoPoint2D& a, const Klv::GeoPoint2D& b) noexcept
    {
        return nearlyEqual(a.latitudeDeg, b.latitudeDeg) && nearlyEqual(a.longitudeDeg, b.longitudeDeg);
    }

    [[nodiscard]] bool nearlyEqualFrustum(const Klv::FrustumCorners& a, const Klv::FrustumCorners& b) noexcept
    {
        return nearlyEqualPoint(a.topLeft, b.topLeft) && nearlyEqualPoint(a.topRight, b.topRight)
            && nearlyEqualPoint(a.bottomRight, b.bottomRight) && nearlyEqualPoint(a.bottomLeft, b.bottomLeft);
    }

} // namespace

TacticalMapQuickItem::TacticalMapQuickItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , m_viewport({ 37.9838, 23.7275 }, 12.0, 800.0, 600.0)
    , m_diskCache { nullptr }
    , m_tileProvider { m_diskCache }
{
    m_tileProvider.setProceduralFallbackEnabled(m_showGrid);
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
        clearDecodedTileCacheLocked();
    }
    emit offlineOnlyChanged();
    update();
}

void TacticalMapQuickItem::setOfflineDirectory(const QString& directoryPath)
{
    const QString trimmed { directoryPath.trimmed() };
    {
        QMutexLocker locker(&m_mutex);
        if (trimmed.isEmpty()) {
            m_diskCache.reset();
            m_tileProvider.setDiskCache(nullptr);
        } else {
            m_diskCache = std::make_shared<Mapping::DiskTileCache>(trimmed.toStdString());
            m_tileProvider.setDiskCache(m_diskCache);
        }
        m_tileProvider.memoryCache().clear();
        clearDecodedTileCacheLocked();
    }
    update();
}

void TacticalMapQuickItem::addOfflineSearchDirectory(const QString& directoryPath)
{
    const QString trimmed { directoryPath.trimmed() };
    if (trimmed.isEmpty()) {
        return;
    }
    {
        QMutexLocker locker(&m_mutex);
        if (m_diskCache != nullptr) {
            m_diskCache->addSearchDirectory(trimmed.toStdString());
        } else {
            auto secondaryCache = std::make_shared<Mapping::DiskTileCache>(trimmed.toStdString());
            m_tileProvider.addOfflineProvider(secondaryCache);
        }
        m_tileProvider.memoryCache().clear();
        clearDecodedTileCacheLocked();
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
        m_tileProvider.setProceduralFallbackEnabled(show);
        m_tileProvider.memoryCache().clear();
        clearDecodedTileCacheLocked();
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

bool TacticalMapQuickItem::hasTarget() const
{
    QMutexLocker locker(&m_mutex);
    return m_hasTarget;
}

double TacticalMapQuickItem::targetLatitude() const
{
    QMutexLocker locker(&m_mutex);
    return m_targetLat;
}

double TacticalMapQuickItem::targetLongitude() const
{
    QMutexLocker locker(&m_mutex);
    return m_targetLon;
}

void TacticalMapQuickItem::setTargetPosition(double lat, double lon)
{
    if (!std::isfinite(lat) || !std::isfinite(lon)) {
        return;
    }
    const double clampedLat { Mapping::MercatorProjection::clampLatitude(lat) };
    const double normLon { Mapping::MercatorProjection::normalizeLongitude(lon) };
    {
        QMutexLocker locker(&m_mutex);
        if (m_hasTarget && nearlyEqual(m_targetLat, clampedLat) && nearlyEqual(m_targetLon, normLon)) {
            return;
        }
        m_targetLat = clampedLat;
        m_targetLon = normLon;
        m_hasTarget = true;
    }
    emit targetChanged();
    update();
}

void TacticalMapQuickItem::clearTarget()
{
    {
        QMutexLocker locker(&m_mutex);
        if (!m_hasTarget) {
            return;
        }
        m_hasTarget = false;
    }
    emit targetChanged();
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
        if (m_hasFrustum && nearlyEqualFrustum(m_frustum, sanitized)) {
            return;
        }
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

void TacticalMapQuickItem::setVmtiTargets(const Klv::VmtiLocalSet& vmti)
{
    {
        QMutexLocker locker(&m_mutex);
        m_vmtiSet = vmti;
        m_hasVmti = !vmti.targets.empty();
    }
    update();
}

void TacticalMapQuickItem::clearVmtiTargets()
{
    {
        QMutexLocker locker(&m_mutex);
        if (!m_hasVmti && m_vmtiSet.targets.empty()) {
            return;
        }
        m_vmtiSet.targets.clear();
        m_hasVmti = false;
    }
    update();
}

std::size_t TacticalMapQuickItem::vmtiTargetCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_vmtiSet.targets.size();
}

std::size_t TacticalMapQuickItem::decodedTileCacheSize() const
{
    QMutexLocker locker(&m_mutex);
    return m_decodedTileCache.size();
}

void TacticalMapQuickItem::clearDecodedTileCacheLocked()
{
    m_decodedTileCache.clear();
    m_decodedTileOrder.clear();
}

QImage TacticalMapQuickItem::findOrDecodeTile(const Mapping::TileCoord& coord)
{
    {
        QMutexLocker locker(&m_mutex);
        const auto it = m_decodedTileCache.find(coord);
        if (it != m_decodedTileCache.end()) {
            return it->second;
        }
    }

    constexpr auto kMaxIntSize { static_cast<std::size_t>(std::numeric_limits<int>::max()) };
    const auto tileData = m_tileProvider.getTile(coord);
    if (!tileData || tileData->bytes.empty() || tileData->bytes.size() > kMaxIntSize) {
        return QImage { };
    }

    QImage decoded { };
    if (!decoded.loadFromData(tileData->bytes.data(), static_cast<int>(tileData->bytes.size()))) {
        return QImage { };
    }

    {
        QMutexLocker locker(&m_mutex);
        if (m_decodedTileCache.find(coord) == m_decodedTileCache.end()) {
            if (m_decodedTileOrder.size() >= kMaxDecodedTiles) {
                const Mapping::TileCoord oldest { m_decodedTileOrder.front() };
                m_decodedTileOrder.pop_front();
                m_decodedTileCache.erase(oldest);
            }
            m_decodedTileOrder.push_back(coord);
            m_decodedTileCache.emplace(coord, decoded);
        }
    }
    return decoded;
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
        m_dragExceededThreshold = false;
        m_pressMousePos = event->position();
        m_lastMousePos = event->position();
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
        const QPointF totalDelta { event->position() - m_pressMousePos };
        if ((totalDelta.x() * totalDelta.x() + totalDelta.y() * totalDelta.y()) > kClickDeadbandSq) {
            m_dragExceededThreshold = true;
        }

        const QPointF stepDelta { event->position() - m_lastMousePos };
        m_lastMousePos = event->position();

        if (!nearlyEqual(stepDelta.x(), 0.0) || !nearlyEqual(stepDelta.y(), 0.0)) {
            {
                QMutexLocker locker(&m_mutex);
                m_viewport.pan(stepDelta.x(), stepDelta.y());
            }
            emit centerChanged();
            update();
        }
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
        const bool wasDragging { m_isDragging };
        const QPointF totalDelta { event->position() - m_pressMousePos };
        const bool withinClickDeadband { !m_dragExceededThreshold
            && ((totalDelta.x() * totalDelta.x() + totalDelta.y() * totalDelta.y()) <= kClickDeadbandSq) };

        m_isDragging = false;
        m_dragExceededThreshold = false;

        if (wasDragging && withinClickDeadband) {
            const QPointF geo { screenToGeo(event->position().x(), event->position().y()) };
            emit coordinateClicked(geo.x(), geo.y());
        }
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
        bool showGrid { true };
        bool showFrustum { true };
        bool hasFrustum { false };
        Klv::FrustumCorners frustum { };
        bool showHeading { true };
        double platformLat { 0.0 };
        double platformLon { 0.0 };
        double platformHeading { 0.0 };
        bool hasTarget { false };
        double targetLat { 0.0 };
        double targetLon { 0.0 };
        bool hasVmti { false };
        Klv::VmtiLocalSet vmtiSet { };
        QColor themeColor { };
    };

    RenderSnapshot snap { Mapping::MapViewport({ 0.0, 0.0 }, 1.0, 1.0, 1.0) };
    {
        QMutexLocker locker(&m_mutex);
        snap.viewport = m_viewport;
        snap.showGrid = m_showGrid;
        snap.showFrustum = m_showFrustum;
        snap.hasFrustum = m_hasFrustum;
        snap.frustum = m_frustum;
        snap.showHeading = m_showHeading;
        snap.platformLat = m_platformLat;
        snap.platformLon = m_platformLon;
        snap.platformHeading = m_platformHeading;
        snap.hasTarget = m_hasTarget;
        snap.targetLat = m_targetLat;
        snap.targetLon = m_targetLon;
        snap.hasVmti = m_hasVmti;
        snap.vmtiSet = m_vmtiSet;
        snap.themeColor = m_themeColor;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 1. Draw base tactical background
    painter->fillRect(boundingRect(), QColor(14, 18, 24));

    // 2. Render visible tiles via bounded decoded QImage cache
    const auto visibleTiles = snap.viewport.calculateVisibleTiles(1);
    for (const auto& vt : visibleTiles) {
        const QImage img { findOrDecodeTile(vt.coord) };
        if (!img.isNull()) {
            const QRectF destRect(vt.screenRect.x, vt.screenRect.y, vt.screenRect.width, vt.screenRect.height);
            painter->drawImage(destRect, img);
        }
    }

    const Mapping::ScreenRect viewportRect { 0.0, 0.0, snap.viewport.width(), snap.viewport.height() };

    // 3. Render subtle tactical coordinate grid lines when showGrid is enabled
    if (snap.showGrid) {
        painter->setPen(QPen(QColor(30, 42, 56, 120), 1.0));
        constexpr double kGridStepPx { 64.0 };
        for (double gx { kGridStepPx }; gx < snap.viewport.width(); gx += kGridStepPx) {
            painter->drawLine(QPointF(gx, 0.0), QPointF(gx, snap.viewport.height()));
        }
        for (double gy { kGridStepPx }; gy < snap.viewport.height(); gy += kGridStepPx) {
            painter->drawLine(QPointF(0.0, gy), QPointF(snap.viewport.width(), gy));
        }
    }

    // 4. Render culled tactical optical footprint frustum
    if (snap.showFrustum && snap.hasFrustum) {
        const auto sf = Mapping::TacticalOverlay::projectFrustum(snap.viewport, snap.frustum);
        if (sf.valid && Mapping::TacticalOverlay::isFrustumVisible(sf, viewportRect)) {
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
    }

    // 5. Render target line-of-sight and target reticle
    if (snap.hasTarget) {
        const auto los = Mapping::TacticalOverlay::projectLineOfSight(
            snap.viewport, { snap.platformLat, snap.platformLon }, { snap.targetLat, snap.targetLon });
        if (los.valid) {
            const QPointF losOrigin(los.origin.x, los.origin.y);
            const QPointF losTip(los.tip.x, los.tip.y);

            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(snap.themeColor, 1.5, Qt::DashLine));
            painter->drawLine(losOrigin, losTip);

            painter->setPen(QPen(Qt::white, 1.5, Qt::SolidLine));
            painter->drawLine(QPointF(losTip.x() - 6.0, losTip.y()), QPointF(losTip.x() + 6.0, losTip.y()));
            painter->drawLine(QPointF(losTip.x(), losTip.y() - 6.0), QPointF(losTip.x(), losTip.y() + 6.0));
        }
    }

    // 6. Render MISB ST 0903 VMTI moving targets
    if (snap.hasVmti) {
        const std::optional<Klv::GeoPoint2D> frameCenter
            = snap.hasTarget ? std::optional<Klv::GeoPoint2D>({ snap.targetLat, snap.targetLon }) : std::nullopt;
        const auto projectedTargets
            = Mapping::VmtiOverlay::projectTargetSeries(snap.viewport, snap.vmtiSet, frameCenter);

        for (const auto& trk : projectedTargets) {
            if (!Mapping::VmtiOverlay::isTargetVisible(trk, viewportRect)) {
                continue;
            }
            const QPointF pos(trk.screenPos.x, trk.screenPos.y);
            const QColor trkColor = trk.isMoving ? QColor(255, 145, 0) : snap.themeColor;

            if (trk.ce90Pixels >= 2.0) {
                QColor ceFill { trkColor };
                ceFill.setAlpha(35);
                painter->setBrush(QBrush(ceFill));
                painter->setPen(QPen(trkColor, 1.0, Qt::DotLine));
                painter->drawEllipse(pos, trk.ce90Pixels, trk.ce90Pixels);
            }

            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(trkColor, 1.5, Qt::SolidLine));
            painter->drawRect(QRectF(pos.x() - 5.0, pos.y() - 5.0, 10.0, 10.0));

            if (!trk.callsign.empty()) {
                painter->drawText(QPointF(pos.x() + 8.0, pos.y() - 4.0), QString::fromStdString(trk.callsign));
            }
        }
    }

    // 7. Render platform marker and heading arrow
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

void TacticalMapQuickItem::registerQmlTypes()
{
    MappingQt::registerQmlTypes();
}

void registerQmlTypes()
{
    qmlRegisterType<TacticalMapQuickItem>("PelcoD.Mapping", 1, 0, "TacticalMap");
    qmlRegisterType<TacticalMapQuickItem>("PelcoD.Mapping", 1, 0, "TacticalMapQuickItem");
    qmlRegisterType<TacticalMapQuickItem>("MappingQt", 1, 0, "TacticalMap");
    qmlRegisterType<TacticalMapQuickItem>("MappingQt", 1, 0, "TacticalMapQuickItem");
}

} // namespace MappingQt
