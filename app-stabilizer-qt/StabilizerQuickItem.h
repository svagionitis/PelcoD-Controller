#pragma once

/// @file StabilizerQuickItem.h
/// @brief Custom QQuickPaintedItem presenting unstabilized and stabilized video frames.

#include <cstdint>

#include <QPainter>
#include <QQuickPaintedItem>

namespace StabilizerApp {

/// @class StabilizerQuickItem
/// @brief Renders video presentation viewport with interactive comparison wipe and overlays.
class StabilizerQuickItem : public QQuickPaintedItem {
    Q_OBJECT

    Q_PROPERTY(float splitPosition READ splitPosition WRITE setSplitPosition NOTIFY splitPositionChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)

public:
    /// @brief Constructor initializing default presentation parameters.
    /// @param parent Optional parent QQuickItem.
    explicit StabilizerQuickItem(QQuickItem* parent = nullptr);

    /// @brief Destructor.
    ~StabilizerQuickItem() override = default;

    StabilizerQuickItem(const StabilizerQuickItem&) = delete;
    StabilizerQuickItem& operator=(const StabilizerQuickItem&) = delete;
    StabilizerQuickItem(StabilizerQuickItem&&) = delete;
    StabilizerQuickItem& operator=(StabilizerQuickItem&&) = delete;

    /// @brief Split-screen wipe divider position [0.0 .. 1.0].
    /// @return Current divider fraction across viewport width.
    [[nodiscard]] float splitPosition() const noexcept;

    /// @brief Sets split-screen wipe divider position.
    /// @param pos Desired fraction [0.0 .. 1.0].
    void setSplitPosition(float pos) noexcept;

    /// @brief Whether tactical reticle and grid overlay are displayed.
    /// @return True if grid overlay is active.
    [[nodiscard]] bool showGrid() const noexcept;

    /// @brief Sets tactical grid overlay visibility.
    /// @param show Desired visibility state.
    void setShowGrid(bool show) noexcept;

    /// @brief QPainter presentation callback.
    /// @param painter Active painter provided by Qt Quick scene graph.
    void paint(QPainter* painter) override;

signals:
    /// @brief Emitted when split divider position changes.
    void splitPositionChanged();

    /// @brief Emitted when grid overlay visibility toggles.
    void showGridChanged();

private:
    float splitPosition_ { 0.50F };
    bool showGrid_ { true };
};

} // namespace StabilizerApp
