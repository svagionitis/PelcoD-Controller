/// @file LensVideoQuickItem.cpp
/// @brief Custom QQuickPaintedItem presenting video viewport, reticles, and optical ROI.
#include "LensVideoQuickItem.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cmath>

#include <QCursor>
#include <QFont>
#include <QHoverEvent>
#include <QPainter>
#include <QPen>

#include "AtomicTripleBuffer.h"
#include "LensFramePacket.h"
#include "LensPlayerController.h"

namespace LensApp {

namespace {

    constexpr qreal kCornerHandleRadius = 8.0;
    constexpr int kPeakingThreshold = 80;

} // namespace

LensVideoQuickItem::LensVideoQuickItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
}

bool LensVideoQuickItem::showPeaking() const noexcept
{
    return m_showPeaking;
}

bool LensVideoQuickItem::showGrid() const noexcept
{
    return m_showGrid;
}

bool LensVideoQuickItem::showReticle() const noexcept
{
    return m_showReticle;
}

bool LensVideoQuickItem::showRoi() const noexcept
{
    return m_showRoi;
}

QRectF LensVideoQuickItem::roiRect() const noexcept
{
    return m_roiRect;
}

LensPlayerController* LensVideoQuickItem::controller() const noexcept
{
    return m_controller;
}

void LensVideoQuickItem::setShowPeaking(bool show)
{
    if (m_showPeaking != show) {
        m_showPeaking = show;
        emit showPeakingChanged();
        update();
    }
}

void LensVideoQuickItem::setShowGrid(bool show)
{
    if (m_showGrid != show) {
        m_showGrid = show;
        emit showGridChanged();
        update();
    }
}

void LensVideoQuickItem::setShowReticle(bool show)
{
    if (m_showReticle != show) {
        m_showReticle = show;
        emit showReticleChanged();
        update();
    }
}

void LensVideoQuickItem::setShowRoi(bool show)
{
    if (m_showRoi != show) {
        m_showRoi = show;
        emit showRoiChanged();
        update();
    }
}

void LensVideoQuickItem::setRoiRect(const QRectF& rect)
{
    const double cx = std::clamp(rect.x(), 0.0, 0.95);
    const double cy = std::clamp(rect.y(), 0.0, 0.95);
    const double cw = std::clamp(rect.width(), 0.05, 1.0 - cx);
    const double ch = std::clamp(rect.height(), 0.05, 1.0 - cy);
    const QRectF clamped(cx, cy, cw, ch);

    if (m_roiRect != clamped) {
        m_roiRect = clamped;
        emit roiRectChanged();
        update();
    }
}

void LensVideoQuickItem::setController(LensPlayerController* ctrl)
{
    if (m_controller != ctrl) {
        if (m_controller != nullptr) {
            disconnect(m_controller, &LensPlayerController::frameReady, this, &LensVideoQuickItem::onFrameReady);
        }

        m_controller = ctrl;
        if (m_controller != nullptr) {
            connect(m_controller, &LensPlayerController::frameReady, this, &LensVideoQuickItem::onFrameReady);
        }

        emit controllerChanged();
        update();
    }
}

void LensVideoQuickItem::onFrameReady()
{
    update();
}

QRectF LensVideoQuickItem::calculateAspectFit(const QRectF& bounds, qreal videoW, qreal videoH) const noexcept
{
    if ((videoW <= 0.0) || (videoH <= 0.0) || (bounds.width() <= 0.0) || (bounds.height() <= 0.0)) {
        return bounds;
    }

    const qreal scale = std::min(bounds.width() / videoW, bounds.height() / videoH);
    const qreal dw = videoW * scale;
    const qreal dh = videoH * scale;
    const qreal dx = bounds.x() + ((bounds.width() - dw) * 0.5);
    const qreal dy = bounds.y() + ((bounds.height() - dh) * 0.5);

    return QRectF(dx, dy, dw, dh);
}

LensVideoQuickItem::DragHandle LensVideoQuickItem::hitTestHandle(
    const QPointF& pos, const QRectF& screenRoi) const noexcept
{
    constexpr qreal r = kCornerHandleRadius + 4.0;

    // Corner handle hit test
    if (QPointF(pos - screenRoi.topLeft()).manhattanLength() <= r * 2.0) {
        return DragHandle::TopLeft;
    }
    if (QPointF(pos - screenRoi.topRight()).manhattanLength() <= r * 2.0) {
        return DragHandle::TopRight;
    }
    if (QPointF(pos - screenRoi.bottomLeft()).manhattanLength() <= r * 2.0) {
        return DragHandle::BottomLeft;
    }
    if (QPointF(pos - screenRoi.bottomRight()).manhattanLength() <= r * 2.0) {
        return DragHandle::BottomRight;
    }

    if (screenRoi.contains(pos)) {
        return DragHandle::InsideRoi;
    }

    return DragHandle::None;
}

void LensVideoQuickItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QQuickPaintedItem::mousePressEvent(event);
        return;
    }

    const QRectF bounds(0.0, 0.0, width(), height());
    qreal vw = 1280.0;
    qreal vh = 720.0;
    if (m_controller != nullptr) {
        const auto& pkt = m_controller->tripleBuffer().getReadBuffer();
        if ((pkt.width > 0U) && (pkt.height > 0U)) {
            vw = static_cast<qreal>(pkt.width);
            vh = static_cast<qreal>(pkt.height);
        }
    }

    const QRectF videoRect = calculateAspectFit(bounds, vw, vh);
    const QRectF screenRoi(videoRect.x() + (m_roiRect.x() * videoRect.width()),
        videoRect.y() + (m_roiRect.y() * videoRect.height()), m_roiRect.width() * videoRect.width(),
        m_roiRect.height() * videoRect.height());

    m_activeHandle = hitTestHandle(event->position(), screenRoi);
    if (m_activeHandle != DragHandle::None) {
        m_dragStartPos = event->position();
        m_initialRoiRect = m_roiRect;
        event->accept();
        return;
    }

    QQuickPaintedItem::mousePressEvent(event);
}

void LensVideoQuickItem::mouseMoveEvent(QMouseEvent* event)
{
    if (m_activeHandle == DragHandle::None) {
        QQuickPaintedItem::mouseMoveEvent(event);
        return;
    }

    const QRectF bounds(0.0, 0.0, width(), height());
    qreal vw = 1280.0;
    qreal vh = 720.0;
    if (m_controller != nullptr) {
        const auto& pkt = m_controller->tripleBuffer().getReadBuffer();
        if ((pkt.width > 0U) && (pkt.height > 0U)) {
            vw = static_cast<qreal>(pkt.width);
            vh = static_cast<qreal>(pkt.height);
        }
    }

    const QRectF videoRect = calculateAspectFit(bounds, vw, vh);
    const qreal invW = (videoRect.width() > 0.0) ? (1.0 / videoRect.width()) : 1.0;
    const qreal invH = (videoRect.height() > 0.0) ? (1.0 / videoRect.height()) : 1.0;

    const qreal dxNorm = (event->position().x() - m_dragStartPos.x()) * invW;
    const qreal dyNorm = (event->position().y() - m_dragStartPos.y()) * invH;

    QRectF candidate = m_initialRoiRect;

    switch (m_activeHandle) {
    case DragHandle::InsideRoi:
        candidate.moveLeft(std::clamp(m_initialRoiRect.x() + dxNorm, 0.0, 1.0 - m_initialRoiRect.width()));
        candidate.moveTop(std::clamp(m_initialRoiRect.y() + dyNorm, 0.0, 1.0 - m_initialRoiRect.height()));
        break;
    case DragHandle::TopLeft: {
        const qreal newX = std::clamp(m_initialRoiRect.x() + dxNorm, 0.0, m_initialRoiRect.right() - 0.05);
        const qreal newY = std::clamp(m_initialRoiRect.y() + dyNorm, 0.0, m_initialRoiRect.bottom() - 0.05);
        candidate.setCoords(newX, newY, m_initialRoiRect.right(), m_initialRoiRect.bottom());
        break;
    }
    case DragHandle::TopRight: {
        const qreal newR = std::clamp(m_initialRoiRect.right() + dxNorm, m_initialRoiRect.left() + 0.05, 1.0);
        const qreal newY = std::clamp(m_initialRoiRect.y() + dyNorm, 0.0, m_initialRoiRect.bottom() - 0.05);
        candidate.setCoords(m_initialRoiRect.left(), newY, newR, m_initialRoiRect.bottom());
        break;
    }
    case DragHandle::BottomLeft: {
        const qreal newX = std::clamp(m_initialRoiRect.x() + dxNorm, 0.0, m_initialRoiRect.right() - 0.05);
        const qreal newB = std::clamp(m_initialRoiRect.bottom() + dyNorm, m_initialRoiRect.top() + 0.05, 1.0);
        candidate.setCoords(newX, m_initialRoiRect.top(), m_initialRoiRect.right(), newB);
        break;
    }
    case DragHandle::BottomRight: {
        const qreal newR = std::clamp(m_initialRoiRect.right() + dxNorm, m_initialRoiRect.left() + 0.05, 1.0);
        const qreal newB = std::clamp(m_initialRoiRect.bottom() + dyNorm, m_initialRoiRect.top() + 0.05, 1.0);
        candidate.setCoords(m_initialRoiRect.left(), m_initialRoiRect.top(), newR, newB);
        break;
    }
    default:
        break;
    }

    setRoiRect(candidate);
    if (m_controller != nullptr) {
        m_controller->setRoi(candidate.x(), candidate.y(), candidate.width(), candidate.height());
    }

    event->accept();
}

void LensVideoQuickItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_activeHandle != DragHandle::None) {
        m_activeHandle = DragHandle::None;
        emit roiMoved(m_roiRect.x(), m_roiRect.y(), m_roiRect.width(), m_roiRect.height());
        event->accept();
        return;
    }

    QQuickPaintedItem::mouseReleaseEvent(event);
}

void LensVideoQuickItem::hoverMoveEvent(QHoverEvent* event)
{
    const QRectF bounds(0.0, 0.0, width(), height());
    qreal vw = 1280.0;
    qreal vh = 720.0;
    if (m_controller != nullptr) {
        const auto& pkt = m_controller->tripleBuffer().getReadBuffer();
        if ((pkt.width > 0U) && (pkt.height > 0U)) {
            vw = static_cast<qreal>(pkt.width);
            vh = static_cast<qreal>(pkt.height);
        }
    }

    const QRectF videoRect = calculateAspectFit(bounds, vw, vh);
    const QRectF screenRoi(videoRect.x() + (m_roiRect.x() * videoRect.width()),
        videoRect.y() + (m_roiRect.y() * videoRect.height()), m_roiRect.width() * videoRect.width(),
        m_roiRect.height() * videoRect.height());

    const DragHandle handle = hitTestHandle(event->position(), screenRoi);
    switch (handle) {
    case DragHandle::TopLeft:
    case DragHandle::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case DragHandle::TopRight:
    case DragHandle::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case DragHandle::InsideRoi:
        setCursor(Qt::PointingHandCursor);
        break;
    default:
        setCursor(Qt::ArrowCursor);
        break;
    }

    QQuickPaintedItem::hoverMoveEvent(event);
}

void LensVideoQuickItem::paint(QPainter* painter)
{
    if (painter == nullptr) {
        return;
    }

    const QRectF bounds(0.0, 0.0, width(), height());
    painter->fillRect(bounds, QColor(0x0a, 0x0c, 0x10));

    if (m_controller == nullptr) {
        return;
    }

    // Attempt lock-free frame swap from background thread
    static_cast<void>(m_controller->tripleBuffer().swapReadBuffer());
    const auto& packet = m_controller->tripleBuffer().getReadBuffer();

    if ((packet.width == 0U) || (packet.height == 0U) || packet.rgbBuffer.empty()) {
        drawGrid(painter, bounds);
        drawReticle(painter, QPointF(bounds.width() * 0.5, bounds.height() * 0.5));
        return;
    }

    const QRectF videoRect = calculateAspectFit(bounds, packet.width, packet.height);

    // Draw active video image
    const QImage videoImage(packet.rgbBuffer.data(), static_cast<int>(packet.width), static_cast<int>(packet.height),
        static_cast<qsizetype>(packet.stride), QImage::Format_RGB888);

    painter->drawImage(videoRect, videoImage);

    // Optional Focus Peaking edge shimmer
    if (m_showPeaking && !packet.lumaBuffer.empty()) {
        drawPeaking(painter, videoRect, packet);
    }

    // Grid Overlay
    if (m_showGrid) {
        drawGrid(painter, videoRect);
    }

    // Center Crosshairs Reticle
    if (m_showReticle) {
        const QPointF center(videoRect.x() + (videoRect.width() * 0.5), videoRect.y() + (videoRect.height() * 0.5));
        drawReticle(painter, center);
    }

    // Interactive AF Region of Interest
    if (m_showRoi) {
        const QRectF screenRoi(videoRect.x() + (m_roiRect.x() * videoRect.width()),
            videoRect.y() + (m_roiRect.y() * videoRect.height()), m_roiRect.width() * videoRect.width(),
            m_roiRect.height() * videoRect.height());
        drawRoi(painter, screenRoi);
    }

    // Tactical HUD Watermark
    drawHud(painter, bounds, packet);
}

void LensVideoQuickItem::drawGrid(QPainter* painter, const QRectF& videoRect)
{
    painter->save();
    QPen pen(QColor(0x1a, 0x22, 0x32, 140), 1.0, Qt::DashLine);
    painter->setPen(pen);

    // Rule of Thirds guidelines
    const qreal wThird = videoRect.width() / 3.0;
    const qreal hThird = videoRect.height() / 3.0;

    painter->drawLine(
        QPointF(videoRect.x() + wThird, videoRect.top()), QPointF(videoRect.x() + wThird, videoRect.bottom()));
    painter->drawLine(QPointF(videoRect.x() + (2.0 * wThird), videoRect.top()),
        QPointF(videoRect.x() + (2.0 * wThird), videoRect.bottom()));

    painter->drawLine(
        QPointF(videoRect.left(), videoRect.y() + hThird), QPointF(videoRect.right(), videoRect.y() + hThird));
    painter->drawLine(QPointF(videoRect.left(), videoRect.y() + (2.0 * hThird)),
        QPointF(videoRect.right(), videoRect.y() + (2.0 * hThird)));

    painter->restore();
}

void LensVideoQuickItem::drawReticle(QPainter* painter, const QPointF& center)
{
    painter->save();
    QPen pen(QColor(0x00, 0xe5, 0xff, 180), 1.5);
    painter->setPen(pen);

    constexpr double kArmLength = 24.0;
    constexpr double kGap = 6.0;
    constexpr double kRadius = 14.0;

    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(center, kRadius, kRadius);

    painter->drawLine(QPointF(center.x() - kArmLength, center.y()), QPointF(center.x() - kGap, center.y()));
    painter->drawLine(QPointF(center.x() + kGap, center.y()), QPointF(center.x() + kArmLength, center.y()));
    painter->drawLine(QPointF(center.x(), center.y() - kArmLength), QPointF(center.x(), center.y() - kGap));
    painter->drawLine(QPointF(center.x(), center.y() + kGap), QPointF(center.x(), center.y() + kArmLength));

    painter->restore();
}

void LensVideoQuickItem::drawRoi(QPainter* painter, const QRectF& screenRoi)
{
    painter->save();

    // Semi-transparent fill and dashed boundary
    QPen pen(QColor(0x00, 0xe6, 0x76, 220), 1.5, Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(QColor(0x00, 0xe6, 0x76, 20));
    painter->drawRect(screenRoi);

    // Corner brackets
    painter->setPen(QPen(QColor(0x00, 0xe6, 0x76), 2.5));
    constexpr double kC = 12.0;
    const double rx = screenRoi.x();
    const double ry = screenRoi.y();
    const double rw = screenRoi.width();
    const double rh = screenRoi.height();

    // Top-left
    painter->drawLine(QPointF(rx, ry), QPointF(rx + kC, ry));
    painter->drawLine(QPointF(rx, ry), QPointF(rx, ry + kC));
    // Top-right
    painter->drawLine(QPointF(rx + rw, ry), QPointF(rx + rw - kC, ry));
    painter->drawLine(QPointF(rx + rw, ry), QPointF(rx + rw, ry + kC));
    // Bottom-left
    painter->drawLine(QPointF(rx, ry + rh), QPointF(rx + kC, ry + rh));
    painter->drawLine(QPointF(rx, ry + rh), QPointF(rx, ry + rh - kC));
    // Bottom-right
    painter->drawLine(QPointF(rx + rw, ry + rh), QPointF(rx + rw - kC, ry + rh));
    painter->drawLine(QPointF(rx + rw, ry + rh), QPointF(rx + rw, ry + rh - kC));

    // Corner handle circles for drag visualization
    painter->setBrush(QColor(0x00, 0xe6, 0x76));
    painter->drawEllipse(QPointF(rx, ry), 3.0, 3.0);
    painter->drawEllipse(QPointF(rx + rw, ry), 3.0, 3.0);
    painter->drawEllipse(QPointF(rx, ry + rh), 3.0, 3.0);
    painter->drawEllipse(QPointF(rx + rw, ry + rh), 3.0, 3.0);

    // Label with normalized dimensions
    QFont font = painter->font();
    font.setPixelSize(10);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(QColor(0x00, 0xe6, 0x76));
    const QString label = QStringLiteral("AF ROI [%1% x %2%]")
                              .arg(static_cast<int>(m_roiRect.width() * 100.0))
                              .arg(static_cast<int>(m_roiRect.height() * 100.0));
    painter->drawText(QPointF(rx + 4.0, ry - 4.0), label);

    painter->restore();
}

void LensVideoQuickItem::drawPeaking(QPainter* painter, const QRectF& videoRect, const LensFramePacket& packet)
{
    const int w = static_cast<int>(packet.width);
    const int h = static_cast<int>(packet.height);
    const std::uint8_t* luma = packet.lumaBuffer.data();

    painter->save();
    QPen peakingPen(QColor(0x00, 0xe6, 0x76, 210), 1.0);
    painter->setPen(peakingPen);

    const qreal scaleX = videoRect.width() / static_cast<qreal>(w);
    const qreal scaleY = videoRect.height() / static_cast<qreal>(h);

    // Fast 3x3 Sobel edge detection on luma subsampled by 2 for real-time presentation
    for (int y = 2; y < h - 2; y += 2) {
        const int rowPrev = (y - 1) * w;
        const int rowCurr = y * w;
        const int rowNext = (y + 1) * w;

        for (int x = 2; x < w - 2; x += 2) {
            const int gx = (luma[rowPrev + x + 1] + (2 * luma[rowCurr + x + 1]) + luma[rowNext + x + 1])
                - (luma[rowPrev + x - 1] + (2 * luma[rowCurr + x - 1]) + luma[rowNext + x - 1]);
            const int gy = (luma[rowNext + x - 1] + (2 * luma[rowNext + x]) + luma[rowNext + x + 1])
                - (luma[rowPrev + x - 1] + (2 * luma[rowPrev + x]) + luma[rowPrev + x + 1]);

            if ((std::abs(gx) + std::abs(gy)) > kPeakingThreshold) {
                const qreal px = videoRect.x() + (static_cast<qreal>(x) * scaleX);
                const qreal py = videoRect.y() + (static_cast<qreal>(y) * scaleY);
                painter->drawPoint(QPointF(px, py));
            }
        }
    }

    painter->restore();
}

void LensVideoQuickItem::drawHud(QPainter* painter, const QRectF& bounds, const LensFramePacket& packet)
{
    painter->save();
    QFont font = painter->font();
    font.setPixelSize(11);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    painter->setFont(font);

    // Top-left HUD badge
    painter->setPen(QColor(0x00, 0xe5, 0xff));
    const QString modeStr = packet.isSynthetic ? QStringLiteral("SYNTHETIC SIMULATOR") : QStringLiteral("LIVE STREAM");
    const QString hudLeft = QStringLiteral("VIEWPORT: %1 | %2x%3 | FRAME #%4")
                                .arg(modeStr)
                                .arg(packet.width)
                                .arg(packet.height)
                                .arg(packet.frameId);
    painter->drawText(QRectF(16.0, 16.0, bounds.width() - 32.0, 20.0), Qt::AlignLeft | Qt::AlignVCenter, hudLeft);

    // Bottom-left HUD badge
    painter->setPen(QColor(0x52, 0x5c, 0x70));
    const QString hudBottom = QStringLiteral("PTS: %1 s | METRIC SCORE: %2 | FOCUS POS: %3")
                                  .arg(packet.timestampSec, 0, 'f', 3)
                                  .arg(packet.metricScore.metric)
                                  .arg(packet.status.focusPos.value());
    painter->drawText(
        QRectF(16.0, bounds.height() - 32.0, bounds.width() - 32.0, 20.0), Qt::AlignLeft | Qt::AlignVCenter, hudBottom);

    painter->restore();
}

} // namespace LensApp
