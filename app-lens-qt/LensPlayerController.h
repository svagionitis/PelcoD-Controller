#pragma once

/// @file LensPlayerController.h
/// @brief Master ViewModel orchestrating lens operations, video playback, and AF telemetry.

#include <cstddef>
#include <cstdint>

#include <array>

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariantList>

#include "AtomicTripleBuffer.h"
#include "LensConfigModel.h"
#include "LensFramePacket.h"

namespace LensApp {

class LensWorker;

/// @class LensPlayerController
/// @brief Master ViewModel orchestrating lens operations, video playback, and telemetry.
class LensPlayerController : public QObject {
    Q_OBJECT

    // Channel & Mode
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
    Q_PROPERTY(double latestMetricScore READ latestMetricScore NOTIFY telemetryUpdated)

    // Autofocus State & Configuration (Phase 4)
    Q_PROPERTY(bool isAfRunning READ isAfRunning NOTIFY afStateChanged)
    Q_PROPERTY(int afState READ afState NOTIFY afStateChanged)
    Q_PROPERTY(QString afStateString READ afStateString NOTIFY afStateChanged)
    Q_PROPERTY(int afAlgorithm READ afAlgorithm WRITE setAfAlgorithm NOTIFY afConfigChanged)
    Q_PROPERTY(int focusMetric READ focusMetric WRITE setFocusMetric NOTIFY afConfigChanged)
    Q_PROPERTY(bool cafEnabled READ cafEnabled WRITE setCafEnabled NOTIFY cafToggled)
    Q_PROPERTY(double cafThreshold READ cafThreshold WRITE setCafThreshold NOTIFY cafConfigChanged)

    // Mathematical Peak Fit Telemetry (Phase 4)
    Q_PROPERTY(double peakSummit READ peakSummit NOTIFY peakFitUpdated)
    Q_PROPERTY(double peakScore READ peakScore NOTIFY peakFitUpdated)
    Q_PROPERTY(double peakFwhm READ peakFwhm NOTIFY peakFitUpdated)
    Q_PROPERTY(double peakCurvature READ peakCurvature NOTIFY peakFitUpdated)
    Q_PROPERTY(double peakR2 READ peakR2 NOTIFY peakFitUpdated)
    Q_PROPERTY(bool isPeakValid READ isPeakValid NOTIFY peakFitUpdated)

    // Recorded Sweep Samples for QML Charting (Phase 4)
    Q_PROPERTY(QVariantList samplePoints READ samplePoints NOTIFY samplesUpdated)

    // Parfocal & Zoom-Track-Focus Properties (Phase 5)
    Q_PROPERTY(bool zoomTrackEnabled READ zoomTrackEnabled WRITE setZoomTrackEnabled NOTIFY zoomTrackChanged)
    Q_PROPERTY(int interpolationMode READ interpolationMode WRITE setInterpolationMode NOTIFY interpolationModeChanged)
    Q_PROPERTY(int activeCurveIndex READ activeCurveIndex WRITE setActiveCurveIndex NOTIFY activeCurveChanged)
    Q_PROPERTY(int curveCount READ curveCount NOTIFY curveSetChanged)
    Q_PROPERTY(
        double activeCurveDistance READ activeCurveDistance WRITE setActiveCurveDistance NOTIFY activeCurveChanged)
    Q_PROPERTY(bool isCurveValid READ isCurveValid NOTIFY activeCurveChanged)
    Q_PROPERTY(QVariantList activeCurveKnots READ activeCurveKnots NOTIFY knotsUpdated)

    // Calibration Wizard Properties (Phase 5)
    Q_PROPERTY(bool isCalibrating READ isCalibrating NOTIFY calibStateChanged)
    Q_PROPERTY(int calibStep READ calibStep NOTIFY calibProgressChanged)
    Q_PROPERTY(int calibTotalSteps READ calibTotalSteps NOTIFY calibProgressChanged)
    Q_PROPERTY(double calibProgress READ calibProgress NOTIFY calibProgressChanged)
    Q_PROPERTY(QString calibStatusText READ calibStatusText NOTIFY calibProgressChanged)

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
    Q_PROPERTY(int peakingThreshold READ peakingThreshold WRITE setPeakingThreshold NOTIFY peakingThresholdChanged)
    Q_PROPERTY(int peakingColorIndex READ peakingColorIndex WRITE setPeakingColorIndex NOTIFY peakingColorChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)
    Q_PROPERTY(bool showReticle READ showReticle WRITE setShowReticle NOTIFY showReticleChanged)
    Q_PROPERTY(bool showRoi READ showRoi WRITE setShowRoi NOTIFY showRoiChanged)
    Q_PROPERTY(bool isRoiLockedToTrack READ isRoiLockedToTrack WRITE setIsRoiLockedToTrack NOTIFY trackerLockChanged)

    // Hardware Driver & Actuation Speeds (Phase 3)
    Q_PROPERTY(int activeDriverType READ activeDriverType WRITE setActiveDriverType NOTIFY driverTypeChanged)
    Q_PROPERTY(QString driverName READ driverName NOTIFY driverTypeChanged)
    Q_PROPERTY(QString boundPortName READ boundPortName NOTIFY portBindingChanged)
    Q_PROPERTY(int boundBaudRate READ boundBaudRate NOTIFY portBindingChanged)
    Q_PROPERTY(int cameraAddress READ cameraAddress WRITE setCameraAddress NOTIFY cameraAddressChanged)
    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY portsScanned)
    Q_PROPERTY(bool isPortConnected READ isPortConnected NOTIFY portBindingChanged)
    Q_PROPERTY(int focusSpeed READ focusSpeed WRITE setFocusSpeed NOTIFY speedsChanged)
    Q_PROPERTY(int zoomSpeed READ zoomSpeed WRITE setZoomSpeed NOTIFY speedsChanged)

    // Sub-models
    Q_PROPERTY(LensConfigModel* configModel READ configModel CONSTANT)

public:
    enum class DriverType : std::uint8_t { Mock = 0U, PelcoD = 1U, Visca = 2U, DynamicDll = 3U };
    Q_ENUM(DriverType)
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
    [[nodiscard]] double latestMetricScore() const noexcept;

    // Autofocus Getters
    [[nodiscard]] bool isAfRunning() const noexcept;
    [[nodiscard]] int afState() const noexcept;
    [[nodiscard]] QString afStateString() const;
    [[nodiscard]] int afAlgorithm() const noexcept;
    [[nodiscard]] int focusMetric() const noexcept;
    [[nodiscard]] bool cafEnabled() const noexcept;
    [[nodiscard]] double cafThreshold() const noexcept;
    [[nodiscard]] double peakSummit() const noexcept;
    [[nodiscard]] double peakScore() const noexcept;
    [[nodiscard]] double peakFwhm() const noexcept;
    [[nodiscard]] double peakCurvature() const noexcept;
    [[nodiscard]] double peakR2() const noexcept;
    [[nodiscard]] bool isPeakValid() const noexcept;
    [[nodiscard]] QVariantList samplePoints() const;

    // Parfocal & Zoom-Track Getters (Phase 5)
    [[nodiscard]] bool zoomTrackEnabled() const noexcept;
    [[nodiscard]] int interpolationMode() const noexcept;
    [[nodiscard]] int activeCurveIndex() const noexcept;
    [[nodiscard]] int curveCount() const noexcept;
    [[nodiscard]] double activeCurveDistance() const noexcept;
    [[nodiscard]] bool isCurveValid() const noexcept;
    [[nodiscard]] QVariantList activeCurveKnots() const;

    // Calibration Wizard Getters (Phase 5)
    [[nodiscard]] bool isCalibrating() const noexcept;
    [[nodiscard]] int calibStep() const noexcept;
    [[nodiscard]] int calibTotalSteps() const noexcept;
    [[nodiscard]] double calibProgress() const noexcept;
    [[nodiscard]] QString calibStatusText() const;

    [[nodiscard]] QString sourceUri() const;
    [[nodiscard]] int playbackState() const noexcept;
    [[nodiscard]] double currentFps() const noexcept;
    [[nodiscard]] double currentPts() const noexcept;

    [[nodiscard]] bool isSynthetic() const noexcept;
    [[nodiscard]] int syntheticPlane() const noexcept;
    [[nodiscard]] double syntheticNoise() const noexcept;

    [[nodiscard]] QRectF roiRect() const noexcept;
    [[nodiscard]] bool showPeaking() const noexcept;
    [[nodiscard]] int peakingThreshold() const noexcept;
    [[nodiscard]] int peakingColorIndex() const noexcept;
    [[nodiscard]] bool showGrid() const noexcept;
    [[nodiscard]] bool showReticle() const noexcept;
    [[nodiscard]] bool showRoi() const noexcept;
    [[nodiscard]] bool isRoiLockedToTrack() const noexcept;

    [[nodiscard]] int activeDriverType() const noexcept;
    [[nodiscard]] QString driverName() const;
    [[nodiscard]] QString boundPortName() const;
    [[nodiscard]] int boundBaudRate() const noexcept;
    [[nodiscard]] int cameraAddress() const noexcept;
    [[nodiscard]] QStringList availablePorts() const;
    [[nodiscard]] bool isPortConnected() const noexcept;
    [[nodiscard]] int focusSpeed() const noexcept;
    [[nodiscard]] int zoomSpeed() const noexcept;

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
    void setPeakingThreshold(int thresh);
    void setPeakingColorIndex(int idx);
    void setShowGrid(bool show);
    void setShowReticle(bool show);
    void setShowRoi(bool show);
    void setIsRoiLockedToTrack(bool locked);
    void applyRoiPreset(int presetIdx);
    void setTrackerTarget(double nx, double ny, double nw, double nh);

    // Optical Motion Commands (< 30 chars)
    void moveFocusStep(int steps);
    void moveZoomStep(int steps);
    void setFocusTarget(int position);
    void setZoomTarget(int position);
    void setFocusSpeed(int speed);
    void setZoomSpeed(int speed);
    void stopMotion();
    void emergencyStop();

    // Autofocus Commands (< 30 chars)
    void triggerAutoFocus();
    void abortAutoFocus();
    void setAfAlgorithm(int algorithm);
    void setFocusMetric(int metric);
    void setCafEnabled(bool enabled);
    void setCafThreshold(double threshold);

    // Parfocal Configuration Slots (< 30 chars)
    void setZoomTrackEnabled(bool enabled);
    void setInterpolationMode(int mode);
    void setActiveCurveIndex(int index);
    void setActiveCurveDistance(double meters);

    // Knot Table Manipulation Slots (< 30 chars)
    void addKnot(int zoom, int focus);
    void updateKnot(int index, int zoom, int focus);
    void removeKnot(int index);
    void clearKnots();
    void sortKnots();

    // Calibration Wizard Slots (< 30 chars)
    void startCalibWizard(int knots, int zMin, int zMax, double dist);
    void cancelCalibWizard();

    // Curve I/O Slots (< 30 chars)
    void loadTrackCurves(const QString& path);
    void saveTrackCurves(const QString& path);
    void exportJsonProfile(const QString& path);
    void importJsonProfile(const QString& path);

    // Driver Binding Slots (< 30 chars)
    void setActiveDriverType(int type);
    void setCameraAddress(int address);
    void scanSerialPorts();
    void bindMockDriver();
    void bindPelcoDriver(int portIdx, int baud, int address);
    void bindViscaDriver(int portIdx, int baud, int address);
    void loadDriverPlugin(const QString& path, int portIdx, int baud);

signals:
    void activeChannelChanged();
    void lensModeChanged();
    void statusMessageChanged();
    void telemetryUpdated();
    void limitsChanged();

    void afStateChanged();
    void afConfigChanged();
    void cafToggled();
    void cafConfigChanged();
    void peakFitUpdated();
    void samplesUpdated();

    void sourceUriChanged();
    void playbackStateChanged();
    void ptsChanged();
    void sourceModeChanged();
    void syntheticPlaneChanged();
    void syntheticNoiseChanged();

    void roiRectChanged();
    void showPeakingChanged();
    void peakingThresholdChanged();
    void peakingColorChanged();
    void showGridChanged();
    void showReticleChanged();
    void showRoiChanged();
    void trackerLockChanged();
    void frameReady();

    void driverTypeChanged();
    void portBindingChanged();
    void cameraAddressChanged();
    void portsScanned();
    void speedsChanged();
    void emergencyStopOccurred();

    // Parfocal & Calibration Signals (Phase 5)
    void zoomTrackChanged();
    void interpolationModeChanged();
    void activeCurveChanged();
    void curveSetChanged();
    void knotsUpdated();
    void calibStateChanged();
    void calibProgressChanged();
    void curveIoResult(bool success, const QString& message);

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

    void requestTriggerAf(int channel, int algorithm, int metric);
    void requestAbortAf(int channel);
    void requestCaf(int channel, bool enabled, double threshold);
    void requestTrackerRoi(int channel, double nx, double ny, double nw, double nh);
    void requestFocusMetric(int channel, int metric);

    void requestBindMock(int channel);
    void requestBindPelco(int channel, int port, int baud, int address);
    void requestBindVisca(int channel, int port, int baud, int address);
    void requestLoadDll(int channel, const QString& path, int port, int baud);

    // Parfocal & Calibration Dispatch Signals (< 30 chars)
    void requestZoomTrack(int channel, bool enabled);
    void requestInterpMode(int channel, int mode);
    void requestActiveCurve(int channel, int index);
    void requestCurveDist(int channel, int index, double dist);
    void requestAddKnot(int channel, int zoom, int focus);
    void requestUpdateKnot(int channel, int index, int zoom, int focus);
    void requestRemoveKnot(int channel, int index);
    void requestClearKnots(int channel);
    void requestSortKnots(int channel);
    void requestStartCalib(int channel, int knots, int zMin, int zMax, double dist);
    void requestCancelCalib(int channel);
    void requestLoadTrack(int channel, const QString& path);
    void requestSaveTrack(int channel, const QString& path);
    void requestImportJson(int channel, const QString& path);
    void requestExportJson(int channel, const QString& path);

private slots:
    void onStatusUpdated(int focus, int zoom, int limits, int mode, bool afActive);
    void onTelemetryReady(double metricScore, int channel);
    void onAfStateUpdated(int state, bool isRunning);
    void onPeakFitted(double vertex, double score, double curvature, double r2, bool valid);
    void onSamplesReady(const QVariantList& samples);
    void onSourceOpened(int width, int height, double fps);
    void onPlaybackStateChanged(int state);
    void onDriverBound(int channel, int driverType, const QString& portName, int baud, int address, bool success,
        const QString& error);

    // Parfocal & Calibration Feedback Slots
    void onZoomTrackState(int channel, bool enabled);
    void onCurveSetUpdated(int channel, int curveCount, int activeIndex, double dist, bool valid);
    void onKnotsReady(int channel, const QVariantList& knots);
    void onCalibProgress(int channel, int step, int total, double progress, const QString& status);
    void onCalibFinished(int channel, bool success, const QString& message);
    void onCurveIoFinished(int channel, bool success, const QString& message);

private:
    void setupWorkerThread();

    struct ChannelDriverState {
        int driverType { 0 };
        QString driverName { QStringLiteral("MOCK") };
        QString portName { QStringLiteral("IN-MEMORY") };
        int baudRate { 115200 };
        int cameraAddress { 1 };
        bool isConnected { true };
        int focusSpeed { 127 };
        int zoomSpeed { 127 };
    };

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
    double m_latestMetricScore { 0.0 };

    // Autofocus State & Fit Telemetry
    bool m_isAfRunning { false };
    int m_afState { 0 };
    int m_afAlgorithm { 1 }; // ScanThreePhase default
    int m_focusMetric { 0 }; // SobelGradientSquared default
    bool m_cafEnabled { false };
    double m_cafThreshold { 0.25 };
    double m_peakSummit { 0.0 };
    double m_peakScore { 0.0 };
    double m_peakFwhm { 0.0 };
    double m_peakCurvature { 0.0 };
    double m_peakR2 { 0.0 };
    bool m_isPeakValid { false };
    QVariantList m_samplePoints { };

    QString m_sourceUri { QStringLiteral("synthetic://siemens-star") };
    int m_playbackState { 0 };
    double m_currentFps { 30.0 };
    double m_currentPts { 0.0 };

    bool m_isSynthetic { true };
    int m_syntheticPlane { 32768 };
    double m_syntheticNoise { 1.5 };

    QRectF m_roiRect { 0.25, 0.25, 0.50, 0.50 };
    bool m_showPeaking { false };
    int m_peakingThreshold { 80 };
    int m_peakingColorIndex { 0 };
    bool m_showGrid { true };
    bool m_showReticle { true };
    bool m_showRoi { true };
    bool m_isRoiLockedToTrack { false };

    std::array<ChannelDriverState, 4> m_driverStates { };
    QStringList m_availablePorts { };

    // Parfocal & Calibration State (Phase 5)
    bool m_zoomTrackEnabled { false };
    int m_interpolationMode { 0 };
    int m_activeCurveIndex { 0 };
    int m_curveCount { 1 };
    double m_activeCurveDistance { 0.0 };
    bool m_isCurveValid { false };
    QVariantList m_activeCurveKnots { };

    bool m_isCalibrating { false };
    int m_calibStep { 0 };
    int m_calibTotalSteps { 0 };
    double m_calibProgress { 0.0 };
    QString m_calibStatusText { QStringLiteral("Idle") };

    QString m_statusMessage { QStringLiteral("Ready") };
};

} // namespace LensApp
