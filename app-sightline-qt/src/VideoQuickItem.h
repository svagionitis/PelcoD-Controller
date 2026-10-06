#pragma once

/// @file VideoQuickItem.h
/// @brief Custom QQuickPaintedItem rendering decoded video frames in Qt Quick QML scenes.

#include "LatencyTracker.h"

#include <QImage>
#include <QMetaObject>
#include <QMutex>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QTimer>

#include <atomic>
#include <cstdint>

class QQuickWindow;

namespace SightlineApp {

/// @class VideoQuickItem
/// @brief Hardware-accelerated Qt Quick video presentation item with aspect ratio handling.
/// @details Also measures decode-to-display latency: each frame may carry a steady_clock
///          decode stamp (updateTimedFrame()); the stamp is latched when the frame is
///          painted and closed when QQuickWindow::frameSwapped fires, i.e. when the frame
///          has actually been handed to the display. Statistics are published to QML
///          every 500 ms via displayLatencyChanged().
class VideoQuickItem : public QQuickPaintedItem {
    Q_OBJECT

    Q_PROPERTY(FillMode fillMode READ fillMode WRITE setFillMode NOTIFY fillModeChanged)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoSizeChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoSizeChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)
    Q_PROPERTY(bool showOsdCrosshair READ showOsdCrosshair WRITE setShowOsdCrosshair NOTIFY showOsdCrosshairChanged)
    Q_PROPERTY(
        InteractionMode interactionMode READ interactionMode WRITE setInteractionMode NOTIFY interactionModeChanged)
    Q_PROPERTY(bool isLassoActive READ isLassoActive NOTIFY lassoActiveChanged)
    Q_PROPERTY(QRectF lassoRect READ lassoRect NOTIFY lassoRectChanged)
    Q_PROPERTY(int defaultGateWidth READ defaultGateWidth WRITE setDefaultGateWidth NOTIFY defaultGateSizeChanged)
    Q_PROPERTY(int defaultGateHeight READ defaultGateHeight WRITE setDefaultGateHeight NOTIFY defaultGateSizeChanged)
    Q_PROPERTY(double displayLatencyMs READ displayLatencyMs NOTIFY displayLatencyChanged)
    Q_PROPERTY(double displayLatencyLastMs READ displayLatencyLastMs NOTIFY displayLatencyChanged)
    Q_PROPERTY(double displayLatencyMinMs READ displayLatencyMinMs NOTIFY displayLatencyChanged)
    Q_PROPERTY(double displayLatencyMaxMs READ displayLatencyMaxMs NOTIFY displayLatencyChanged)
    Q_PROPERTY(qulonglong presentedFrames READ presentedFrames NOTIFY displayLatencyChanged)

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
    /// @details Latches the pending decode stamp of the frame being painted so that the
    ///          next notifyFrameSwapped() can close the latency measurement.
    /// @param[in] painter Active QPainter instance.
    void paint(QPainter* painter) override;

    /// @brief Rolling average decode-to-display latency.
    /// @return Average latency in milliseconds over the last window (0 if no samples).
    [[nodiscard]] double displayLatencyMs() const noexcept;

    /// @brief Most recent decode-to-display latency sample.
    /// @return Latency in milliseconds (0 if no samples).
    [[nodiscard]] double displayLatencyLastMs() const noexcept;

    /// @brief Minimum decode-to-display latency over the rolling window.
    /// @return Latency in milliseconds (0 if no samples).
    [[nodiscard]] double displayLatencyMinMs() const noexcept;

    /// @brief Maximum decode-to-display latency over the rolling window.
    /// @return Latency in milliseconds (0 if no samples).
    [[nodiscard]] double displayLatencyMaxMs() const noexcept;

    /// @brief Number of stamped frames whose presentation has been measured.
    /// @return Total measured frames since construction or last reset.
    [[nodiscard]] qulonglong presentedFrames() const noexcept;

    /// @brief Clears all latency samples and pending stamps.
    /// @details Publishes the cleared statistics immediately.
    Q_INVOKABLE void resetLatency();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// @brief Tracks window changes to (re)bind the frameSwapped latency hook.
    /// @param[in] change Kind of item change.
    /// @param[in] value Change payload (window for ItemSceneChange).
    void itemChange(ItemChange change, const ItemChangeData& value) override;

public slots:
    /// @brief Ingests a newly decoded video frame for immediate display.
    /// @details Equivalent to updateTimedFrame(frame, 0): the frame is shown but not measured.
    /// @param[in] frame Raw or filtered QImage frame.
    void updateFrame(const QImage& frame);

    /// @brief Ingests a decoded frame together with its decode-completion stamp.
    /// @param[in] frame Raw or filtered QImage frame.
    /// @param[in] decodedAtNs Video::steadyNowNs() stamp at decode completion (0 = do not measure).
    void updateTimedFrame(const QImage& frame, qint64 decodedAtNs);

    /// @brief Closes the latency measurement of the last painted frame.
    /// @details Connected to QQuickWindow::frameSwapped with Qt::DirectConnection, so it
    ///          may run on the scene graph render thread. Thread-safe.
    void notifyFrameSwapped();

    /// @brief Publishes latency statistics to QML if new samples arrived.
    /// @details Invoked on the GUI thread by an internal 500 ms timer.
    void publishLatency();

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

    /// @brief Emitted (at most every 500 ms) when display latency statistics change.
    void displayLatencyChanged();

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

    void bindWindow(QQuickWindow* window);

    static constexpr int kLatencyPublishMs { 500 };

    qint64 m_pendingStampNs { 0 }; ///< Decode stamp of m_currentFrame, guarded by m_mutex.
    std::atomic<std::int64_t> m_paintedStampNs { 0 }; ///< Stamp latched by paint(), consumed on swap.
    Video::LatencyTracker m_latency {}; ///< Thread-safe rolling latency window.
    Video::LatencyStats m_latencyStats {}; ///< GUI-thread snapshot exposed to QML.
    QTimer m_latencyTimer {}; ///< GUI-thread publisher for displayLatencyChanged().
    QMetaObject::Connection m_swapConnection {}; ///< Active frameSwapped hook.
};

} // namespace SightlineApp
