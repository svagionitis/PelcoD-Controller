#pragma once

/// @file VideoQuickItem.h
/// @brief Custom QQuickPaintedItem rendering decoded video frames in Qt Quick QML scenes.

#include <QImage>
#include <QMutex>
#include <QPainter>
#include <QQuickPaintedItem>

namespace VideoApp {

/// @class VideoQuickItem
/// @brief Hardware-accelerated Qt Quick video presentation item with aspect ratio handling.
class VideoQuickItem : public QQuickPaintedItem {
    Q_OBJECT

    Q_PROPERTY(FillMode fillMode READ fillMode WRITE setFillMode NOTIFY fillModeChanged)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoSizeChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoSizeChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)
    Q_PROPERTY(bool showOsdCrosshair READ showOsdCrosshair WRITE setShowOsdCrosshair NOTIFY showOsdCrosshairChanged)

public:
    /// @enum FillMode
    /// @brief Video frame scaling strategy within the QML item bounds.
    enum class FillMode { Stretch, PreserveAspectFit, PreserveAspectCrop };
    Q_ENUM(FillMode)

    /// @brief Constructor.
    /// @param[in] parent Optional parent quick item.
    explicit VideoQuickItem(QQuickItem* parent = nullptr);

    /// @brief Destructor.
    ~VideoQuickItem() override = default;

    /// @brief Current fill mode.
    [[nodiscard]] FillMode fillMode() const noexcept;

    /// @brief Sets fill mode.
    /// @param[in] mode Desired FillMode.
    void setFillMode(FillMode mode);

    /// @brief Video frame width in pixels.
    [[nodiscard]] int videoWidth() const noexcept;

    /// @brief Video frame height in pixels.
    [[nodiscard]] int videoHeight() const noexcept;

    /// @brief True if at least one frame has been delivered and rendered.
    [[nodiscard]] bool hasFrame() const noexcept;

    /// @brief Whether a subtle tactical crosshair is rendered on the canvas.
    [[nodiscard]] bool showOsdCrosshair() const noexcept;

    /// @brief Sets crosshair visibility.
    void setShowOsdCrosshair(bool show);

    /// @brief Renders the video frame and overlay onto the painter surface.
    /// @param[in] painter Active QPainter instance.
    void paint(QPainter* painter) override;

public slots:
    /// @brief Ingests a newly decoded video frame for immediate display.
    /// @param[in] frame Raw or filtered QImage frame.
    void updateFrame(const QImage& frame);

    /// @brief Clears current frame and resets to placeholder state.
    void clearFrame();

signals:
    /// @brief Emitted when fillMode changes.
    void fillModeChanged();

    /// @brief Emitted when video dimensions change.
    void videoSizeChanged();

    /// @brief Emitted when frame readiness changes.
    void hasFrameChanged();

    /// @brief Emitted when OSD crosshair setting changes.
    void showOsdCrosshairChanged();

private:
    void renderPlaceholder(QPainter* painter, const QRectF& bounds);
    void renderCrosshair(QPainter* painter, const QRectF& bounds);

    mutable QMutex m_mutex {};
    QImage m_currentFrame {};
    FillMode m_fillMode { FillMode::PreserveAspectFit };
    int m_videoWidth { 0 };
    int m_videoHeight { 0 };
    bool m_hasFrame { false };
    bool m_showOsdCrosshair { false };
};

} // namespace VideoApp
