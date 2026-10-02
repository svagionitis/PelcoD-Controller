#pragma once

/// @file VideoQuickItem.h
/// @brief Custom QQuickPaintedItem rendering decoded video frames in Qt Quick QML scenes.

#include <QImage>
#include <QMutex>
#include <QPainter>
#include <QQuickPaintedItem>

namespace SightlineApp {

/// @class VideoQuickItem
/// @brief Hardware-accelerated Qt Quick video presentation item with aspect ratio handling.
class VideoQuickItem : public QQuickPaintedItem {
    Q_OBJECT

    Q_PROPERTY(FillMode fillMode READ fillMode WRITE setFillMode NOTIFY fillModeChanged)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoSizeChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoSizeChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)
    Q_PROPERTY(bool showOsdCrosshair READ showOsdCrosshair WRITE setShowOsdCrosshair NOTIFY showOsdCrosshairChanged)
    Q_PROPERTY(InteractionMode interactionMode READ interactionMode WRITE setInteractionMode NOTIFY interactionModeChanged)
    Q_PROPERTY(bool isLassoActive READ isLassoActive NOTIFY lassoActiveChanged)
    Q_PROPERTY(QRectF lassoRect READ lassoRect NOTIFY lassoRectChanged)
    Q_PROPERTY(int defaultGateWidth READ defaultGateWidth WRITE setDefaultGateWidth NOTIFY defaultGateSizeChanged)
    Q_PROPERTY(int defaultGateHeight READ defaultGateHeight WRITE setDefaultGateHeight NOTIFY defaultGateSizeChanged)

public:
    /// @enum FillMode
    /// @brief Video frame scaling strategy within the QML item bounds.
    enum class FillMode { Stretch, PreserveAspectFit, PreserveAspectCrop };
    Q_ENUM(FillMode)

    /// @enum InteractionMode
    /// @brief Interactive mouse gestures on video surface.
    enum class InteractionMode { None = 0, ClickToTrack = 1, LassoAcquire = 2 };
    Q_ENUM(InteractionMode)

    /// @brief Constructor.
    /// @param[in] parent Optional parent quick item.
    explicit VideoQuickItem(QQuickItem* parent = nullptr);

    /// @brief Destructor.
    ~VideoQuickItem() override = default;

    /// @brief Current fill mode.
    /// @return Active FillMode enum value.
    [[nodiscard]] FillMode fillMode() const noexcept;

    /// @brief Sets fill mode.
    /// @param[in] mode Desired FillMode.
    void setFillMode(FillMode mode);

    /// @brief Video frame width in pixels.
    /// @return Current frame width in pixels.
    [[nodiscard]] int videoWidth() const noexcept;

    /// @brief Video frame height in pixels.
    /// @return Current frame height in pixels.
    [[nodiscard]] int videoHeight() const noexcept;

    /// @brief True if at least one frame has been delivered and rendered.
    /// @return True if valid frame is loaded.
    [[nodiscard]] bool hasFrame() const noexcept;

    /// @brief Whether a subtle tactical crosshair is rendered on the canvas.
    /// @return True if OSD crosshair is visible.
    [[nodiscard]] bool showOsdCrosshair() const noexcept;

    /// @brief Sets crosshair visibility.
    /// @param[in] show Crosshair visibility flag.
    void setShowOsdCrosshair(bool show);

    /// @brief Active mouse interaction mode.
    /// @return InteractionMode enum value.
    [[nodiscard]] InteractionMode interactionMode() const noexcept;

    /// @brief Sets active mouse interaction mode.
    /// @param[in] mode Desired interaction mode.
    void setInteractionMode(InteractionMode mode);

    /// @brief True if lasso selection is currently being dragged.
    /// @return True if actively dragging lasso.
    [[nodiscard]] bool isLassoActive() const noexcept;

    /// @brief Current lasso selection rectangle in item coordinates.
    /// @return Active lasso selection rectangle.
    [[nodiscard]] QRectF lassoRect() const noexcept;

    /// @brief Default width for click-to-acquire gates.
    /// @return Default gate width in pixels.
    [[nodiscard]] int defaultGateWidth() const noexcept;

    /// @brief Sets default gate width.
    /// @param[in] w Gate width in pixels.
    void setDefaultGateWidth(int w) noexcept;

    /// @brief Default height for click-to-acquire gates.
    /// @return Default gate height in pixels.
    [[nodiscard]] int defaultGateHeight() const noexcept;

    /// @brief Sets default gate height.
    /// @param[in] h Gate height in pixels.
    void setDefaultGateHeight(int h) noexcept;

    /// @brief Content rendering destination rectangle within item bounds.
    /// @return Sub-rectangle where video image is scaled and positioned.
    [[nodiscard]] QRectF contentRect() const;

    /// @brief Maps item local coordinate to native video sensor coordinate.
    /// @param[in] itemPoint Point in item local coordinates.
    /// @return Point in native video frame pixel coordinates.
    Q_INVOKABLE [[nodiscard]] QPointF mapToVideo(const QPointF& itemPoint) const;

    /// @brief Maps item local rectangle to native video sensor bounding box.
    /// @param[in] itemRect Bounding rectangle in item local coordinates.
    /// @return Bounding rectangle in native video frame coordinates.
    Q_INVOKABLE [[nodiscard]] QRectF mapToVideoRect(const QRectF& itemRect) const;

    /// @brief Maps native video sensor coordinate to item local coordinate.
    /// @param[in] videoPoint Point in native video sensor coordinates.
    /// @return Point in item local coordinates.
    Q_INVOKABLE [[nodiscard]] QPointF mapFromVideo(const QPointF& videoPoint) const;

    /// @brief Maps native video sensor rectangle to item local display rectangle.
    /// @param[in] videoRect Rectangle in native video sensor coordinates.
    /// @return Rectangle in item local coordinates.
    Q_INVOKABLE [[nodiscard]] QRectF mapFromVideoRect(const QRectF& videoRect) const;

    /// @brief Renders the video frame and overlay onto the painter surface.
    /// @param[in] painter Active QPainter instance.
    void paint(QPainter* painter) override;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

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

    /// @brief Emitted when interaction mode changes.
    void interactionModeChanged();

    /// @brief Emitted when lasso dragging status changes.
    void lassoActiveChanged();

    /// @brief Emitted when lasso bounding box updates during drag.
    void lassoRectChanged();

    /// @brief Emitted when default gate dimensions change.
    void defaultGateSizeChanged();

    /// @brief Emitted when user initiates target acquisition via click or lasso.
    /// @param[in] col Target center column coordinate in video sensor pixels.
    /// @param[in] row Target center row coordinate in video sensor pixels.
    /// @param[in] width Target gate width in sensor pixels.
    /// @param[in] height Target gate height in sensor pixels.
    void targetAcquired(int col, int row, int width, int height);

    /// @brief Emitted when user initiates precision target acquisition with timestamp.
    /// @param[in] col Target center column coordinate.
    /// @param[in] row Target center row coordinate.
    /// @param[in] width Target gate width.
    /// @param[in] height Target gate height.
    /// @param[in] ptsUs Frame presentation timestamp in microseconds.
    void precisionAcquired(int col, int row, int width, int height, quint64 ptsUs);

private:
    void renderPlaceholder(QPainter* painter, const QRectF& bounds);
    void renderCrosshair(QPainter* painter, const QRectF& bounds);
    void renderLasso(QPainter* painter);

    mutable QMutex m_mutex {};
    QImage m_currentFrame {};
    FillMode m_fillMode { FillMode::PreserveAspectFit };
    int m_videoWidth { 0 };
    int m_videoHeight { 0 };
    bool m_hasFrame { false };
    bool m_showOsdCrosshair { false };

    InteractionMode m_interactionMode { InteractionMode::None };
    bool m_isLassoActive { false };
    QPointF m_lassoStartPoint {};
    QRectF m_lassoRect {};
    int m_defaultGateWidth { 80 };
    int m_defaultGateHeight { 80 };
};

} // namespace SightlineApp
