#pragma once

/// @file OverlayRenderer.h
/// @brief Zero-heap tactical overlay rendering routines for QPainter.

#include <cstddef>
#include <cstdint>

#include <QColor>
#include <QPainter>
#include <QRectF>
#include <QString>

#include "StabilizerFramePacket.h"

namespace StabilizerApp {

/// @struct OverlayOptions
/// @brief Configuration toggles for diagnostic overlays.
struct OverlayOptions {
    bool showLandmarks { true };
    bool showQuiver { true };
    bool showCropMargin { true };
    bool showReticle { true };
    bool showGrid { true };
    float quiverScale { 2.5F };
    float cropRatio { 0.05F };
    float softMarginRatio { 0.80F };
};

/// @class OverlayRenderer
/// @brief Helper class rendering overlays into active QPainter context.
class OverlayRenderer {
public:
    /// @brief Render tactical center reticle and crosshair.
    /// @param painter Target QPainter.
    /// @param targetRect Target bounding rectangle.
    static void renderReticle(QPainter* painter, const QRectF& targetRect) noexcept;

    /// @brief Render feature landmark points (green inliers, red outliers).
    /// @param painter Target QPainter.
    /// @param targetRect Target bounding rectangle.
    /// @param features Array of visual features.
    /// @param count Number of features in array.
    /// @param scaleX Horizontal scaling from video space to targetRect.
    /// @param scaleY Vertical scaling from video space to targetRect.
    static void renderLandmarks(QPainter* painter, const QRectF& targetRect, const VisualFeaturePoint* features,
        std::size_t count, float scaleX, float scaleY) noexcept;

    /// @brief Render optical flow quiver displacement vectors.
    /// @param painter Target QPainter.
    /// @param targetRect Target bounding rectangle.
    /// @param features Array of visual features.
    /// @param count Number of features in array.
    /// @param scaleX Horizontal scaling from video space to targetRect.
    /// @param scaleY Vertical scaling from video space to targetRect.
    /// @param quiverScale Visual vector magnification factor.
    static void renderQuiverPlot(QPainter* painter, const QRectF& targetRect, const VisualFeaturePoint* features,
        std::size_t count, float scaleX, float scaleY, float quiverScale) noexcept;

    /// @brief Render auto-crop border and soft damping boundary.
    /// @param painter Target QPainter.
    /// @param targetRect Target bounding rectangle.
    /// @param cropRatio Maximum auto-crop fraction.
    /// @param softMarginRatio Ratio engaging soft boundary damping.
    static void renderCropBoundary(
        QPainter* painter, const QRectF& targetRect, float cropRatio, float softMarginRatio) noexcept;

    /// @brief Render tactical status badges and labels.
    /// @param painter Target QPainter.
    /// @param badgeRect Bounding box for badge pill.
    /// @param text Label string.
    /// @param textColor Font foreground color.
    /// @param bgColor Background pill fill color.
    static void renderBadge(QPainter* painter, const QRectF& badgeRect, const QString& text, const QColor& textColor,
        const QColor& bgColor) noexcept;
};

} // namespace StabilizerApp
