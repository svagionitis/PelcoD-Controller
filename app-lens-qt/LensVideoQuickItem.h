#pragma once

/// @file LensVideoQuickItem.h
/// @brief Custom QQuickPaintedItem presenting video viewport, reticles, and optical ROI.

#include <cstdint>

#include <QColor>
#include <QPainter>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QRectF>

namespace LensApp {

class LensPlayerController;

/// @class LensVideoQuickItem
/// @brief Renders tactical viewport canvas with grid, crosshairs reticle, and focus ROI box.
class LensVideoQuickItem : public QQuickPaintedItem {
    Q_OBJECT

    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)
    Q_PROPERTY(bool showReticle READ showReticle WRITE setShowReticle NOTIFY showReticleChanged)
    Q_PROPERTY(bool showRoi READ showRoi WRITE setShowRoi NOTIFY showRoiChanged)
    Q_PROPERTY(QRectF roiRect READ roiRect WRITE setRoiRect NOTIFY roiRectChanged)
    Q_PROPERTY(LensPlayerController* controller READ controller WRITE setController NOTIFY controllerChanged)

public:
    explicit LensVideoQuickItem(QQuickItem* parent = nullptr);
    ~LensVideoQuickItem() override = default;

    LensVideoQuickItem(const LensVideoQuickItem&) = delete;
    LensVideoQuickItem& operator=(const LensVideoQuickItem&) = delete;
    LensVideoQuickItem(LensVideoQuickItem&&) = delete;
    LensVideoQuickItem& operator=(LensVideoQuickItem&&) = delete;

    void paint(QPainter* painter) override;

    [[nodiscard]] bool showGrid() const noexcept;
    [[nodiscard]] bool showReticle() const noexcept;
    [[nodiscard]] bool showRoi() const noexcept;
    [[nodiscard]] QRectF roiRect() const noexcept;
    [[nodiscard]] LensPlayerController* controller() const noexcept;

public slots:
    void setShowGrid(bool show);
    void setShowReticle(bool show);
    void setShowRoi(bool show);
    void setRoiRect(const QRectF& rect);
    void setController(LensPlayerController* ctrl);

signals:
    void showGridChanged();
    void showReticleChanged();
    void showRoiChanged();
    void roiRectChanged();
    void controllerChanged();

private:
    void drawGrid(QPainter* painter, const QRectF& bounds);
    void drawReticle(QPainter* painter, const QPointF& center);
    void drawRoi(QPainter* painter, const QRectF& bounds);

    LensPlayerController* m_controller { nullptr };
    QRectF m_roiRect { 0.25, 0.25, 0.5, 0.5 };
    bool m_showGrid { true };
    bool m_showReticle { true };
    bool m_showRoi { true };
};

} // namespace LensApp
