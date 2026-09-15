#pragma once

/// @file VideoOverlayWidget.h
/// @brief High-performance video rendering canvas with tactical HUD telemetry overlays and interactive PTZ controls.

#include <QColor>
#include <QImage>
#include <QMutex>
#include <QPoint>
#include <QWidget>
#include <cstdint>

namespace PelcoDApp {

/// @class VideoOverlayWidget
/// @brief Custom QWidget rendering decoded video frames alongside tactical compass, pitch, optics, and PTZ joystick HUD layers.
class VideoOverlayWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor.
    /// @param[in] parent Optional parent widget.
    explicit VideoOverlayWidget(QWidget* parent = nullptr);

    /// @brief Destructor.
    ~VideoOverlayWidget() override = default;

    /// @brief Clears the active frame to black screen.
    void clearFrame();

    /// @brief Sets the HUD theme accent color.
    /// @param[in] color Accent color (e.g. Cyan, Green, Amber, White).
    void setHudColor(const QColor& color);

    /// @brief Queries current HUD accent color.
    [[nodiscard]] QColor hudColor() const;

    // Layer visibility toggles
    void setShowCrosshair(bool show);
    void setShowCompass(bool show);
    void setShowPitchLadder(bool show);
    void setShowOpticsHud(bool show);
    void setShowDiagnostics(bool show);
    void setInteractivePtzEnabled(bool enabled);

    [[nodiscard]] bool isCrosshairVisible() const;
    [[nodiscard]] bool isCompassVisible() const;
    [[nodiscard]] bool isPitchLadderVisible() const;
    [[nodiscard]] bool isOpticsHudVisible() const;
    [[nodiscard]] bool isDiagnosticsVisible() const;
    [[nodiscard]] bool isInteractivePtzEnabled() const;

    /// @brief Renders the current frame to a QImage for snapshot capture.
    /// @param[in] includeOverlay True to bake HUD vector overlays into the exported image.
    /// @return Rendered QImage.
    [[nodiscard]] QImage captureSnapshot(bool includeOverlay) const;

public slots:
    /// @brief Updates the active frame and triggers an optimized repaint.
    /// @param[in] frame Decoded video frame in RGB layout.
    /// @param[in] pts Presentation timestamp in seconds.
    /// @param[in] decodeLatencyMs Frame decode duration in milliseconds.
    void updateFrame(const QImage& frame, double pts, double decodeLatencyMs);

    /// @brief Updates device connection telemetry.
    void setDeviceStatus(bool connected, std::uint8_t address);

    /// @brief Updates PTZ pan angle in degrees (0.0 to 359.9).
    void setPanAngle(double degrees);

    /// @brief Updates PTZ tilt angle in degrees (-90.0 to +90.0 or 0.0 to 359.9).
    void setTiltAngle(double degrees);

    /// @brief Updates optics zoom telemetry.
    void setZoomInfo(int rawZoom, double magnification, double focalLengthMm);

    /// @brief Updates optical camera parameters.
    void setOpticalStatus(const QString& focusMode,
                          const QString& irisMode,
                          const QString& oisMode,
                          const QString& defogMode,
                          const QString& dayNightMode);

    /// @brief Updates stream diagnostics.
    void setStreamDiagnostics(const QString& backend,
                              int width,
                              int height,
                              double fps,
                              double decodeMs,
                              double rttMs,
                              double jitterMs);

signals:
    /// @brief Emitted when the user interacts with the video canvas to command pan/tilt motion.
    void panTiltRequested(int panSpeed, int tiltSpeed, bool left, bool right, bool up, bool down);

    /// @brief Emitted when mouse drag is released to halt camera motion.
    void stopPtzRequested();

    /// @brief Emitted when mouse wheel is scrolled over video.
    void zoomRequested(bool zoomIn);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawVideoFrame(QPainter& painter, const QRect& targetRect) const;
    void drawCrosshairLayer(QPainter& painter, const QRect& targetRect) const;
    void drawCompassLayer(QPainter& painter, const QRect& targetRect) const;
    void drawPitchLadderLayer(QPainter& painter, const QRect& targetRect) const;
    void drawOpticsHudLayer(QPainter& painter, const QRect& targetRect) const;
    void drawDiagnosticsLayer(QPainter& painter, const QRect& targetRect) const;
    void drawVirtualJoystickLayer(QPainter& painter) const;

    [[nodiscard]] QRect calculateAspectFitRect() const;

    mutable QMutex m_frameMutex;
    QImage m_currentFrame;
    double m_currentPts { 0.0 };
    double m_currentDecodeMs { 0.0 };

    // HUD theme color
    QColor m_hudColor { 0, 229, 255 }; // Tactical Cyan (#00E5FF)

    // Layer visibility
    bool m_showCrosshair { true };
    bool m_showCompass { true };
    bool m_showPitchLadder { true };
    bool m_showOpticsHud { true };
    bool m_showDiagnostics { true };
    bool m_interactivePtzEnabled { true };

    // Device & PTZ state
    bool m_connected { false };
    std::uint8_t m_address { 1U };
    double m_panDegrees { 0.0 };
    double m_tiltDegrees { 0.0 };

    // Optics state
    int m_rawZoom { 0 };
    double m_magnification { 1.0 };
    double m_focalLengthMm { 35.0 };
    QString m_focusMode { "AUTO" };
    QString m_irisMode { "AUTO" };
    QString m_oisMode { "AUTO" };
    QString m_defogMode { "OFF" };
    QString m_dayNightMode { "DAY" };

    // Diagnostics state
    QString m_backendName { "FFmpeg" };
    int m_videoWidth { 0 };
    int m_videoHeight { 0 };
    double m_fps { 0.0 };
    double m_avgDecodeMs { 0.0 };
    double m_rttMs { 0.0 };
    double m_jitterMs { 0.0 };

    // Virtual Joystick mouse drag tracking
    bool m_isDragging { false };
    QPoint m_dragStartPos;
    QPoint m_dragCurrentPos;
};

} // namespace PelcoDApp
