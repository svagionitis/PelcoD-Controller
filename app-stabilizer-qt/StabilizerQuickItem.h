#pragma once

/// @file StabilizerQuickItem.h
/// @brief Custom QQuickPaintedItem presenting unstabilized and stabilized video frames.

#include <cstdint>

#include <QImage>
#include <QPainter>
#include <QQuickPaintedItem>

namespace StabilizerApp {

class StabilizerPlayerController;

/// @class StabilizerQuickItem
/// @brief Renders video presentation viewport with interactive comparison wipe and overlays.
class StabilizerQuickItem : public QQuickPaintedItem {
    Q_OBJECT

    Q_PROPERTY(float splitPosition READ splitPosition WRITE setSplitPosition NOTIFY splitPositionChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)
    Q_PROPERTY(StabilizerApp::StabilizerPlayerController* controller READ controller WRITE setController NOTIFY
            controllerChanged)

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

    /// @brief Attached player controller providing lock-free frame data.
    /// @return Pointer to controller, or nullptr if unbound.
    [[nodiscard]] StabilizerPlayerController* controller() const noexcept;

    /// @brief Sets attached player controller.
    /// @param ctrl Pointer to controller instance.
    void setController(StabilizerPlayerController* ctrl);

    /// @brief QPainter presentation callback.
    /// @param painter Active painter provided by Qt Quick scene graph.
    void paint(QPainter* painter) override;

public slots:
    /// @brief Callback invoked when a fresh frame is published by worker.
    void onFrameReady();

signals:
    /// @brief Emitted when split divider position changes.
    void splitPositionChanged();

    /// @brief Emitted when grid overlay visibility toggles.
    void showGridChanged();

    /// @brief Emitted when attached controller changes.
    void controllerChanged();

private:
    void renderStandbyPattern(QPainter* painter);

    float splitPosition_ { 0.50F };
    bool showGrid_ { true };
    StabilizerPlayerController* controller_ { nullptr };

    QImage rawImage_ { };
    QImage stabilizedImage_ { };
    bool hasValidFrame_ { false };
};

} // namespace StabilizerApp
