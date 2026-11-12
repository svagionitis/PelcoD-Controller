#pragma once

/// @file StabilizerQuickItem.h
/// @brief Custom QQuickPaintedItem presenting unstabilized and stabilized video frames.

#include <cstddef>
#include <cstdint>

#include <array>
#include <vector>

#include <QColor>
#include <QHoverEvent>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QRectF>
#include <QWheelEvent>

#include "HeatmapColorMap.h"
#include "OverlayRenderer.h"
#include "PresentationTypes.h"
#include "StabilizerFramePacket.h"

namespace StabilizerApp {

class StabilizerPlayerController;

/// @class StabilizerQuickItem
/// @brief Renders video presentation viewport with interactive comparison wipe and overlays.
class StabilizerQuickItem : public QQuickPaintedItem {
    Q_OBJECT

public:
    /// @brief Video presentation layout modes.
    enum PresentationMode {
        SplitWipe = 0, ///< Interactive split-screen wipe divider
        SideBySide, ///< Synchronized dual-viewport view
        SingleToggle, ///< Fullscreen view toggling between raw and stabilized
        DifferenceHeatmap ///< Pixel-difference heatmap with Turbo colormap LUT
    };
    Q_ENUM(PresentationMode)

    /// @brief Orientation of the split wipe divider.
    enum SplitOrientation {
        Vertical = 0, ///< Vertical divider (left = raw, right = stabilized)
        Horizontal ///< Horizontal divider (top = raw, bottom = stabilized)
    };
    Q_ENUM(SplitOrientation)

    Q_PROPERTY(PresentationMode presentationMode READ presentationMode WRITE setPresentationMode NOTIFY
            presentationModeChanged)
    Q_PROPERTY(SplitOrientation splitOrientation READ splitOrientation WRITE setSplitOrientation NOTIFY
            splitOrientationChanged)
    Q_PROPERTY(float splitPosition READ splitPosition WRITE setSplitPosition NOTIFY splitPositionChanged)
    Q_PROPERTY(bool showStabilizedInToggle READ showStabilizedInToggle WRITE setShowStabilizedInToggle NOTIFY
            showStabilizedInToggleChanged)

    Q_PROPERTY(bool showLandmarks READ showLandmarks WRITE setShowLandmarks NOTIFY showLandmarksChanged)
    Q_PROPERTY(bool showQuiver READ showQuiver WRITE setShowQuiver NOTIFY showQuiverChanged)
    Q_PROPERTY(bool showCropMargin READ showCropMargin WRITE setShowCropMargin NOTIFY showCropMarginChanged)
    Q_PROPERTY(bool showReticle READ showReticle WRITE setShowReticle NOTIFY showReticleChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)

    Q_PROPERTY(float zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY zoomFactorChanged)
    Q_PROPERTY(QPointF panOffset READ panOffset WRITE setPanOffset NOTIFY panOffsetChanged)
    Q_PROPERTY(float quiverScale READ quiverScale WRITE setQuiverScale NOTIFY quiverScaleChanged)
    Q_PROPERTY(float heatmapGain READ heatmapGain WRITE setHeatmapGain NOTIFY heatmapGainChanged)

    Q_PROPERTY(StabilizerApp::StabilizerPlayerController* controller READ controller WRITE setController NOTIFY
            controllerChanged)

    explicit StabilizerQuickItem(QQuickItem* parent = nullptr);
    ~StabilizerQuickItem() override = default;

    StabilizerQuickItem(const StabilizerQuickItem&) = delete;
    StabilizerQuickItem& operator=(const StabilizerQuickItem&) = delete;
    StabilizerQuickItem(StabilizerQuickItem&&) = delete;
    StabilizerQuickItem& operator=(StabilizerQuickItem&&) = delete;

    [[nodiscard]] PresentationMode presentationMode() const noexcept;
    void setPresentationMode(PresentationMode mode) noexcept;

    [[nodiscard]] SplitOrientation splitOrientation() const noexcept;
    void setSplitOrientation(SplitOrientation orient) noexcept;

    [[nodiscard]] float splitPosition() const noexcept;
    void setSplitPosition(float pos) noexcept;

    [[nodiscard]] bool showStabilizedInToggle() const noexcept;
    void setShowStabilizedInToggle(bool show) noexcept;

    [[nodiscard]] bool showLandmarks() const noexcept;
    void setShowLandmarks(bool show) noexcept;

    [[nodiscard]] bool showQuiver() const noexcept;
    void setShowQuiver(bool show) noexcept;

    [[nodiscard]] bool showCropMargin() const noexcept;
    void setShowCropMargin(bool show) noexcept;

    [[nodiscard]] bool showReticle() const noexcept;
    void setShowReticle(bool show) noexcept;

    [[nodiscard]] bool showGrid() const noexcept;
    void setShowGrid(bool show) noexcept;

    [[nodiscard]] float zoomFactor() const noexcept;
    void setZoomFactor(float zoom) noexcept;

    [[nodiscard]] QPointF panOffset() const noexcept;
    void setPanOffset(const QPointF& offset) noexcept;

    [[nodiscard]] float quiverScale() const noexcept;
    void setQuiverScale(float scale) noexcept;

    [[nodiscard]] float heatmapGain() const noexcept;
    void setHeatmapGain(float gain) noexcept;

    [[nodiscard]] StabilizerPlayerController* controller() const noexcept;
    void setController(StabilizerPlayerController* ctrl);

    Q_INVOKABLE void resetZoomAndPan() noexcept;
    Q_INVOKABLE void toggleSourceView() noexcept;

    void paint(QPainter* painter) override;

public slots:
    void onFrameReady();

signals:
    void presentationModeChanged();
    void splitOrientationChanged();
    void splitPositionChanged();
    void showStabilizedInToggleChanged();
    void showLandmarksChanged();
    void showQuiverChanged();
    void showCropMarginChanged();
    void showReticleChanged();
    void showGridChanged();
    void zoomFactorChanged();
    void panOffsetChanged();
    void quiverScaleChanged();
    void heatmapGainChanged();
    void controllerChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;

private:
    void renderStandbyPattern(QPainter* painter);
    void renderSplitWipe(QPainter* painter, const QRectF& targetRect);
    void renderSideBySide(QPainter* painter, const QRectF& itemRect);
    void renderSingleToggle(QPainter* painter, const QRectF& targetRect);
    void renderDifferenceHeatmap(QPainter* painter, const QRectF& targetRect);

    [[nodiscard]] QRectF calculateAspectRect(const QRectF& bounds, qreal imgW, qreal imgH) const noexcept;
    [[nodiscard]] bool hitTestDivider(const QPointF& pos, const QRectF& targetRect) const noexcept;

    PresentationMode mode_ { PresentationMode::SplitWipe };
    SplitOrientation orientation_ { SplitOrientation::Vertical };
    float splitPosition_ { 0.50F };
    bool showStabilizedInToggle_ { true };

    OverlayOptions overlays_ { };
    float zoomFactor_ { 1.0F };
    QPointF panOffset_ { 0.0, 0.0 };
    float heatmapGain_ { 3.0F };

    bool isDraggingDivider_ { false };
    bool isPanning_ { false };
    QPointF lastMousePos_ { };

    StabilizerPlayerController* controller_ { nullptr };
    HeatmapColorMap heatmapEngine_ { };

    QImage rawImage_ { };
    QImage stabilizedImage_ { };
    QImage heatmapImage_ { };
    std::vector<std::uint8_t> heatmapBuffer_ { };

    std::array<VisualFeaturePoint, StabilizerFramePacket::MaxVisualFeatures> cachedFeatures_ { };
    std::size_t cachedFeatureCount_ { 0U };
    float activeCropRatio_ { 0.05F };
    bool hasValidFrame_ { false };
};

} // namespace StabilizerApp
