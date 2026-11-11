/// @file StabilizerQuickItem.cpp
/// @brief Custom QQuickPaintedItem presenting unstabilized and stabilized video frames.
#include "StabilizerQuickItem.h"

#include <cstdint>

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QRectF>

#include "AtomicTripleBuffer.h"
#include "StabilizerFramePacket.h"
#include "StabilizerPlayerController.h"

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

StabilizerPlayerController* StabilizerQuickItem::controller() const noexcept
{
    return controller_;
}

void StabilizerQuickItem::setController(StabilizerPlayerController* ctrl)
{
    if (controller_ != ctrl) {
        if (controller_ != nullptr) {
            disconnect(controller_, &StabilizerPlayerController::frameReady, this, &StabilizerQuickItem::onFrameReady);
        }
        controller_ = ctrl;
        if (controller_ != nullptr) {
            connect(controller_, &StabilizerPlayerController::frameReady, this, &StabilizerQuickItem::onFrameReady,
                Qt::QueuedConnection);
        }
        emit controllerChanged();
        update();
    }
}

void StabilizerQuickItem::onFrameReady()
{
    if (controller_ == nullptr) {
        return;
    }

    auto& tb = controller_->tripleBuffer();
    if (tb.swapReadBuffer()) {
        const auto& readSlot = tb.getReadBuffer();
        if ((readSlot.width > 0U) && (readSlot.height > 0U) && !readSlot.rawRgb.empty()
            && !readSlot.stabilizedRgb.empty()) {
            rawImage_ = QImage(readSlot.rawRgb.data(), static_cast<int>(readSlot.width),
                static_cast<int>(readSlot.height), static_cast<qsizetype>(readSlot.stride), QImage::Format_RGB888)
                            .copy();

            stabilizedImage_ = QImage(readSlot.stabilizedRgb.data(), static_cast<int>(readSlot.width),
                static_cast<int>(readSlot.height), static_cast<qsizetype>(readSlot.stride), QImage::Format_RGB888)
                                   .copy();

            hasValidFrame_ = true;
            update();
        }
    }
}

void StabilizerQuickItem::renderStandbyPattern(QPainter* painter)
{
    const QRectF rect = boundingRect();
    painter->fillRect(rect, QColor(0x0e, 0x10, 0x14));

    if (showGrid_) {
        // Subtle tactical standby grid
        QPen gridPen(QColor(0x1a, 0x20, 0x2c), 1.0);
        painter->setPen(gridPen);

        constexpr qreal GridStep = 40.0;
        for (qreal x = 0; x < rect.width(); x += GridStep) {
            painter->drawLine(QPointF(x, 0), QPointF(x, rect.height()));
        }
        for (qreal y = 0; y < rect.height(); y += GridStep) {
            painter->drawLine(QPointF(0, y), QPointF(rect.width(), y));
        }

        // Center reticle
        const QPointF center = rect.center();
        QPen reticlePen(QColor(0x00, 0xe5, 0xff, 120), 1.5);
        painter->setPen(reticlePen);
        painter->drawLine(QPointF(center.x() - 24, center.y()), QPointF(center.x() + 24, center.y()));
        painter->drawLine(QPointF(center.x(), center.y() - 24), QPointF(center.x(), center.y() + 24));
        painter->drawEllipse(center, 12, 12);
        painter->drawEllipse(center, 36, 36);

        // Standby legend
        QFont font = painter->font();
        font.setPointSize(10);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        painter->setFont(font);
        painter->setPen(QColor(0x52, 0x5c, 0x70));
        painter->drawText(
            rect, Qt::AlignCenter, QStringLiteral("VIDEO STABILIZER STANDBY\nNO ACTIVE STREAM OR FILE LOADED"));
    }
}

void StabilizerQuickItem::paint(QPainter* painter)
{
    const QRectF itemRect = boundingRect();
    if (!hasValidFrame_ || rawImage_.isNull() || stabilizedImage_.isNull()) {
        renderStandbyPattern(painter);
        return;
    }

    // Letterbox aspect ratio fit within viewport
    const qreal imgW = rawImage_.width();
    const qreal imgH = rawImage_.height();
    const qreal scale = std::min(itemRect.width() / imgW, itemRect.height() / imgH);
    const qreal targetW = imgW * scale;
    const qreal targetH = imgH * scale;
    const qreal targetX = itemRect.left() + ((itemRect.width() - targetW) * 0.5);
    const qreal targetY = itemRect.top() + ((itemRect.height() - targetH) * 0.5);
    const QRectF targetRect(targetX, targetY, targetW, targetH);

    painter->fillRect(itemRect, QColor(0x0e, 0x10, 0x14));

    const qreal splitX = targetRect.left() + (static_cast<qreal>(splitPosition_) * targetRect.width());

    // Left Region: Raw Unstabilized Frame
    painter->save();
    painter->setClipRect(QRectF(targetRect.left(), targetRect.top(), splitX - targetRect.left(), targetRect.height()));
    painter->drawImage(targetRect, rawImage_);
    painter->restore();

    // Right Region: Stabilized Frame
    painter->save();
    painter->setClipRect(QRectF(splitX, targetRect.top(), targetRect.right() - splitX, targetRect.height()));
    painter->drawImage(targetRect, stabilizedImage_);
    painter->restore();

    // Vertical Split Divider Line
    QPen dividerPen(QColor(0x00, 0xe5, 0xff), 2.0);
    painter->setPen(dividerPen);
    painter->drawLine(QPointF(splitX, targetRect.top()), QPointF(splitX, targetRect.bottom()));

    // Tactical Pill Indicator on divider handle
    const qreal handleY = targetRect.top() + (targetRect.height() * 0.5);
    painter->setBrush(QColor(0x00, 0xe5, 0xff));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(QRectF(splitX - 3.0, handleY - 16.0, 6.0, 32.0), 3.0, 3.0);

    // Overlay Header Badges
    QFont labelFont = painter->font();
    labelFont.setPixelSize(11);
    labelFont.setBold(true);
    painter->setFont(labelFont);

    // Raw Badge (Left)
    if (splitX > targetRect.left() + 100.0) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 160));
        painter->drawRoundedRect(QRectF(targetRect.left() + 12.0, targetRect.top() + 12.0, 140.0, 24.0), 4.0, 4.0);
        painter->setPen(QColor(0xff, 0x91, 0x00));
        painter->drawText(QRectF(targetRect.left() + 12.0, targetRect.top() + 12.0, 140.0, 24.0), Qt::AlignCenter,
            QStringLiteral("RAW (UNSTABILIZED)"));
    }

    // Stabilized Badge (Right)
    if (splitX < targetRect.right() - 100.0) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 160));
        painter->drawRoundedRect(QRectF(targetRect.right() - 132.0, targetRect.top() + 12.0, 120.0, 24.0), 4.0, 4.0);
        painter->setPen(QColor(0x00, 0xe6, 0x76));
        painter->drawText(QRectF(targetRect.right() - 132.0, targetRect.top() + 12.0, 120.0, 24.0), Qt::AlignCenter,
            QStringLiteral("STABILIZED"));
    }

    if (showGrid_) {
        // Tactical Center Crosshair
        const QPointF center = targetRect.center();
        QPen reticlePen(QColor(0xff, 0xff, 0xff, 90), 1.0);
        painter->setPen(reticlePen);
        painter->drawLine(QPointF(center.x() - 16, center.y()), QPointF(center.x() + 16, center.y()));
        painter->drawLine(QPointF(center.x(), center.y() - 16), QPointF(center.x(), center.y() + 16));
        painter->drawEllipse(center, 8, 8);
    }
}

} // namespace StabilizerApp
