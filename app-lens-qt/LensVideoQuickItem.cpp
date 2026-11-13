/// @file LensVideoQuickItem.cpp
#include "LensVideoQuickItem.h"

#include <cstdint>

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>

#include "LensPlayerController.h"

namespace LensApp {

LensVideoQuickItem::LensVideoQuickItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
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
    if (m_roiRect != rect) {
        m_roiRect = rect;
        emit roiRectChanged();
        update();
    }
}

void LensVideoQuickItem::setController(LensPlayerController* ctrl)
{
    if (m_controller != ctrl) {
        m_controller = ctrl;
        emit controllerChanged();
        update();
    }
}

void LensVideoQuickItem::paint(QPainter* painter)
{
    if (painter == nullptr) {
        return;
    }

    const QRectF bounds(0.0, 0.0, width(), height());
    painter->fillRect(bounds, QColor(0x0a, 0x0c, 0x10));

    if (m_showGrid) {
        drawGrid(painter, bounds);
    }

    if (m_showRoi) {
        drawRoi(painter, bounds);
    }

    if (m_showReticle) {
        const QPointF center(bounds.width() * 0.5, bounds.height() * 0.5);
        drawReticle(painter, center);
    }

    // Watermark banner
    painter->save();
    QFont font = painter->font();
    font.setPixelSize(11);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    painter->setFont(font);
    painter->setPen(QColor(0x52, 0x5c, 0x70));
    painter->drawText(QRectF(16.0, bounds.height() - 32.0, bounds.width() - 32.0, 20.0),
        Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("LENS VIEWPORT — MOCK OPTICAL SIMULATION ACTIVE"));
    painter->restore();
}

void LensVideoQuickItem::drawGrid(QPainter* painter, const QRectF& bounds)
{
    painter->save();
    QPen pen(QColor(0x1a, 0x1e, 0x28), 1.0, Qt::DashLine);
    painter->setPen(pen);

    constexpr double kSpacing = 60.0;
    for (double x = kSpacing; x < bounds.width(); x += kSpacing) {
        painter->drawLine(QPointF(x, 0.0), QPointF(x, bounds.height()));
    }
    for (double y = kSpacing; y < bounds.height(); y += kSpacing) {
        painter->drawLine(QPointF(0.0, y), QPointF(bounds.width(), y));
    }
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

    // Center circular ring
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(center, kRadius, kRadius);

    // Crosshairs with gap
    painter->drawLine(QPointF(center.x() - kArmLength, center.y()), QPointF(center.x() - kGap, center.y()));
    painter->drawLine(QPointF(center.x() + kGap, center.y()), QPointF(center.x() + kArmLength, center.y()));
    painter->drawLine(QPointF(center.x(), center.y() - kArmLength), QPointF(center.x(), center.y() - kGap));
    painter->drawLine(QPointF(center.x(), center.y() + kGap), QPointF(center.x(), center.y() + kArmLength));

    painter->restore();
}

void LensVideoQuickItem::drawRoi(QPainter* painter, const QRectF& bounds)
{
    painter->save();
    const double rx = m_roiRect.x() * bounds.width();
    const double ry = m_roiRect.y() * bounds.height();
    const double rw = m_roiRect.width() * bounds.width();
    const double rh = m_roiRect.height() * bounds.height();
    const QRectF screenRoi(rx, ry, rw, rh);

    QPen pen(QColor(0xff, 0x91, 0x00, 200), 1.5, Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(QColor(0xff, 0x91, 0x00, 20));
    painter->drawRect(screenRoi);

    // Corner brackets
    painter->setPen(QPen(QColor(0xff, 0x91, 0x00), 2.0));
    constexpr double kCorner = 10.0;
    // Top-left
    painter->drawLine(QPointF(rx, ry), QPointF(rx + kCorner, ry));
    painter->drawLine(QPointF(rx, ry), QPointF(rx, ry + kCorner));
    // Top-right
    painter->drawLine(QPointF(rx + rw, ry), QPointF(rx + rw - kCorner, ry));
    painter->drawLine(QPointF(rx + rw, ry), QPointF(rx + rw, ry + kCorner));
    // Bottom-left
    painter->drawLine(QPointF(rx, ry + rh), QPointF(rx + kCorner, ry + rh));
    painter->drawLine(QPointF(rx, ry + rh), QPointF(rx, ry + rh - kCorner));
    // Bottom-right
    painter->drawLine(QPointF(rx + rw, ry + rh), QPointF(rx + rw - kCorner, ry + rh));
    painter->drawLine(QPointF(rx + rw, ry + rh), QPointF(rx + rw, ry + rh - kCorner));

    // Label
    QFont font = painter->font();
    font.setPixelSize(10);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(QColor(0xff, 0x91, 0x00));
    painter->drawText(QPointF(rx + 4.0, ry - 4.0), QStringLiteral("AF ROI (CENTER 50%)"));

    painter->restore();
}

} // namespace LensApp
