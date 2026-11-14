#pragma once

/// @file LensVideoQuickItem.h
/// @brief Custom QQuickPaintedItem presenting video viewport, reticles, and optical ROI.

#include <cstddef>
#include <cstdint>

#include <QColor>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QRectF>

namespace LensApp {

class LensPlayerController;
struct LensFramePacket;

/// @class LensVideoQuickItem
/// @brief Renders tactical viewport canvas with aspect-ratio letterboxing, draggable ROI, and focus peaking.
class LensVideoQuickItem : public QQuickPaintedItem {
    Q_OBJECT

    Q_PROPERTY(bool showPeaking READ showPeaking WRITE setShowPeaking NOTIFY showPeakingChanged)
    Q_PROPERTY(int peakingThreshold READ peakingThreshold WRITE setPeakingThreshold NOTIFY peakingThresholdChanged)
    Q_PROPERTY(int peakingColorIndex READ peakingColorIndex WRITE setPeakingColorIndex NOTIFY peakingColorChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)
    Q_PROPERTY(bool showReticle READ showReticle WRITE setShowReticle NOTIFY showReticleChanged)
    Q_PROPERTY(bool showRoi READ showRoi WRITE setShowRoi NOTIFY showRoiChanged)
    Q_PROPERTY(QRectF roiRect READ roiRect WRITE setRoiRect NOTIFY roiRectChanged)
    Q_PROPERTY(bool isRoiLockedToTrack READ isRoiLockedToTrack WRITE setIsRoiLockedToTrack NOTIFY trackerLockChanged)
    Q_PROPERTY(LensPlayerController* controller READ controller WRITE setController NOTIFY controllerChanged)

public:
    enum class DragHandle : std::uint8_t {
        None = 0U,
        InsideRoi,
        TopLeft,
        TopCenter,
        TopRight,
        MiddleLeft,
        MiddleRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    explicit LensVideoQuickItem(QQuickItem* parent = nullptr);
    ~LensVideoQuickItem() override = default;

    LensVideoQuickItem(const LensVideoQuickItem&) = delete;
    LensVideoQuickItem& operator=(const LensVideoQuickItem&) = delete;
    LensVideoQuickItem(LensVideoQuickItem&&) = delete;
    LensVideoQuickItem& operator=(LensVideoQuickItem&&) = delete;

    void paint(QPainter* painter) override;

    [[nodiscard]] bool showPeaking() const noexcept;
    [[nodiscard]] int peakingThreshold() const noexcept;
    [[nodiscard]] int peakingColorIndex() const noexcept;
    [[nodiscard]] bool showGrid() const noexcept;
    [[nodiscard]] bool showReticle() const noexcept;
    [[nodiscard]] bool showRoi() const noexcept;
    [[nodiscard]] QRectF roiRect() const noexcept;
    [[nodiscard]] bool isRoiLockedToTrack() const noexcept;
    [[nodiscard]] LensPlayerController* controller() const noexcept;

public slots:
    void setShowPeaking(bool show);
    void setPeakingThreshold(int thresh);
    void setPeakingColorIndex(int idx);
    void setShowGrid(bool show);
    void setShowReticle(bool show);
    void setShowRoi(bool show);
    void setRoiRect(const QRectF& rect);
    void setIsRoiLockedToTrack(bool locked);
    void applyRoiPreset(int presetIdx);
    void setTrackerTargetBox(double nx, double ny, double nw, double nh);
    void setController(LensPlayerController* ctrl);
    void onFrameReady();

signals:
    void showPeakingChanged();
    void peakingThresholdChanged();
    void peakingColorChanged();
    void showGridChanged();
    void showReticleChanged();
    void showRoiChanged();
    void roiRectChanged();
    void trackerLockChanged();
    void controllerChanged();
    void roiMoved(double x, double y, double w, double h);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;

private:
    [[nodiscard]] QRectF calculateAspectFit(const QRectF& bounds, qreal videoW, qreal videoH) const noexcept;
    [[nodiscard]] DragHandle hitTestHandle(const QPointF& pos, const QRectF& screenRoi) const noexcept;
    void drawGrid(QPainter* painter, const QRectF& videoRect);
    void drawReticle(QPainter* painter, const QPointF& center);
    void drawRoi(QPainter* painter, const QRectF& screenRoi);
    void drawPeaking(QPainter* painter, const QRectF& videoRect, const LensFramePacket& packet);
    void drawHud(QPainter* painter, const QRectF& bounds, const LensFramePacket& packet);

    LensPlayerController* m_controller { nullptr };
    QRectF m_roiRect { 0.25, 0.25, 0.50, 0.50 };
    bool m_showPeaking { false };
    int m_peakingThreshold { 80 };
    int m_peakingColorIndex { 0 }; // 0: Emerald, 1: Cyan, 2: Coral
    bool m_showGrid { true };
    bool m_showReticle { true };
    bool m_showRoi { true };
    bool m_isRoiLockedToTrack { false };

    DragHandle m_activeHandle { DragHandle::None };
    QPointF m_dragStartPos { };
    QRectF m_initialRoiRect { };
};

} // namespace LensApp
