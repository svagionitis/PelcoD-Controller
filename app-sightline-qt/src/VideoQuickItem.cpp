#include "VideoQuickItem.h"

#include <QFont>
#include <QMouseEvent>
#include <QPen>
#include <QQuickWindow>
#include <algorithm>
#include <cmath>

namespace SightlineApp {

VideoQuickItem::VideoQuickItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(true);
    setAcceptedMouseButtons(Qt::LeftButton);

    m_latencyTimer.setInterval(kLatencyPublishMs);
    connect(&m_latencyTimer, &QTimer::timeout, this, &VideoQuickItem::publishLatency);
    m_latencyTimer.start();
}

VideoQuickItem::FillMode VideoQuickItem::fillMode() const noexcept
{
    return m_fillMode;
}

void VideoQuickItem::setFillMode(FillMode mode)
{
    if (m_fillMode != mode) {
        m_fillMode = mode;
        emit fillModeChanged();
        update();
    }
}

int VideoQuickItem::videoWidth() const noexcept
{
    return m_videoWidth;
}

int VideoQuickItem::videoHeight() const noexcept
{
    return m_videoHeight;
}

bool VideoQuickItem::hasFrame() const noexcept
{
    return m_hasFrame;
}

bool VideoQuickItem::showOsdCrosshair() const noexcept
{
    return m_showOsdCrosshair;
}

void VideoQuickItem::setShowOsdCrosshair(bool show)
{
    if (m_showOsdCrosshair != show) {
        m_showOsdCrosshair = show;
        emit showOsdCrosshairChanged();
        update();
    }
}

VideoQuickItem::InteractionMode VideoQuickItem::interactionMode() const noexcept
{
    return m_interactionMode;
}

void VideoQuickItem::setInteractionMode(InteractionMode mode)
{
    if (m_interactionMode != mode) {
        m_interactionMode = mode;
        if (mode != InteractionMode::LassoAcquire && m_isLassoActive) {
            m_isLassoActive = false;
            emit lassoActiveChanged();
            update();
        }
        emit interactionModeChanged();
    }
}

bool VideoQuickItem::isLassoActive() const noexcept
{
    return m_isLassoActive;
}

QRectF VideoQuickItem::lassoRect() const noexcept
{
    return m_lassoRect;
}

int VideoQuickItem::defaultGateWidth() const noexcept
{
    return m_defaultGateWidth;
}

void VideoQuickItem::setDefaultGateWidth(int w) noexcept
{
    if (m_defaultGateWidth != w && w > 0) {
        m_defaultGateWidth = w;
        emit defaultGateSizeChanged();
    }
}

int VideoQuickItem::defaultGateHeight() const noexcept
{
    return m_defaultGateHeight;
}

void VideoQuickItem::setDefaultGateHeight(int h) noexcept
{
    if (m_defaultGateHeight != h && h > 0) {
        m_defaultGateHeight = h;
        emit defaultGateSizeChanged();
    }
}

QRectF VideoQuickItem::contentRect() const
{
    const QRectF bounds { boundingRect() };
    double frameW { 0.0 };
    double frameH { 0.0 };
    {
        QMutexLocker locker(&m_mutex);
        frameW = static_cast<double>(m_videoWidth);
        frameH = static_cast<double>(m_videoHeight);
    }

    if (frameW <= 0.0 || frameH <= 0.0 || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return bounds;
    }

    if (m_fillMode == FillMode::Stretch) {
        return bounds;
    }

    if (m_fillMode == FillMode::PreserveAspectFit) {
        const double scale { std::min(bounds.width() / frameW, bounds.height() / frameH) };
        const double targetW { frameW * scale };
        const double targetH { frameH * scale };
        const double targetX { bounds.x() + (bounds.width() - targetW) / 2.0 };
        const double targetY { bounds.y() + (bounds.height() - targetH) / 2.0 };
        return QRectF(targetX, targetY, targetW, targetH);
    }

    if (m_fillMode == FillMode::PreserveAspectCrop) {
        const double scale { std::max(bounds.width() / frameW, bounds.height() / frameH) };
        const double targetW { frameW * scale };
        const double targetH { frameH * scale };
        const double targetX { bounds.x() + (bounds.width() - targetW) / 2.0 };
        const double targetY { bounds.y() + (bounds.height() - targetH) / 2.0 };
        return QRectF(targetX, targetY, targetW, targetH);
    }

    return bounds;
}

QPointF VideoQuickItem::mapToVideo(const QPointF& itemPoint) const
{
    const QRectF cRect { contentRect() };
    double frameW { 0.0 };
    double frameH { 0.0 };
    {
        QMutexLocker locker(&m_mutex);
        frameW = static_cast<double>(m_videoWidth);
        frameH = static_cast<double>(m_videoHeight);
    }

    if (cRect.width() <= 0.0 || cRect.height() <= 0.0 || frameW <= 0.0 || frameH <= 0.0) {
        return itemPoint;
    }

    const double normX { (itemPoint.x() - cRect.x()) / cRect.width() };
    const double normY { (itemPoint.y() - cRect.y()) / cRect.height() };

    const double vX { std::clamp(normX * frameW, 0.0, frameW - 1.0) };
    const double vY { std::clamp(normY * frameH, 0.0, frameH - 1.0) };

    return QPointF(vX, vY);
}

QRectF VideoQuickItem::mapToVideoRect(const QRectF& itemRect) const
{
    const QPointF p1 { mapToVideo(itemRect.topLeft()) };
    const QPointF p2 { mapToVideo(itemRect.bottomRight()) };

    const double x { std::min(p1.x(), p2.x()) };
    const double y { std::min(p1.y(), p2.y()) };
    const double w { std::abs(p2.x() - p1.x()) };
    const double h { std::abs(p2.y() - p1.y()) };

    return QRectF(x, y, w, h);
}

QPointF VideoQuickItem::mapFromVideo(const QPointF& videoPoint) const
{
    const QRectF cRect { contentRect() };
    double frameW { 0.0 };
    double frameH { 0.0 };
    {
        QMutexLocker locker(&m_mutex);
        frameW = static_cast<double>(m_videoWidth);
        frameH = static_cast<double>(m_videoHeight);
    }

    if (frameW <= 0.0 || frameH <= 0.0) {
        return videoPoint;
    }

    const double itemX { cRect.x() + (videoPoint.x() / frameW) * cRect.width() };
    const double itemY { cRect.y() + (videoPoint.y() / frameH) * cRect.height() };

    return QPointF(itemX, itemY);
}

QRectF VideoQuickItem::mapFromVideoRect(const QRectF& videoRect) const
{
    const QPointF p1 { mapFromVideo(videoRect.topLeft()) };
    const QPointF p2 { mapFromVideo(videoRect.bottomRight()) };

    const double x { std::min(p1.x(), p2.x()) };
    const double y { std::min(p1.y(), p2.y()) };
    const double w { std::abs(p2.x() - p1.x()) };
    const double h { std::abs(p2.y() - p1.y()) };

    return QRectF(x, y, w, h);
}

void VideoQuickItem::mousePressEvent(QMouseEvent* event)
{
    if (m_interactionMode == InteractionMode::None) {
        event->ignore();
        return;
    }

    event->accept();
    if (m_interactionMode == InteractionMode::LassoAcquire) {
        m_lassoStartPoint = event->position();
        m_lassoRect = QRectF(m_lassoStartPoint, QSizeF(0.0, 0.0));
        m_isLassoActive = true;
        emit lassoActiveChanged();
        emit lassoRectChanged();
        update();
    }
}

void VideoQuickItem::mouseMoveEvent(QMouseEvent* event)
{
    if (m_interactionMode == InteractionMode::LassoAcquire && m_isLassoActive) {
        event->accept();
        const QPointF curr { event->position() };
        const double left { std::min(m_lassoStartPoint.x(), curr.x()) };
        const double top { std::min(m_lassoStartPoint.y(), curr.y()) };
        const double w { std::abs(curr.x() - m_lassoStartPoint.x()) };
        const double h { std::abs(curr.y() - m_lassoStartPoint.y()) };
        m_lassoRect = QRectF(left, top, w, h);
        emit lassoRectChanged();
        update();
        return;
    }
    event->ignore();
}

void VideoQuickItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_interactionMode == InteractionMode::ClickToTrack) {
        event->accept();
        const QPointF vPos { mapToVideo(event->position()) };
        const int col { static_cast<int>(std::round(vPos.x())) };
        const int row { static_cast<int>(std::round(vPos.y())) };
        emit targetAcquired(col, row, m_defaultGateWidth, m_defaultGateHeight);
        return;
    }

    if (m_interactionMode == InteractionMode::LassoAcquire && m_isLassoActive) {
        event->accept();
        m_isLassoActive = false;
        emit lassoActiveChanged();

        const QRectF vRect { mapToVideoRect(m_lassoRect) };
        int col { static_cast<int>(std::round(vRect.center().x())) };
        int row { static_cast<int>(std::round(vRect.center().y())) };
        int w { static_cast<int>(std::round(vRect.width())) };
        int h { static_cast<int>(std::round(vRect.height())) };

        if (w < 8 || h < 8) {
            w = m_defaultGateWidth;
            h = m_defaultGateHeight;
        }

        emit targetAcquired(col, row, w, h);
        update();
        return;
    }

    event->ignore();
}

void VideoQuickItem::updateFrame(const QImage& frame)
{
    updateTimedFrame(frame, 0);
}

void VideoQuickItem::updateTimedFrame(const QImage& frame, qint64 decodedAtNs)
{
    if (frame.isNull()) {
        return;
    }

    bool sizeChanged { false };
    bool hadFrameBefore { false };

    {
        QMutexLocker locker(&m_mutex);
        m_currentFrame = frame;
        // A newer frame supersedes an unpainted one: that frame never reached the display.
        m_pendingStampNs = (decodedAtNs > 0) ? decodedAtNs : 0;
        hadFrameBefore = m_hasFrame;
        m_hasFrame = true;

        if (m_videoWidth != frame.width() || m_videoHeight != frame.height()) {
            m_videoWidth = frame.width();
            m_videoHeight = frame.height();
            sizeChanged = true;
        }
    }

    if (!hadFrameBefore) {
        emit hasFrameChanged();
    }
    if (sizeChanged) {
        emit videoSizeChanged();
    }

    update();
}

void VideoQuickItem::notifyFrameSwapped()
{
    const std::int64_t stampNs { m_paintedStampNs.exchange(0) };
    if (stampNs > 0) {
        static_cast<void>(m_latency.addInterval(stampNs, Video::steadyNowNs()));
    }
}

void VideoQuickItem::publishLatency()
{
    const Video::LatencyStats stats { m_latency.snapshot() };
    if (stats.count != m_latencyStats.count) {
        m_latencyStats = stats;
        emit displayLatencyChanged();
    }
}

void VideoQuickItem::resetLatency()
{
    {
        QMutexLocker locker(&m_mutex);
        m_pendingStampNs = 0;
    }
    m_paintedStampNs.store(0);
    m_latency.reset();
    publishLatency();
}

double VideoQuickItem::displayLatencyMs() const noexcept
{
    return m_latencyStats.avgMs;
}

double VideoQuickItem::displayLatencyLastMs() const noexcept
{
    return m_latencyStats.lastMs;
}

double VideoQuickItem::displayLatencyMinMs() const noexcept
{
    return m_latencyStats.minMs;
}

double VideoQuickItem::displayLatencyMaxMs() const noexcept
{
    return m_latencyStats.maxMs;
}

qulonglong VideoQuickItem::presentedFrames() const noexcept
{
    return static_cast<qulonglong>(m_latencyStats.count);
}

void VideoQuickItem::itemChange(ItemChange change, const ItemChangeData& value)
{
    if (change == ItemSceneChange) {
        bindWindow(value.window);
    }
    QQuickPaintedItem::itemChange(change, value);
}

void VideoQuickItem::bindWindow(QQuickWindow* window)
{
    if (m_swapConnection) {
        static_cast<void>(disconnect(m_swapConnection));
        m_swapConnection = QMetaObject::Connection {};
    }
    if (window != nullptr) {
        // Direct: frameSwapped is emitted on the render thread right after the swap.
        m_swapConnection = connect(
            window, &QQuickWindow::frameSwapped, this, &VideoQuickItem::notifyFrameSwapped, Qt::DirectConnection);
    }
}

void VideoQuickItem::clearFrame()
{
    {
        QMutexLocker locker(&m_mutex);
        m_currentFrame = QImage {};
        m_pendingStampNs = 0;
        m_hasFrame = false;
        m_videoWidth = 0;
        m_videoHeight = 0;
    }
    m_paintedStampNs.store(0);

    emit hasFrameChanged();
    emit videoSizeChanged();
    update();
}

void VideoQuickItem::paint(QPainter* painter)
{
    if (painter == nullptr) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF bounds { boundingRect() };

    // Paint tactical canvas background
    painter->fillRect(bounds, QColor(8, 10, 15));

    QImage frameCopy {};
    bool valid { false };

    {
        QMutexLocker locker(&m_mutex);
        if (m_hasFrame && !m_currentFrame.isNull()) {
            frameCopy = m_currentFrame;
            valid = true;
            // Latch once per frame: repaints of the same frame (overlays) are not re-measured.
            if (m_pendingStampNs > 0) {
                m_paintedStampNs.store(static_cast<std::int64_t>(m_pendingStampNs));
                m_pendingStampNs = 0;
            }
        }
    }

    if (!valid) {
        renderPlaceholder(painter, bounds);
        return;
    }

    const QRectF destRect { contentRect() };
    painter->drawImage(destRect, frameCopy);

    if (m_showOsdCrosshair) {
        renderCrosshair(painter, destRect);
    }

    renderLasso(painter);
}

void VideoQuickItem::renderPlaceholder(QPainter* painter, const QRectF& bounds)
{
    const double cx { bounds.center().x() };
    const double cy { bounds.center().y() };

    painter->save();

    // Reticle concentric circles
    QPen circlePen(QColor(0, 229, 255, 35));
    circlePen.setWidth(1);
    circlePen.setStyle(Qt::DashLine);
    painter->setPen(circlePen);
    painter->setBrush(Qt::NoBrush);

    painter->drawEllipse(QPointF(cx, cy), 70, 70);
    painter->drawEllipse(QPointF(cx, cy), 130, 130);

    // Crosshairs
    QPen linePen(QColor(0, 229, 255, 50));
    painter->setPen(linePen);
    painter->drawLine(QPointF(cx - 150, cy), QPointF(cx + 150, cy));
    painter->drawLine(QPointF(cx, cy - 150), QPointF(cx, cy + 150));

    // Status message badge
    QFont font("Monospace", 10, QFont::DemiBold);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    painter->setFont(font);
    painter->setPen(QColor(140, 150, 170));
    const QString msg { tr("NO SENSOR FEED // AWAITING STREAM INPUT") };
    painter->drawText(QRectF(cx - 220, cy + 25, 440, 26), Qt::AlignCenter, msg);

    QFont subFont("Segoe UI", 8);
    painter->setFont(subFont);
    painter->setPen(QColor(90, 100, 120));
    const QString subMsg { tr("Configure RTSP/UDP address or enable Synthetic Test Feed") };
    painter->drawText(QRectF(cx - 250, cy + 50, 500, 22), Qt::AlignCenter, subMsg);

    painter->restore();
}

void VideoQuickItem::renderCrosshair(QPainter* painter, const QRectF& bounds)
{
    painter->save();
    const double cx { bounds.center().x() };
    const double cy { bounds.center().y() };

    QPen pen(QColor(0, 229, 255, 180));
    pen.setWidth(1);
    painter->setPen(pen);

    constexpr double arm { 22.0 };
    constexpr double gap { 6.0 };

    // Center crosshair with center gap
    painter->drawLine(QPointF(cx - arm, cy), QPointF(cx - gap, cy));
    painter->drawLine(QPointF(cx + gap, cy), QPointF(cx + arm, cy));
    painter->drawLine(QPointF(cx, cy - arm), QPointF(cx, cy - gap));
    painter->drawLine(QPointF(cx, cy + gap), QPointF(cx, cy + arm));

    // Corner brackets
    constexpr double cornerLen { 20.0 };
    const double left { bounds.left() + 16.0 };
    const double right { bounds.right() - 16.0 };
    const double top { bounds.top() + 16.0 };
    const double bottom { bounds.bottom() - 16.0 };

    // Top-left
    painter->drawLine(QPointF(left, top), QPointF(left + cornerLen, top));
    painter->drawLine(QPointF(left, top), QPointF(left, top + cornerLen));

    // Top-right
    painter->drawLine(QPointF(right, top), QPointF(right - cornerLen, top));
    painter->drawLine(QPointF(right, top), QPointF(right, top + cornerLen));

    // Bottom-left
    painter->drawLine(QPointF(left, bottom), QPointF(left + cornerLen, bottom));
    painter->drawLine(QPointF(left, bottom), QPointF(left, bottom - cornerLen));

    // Bottom-right
    painter->drawLine(QPointF(right, bottom), QPointF(right - cornerLen, bottom));
    painter->drawLine(QPointF(right, bottom), QPointF(right, bottom - cornerLen));

    painter->restore();
}

void VideoQuickItem::renderLasso(QPainter* painter)
{
    if (!m_isLassoActive || m_lassoRect.width() < 2.0 || m_lassoRect.height() < 2.0) {
        return;
    }

    painter->save();

    const QColor cyan(0, 229, 255);
    QPen dashedPen(cyan, 1.5, Qt::DashLine);
    painter->setPen(dashedPen);
    painter->setBrush(QColor(0, 229, 255, 30));
    painter->drawRect(m_lassoRect);

    // Corner brackets (length 8px)
    QPen solidPen(cyan, 2.0, Qt::SolidLine);
    painter->setPen(solidPen);
    const double L { std::min({ 8.0, m_lassoRect.width() / 2.0, m_lassoRect.height() / 2.0 }) };

    // Top-Left
    painter->drawLine(
        QPointF(m_lassoRect.left(), m_lassoRect.top()), QPointF(m_lassoRect.left() + L, m_lassoRect.top()));
    painter->drawLine(
        QPointF(m_lassoRect.left(), m_lassoRect.top()), QPointF(m_lassoRect.left(), m_lassoRect.top() + L));

    // Top-Right
    painter->drawLine(
        QPointF(m_lassoRect.right(), m_lassoRect.top()), QPointF(m_lassoRect.right() - L, m_lassoRect.top()));
    painter->drawLine(
        QPointF(m_lassoRect.right(), m_lassoRect.top()), QPointF(m_lassoRect.right(), m_lassoRect.top() + L));

    // Bottom-Left
    painter->drawLine(
        QPointF(m_lassoRect.left(), m_lassoRect.bottom()), QPointF(m_lassoRect.left() + L, m_lassoRect.bottom()));
    painter->drawLine(
        QPointF(m_lassoRect.left(), m_lassoRect.bottom()), QPointF(m_lassoRect.left(), m_lassoRect.bottom() - L));

    // Bottom-Right
    painter->drawLine(
        QPointF(m_lassoRect.right(), m_lassoRect.bottom()), QPointF(m_lassoRect.right() - L, m_lassoRect.bottom()));
    painter->drawLine(
        QPointF(m_lassoRect.right(), m_lassoRect.bottom()), QPointF(m_lassoRect.right(), m_lassoRect.bottom() - L));

    // Center crosshair
    const QPointF center { m_lassoRect.center() };
    painter->drawLine(QPointF(center.x() - 4.0, center.y()), QPointF(center.x() + 4.0, center.y()));
    painter->drawLine(QPointF(center.x(), center.y() - 4.0), QPointF(center.x(), center.y() + 4.0));

    // Dimension banner
    const QRectF vRect { mapToVideoRect(m_lassoRect) };
    const QString dimText { QStringLiteral("%1 x %2")
                                .arg(static_cast<int>(std::round(vRect.width())))
                                .arg(static_cast<int>(std::round(vRect.height()))) };

    QFont font { painter->font() };
    font.setPixelSize(10);
    font.setBold(true);
    painter->setFont(font);

    const QRectF bannerRect(m_lassoRect.left(), m_lassoRect.top() - 16.0, 70.0, 14.0);
    painter->fillRect(bannerRect, QColor(8, 10, 15, 200));
    painter->setPen(cyan);
    painter->drawText(bannerRect, Qt::AlignCenter, dimText);

    painter->restore();
}

} // namespace SightlineApp
