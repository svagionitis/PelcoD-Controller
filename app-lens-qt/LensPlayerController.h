#pragma once

/// @file LensPlayerController.h
/// @brief Master QML ViewModel bridging UI events with LensWorker and LensController subsystems.

#include <cstdint>

#include <QObject>
#include <QRectF>
#include <QString>
#include <QThread>

#include "AtomicTripleBuffer.h"
#include "LensConfigModel.h"
#include "LensFramePacket.h"

namespace LensApp {

class LensWorker;

/// @class LensPlayerController
/// @brief Master ViewModel managing multi-channel optical actuation, video playback, and telemetry.
class LensPlayerController : public QObject {
    Q_OBJECT

    // Channel & Mode Properties
    Q_PROPERTY(int activeChannel READ activeChannel WRITE setActiveChannel NOTIFY activeChannelChanged)
    Q_PROPERTY(int lensMode READ lensMode WRITE setLensMode NOTIFY lensModeChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

    // Optical Actuator Telemetry
    Q_PROPERTY(int currentFocus READ currentFocus NOTIFY telemetryUpdated)
    Q_PROPERTY(int currentZoom READ currentZoom NOTIFY telemetryUpdated)
    Q_PROPERTY(int focusMin READ focusMin NOTIFY limitsChanged)
    Q_PROPERTY(int focusMax READ focusMax NOTIFY limitsChanged)
    Q_PROPERTY(int zoomMin READ zoomMin NOTIFY limitsChanged)
    Q_PROPERTY(int zoomMax READ zoomMax NOTIFY limitsChanged)
    Q_PROPERTY(int limitFlags READ limitFlags NOTIFY telemetryUpdated)
    Q_PROPERTY(bool isAfRunning READ isAfRunning NOTIFY afStateChanged)
    Q_PROPERTY(double latestMetricScore READ latestMetricScore NOTIFY telemetryUpdated)

    // Media Ingestion & Playback State
    Q_PROPERTY(QString sourceUri READ sourceUri WRITE setSourceUri NOTIFY sourceUriChanged)
    Q_PROPERTY(int playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(double currentFps READ currentFps NOTIFY telemetryUpdated)
    Q_PROPERTY(double currentPts READ currentPts NOTIFY ptsChanged)

    // Synthetic Defocus Simulator Properties
    Q_PROPERTY(bool isSynthetic READ isSynthetic NOTIFY sourceModeChanged)
    Q_PROPERTY(int syntheticPlane READ syntheticPlane WRITE setSyntheticPlane NOTIFY syntheticPlaneChanged)
    Q_PROPERTY(double syntheticNoise READ syntheticNoise WRITE setSyntheticNoise NOTIFY syntheticNoiseChanged)

    // Viewport & ROI Properties
    Q_PROPERTY(QRectF roiRect READ roiRect WRITE setRoiRect NOTIFY roiRectChanged)
    Q_PROPERTY(bool showPeaking READ showPeaking WRITE setShowPeaking NOTIFY showPeakingChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)
    Q_PROPERTY(bool showReticle READ showReticle WRITE setShowReticle NOTIFY showReticleChanged)
    Q_PROPERTY(bool showRoi READ showRoi WRITE setShowRoi NOTIFY showRoiChanged)

    // Sub-models
    Q_PROPERTY(LensConfigModel* configModel READ configModel CONSTANT)

public:
    explicit LensPlayerController(QObject* parent = nullptr);
    ~LensPlayerController() override;

    LensPlayerController(const LensPlayerController&) = delete;
    LensPlayerController& operator=(const LensPlayerController&) = delete;
    LensPlayerController(LensPlayerController&&) = delete;
    LensPlayerController& operator=(LensPlayerController&&) = delete;

    [[nodiscard]] int activeChannel() const noexcept;
    [[nodiscard]] int lensMode() const noexcept;
    [[nodiscard]] QString statusMessage() const;

    [[nodiscard]] int currentFocus() const noexcept;
    [[nodiscard]] int currentZoom() const noexcept;
    [[nodiscard]] int focusMin() const noexcept;
    [[nodiscard]] int focusMax() const noexcept;
    [[nodiscard]] int zoomMin() const noexcept;
    [[nodiscard]] int zoomMax() const noexcept;
    [[nodiscard]] int limitFlags() const noexcept;
    [[nodiscard]] bool isAfRunning() const noexcept;
    [[nodiscard]] double latestMetricScore() const noexcept;

    [[nodiscard]] QString sourceUri() const;
    [[nodiscard]] int playbackState() const noexcept;
    [[nodiscard]] double currentFps() const noexcept;
    [[nodiscard]] double currentPts() const noexcept;

    [[nodiscard]] bool isSynthetic() const noexcept;
    [[nodiscard]] int syntheticPlane() const noexcept;
    [[nodiscard]] double syntheticNoise() const noexcept;

    [[nodiscard]] QRectF roiRect() const noexcept;
    [[nodiscard]] bool showPeaking() const noexcept;
    [[nodiscard]] bool showGrid() const noexcept;
    [[nodiscard]] bool showReticle() const noexcept;
    [[nodiscard]] bool showRoi() const noexcept;

    [[nodiscard]] LensConfigModel* configModel() const noexcept;
    [[nodiscard]] Video::AtomicTripleBuffer<LensFramePacket>& tripleBuffer() noexcept;

public slots:
    void setActiveChannel(int channel);
    void setLensMode(int mode);
    void setSourceUri(const QString& uri);

    // Media Transport Slots (< 30 chars)
    void openSource(const QString& uri);
    void play();
    void pause();
    void stop();
    void stepFrame();
    void seek(double seconds);
    void setPlaybackRate(double rate);

    // Synthetic Simulator Slots (< 30 chars)
    void setSyntheticPlane(int plane);
    void setSyntheticNoise(double sigma);
    void setSyntheticPattern(int pattern);

    // ROI & Overlay Slots (< 30 chars)
    void setRoiRect(const QRectF& rect);
    void setRoi(double x, double y, double w, double h);
    void setShowPeaking(bool show);
    void setShowGrid(bool show);
    void setShowReticle(bool show);
    void setShowRoi(bool show);

    // Optical Motion Commands (< 30 chars)
    void moveFocusStep(int steps);
    void moveZoomStep(int steps);
    void setFocusTarget(int position);
    void setZoomTarget(int position);
    void setFocusSpeed(int speed);
    void setZoomSpeed(int speed);
    void stopMotion();
    void emergencyStop();

signals:
    void activeChannelChanged();
    void lensModeChanged();
    void statusMessageChanged();
    void telemetryUpdated();
    void limitsChanged();
    void afStateChanged();

    void sourceUriChanged();
    void playbackStateChanged();
    void ptsChanged();
    void sourceModeChanged();
    void syntheticPlaneChanged();
    void syntheticNoiseChanged();

    void roiRectChanged();
    void showPeakingChanged();
    void showGridChanged();
    void showReticleChanged();
    void showRoiChanged();
    void frameReady();

    // Queued worker dispatch signals (< 30 chars)
    void requestOpenSource(const QString& uri);
    void requestPlay();
    void requestPause();
    void requestStop();
    void requestStepFrame();
    void requestSeek(double seconds);
    void requestRate(double rate);
    void requestPlane(int plane);
    void requestNoise(double sigma);
    void requestPattern(int pattern);
    void requestRoi(double x, double y, double w, double h);

    void requestMoveFocus(int channel, int steps);
    void requestMoveZoom(int channel, int steps);
    void requestSetFocus(int channel, int position);
    void requestSetZoom(int channel, int position);
    void requestSetFocusSpeed(int channel, int speed);
    void requestSetZoomSpeed(int channel, int speed);
    void requestStopMotion(int channel);
    void requestEmergencyStop();
    void requestSetChannel(int channel);
    void requestSetMode(int channel, int mode);

private slots:
    void onStatusUpdated(int focus, int zoom, int limits, int mode, bool afActive);
    void onTelemetryReady(double metricScore, int channel);
    void onSourceOpened(int width, int height, double fps);
    void onPlaybackStateChanged(int state);

private:
    void setupWorkerThread();

    Video::AtomicTripleBuffer<LensFramePacket> m_tripleBuffer { };
    QThread m_workerThread { };
    LensWorker* m_worker { nullptr };
    LensConfigModel* m_configModel { nullptr };

    int m_activeChannel { 0 };
    int m_lensMode { 0 };
    int m_currentFocus { 0 };
    int m_currentZoom { 0 };
    int m_focusMin { 0 };
    int m_focusMax { 65535 };
    int m_zoomMin { 0 };
    int m_zoomMax { 65535 };
    int m_limitFlags { 0 };
    bool m_isAfRunning { false };
    double m_latestMetricScore { 0.0 };

    QString m_sourceUri { QStringLiteral("synthetic://siemens-star") };
    int m_playbackState { 0 };
    double m_currentFps { 30.0 };
    double m_currentPts { 0.0 };

    bool m_isSynthetic { true };
    int m_syntheticPlane { 32768 };
    double m_syntheticNoise { 1.5 };

    QRectF m_roiRect { 0.25, 0.25, 0.50, 0.50 };
    bool m_showPeaking { false };
    bool m_showGrid { true };
    bool m_showReticle { true };
    bool m_showRoi { true };

    QString m_statusMessage { QStringLiteral("Ready") };
};

} // namespace LensApp
