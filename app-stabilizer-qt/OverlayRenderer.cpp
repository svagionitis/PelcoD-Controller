/// @file OverlayRenderer.cpp
/// @brief Zero-heap tactical overlay rendering routines for QPainter.
#include "OverlayRenderer.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cmath>

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QLineF>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace StabilizerApp {

void OverlayRenderer::renderReticle(QPainter* painter, const QRectF& targetRect) noexcept
{
    if (painter == nullptr) {
        return;
    }

    const QPointF center = targetRect.center();

    // Semi-transparent tactical HUD pen
    QPen hudPen(QColor(0x00, 0xe5, 0xff, 130), 1.0);
    painter->setPen(hudPen);
    painter->setBrush(Qt::NoBrush);

    // Crosshairs with center gap
    constexpr qreal CenterGap = 4.0;
    constexpr qreal ArmLength = 28.0;

    // Horizontal arms
    painter->drawLine(QPointF(center.x() - ArmLength, center.y()), QPointF(center.x() - CenterGap, center.y()));
    painter->drawLine(QPointF(center.x() + CenterGap, center.y()), QPointF(center.x() + ArmLength, center.y()));

    // Vertical arms
    painter->drawLine(QPointF(center.x(), center.y() - ArmLength), QPointF(center.x(), center.y() - CenterGap));
    painter->drawLine(QPointF(center.x(), center.y() + CenterGap), QPointF(center.x(), center.y() + ArmLength));

    // Range rings
    painter->drawEllipse(center, 14.0, 14.0);
    painter->drawEllipse(center, 42.0, 42.0);

    QPen outerRingPen(QColor(0x00, 0xe5, 0xff, 60), 1.0, Qt::DashLine);
    painter->setPen(outerRingPen);
    painter->drawEllipse(center, 84.0, 84.0);

    // Center precision dot
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0x00, 0xe5, 0xff, 180));
    painter->drawEllipse(center, 1.5, 1.5);
}

void OverlayRenderer::renderLandmarks(QPainter* painter, const QRectF& targetRect, const VisualFeaturePoint* features,
    std::size_t count, float scaleX, float scaleY) noexcept
{
    if ((painter == nullptr) || (features == nullptr) || (count == 0U)) {
        return;
    }

    painter->save();
    painter->setClipRect(targetRect);

    const QColor inlierFill(0x00, 0xe6, 0x76, 120);
    const QColor inlierBorder(0x00, 0xe6, 0x76, 230);
    const QPen inlierPen(inlierBorder, 1.2);
    const QBrush inlierBrush(inlierFill);

    const QColor outlierColor(0xff, 0x17, 0x44, 220);
    const QPen outlierPen(outlierColor, 1.5);

    constexpr qreal InlierRadius = 3.0;
    constexpr qreal OutlierArm = 3.5;

    for (std::size_t i = 0U; i < count; ++i) {
        const qreal px = targetRect.left() + (static_cast<qreal>(features[i].x) * static_cast<qreal>(scaleX));
        const qreal py = targetRect.top() + (static_cast<qreal>(features[i].y) * static_cast<qreal>(scaleY));

        if (!targetRect.contains(px, py)) {
            continue;
        }

        if (features[i].isInlier) {
            painter->setPen(inlierPen);
            painter->setBrush(inlierBrush);
            painter->drawEllipse(QPointF(px, py), InlierRadius, InlierRadius);
            painter->drawPoint(QPointF(px, py));
        } else {
            painter->setPen(outlierPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawLine(QPointF(px - OutlierArm, py - OutlierArm), QPointF(px + OutlierArm, py + OutlierArm));
            painter->drawLine(QPointF(px - OutlierArm, py + OutlierArm), QPointF(px + OutlierArm, py - OutlierArm));
        }
    }

    painter->restore();
}

void OverlayRenderer::renderQuiverPlot(QPainter* painter, const QRectF& targetRect, const VisualFeaturePoint* features,
    std::size_t count, float scaleX, float scaleY, float quiverScale) noexcept
{
    if ((painter == nullptr) || (features == nullptr) || (count == 0U)) {
        return;
    }

    painter->save();
    painter->setClipRect(targetRect);

    const QPen inlierPen(QColor(0x00, 0xe5, 0xff, 220), 1.2);
    const QPen outlierPen(QColor(0xff, 0x91, 0x00, 180), 1.0, Qt::DashLine);

    constexpr qreal MinDispSq = 0.04; // 0.2px minimum displacement threshold
    constexpr qreal ArrowLength = 4.5;
    constexpr qreal ArrowAngle = 0.5235987756; // 30 degrees in radians

    for (std::size_t i = 0U; i < count; ++i) {
        const qreal vx = static_cast<qreal>(features[i].vx);
        const qreal vy = static_cast<qreal>(features[i].vy);
        const qreal dispSq = (vx * vx) + (vy * vy);

        if (dispSq < MinDispSq) {
            continue;
        }

        const qreal x0 = targetRect.left() + (static_cast<qreal>(features[i].x) * static_cast<qreal>(scaleX));
        const qreal y0 = targetRect.top() + (static_cast<qreal>(features[i].y) * static_cast<qreal>(scaleY));

        const qreal dx = vx * static_cast<qreal>(quiverScale) * static_cast<qreal>(scaleX);
        const qreal dy = vy * static_cast<qreal>(quiverScale) * static_cast<qreal>(scaleY);

        const qreal x1 = x0 + dx;
        const qreal y1 = y0 + dy;

        painter->setPen(features[i].isInlier ? inlierPen : outlierPen);
        painter->drawLine(QPointF(x0, y0), QPointF(x1, y1));

        // Arrow head
        const qreal angle = std::atan2(dy, dx);
        const qreal leftX = x1 - (ArrowLength * std::cos(angle - ArrowAngle));
        const qreal leftY = y1 - (ArrowLength * std::sin(angle - ArrowAngle));
        const qreal rightX = x1 - (ArrowLength * std::cos(angle + ArrowAngle));
        const qreal rightY = y1 - (ArrowLength * std::sin(angle + ArrowAngle));

        painter->drawLine(QPointF(x1, y1), QPointF(leftX, leftY));
        painter->drawLine(QPointF(x1, y1), QPointF(rightX, rightY));
    }

    painter->restore();
}

void OverlayRenderer::renderCropBoundary(
    QPainter* painter, const QRectF& targetRect, float cropRatio, float softMarginRatio) noexcept
{
    if (painter == nullptr) {
        return;
    }

    const qreal clampedCrop = std::clamp(static_cast<qreal>(cropRatio), 0.0, 0.30);
    const qreal clampedSoft = std::clamp(static_cast<qreal>(softMarginRatio), 0.10, 0.99);

    const qreal mx = targetRect.width() * clampedCrop;
    const qreal my = targetRect.height() * clampedCrop;

    // Hard auto-crop boundary
    const QRectF hardRect(targetRect.left() + mx, targetRect.top() + my, targetRect.width() - (2.0 * mx),
        targetRect.height() - (2.0 * my));

    QPen hardPen(QColor(0xff, 0xd7, 0x00, 180), 1.5);
    painter->setPen(hardPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(hardRect);

    // Soft damping engagement threshold
    const qreal sx = mx * clampedSoft;
    const qreal sy = my * clampedSoft;
    const QRectF softRect(targetRect.left() + sx, targetRect.top() + sy, targetRect.width() - (2.0 * sx),
        targetRect.height() - (2.0 * sy));

    QPen softPen(QColor(0xff, 0x91, 0x00, 140), 1.0, Qt::DashLine);
    painter->setPen(softPen);
    painter->drawRect(softRect);
}

void OverlayRenderer::renderBadge(QPainter* painter, const QRectF& badgeRect, const QString& text,
    const QColor& textColor, const QColor& bgColor) noexcept
{
    if (painter == nullptr) {
        return;
    }

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(bgColor);
    painter->drawRoundedRect(badgeRect, 4.0, 4.0);

    QFont font = painter->font();
    font.setPixelSize(11);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(textColor);
    painter->drawText(badgeRect, Qt::AlignCenter, text);
    painter->restore();
}

} // namespace StabilizerApp
