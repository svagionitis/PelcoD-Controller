#include "VideoOverlayWidget.h"

#include <QDateTime>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace PelcoDApp {

VideoOverlayWidget::VideoOverlayWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 240);
}

void VideoOverlayWidget::clearFrame()
{
    {
        QMutexLocker locker(&m_frameMutex);
        m_currentFrame = QImage();
    }
    update();
}

void VideoOverlayWidget::setHudColor(const QColor& color)
{
    m_hudColor = color;
    update();
}

QColor VideoOverlayWidget::hudColor() const
{
    return m_hudColor;
}

void VideoOverlayWidget::setShowCrosshair(bool show)
{
    m_showCrosshair = show;
    update();
}

void VideoOverlayWidget::setShowCompass(bool show)
{
    m_showCompass = show;
    update();
}

void VideoOverlayWidget::setShowPitchLadder(bool show)
{
    m_showPitchLadder = show;
    update();
}

void VideoOverlayWidget::setShowOpticsHud(bool show)
{
    m_showOpticsHud = show;
    update();
}

void VideoOverlayWidget::setShowDiagnostics(bool show)
{
    m_showDiagnostics = show;
    update();
}

void VideoOverlayWidget::setInteractivePtzEnabled(bool enabled)
{
    m_interactivePtzEnabled = enabled;
    if (!enabled && m_isDragging) {
        m_isDragging = false;
        emit stopPtzRequested();
        update();
    }
}

bool VideoOverlayWidget::isCrosshairVisible() const { return m_showCrosshair; }
bool VideoOverlayWidget::isCompassVisible() const { return m_showCompass; }
bool VideoOverlayWidget::isPitchLadderVisible() const { return m_showPitchLadder; }
bool VideoOverlayWidget::isOpticsHudVisible() const { return m_showOpticsHud; }
bool VideoOverlayWidget::isDiagnosticsVisible() const { return m_showDiagnostics; }
bool VideoOverlayWidget::isInteractivePtzEnabled() const { return m_interactivePtzEnabled; }

void VideoOverlayWidget::updateFrame(const QImage& frame, double pts, double decodeLatencyMs)
{
    {
        QMutexLocker locker(&m_frameMutex);
        m_currentFrame = frame;
        m_currentPts = pts;
        m_currentDecodeMs = decodeLatencyMs;
        m_videoWidth = frame.width();
        m_videoHeight = frame.height();
    }
    update();
}

void VideoOverlayWidget::setDeviceStatus(bool connected, std::uint8_t address)
{
    m_connected = connected;
    m_address = address;
    update();
}

void VideoOverlayWidget::setPanAngle(double degrees)
{
    while (degrees < 0.0) degrees += 360.0;
    while (degrees >= 360.0) degrees -= 360.0;
    m_panDegrees = degrees;
    update();
}

void VideoOverlayWidget::setTiltAngle(double degrees)
{
    m_tiltDegrees = degrees;
    update();
}

void VideoOverlayWidget::setZoomInfo(int rawZoom, double magnification, double focalLengthMm)
{
    m_rawZoom = rawZoom;
    m_magnification = magnification;
    m_focalLengthMm = focalLengthMm;
    update();
}

void VideoOverlayWidget::setOpticalStatus(const QString& focusMode,
                                         const QString& irisMode,
                                         const QString& oisMode,
                                         const QString& defogMode,
                                         const QString& dayNightMode)
{
    m_focusMode = focusMode;
    m_irisMode = irisMode;
    m_oisMode = oisMode;
    m_defogMode = defogMode;
    m_dayNightMode = dayNightMode;
    update();
}

void VideoOverlayWidget::setStreamDiagnostics(const QString& backend,
                                             int width,
                                             int height,
                                             double fps,
                                             double decodeMs,
                                             double rttMs,
                                             double jitterMs)
{
    m_backendName = backend;
    m_videoWidth = width;
    m_videoHeight = height;
    m_fps = fps;
    m_avgDecodeMs = decodeMs;
    m_rttMs = rttMs;
    m_jitterMs = jitterMs;
    update();
}

QRect VideoOverlayWidget::calculateAspectFitRect() const
{
    int imgW = 16;
    int imgH = 9;

    {
        QMutexLocker locker(&m_frameMutex);
        if (!m_currentFrame.isNull()) {
            imgW = m_currentFrame.width();
            imgH = m_currentFrame.height();
        } else if (m_videoWidth > 0 && m_videoHeight > 0) {
            imgW = m_videoWidth;
            imgH = m_videoHeight;
        }
    }

    const double widgetW = width();
    const double widgetH = height();

    const double scale = std::min(widgetW / static_cast<double>(imgW), widgetH / static_cast<double>(imgH));
    const int targetW = static_cast<int>(std::round(static_cast<double>(imgW) * scale));
    const int targetH = static_cast<int>(std::round(static_cast<double>(imgH) * scale));
    const int targetX = (static_cast<int>(widgetW) - targetW) / 2;
    const int targetY = (static_cast<int>(widgetH) - targetH) / 2;

    return QRect(targetX, targetY, targetW, targetH);
}

void VideoOverlayWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Letterbox background
    painter.fillRect(rect(), Qt::black);

    const QRect targetRect = calculateAspectFitRect();

    drawVideoFrame(painter, targetRect);

    if (m_showCrosshair) {
        drawCrosshairLayer(painter, targetRect);
    }
    if (m_showCompass) {
        drawCompassLayer(painter, targetRect);
    }
    if (m_showPitchLadder) {
        drawPitchLadderLayer(painter, targetRect);
    }
    if (m_showOpticsHud) {
        drawOpticsHudLayer(painter, targetRect);
    }
    if (m_showDiagnostics) {
        drawDiagnosticsLayer(painter, targetRect);
    }
    if (m_isDragging && m_interactivePtzEnabled) {
        drawVirtualJoystickLayer(painter);
    }
}

void VideoOverlayWidget::drawVideoFrame(QPainter& painter, const QRect& targetRect) const
{
    QMutexLocker locker(&m_frameMutex);
    if (!m_currentFrame.isNull()) {
        painter.drawImage(targetRect, m_currentFrame);
    } else {
        // No active stream placeholder
        painter.setPen(QColor(100, 110, 120));
        painter.setFont(QFont("Segoe UI", 12, QFont::DemiBold));
        painter.drawText(targetRect, Qt::AlignCenter, tr("NO VIDEO SIGNAL\nConnect to RTSP Stream or Mock Test Pattern"));
    }
}

void VideoOverlayWidget::drawCrosshairLayer(QPainter& painter, const QRect& targetRect) const
{
    const QPoint center = targetRect.center();
    const int crosshairRadius = std::clamp(std::min(targetRect.width(), targetRect.height()) / 10, 20, 60);

    QPen pen(m_hudColor, 1.5);
    painter.setPen(pen);

    // Center crosshair with central gap
    const int gap = 6;
    painter.drawLine(center.x() - crosshairRadius, center.y(), center.x() - gap, center.y());
    painter.drawLine(center.x() + gap, center.y(), center.x() + crosshairRadius, center.y());
    painter.drawLine(center.x(), center.y() - crosshairRadius, center.x(), center.y() - gap);
    painter.drawLine(center.x(), center.y() + gap, center.x(), center.y() + crosshairRadius);

    // Mil-dot tick marks along arms
    for (int d = 15; d < crosshairRadius; d += 15) {
        painter.drawLine(center.x() - d, center.y() - 3, center.x() - d, center.y() + 3);
        painter.drawLine(center.x() + d, center.y() - 3, center.x() + d, center.y() + 3);
        painter.drawLine(center.x() - 3, center.y() - d, center.x() + 3, center.y() - d);
        painter.drawLine(center.x() - 3, center.y() + d, center.x() + 3, center.y() + d);
    }

    // Outer corner targeting brackets
    const int boxRadius = crosshairRadius * 2;
    const int cornerLen = 14;
    const int left = center.x() - boxRadius;
    const int right = center.x() + boxRadius;
    const int top = center.y() - boxRadius;
    const int bottom = center.y() + boxRadius;

    // Top-Left
    painter.drawLine(left, top, left + cornerLen, top);
    painter.drawLine(left, top, left, top + cornerLen);
    // Top-Right
    painter.drawLine(right - cornerLen, top, right, top);
    painter.drawLine(right, top, right, top + cornerLen);
    // Bottom-Left
    painter.drawLine(left, bottom, left + cornerLen, bottom);
    painter.drawLine(left, bottom, left, bottom - cornerLen);
    // Bottom-Right
    painter.drawLine(right - cornerLen, bottom, right, bottom);
    painter.drawLine(right, bottom, right, bottom - cornerLen);
}

void VideoOverlayWidget::drawCompassLayer(QPainter& painter, const QRect& targetRect) const
{
    const int tapeW = std::clamp((targetRect.width() * 6) / 10, 240, 560);
    const int tapeH = 34;
    const int tapeX = targetRect.center().x() - (tapeW / 2);
    const int tapeY = targetRect.top() + 10;

    // Background pill
    painter.setPen(QPen(m_hudColor.darker(150), 1.0));
    painter.setBrush(QColor(10, 16, 22, 200));
    painter.drawRoundedRect(tapeX, tapeY, tapeW, tapeH, 4, 4);

    // Pixels per degree
    const double pxPerDeg = static_cast<double>(tapeW) / 60.0; // Visible window: +/- 30 degrees

    painter.save();
    painter.setClipRect(tapeX, tapeY, tapeW, tapeH);

    QFont font("Segoe UI", 8, QFont::Bold);
    painter.setFont(font);

    const int centerDeg = static_cast<int>(std::round(m_panDegrees));
    for (int deg = centerDeg - 35; deg <= centerDeg + 35; ++deg) {
        int normDeg = deg % 360;
        if (normDeg < 0) normDeg += 360;

        const double offsetDeg = static_cast<double>(deg) - m_panDegrees;
        const int tickX = targetRect.center().x() + static_cast<int>(std::round(offsetDeg * pxPerDeg));

        if (tickX < tapeX || tickX > tapeX + tapeW) continue;

        if (normDeg % 30 == 0) {
            // Major tick mark & Cardinal label
            painter.setPen(QPen(m_hudColor, 1.5));
            painter.drawLine(tickX, tapeY + tapeH - 12, tickX, tapeY + tapeH - 2);

            QString label;
            switch (normDeg) {
            case 0:   label = "N"; break;
            case 90:  label = "E"; break;
            case 180: label = "S"; break;
            case 270: label = "W"; break;
            default:  label = QString::number(normDeg); break;
            }

            const QRect textRect(tickX - 16, tapeY + 2, 32, 14);
            painter.drawText(textRect, Qt::AlignCenter, label);
        } else if (normDeg % 10 == 0) {
            // Medium tick mark
            painter.setPen(QPen(m_hudColor.lighter(120), 1.0));
            painter.drawLine(tickX, tapeY + tapeH - 8, tickX, tapeY + tapeH - 2);
        } else if (normDeg % 5 == 0) {
            // Minor tick mark
            painter.setPen(QPen(m_hudColor.darker(120), 0.8));
            painter.drawLine(tickX, tapeY + tapeH - 5, tickX, tapeY + tapeH - 2);
        }
    }

    painter.restore();

    // Center needle & Current Heading Readout
    painter.setPen(QPen(QColor(255, 60, 60), 1.8)); // Amber/Red pointer
    painter.drawLine(targetRect.center().x(), tapeY + tapeH - 14, targetRect.center().x(), tapeY + tapeH - 2);

    // Heading text badge right below compass tape
    const QString headingStr = QString("%1° %2")
        .arg(m_panDegrees, 5, 'f', 1)
        .arg([](double deg) -> const char* {
            static const char* kCardinals[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
            const int idx = static_cast<int>(std::floor((deg + 22.5) / 45.0)) % 8;
            return kCardinals[idx];
        }(m_panDegrees));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(10, 16, 22, 220));
    painter.drawRoundedRect(targetRect.center().x() - 40, tapeY + tapeH + 3, 80, 18, 3, 3);

    painter.setPen(m_hudColor);
    painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
    painter.drawText(QRect(targetRect.center().x() - 40, tapeY + tapeH + 3, 80, 18), Qt::AlignCenter, headingStr);
}

void VideoOverlayWidget::drawPitchLadderLayer(QPainter& painter, const QRect& targetRect) const
{
    const int ladderW = 46;
    const int ladderH = std::clamp((targetRect.height() * 5) / 10, 140, 280);
    const int ladderX = targetRect.right() - ladderW - 10;
    const int ladderY = targetRect.center().y() - (ladderH / 2);

    // Background pill
    painter.setPen(QPen(m_hudColor.darker(150), 1.0));
    painter.setBrush(QColor(10, 16, 22, 190));
    painter.drawRoundedRect(ladderX, ladderY, ladderW, ladderH, 4, 4);

    const double pxPerDeg = static_cast<double>(ladderH) / 40.0; // Visible window: +/- 20 degrees
    const int centerY = ladderY + (ladderH / 2);

    painter.save();
    painter.setClipRect(ladderX, ladderY, ladderW, ladderH);

    painter.setFont(QFont("Segoe UI", 7, QFont::DemiBold));

    const int centerTiltDeg = static_cast<int>(std::round(m_tiltDegrees));
    for (int deg = centerTiltDeg - 25; deg <= centerTiltDeg + 25; ++deg) {
        if (deg < -90 || deg > 90) continue;

        const double offsetDeg = static_cast<double>(deg) - m_tiltDegrees;
        const int tickY = centerY - static_cast<int>(std::round(offsetDeg * pxPerDeg));

        if (tickY < ladderY || tickY > ladderY + ladderH) continue;

        if (deg % 10 == 0) {
            painter.setPen(QPen(m_hudColor, 1.2));
            painter.drawLine(ladderX + 2, tickY, ladderX + 12, tickY);

            const QString label = QString("%1%2°").arg(deg > 0 ? "+" : "").arg(deg);
            painter.drawText(QRect(ladderX + 14, tickY - 7, 28, 14), Qt::AlignVCenter | Qt::AlignLeft, label);
        } else if (deg % 5 == 0) {
            painter.setPen(QPen(m_hudColor.darker(120), 0.8));
            painter.drawLine(ladderX + 2, tickY, ladderX + 7, tickY);
        }
    }

    painter.restore();

    // Current Tilt indicator needle
    painter.setPen(QPen(QColor(255, 60, 60), 1.8));
    painter.drawLine(ladderX + 2, centerY, ladderX + 14, centerY);
}

void VideoOverlayWidget::drawOpticsHudLayer(QPainter& painter, const QRect& targetRect) const
{
    const int hudW = std::clamp(targetRect.width() / 3, 220, 320);
    const int hudH = 76;
    const int hudX = targetRect.left() + 12;
    const int hudY = targetRect.bottom() - hudH - 12;

    painter.setPen(QPen(m_hudColor.darker(140), 1.0));
    painter.setBrush(QColor(8, 14, 20, 210));
    painter.drawRoundedRect(hudX, hudY, hudW, hudH, 4, 4);

    // Title / Camera Status Row
    painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
    painter.setPen(m_connected ? QColor(0, 230, 118) : QColor(255, 82, 82));
    const QString statusStr = m_connected ? tr("ONLINE [ADDR: %1]").arg(m_address, 2, 10, QChar('0')) : tr("OFFLINE");
    painter.drawText(hudX + 8, hudY + 16, statusStr);

    // Zoom and Focal Length Row
    painter.setPen(Qt::white);
    painter.setFont(QFont("Segoe UI", 8, QFont::DemiBold));
    const QString zoomStr = tr("ZOOM: %1x (%2 mm) | RAW: %3")
        .arg(m_magnification, 0, 'f', 1)
        .arg(m_focalLengthMm, 0, 'f', 0)
        .arg(m_rawZoom);
    painter.drawText(hudX + 8, hudY + 34, zoomStr);

    // Focus & Iris Modes
    painter.setPen(m_hudColor);
    painter.setFont(QFont("Segoe UI", 7, QFont::Normal));
    const QString opticalModes = tr("FOCUS: %1 | IRIS: %2 | OIS: %3")
        .arg(m_focusMode).arg(m_irisMode).arg(m_oisMode);
    painter.drawText(hudX + 8, hudY + 50, opticalModes);

    // Enhancements / Defog
    painter.setPen(QColor(180, 195, 210));
    const QString fxStr = tr("DEFOG: %1 | D/N: %2")
        .arg(m_defogMode).arg(m_dayNightMode);
    painter.drawText(hudX + 8, hudY + 66, fxStr);
}

void VideoOverlayWidget::drawDiagnosticsLayer(QPainter& painter, const QRect& targetRect) const
{
    const int hudW = std::clamp(targetRect.width() / 4, 180, 240);
    const int hudH = 58;
    const int hudX = targetRect.right() - hudW - 12;
    const int hudY = targetRect.top() + 12;

    painter.setPen(QPen(m_hudColor.darker(140), 1.0));
    painter.setBrush(QColor(8, 14, 20, 210));
    painter.drawRoundedRect(hudX, hudY, hudW, hudH, 4, 4);

    painter.setFont(QFont("Segoe UI", 8, QFont::DemiBold));
    painter.setPen(m_hudColor);
    painter.drawText(hudX + 8, hudY + 16, tr("STREAM: %1").arg(m_backendName.toUpper()));

    painter.setPen(Qt::white);
    painter.setFont(QFont("Segoe UI", 7, QFont::Normal));
    painter.drawText(hudX + 8, hudY + 32, tr("%1x%2 @ %3 FPS")
        .arg(m_videoWidth).arg(m_videoHeight).arg(m_fps, 0, 'f', 1));

    painter.setPen(QColor(180, 195, 210));
    painter.drawText(hudX + 8, hudY + 48, tr("DEC: %1 ms | RTT: %2 ms")
        .arg(m_avgDecodeMs, 0, 'f', 1).arg(m_rttMs, 0, 'f', 1));
}

void VideoOverlayWidget::drawVirtualJoystickLayer(QPainter& painter) const
{
    painter.setPen(QPen(m_hudColor.lighter(120), 1.5, Qt::DashLine));
    painter.setBrush(QColor(0, 229, 255, 40));

    // Outer deadzone circle
    painter.drawEllipse(m_dragStartPos, 20, 20);

    // Vector line
    painter.setPen(QPen(QColor(255, 170, 0), 2.0));
    painter.drawLine(m_dragStartPos, m_dragCurrentPos);

    // Current position circle
    painter.setBrush(QColor(255, 170, 0, 180));
    painter.drawEllipse(m_dragCurrentPos, 6, 6);
}

void VideoOverlayWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_interactivePtzEnabled) {
        m_isDragging = true;
        m_dragStartPos = event->pos();
        m_dragCurrentPos = event->pos();
        update();
    }
    QWidget::mousePressEvent(event);
}

void VideoOverlayWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDragging && m_interactivePtzEnabled) {
        m_dragCurrentPos = event->pos();

        const int dx = m_dragCurrentPos.x() - m_dragStartPos.x();
        const int dy = m_dragCurrentPos.y() - m_dragStartPos.y();

        static constexpr int kDeadzone = 12;
        static constexpr int kMaxDist = 120;

        int panSpeed = 0;
        int tiltSpeed = 0;
        bool left = false;
        bool right = false;
        bool up = false;
        bool down = false;

        if (std::abs(dx) > kDeadzone) {
            const int effDx = std::abs(dx) - kDeadzone;
            panSpeed = std::clamp((effDx * 63) / (kMaxDist - kDeadzone), 1, 63);
            left = (dx < 0);
            right = (dx > 0);
        }

        if (std::abs(dy) > kDeadzone) {
            const int effDy = std::abs(dy) - kDeadzone;
            tiltSpeed = std::clamp((effDy * 63) / (kMaxDist - kDeadzone), 1, 63);
            up = (dy < 0);
            down = (dy > 0);
        }

        if (panSpeed > 0 || tiltSpeed > 0) {
            emit panTiltRequested(panSpeed, tiltSpeed, left, right, up, down);
        } else {
            emit stopPtzRequested();
        }

        update();
    }
    QWidget::mouseMoveEvent(event);
}

void VideoOverlayWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        emit stopPtzRequested();
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void VideoOverlayWidget::wheelEvent(QWheelEvent* event)
{
    if (m_interactivePtzEnabled) {
        const int delta = event->angleDelta().y();
        if (delta != 0) {
            emit zoomRequested(delta > 0);
            event->accept();
            return;
        }
    }
    QWidget::wheelEvent(event);
}

void VideoOverlayWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

QImage VideoOverlayWidget::captureSnapshot(bool includeOverlay) const
{
    QMutexLocker locker(&m_frameMutex);
    if (m_currentFrame.isNull()) {
        return {};
    }

    if (!includeOverlay) {
        return m_currentFrame.copy();
    }

    // Render snapshot with HUD overlaid on top of original resolution
    QImage result(m_currentFrame.size(), QImage::Format_RGB888);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect fullRect(0, 0, result.width(), result.height());
    painter.drawImage(fullRect, m_currentFrame);

    if (m_showCrosshair) drawCrosshairLayer(painter, fullRect);
    if (m_showCompass) drawCompassLayer(painter, fullRect);
    if (m_showPitchLadder) drawPitchLadderLayer(painter, fullRect);
    if (m_showOpticsHud) drawOpticsHudLayer(painter, fullRect);
    if (m_showDiagnostics) drawDiagnosticsLayer(painter, fullRect);

    return result;
}

} // namespace PelcoDApp
