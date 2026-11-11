/// @file StabilizerQuickItem.cpp
/// @brief Custom QQuickPaintedItem presenting unstabilized and stabilized video frames.

#include "StabilizerQuickItem.h"

#include <cstdint>

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QFont>
#include <QPen>
#include <QPointF>
#include <QRectF>

namespace StabilizerApp {

StabilizerQuickItem::StabilizerQuickItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(true);
}

float StabilizerQuickItem::splitPosition() const noexcept
{
    return splitPosition_;
}

void StabilizerQuickItem::setSplitPosition(float pos) noexcept
{
    const float clamped = std::clamp(pos, 0.0F, 1.0F);
    if (std::abs(splitPosition_ - clamped) > 1e-4F) {
        splitPosition_ = clamped;
        emit splitPositionChanged();
        update();
    }
}

bool StabilizerQuickItem::showGrid() const noexcept
{
    return showGrid_;
}

void StabilizerQuickItem::setShowGrid(bool show) noexcept
{
    if (showGrid_ != show) {
        showGrid_ = show;
        emit showGridChanged();
        update();
    }
}

void StabilizerQuickItem::paint(QPainter* painter)
{
    if (painter == nullptr) {
        return;
    }

    const qreal w = width();
    const qreal h = height();

    if ((w <= 0.0) || (h <= 0.0)) {
        return;
    }

    // 1. Draw canvas background
    painter->fillRect(QRectF(0.0, 0.0, w, h), QColor(0x0a, 0x0c, 0x10));

    // 2. Draw tactical coordinate grid
    if (showGrid_) {
        QPen gridPen(QColor(0x16, 0x1a, 0x24), 1.0);
        painter->setPen(gridPen);

        constexpr qreal kGridSpacing = 64.0;
        for (qreal x = kGridSpacing; x < w; x += kGridSpacing) {
            painter->drawLine(QPointF(x, 0.0), QPointF(x, h));
        }
        for (qreal y = kGridSpacing; y < h; y += kGridSpacing) {
            painter->drawLine(QPointF(0.0, y), QPointF(w, y));
        }
    }

    // 3. Draw center tactical reticle
    const qreal cx = w * 0.5;
    const qreal cy = h * 0.5;
    constexpr qreal kReticleGap = 8.0;
    constexpr qreal kReticleLen = 32.0;

    QPen reticlePen(QColor(0x2a, 0x32, 0x48), 1.5);
    painter->setPen(reticlePen);
    painter->drawLine(QPointF(cx - kReticleLen, cy), QPointF(cx - kReticleGap, cy));
    painter->drawLine(QPointF(cx + kReticleGap, cy), QPointF(cx + kReticleLen, cy));
    painter->drawLine(QPointF(cx, cy - kReticleLen), QPointF(cx, cy - kReticleGap));
    painter->drawLine(QPointF(cx, cy + kReticleGap), QPointF(cx, cy + kReticleLen));

    // 4. Draw split comparison divider line
    const qreal splitX = w * static_cast<qreal>(splitPosition_);
    QPen splitPen(QColor(0x00, 0xe5, 0xff), 1.5);
    painter->setPen(splitPen);
    painter->drawLine(QPointF(splitX, 0.0), QPointF(splitX, h));

    // 5. Labels: RAW (LEFT) vs STABILIZED (RIGHT)
    QFont labelFont = painter->font();
    labelFont.setPixelSize(11);
    labelFont.setBold(true);
    painter->setFont(labelFont);

    painter->setPen(QColor(0x52, 0x5c, 0x70));
    painter->drawText(QRectF(16.0, 16.0, 200.0, 24.0), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("RAW INPUT"));
    painter->drawText(
        QRectF(w - 216.0, 16.0, 200.0, 24.0), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("STABILIZED VIEW"));
}

} // namespace StabilizerApp
