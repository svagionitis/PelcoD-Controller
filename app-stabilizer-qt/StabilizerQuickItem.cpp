/// @file StabilizerQuickItem.cpp
/// @brief Custom QQuickPaintedItem presenting unstabilized and stabilized video frames.
#include "StabilizerQuickItem.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QCursor>
#include <QFont>
#include <QHoverEvent>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QWheelEvent>

#include "AtomicTripleBuffer.h"
#include "OverlayRenderer.h"
#include "PresentationTypes.h"
#include "StabilizerFramePacket.h"
#include "StabilizerPlayerController.h"

namespace StabilizerApp {

StabilizerQuickItem::StabilizerQuickItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
}

StabilizerQuickItem::PresentationMode StabilizerQuickItem::presentationMode() const noexcept
{
    return mode_;
}

void StabilizerQuickItem::setPresentationMode(PresentationMode mode) noexcept
{
    if (mode_ != mode) {
        mode_ = mode;
        emit presentationModeChanged();
        update();
    }
}

StabilizerQuickItem::SplitOrientation StabilizerQuickItem::splitOrientation() const noexcept
{
    return orientation_;
}

void StabilizerQuickItem::setSplitOrientation(SplitOrientation orient) noexcept
{
    if (orientation_ != orient) {
        orientation_ = orient;
        emit splitOrientationChanged();
        update();
    }
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

bool StabilizerQuickItem::showStabilizedInToggle() const noexcept
{
    return showStabilizedInToggle_;
}

void StabilizerQuickItem::setShowStabilizedInToggle(bool show) noexcept
{
    if (showStabilizedInToggle_ != show) {
        showStabilizedInToggle_ = show;
        emit showStabilizedInToggleChanged();
        update();
    }
}

bool StabilizerQuickItem::showLandmarks() const noexcept
{
    return overlays_.showLandmarks;
}

void StabilizerQuickItem::setShowLandmarks(bool show) noexcept
{
    if (overlays_.showLandmarks != show) {
        overlays_.showLandmarks = show;
        emit showLandmarksChanged();
        update();
    }
}

bool StabilizerQuickItem::showQuiver() const noexcept
{
    return overlays_.showQuiver;
}

void StabilizerQuickItem::setShowQuiver(bool show) noexcept
{
    if (overlays_.showQuiver != show) {
        overlays_.showQuiver = show;
        emit showQuiverChanged();
        update();
    }
}

bool StabilizerQuickItem::showCropMargin() const noexcept
{
    return overlays_.showCropMargin;
}

void StabilizerQuickItem::setShowCropMargin(bool show) noexcept
{
    if (overlays_.showCropMargin != show) {
        overlays_.showCropMargin = show;
        emit showCropMarginChanged();
        update();
    }
}

bool StabilizerQuickItem::showReticle() const noexcept
{
    return overlays_.showReticle;
}

void StabilizerQuickItem::setShowReticle(bool show) noexcept
{
    if (overlays_.showReticle != show) {
        overlays_.showReticle = show;
        emit showReticleChanged();
        update();
    }
}

bool StabilizerQuickItem::showGrid() const noexcept
{
    return overlays_.showGrid;
}

void StabilizerQuickItem::setShowGrid(bool show) noexcept
{
    if (overlays_.showGrid != show) {
        overlays_.showGrid = show;
        emit showGridChanged();
        update();
    }
}

float StabilizerQuickItem::zoomFactor() const noexcept
{
    return zoomFactor_;
}

void StabilizerQuickItem::setZoomFactor(float zoom) noexcept
{
    const float clamped = std::clamp(zoom, 1.0F, 8.0F);
    if (std::abs(zoomFactor_ - clamped) > 1e-3F) {
        zoomFactor_ = clamped;
        if (std::abs(zoomFactor_ - 1.0F) < 1e-3F) {
            panOffset_ = QPointF(0.0, 0.0);
            emit panOffsetChanged();
        }
        emit zoomFactorChanged();
        update();
    }
}

QPointF StabilizerQuickItem::panOffset() const noexcept
{
    return panOffset_;
}

void StabilizerQuickItem::setPanOffset(const QPointF& offset) noexcept
{
    if (panOffset_ != offset) {
        panOffset_ = offset;
        emit panOffsetChanged();
        update();
    }
}

float StabilizerQuickItem::quiverScale() const noexcept
{
    return overlays_.quiverScale;
}

void StabilizerQuickItem::setQuiverScale(float scale) noexcept
{
    const float clamped = std::clamp(scale, 0.5F, 10.0F);
    if (std::abs(overlays_.quiverScale - clamped) > 1e-3F) {
        overlays_.quiverScale = clamped;
        emit quiverScaleChanged();
        update();
    }
}

float StabilizerQuickItem::heatmapGain() const noexcept
{
    return heatmapGain_;
}

void StabilizerQuickItem::setHeatmapGain(float gain) noexcept
{
    const float clamped = std::clamp(gain, 1.0F, 10.0F);
    if (std::abs(heatmapGain_ - clamped) > 1e-3F) {
        heatmapGain_ = clamped;
        emit heatmapGainChanged();
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

void StabilizerQuickItem::resetZoomAndPan() noexcept
{
    zoomFactor_ = 1.0F;
    panOffset_ = QPointF(0.0, 0.0);
    emit zoomFactorChanged();
    emit panOffsetChanged();
    update();
}

void StabilizerQuickItem::toggleSourceView() noexcept
{
    showStabilizedInToggle_ = !showStabilizedInToggle_;
    emit showStabilizedInToggleChanged();
    update();
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

            const auto w = static_cast<int>(readSlot.width);
            const auto h = static_cast<int>(readSlot.height);
            const auto s = static_cast<qsizetype>(readSlot.stride);

            if ((rawImage_.width() != w) || (rawImage_.height() != h)) {
                heatmapBuffer_.assign(readSlot.rawRgb.size(), 0U);
                heatmapImage_ = QImage(heatmapBuffer_.data(), w, h, s, QImage::Format_RGB888);
            }

            rawImage_ = QImage(readSlot.rawRgb.data(), w, h, s, QImage::Format_RGB888).copy();
            stabilizedImage_ = QImage(readSlot.stabilizedRgb.data(), w, h, s, QImage::Format_RGB888).copy();

            cachedFeatureCount_ = std::min(readSlot.featureCount, cachedFeatures_.size());
            for (std::size_t i = 0U; i < cachedFeatureCount_; ++i) {
                cachedFeatures_[i] = readSlot.features[i];
            }

            hasValidFrame_ = true;
            update();
        }
    }
}

QRectF StabilizerQuickItem::calculateAspectRect(const QRectF& bounds, qreal imgW, qreal imgH) const noexcept
{
    if ((imgW <= 0.0) || (imgH <= 0.0) || (bounds.width() <= 0.0) || (bounds.height() <= 0.0)) {
        return bounds;
    }

    const qreal scale = std::min(bounds.width() / imgW, bounds.height() / imgH);
    const qreal targetW = imgW * scale;
    const qreal targetH = imgH * scale;
    const qreal targetX = bounds.left() + ((bounds.width() - targetW) * 0.5);
    const qreal targetY = bounds.top() + ((bounds.height() - targetH) * 0.5);

    return QRectF(targetX, targetY, targetW, targetH);
}

bool StabilizerQuickItem::hitTestDivider(const QPointF& pos, const QRectF& targetRect) const noexcept
{
    if (orientation_ == SplitOrientation::Vertical) {
        const qreal splitX = targetRect.left() + (static_cast<qreal>(splitPosition_) * targetRect.width());
        return (std::abs(pos.x() - splitX) <= 8.0) && (pos.y() >= targetRect.top()) && (pos.y() <= targetRect.bottom());
    }

    const qreal splitY = targetRect.top() + (static_cast<qreal>(splitPosition_) * targetRect.height());
    return (std::abs(pos.y() - splitY) <= 8.0) && (pos.x() >= targetRect.left()) && (pos.x() <= targetRect.right());
}

void StabilizerQuickItem::mousePressEvent(QMouseEvent* event)
{
    if (!hasValidFrame_) {
        QQuickPaintedItem::mousePressEvent(event);
        return;
    }

    const QRectF itemRect = boundingRect();
    const QRectF baseRect = calculateAspectRect(itemRect, rawImage_.width(), rawImage_.height());

    if (event->button() == Qt::LeftButton) {
        if ((mode_ == PresentationMode::SplitWipe) && hitTestDivider(event->position(), baseRect)) {
            isDraggingDivider_ = true;
            event->accept();
            return;
        }

        if (mode_ == PresentationMode::SingleToggle) {
            toggleSourceView();
            event->accept();
            return;
        }

        if (zoomFactor_ > 1.001F) {
            isPanning_ = true;
            lastMousePos_ = event->position();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    } else if ((event->button() == Qt::RightButton) || (event->button() == Qt::MiddleButton)) {
        if (zoomFactor_ > 1.001F) {
            isPanning_ = true;
            lastMousePos_ = event->position();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }

    QQuickPaintedItem::mousePressEvent(event);
}

void StabilizerQuickItem::mouseMoveEvent(QMouseEvent* event)
{
    if (!hasValidFrame_) {
        QQuickPaintedItem::mouseMoveEvent(event);
        return;
    }

    const QRectF itemRect = boundingRect();
    const QRectF baseRect = calculateAspectRect(itemRect, rawImage_.width(), rawImage_.height());

    if (isDraggingDivider_ && (mode_ == PresentationMode::SplitWipe)) {
        qreal pos = 0.5;
        if (orientation_ == SplitOrientation::Vertical) {
            pos = (event->position().x() - baseRect.left()) / baseRect.width();
        } else {
            pos = (event->position().y() - baseRect.top()) / baseRect.height();
        }
        setSplitPosition(static_cast<float>(pos));
        event->accept();
        return;
    }

    if (isPanning_) {
        const QPointF delta = event->position() - lastMousePos_;
        panOffset_ += delta;
        lastMousePos_ = event->position();
        emit panOffsetChanged();
        update();
        event->accept();
        return;
    }

    QQuickPaintedItem::mouseMoveEvent(event);
}

void StabilizerQuickItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (isDraggingDivider_) {
        isDraggingDivider_ = false;
        event->accept();
        return;
    }

    if (isPanning_) {
        isPanning_ = false;
        setCursor((zoomFactor_ > 1.001F) ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }

    QQuickPaintedItem::mouseReleaseEvent(event);
}

void StabilizerQuickItem::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        resetZoomAndPan();
        event->accept();
        return;
    }

    QQuickPaintedItem::mouseDoubleClickEvent(event);
}

void StabilizerQuickItem::wheelEvent(QWheelEvent* event)
{
    const qreal numDegrees = event->angleDelta().y() / 8.0;
    const qreal numSteps = numDegrees / 15.0;
    const auto zoomDelta = static_cast<float>(numSteps * 0.20);

    const float newZoom = std::clamp(zoomFactor_ + zoomDelta, 1.0F, 8.0F);
    setZoomFactor(newZoom);
    event->accept();
}

void StabilizerQuickItem::hoverMoveEvent(QHoverEvent* event)
{
    if (!hasValidFrame_) {
        setCursor(Qt::ArrowCursor);
        return;
    }

    const QRectF itemRect = boundingRect();
    const QRectF baseRect = calculateAspectRect(itemRect, rawImage_.width(), rawImage_.height());

    if ((mode_ == PresentationMode::SplitWipe) && hitTestDivider(event->position(), baseRect)) {
        setCursor((orientation_ == SplitOrientation::Vertical) ? Qt::SplitHCursor : Qt::SplitVCursor);
    } else if (zoomFactor_ > 1.001F) {
        setCursor(isPanning_ ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void StabilizerQuickItem::renderStandbyPattern(QPainter* painter)
{
    const QRectF rect = boundingRect();
    painter->fillRect(rect, QColor(0x0e, 0x10, 0x14));

    if (overlays_.showGrid) {
        QPen gridPen(QColor(0x1a, 0x20, 0x2c), 1.0);
        painter->setPen(gridPen);

        constexpr qreal GridStep = 40.0;
        for (qreal x = 0; x < rect.width(); x += GridStep) {
            painter->drawLine(QPointF(x, 0), QPointF(x, rect.height()));
        }
        for (qreal y = 0; y < rect.height(); y += GridStep) {
            painter->drawLine(QPointF(0, y), QPointF(rect.width(), y));
        }

        OverlayRenderer::renderReticle(painter, rect);

        QFont font = painter->font();
        font.setPointSize(10);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        painter->setFont(font);
        painter->setPen(QColor(0x52, 0x5c, 0x70));
        painter->drawText(
            rect, Qt::AlignCenter, QStringLiteral("VIDEO STABILIZER STANDBY\nNO ACTIVE STREAM OR FILE LOADED"));
    }
}

            QPainter * painter, const QRectF& targetRect)
            {
                const qreal imgW = rawImage_.width();
                const qreal imgH = rawImage_.height();
                const auto scaleX = static_cast<float>(targetRect.width() / imgW);
                const auto scaleY = static_cast<float>(targetRect.height() / imgH);

                if (orientation_ == SplitOrientation::Vertical) {
                    const qreal splitX = targetRect.left() + (static_cast<qreal>(splitPosition_) * targetRect.width());

                    // Left Region: Raw Unstabilized Frame
                    painter->save();
                    painter->setClipRect(
                        QRectF(targetRect.left(), targetRect.top(), splitX - targetRect.left(), targetRect.height()));
                    painter->drawImage(targetRect, rawImage_);
                    if (overlays_.showLandmarks) {
                        OverlayRenderer::renderLandmarks(
                            painter, targetRect, cachedFeatures_.data(), cachedFeatureCount_, scaleX, scaleY);
                    }
                    if (overlays_.showQuiver) {
                        OverlayRenderer::renderQuiverPlot(painter, targetRect, cachedFeatures_.data(),
                            cachedFeatureCount_, scaleX, scaleY, overlays_.quiverScale);
                    }
                    painter->restore();

                    // Right Region: Stabilized Frame
                    painter->save();
                    painter->setClipRect(
                        QRectF(splitX, targetRect.top(), targetRect.right() - splitX, targetRect.height()));
                    painter->drawImage(targetRect, stabilizedImage_);
                    if (overlays_.showCropMargin) {
                        OverlayRenderer::renderCropBoundary(
                            painter, targetRect, overlays_.cropRatio, overlays_.softMarginRatio);
                    }
                    if (overlays_.showReticle) {
                        OverlayRenderer::renderReticle(painter, targetRect);
                    }
                    painter->restore();

                    // Split Divider Line
                    QPen dividerPen(QColor(0x00, 0xe5, 0xff), 2.0);
                    painter->setPen(dividerPen);
                    painter->drawLine(QPointF(splitX, targetRect.top()), QPointF(splitX, targetRect.bottom()));

                    // Divider Handle Pill
                    const qreal handleY = targetRect.top() + (targetRect.height() * 0.5);
                    painter->setBrush(QColor(0x00, 0xe5, 0xff));
                    painter->setPen(Qt::NoPen);
                    painter->drawRoundedRect(QRectF(splitX - 3.0, handleY - 16.0, 6.0, 32.0), 3.0, 3.0);

                    // Header Badges
                    if (splitX > targetRect.left() + 110.0) {
                        OverlayRenderer::renderBadge(painter,
                            QRectF(targetRect.left() + 12.0, targetRect.top() + 12.0, 140.0, 24.0),
                            QStringLiteral("RAW (UNSTABILIZED)"), QColor(0xff, 0x91, 0x00), QColor(0, 0, 0, 160));
                    }
                    if (splitX < targetRect.right() - 110.0) {
                        OverlayRenderer::renderBadge(painter,
                            QRectF(targetRect.right() - 112.0, targetRect.top() + 12.0, 100.0, 24.0),
                            QStringLiteral("STABILIZED"), QColor(0x00, 0xe6, 0x76), QColor(0, 0, 0, 160));
                    }
                } else {
                    // Horizontal Wipe
                    const qreal splitY = targetRect.top() + (static_cast<qreal>(splitPosition_) * targetRect.height());

                    // Top Region: Raw Unstabilized Frame
                    painter->save();
                    painter->setClipRect(
                        QRectF(targetRect.left(), targetRect.top(), targetRect.width(), splitY - targetRect.top()));
                    painter->drawImage(targetRect, rawImage_);
                    if (overlays_.showLandmarks) {
                        OverlayRenderer::renderLandmarks(
                            painter, targetRect, cachedFeatures_.data(), cachedFeatureCount_, scaleX, scaleY);
                    }
                    painter->restore();

                    // Bottom Region: Stabilized Frame
                    painter->save();
                    painter->setClipRect(
                        QRectF(targetRect.left(), splitY, targetRect.width(), targetRect.bottom() - splitY));
                    painter->drawImage(targetRect, stabilizedImage_);
                    if (overlays_.showCropMargin) {
                        OverlayRenderer::renderCropBoundary(
                            painter, targetRect, overlays_.cropRatio, overlays_.softMarginRatio);
                    }
                    if (overlays_.showReticle) {
                        OverlayRenderer::renderReticle(painter, targetRect);
                    }
                    painter->restore();

                    // Horizontal Divider Line
                    QPen dividerPen(QColor(0x00, 0xe5, 0xff), 2.0);
                    painter->setPen(dividerPen);
                    painter->drawLine(QPointF(targetRect.left(), splitY), QPointF(targetRect.right(), splitY));

                    const qreal handleX = targetRect.left() + (targetRect.width() * 0.5);
                    painter->setBrush(QColor(0x00, 0xe5, 0xff));
                    painter->setPen(Qt::NoPen);
                    painter->drawRoundedRect(QRectF(handleX - 16.0, splitY - 3.0, 32.0, 6.0), 3.0, 3.0);
                }
            }

            void StabilizerQuickItem::renderSideBySide(QPainter* painter, const QRectF& itemRect)
            {
                const qreal halfW = (itemRect.width() * 0.5) - 2.0;
                const QRectF leftBounds(itemRect.left(), itemRect.top(), halfW, itemRect.height());
                const QRectF rightBounds(itemRect.left() + halfW + 4.0, itemRect.top(), halfW, itemRect.height());

                const QRectF leftRect = calculateAspectRect(leftBounds, rawImage_.width(), rawImage_.height());
                const QRectF rightRect
                    = calculateAspectRect(rightBounds, stabilizedImage_.width(), stabilizedImage_.height());

                // Vertical partition separator
                QPen sepPen(QColor(0x24, 0x29, 0x38), 2.0);
                painter->setPen(sepPen);
                painter->drawLine(QPointF(itemRect.left() + halfW + 2.0, itemRect.top()),
                    QPointF(itemRect.left() + halfW + 2.0, itemRect.bottom()));

                const auto scaleX = static_cast<float>(leftRect.width() / rawImage_.width());
                const auto scaleY = static_cast<float>(leftRect.height() / rawImage_.height());

                // Left Viewport: Raw
                painter->drawImage(leftRect, rawImage_);
                if (overlays_.showLandmarks) {
                    OverlayRenderer::renderLandmarks(
                        painter, leftRect, cachedFeatures_.data(), cachedFeatureCount_, scaleX, scaleY);
                }
                if (overlays_.showQuiver) {
                    OverlayRenderer::renderQuiverPlot(painter, leftRect, cachedFeatures_.data(), cachedFeatureCount_,
                        scaleX, scaleY, overlays_.quiverScale);
                }
                OverlayRenderer::renderBadge(painter,
                    QRectF(leftRect.left() + 12.0, leftRect.top() + 12.0, 140.0, 24.0),
                    QStringLiteral("RAW (UNSTABILIZED)"), QColor(0xff, 0x91, 0x00), QColor(0, 0, 0, 160));

                // Right Viewport: Stabilized
                painter->drawImage(rightRect, stabilizedImage_);
                if (overlays_.showCropMargin) {
                    OverlayRenderer::renderCropBoundary(
                        painter, rightRect, overlays_.cropRatio, overlays_.softMarginRatio);
                }
                if (overlays_.showReticle) {
                    OverlayRenderer::renderReticle(painter, rightRect);
                }
                OverlayRenderer::renderBadge(painter,
                    QRectF(rightRect.right() - 112.0, rightRect.top() + 12.0, 100.0, 24.0),
                    QStringLiteral("STABILIZED"), QColor(0x00, 0xe6, 0x76), QColor(0, 0, 0, 160));
            }

            void StabilizerQuickItem::renderSingleToggle(QPainter* painter, const QRectF& targetRect)
            {
                const qreal imgW = rawImage_.width();
                const qreal imgH = rawImage_.height();
                const auto scaleX = static_cast<float>(targetRect.width() / imgW);
                const auto scaleY = static_cast<float>(targetRect.height() / imgH);

                if (showStabilizedInToggle_) {
                    painter->drawImage(targetRect, stabilizedImage_);
                    if (overlays_.showCropMargin) {
                        OverlayRenderer::renderCropBoundary(
                            painter, targetRect, overlays_.cropRatio, overlays_.softMarginRatio);
                    }
                    if (overlays_.showReticle) {
                        OverlayRenderer::renderReticle(painter, targetRect);
                    }
                    OverlayRenderer::renderBadge(painter,
                        QRectF(targetRect.left() + 12.0, targetRect.top() + 12.0, 100.0, 24.0),
                        QStringLiteral("STABILIZED"), QColor(0x00, 0xe6, 0x76), QColor(0, 0, 0, 160));
                } else {
                    painter->drawImage(targetRect, rawImage_);
                    if (overlays_.showLandmarks) {
                        OverlayRenderer::renderLandmarks(
                            painter, targetRect, cachedFeatures_.data(), cachedFeatureCount_, scaleX, scaleY);
                    }
                    if (overlays_.showQuiver) {
                        OverlayRenderer::renderQuiverPlot(painter, targetRect, cachedFeatures_.data(),
                            cachedFeatureCount_, scaleX, scaleY, overlays_.quiverScale);
                    }
                    OverlayRenderer::renderBadge(painter,
                        QRectF(targetRect.left() + 12.0, targetRect.top() + 12.0, 140.0, 24.0),
                        QStringLiteral("RAW (UNSTABILIZED)"), QColor(0xff, 0x91, 0x00), QColor(0, 0, 0, 160));
                }
            }

            void StabilizerQuickItem::renderDifferenceHeatmap(QPainter* painter, const QRectF& targetRect)
            {
                if ((heatmapBuffer_.size() == static_cast<std::size_t>(rawImage_.sizeInBytes()))
                    && (heatmapBuffer_.size() == static_cast<std::size_t>(stabilizedImage_.sizeInBytes()))) {

                    const auto pixelCount
                        = static_cast<std::size_t>(rawImage_.width()) * static_cast<std::size_t>(rawImage_.height());
                    heatmapEngine_.computeDifference(
                        rawImage_.bits(), stabilizedImage_.bits(), heatmapBuffer_.data(), pixelCount, heatmapGain_);

                    painter->drawImage(targetRect, heatmapImage_);
                }

                if (overlays_.showReticle) {
                    OverlayRenderer::renderReticle(painter, targetRect);
                }

                OverlayRenderer::renderBadge(painter,
                    QRectF(targetRect.left() + 12.0, targetRect.top() + 12.0, 180.0, 24.0),
                    QStringLiteral("DIFFERENCE HEATMAP (TURBO)"), QColor(0x00, 0xe5, 0xff), QColor(0, 0, 0, 160));
            }

            void StabilizerQuickItem::paint(QPainter* painter)
            {
                const QRectF itemRect = boundingRect();
                painter->fillRect(itemRect, QColor(0x0e, 0x10, 0x14));

                if (!hasValidFrame_ || rawImage_.isNull() || stabilizedImage_.isNull()) {
                    renderStandbyPattern(painter);
                    return;
                }

                if (mode_ == PresentationMode::SideBySide) {
                    renderSideBySide(painter, itemRect);
                    return;
                }

                const QRectF baseRect = calculateAspectRect(itemRect, rawImage_.width(), rawImage_.height());

                // Apply interactive zoom and pan transform
                const QPointF center = baseRect.center() + panOffset_;
                const qreal w = baseRect.width() * static_cast<qreal>(zoomFactor_);
                const qreal h = baseRect.height() * static_cast<qreal>(zoomFactor_);
                const QRectF targetRect(center.x() - (w * 0.5), center.y() - (h * 0.5), w, h);

                switch (mode_) {
                case PresentationMode::SplitWipe:
                    renderSplitWipe(painter, targetRect);
                    break;
                case PresentationMode::SingleToggle:
                    renderSingleToggle(painter, targetRect);
                    break;
                case PresentationMode::DifferenceHeatmap:
                    renderDifferenceHeatmap(painter, targetRect);
                    break;
                default:
                    renderSplitWipe(painter, targetRect);
                    break;
                }
            }

            } // namespace StabilizerApp
