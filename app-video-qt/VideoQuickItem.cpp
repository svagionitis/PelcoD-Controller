/// @file VideoQuickItem.cpp
/// @brief Implementation of custom QQuickPaintedItem for rendering video frames in QML scenes.

#include "VideoQuickItem.h"

#include <QFont>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace VideoApp {

VideoQuickItem::VideoQuickItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(true);
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

void VideoQuickItem::updateFrame(const QImage& frame)
{
    if (frame.isNull()) {
        return;
    }

    bool sizeChanged = false;
    bool hadFrameBefore = false;

    {
        QMutexLocker locker(&m_mutex);
        m_currentFrame = frame;
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

void VideoQuickItem::clearFrame()
{
    {
        QMutexLocker locker(&m_mutex);
        m_currentFrame = QImage();
        m_hasFrame = false;
        m_videoWidth = 0;
        m_videoHeight = 0;
    }

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

    const QRectF bounds = boundingRect();

    // Paint dark tactical canvas background
    painter->fillRect(bounds, QColor(16, 18, 22));

    QImage frameCopy;
    bool valid = false;

    {
        QMutexLocker locker(&m_mutex);
        if (m_hasFrame && !m_currentFrame.isNull()) {
            frameCopy = m_currentFrame;
            valid = true;
        }
    }

    if (!valid) {
        renderPlaceholder(painter, bounds);
        return;
    }

    QRectF destRect = bounds;
    const double frameW = static_cast<double>(frameCopy.width());
    const double frameH = static_cast<double>(frameCopy.height());

    if (m_fillMode == FillMode::PreserveAspectFit && frameW > 0.0 && frameH > 0.0) {
        const double scale = std::min(bounds.width() / frameW, bounds.height() / frameH);
        const double targetW = frameW * scale;
        const double targetH = frameH * scale;
        const double targetX = bounds.x() + (bounds.width() - targetW) / 2.0;
        const double targetY = bounds.y() + (bounds.height() - targetH) / 2.0;
        destRect = QRectF(targetX, targetY, targetW, targetH);
    } else if (m_fillMode == FillMode::PreserveAspectCrop && frameW > 0.0 && frameH > 0.0) {
        const double scale = std::max(bounds.width() / frameW, bounds.height() / frameH);
        const double targetW = frameW * scale;
        const double targetH = frameH * scale;
        const double targetX = bounds.x() + (bounds.width() - targetW) / 2.0;
        const double targetY = bounds.y() + (bounds.height() - targetH) / 2.0;
        destRect = QRectF(targetX, targetY, targetW, targetH);
    }

    painter->drawImage(destRect, frameCopy);

    if (m_showOsdCrosshair) {
        renderCrosshair(painter, destRect);
    }
}

void VideoQuickItem::renderPlaceholder(QPainter* painter, const QRectF& bounds)
{
    const double cx = bounds.center().x();
    const double cy = bounds.center().y();

    // Subtle tactical reticle circles
    painter->save();
    QPen circlePen(QColor(0, 229, 255, 30));
    circlePen.setWidth(1);
    circlePen.setStyle(Qt::DashLine);
    painter->setPen(circlePen);
    painter->setBrush(Qt::NoBrush);

    painter->drawEllipse(QPointF(cx, cy), 80, 80);
    painter->drawEllipse(QPointF(cx, cy), 140, 140);

    // Crosshairs
    QPen linePen(QColor(0, 229, 255, 45));
    painter->setPen(linePen);
    painter->drawLine(QPointF(cx - 160, cy), QPointF(cx + 160, cy));
    painter->drawLine(QPointF(cx, cy - 160), QPointF(cx, cy + 160));

    // Status message badge
    QFont font("Segoe UI", 10, QFont::DemiBold);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    painter->setFont(font);
    painter->setPen(QColor(139, 148, 158));
    const QString msg = tr("NO SIGNAL // AWAITING STREAM INPUT");
    painter->drawText(QRectF(cx - 200, cy + 30, 400, 30), Qt::AlignCenter, msg);

    QFont subFont("Segoe UI", 8);
    painter->setFont(subFont);
    painter->setPen(QColor(90, 100, 115));
    const QString subMsg = tr("Select stream, file, or hardware capture device to begin playback");
    painter->drawText(QRectF(cx - 250, cy + 55, 500, 25), Qt::AlignCenter, subMsg);

    painter->restore();
}

void VideoQuickItem::renderCrosshair(QPainter* painter, const QRectF& bounds)
{
    painter->save();
    const double cx = bounds.center().x();
    const double cy = bounds.center().y();

    QPen pen(QColor(0, 229, 255, 160));
    pen.setWidth(1);
    painter->setPen(pen);

    constexpr double arm = 20.0;
    constexpr double gap = 6.0;

    // Center crosshair with center gap
    painter->drawLine(QPointF(cx - arm, cy), QPointF(cx - gap, cy));
    painter->drawLine(QPointF(cx + gap, cy), QPointF(cx + arm, cy));
    painter->drawLine(QPointF(cx, cy - arm), QPointF(cx, cy - gap));
    painter->drawLine(QPointF(cx, cy + gap), QPointF(cx, cy + arm));

    // Corner brackets
    constexpr double cornerLen = 24.0;
    const double left = bounds.left() + 20.0;
    const double right = bounds.right() - 20.0;
    const double top = bounds.top() + 20.0;
    const double bottom = bounds.bottom() - 20.0;

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

} // namespace VideoApp
