#pragma once

/// @file LensWorker.h
/// @brief Dedicated background worker for video decoding and optical engine execution.

#include <cstddef>
#include <cstdint>

#include <array>
#include <chrono>
#include <memory>
#include <string>

#include <QObject>
#include <QPointF>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include "AtomicTripleBuffer.h"
#include "IVideoDecoder.h"
#include "LensController.h"
#include "LensFramePacket.h"
#include "SerialTransport.h"
#include "SyntheticDefocusGenerator.h"
#include "ZoomTrackFocusEngine.h"
#include "ZoomTrackTypes.h"

namespace LensApp {

/// @class LensWorker
/// @brief Background thread worker driving video ingestion, zero-heap focus sharpness, and actuators.
class LensWorker : public QObject {
    Q_OBJECT

public:
    explicit LensWorker(Video::AtomicTripleBuffer<LensFramePacket>& buffer, QObject* parent = nullptr);
    ~LensWorker() override;

    LensWorker(const LensWorker&) = delete;
    LensWorker& operator=(const LensWorker&) = delete;
    LensWorker(LensWorker&&) = delete;
    LensWorker& operator=(LensWorker&&) = delete;

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] double currentFps() const noexcept;
    [[nodiscard]] int activeChannel() const noexcept;

public slots:
    void initWorker();
    void stopWorker();

    // Multimedia Ingestion Transport Slots (< 30 chars)
    void openSource(const QString& uri);
    void play();
    void pause();
    void stop();
    void stepFrame();
    void seek(double timestampSec);
    void setPlaybackRate(double rate);

    // Defocus Simulator Configuration (< 30 chars)
    void setSyntheticTarget(int focusPlane);
    void setSyntheticPattern(int pattern);
    void setSyntheticNoise(double sigma);

    // ROI Configuration (< 30 chars)
    void setRoi(double x, double y, double w, double h);

    // Optical Axis Commands (< 30 chars)
    void onSetActiveChannel(int channel);
    void onSetLensMode(int channel, int mode);
    void onMoveFocusStep(int channel, int steps);
    void onMoveZoomStep(int channel, int steps);
    void onSetFocusTarget(int channel, int position);
    void onSetZoomTarget(int channel, int position);
    void onSetFocusSpeed(int channel, int speed);
    void onSetZoomSpeed(int channel, int speed);
    void onStopMotion(int channel);
    void onEmergencyStop();

    // Autofocus Slots (< 30 chars)
    void onTriggerAf(int channel, int algorithm, int metric);
    void onAbortAf(int channel);
    void onSetCaf(int channel, bool enabled, double threshold);
    void onSetTrackerRoi(int channel, double nx, double ny, double nw, double nh);
    void onSetFocusMetric(int channel, int metric);

    // Driver Binding Slots (< 30 chars)
    void onBindMock(int channel);
    void onBindPelco(int channel, int port, int baud, int address);
    void onBindVisca(int channel, int port, int baud, int address);
    void onLoadDll(int channel, const QString& path, int port, int baud);

    // Parfocal & Zoom-Track Slots (< 30 chars)
    void onSetZoomTrack(int channel, bool enabled);
    void onSetInterpMode(int channel, int mode);
    void onSetActiveCurve(int channel, int index);
    void onSetCurveDistance(int channel, int index, double distanceMeters);
    void onAddKnot(int channel, int zoom, int focus);
    void onUpdateKnot(int channel, int index, int zoom, int focus);
    void onRemoveKnot(int channel, int index);
    void onClearKnots(int channel);
    void onSortKnots(int channel);
    void onRequestKnots(int channel);

    // Calibration Wizard Slots (< 30 chars)
    void onStartCalib(int channel, int knots, int zMin, int zMax, double dist);
    void onCancelCalib(int channel);

    // Curve File I/O Slots (< 30 chars)
    void onLoadTrackFile(int channel, const QString& path);
    void onSaveTrackFile(int channel, const QString& path);
    void onImportJsonProfile(int channel, const QString& path);
    void onExportJsonProfile(int channel, const QString& path);

signals:
    void workerInitialized();
    void frameReady();
    void sourceOpened(int width, int height, double fps);
    void playbackStateChanged(int state);
    void statusUpdated(int focus, int zoom, int limits, int mode, bool afActive);
    void telemetryReady(double metricScore, int channel);
    void afStateUpdated(int state, bool isRunning);
    void peakFitted(double vertex, double score, double curvature, double r2, bool valid);
    void samplesReady(const QVariantList& samples);
    void errorOccurred(const QString& message);
    void driverBound(int channel, int driverType, const QString& portName, int baud, int address, bool success,
        const QString& error);

    // Parfocal & Calibration Signals (< 30 chars)
    void zoomTrackState(int channel, bool enabled);
    void curveSetUpdated(int channel, int curveCount, int activeIndex, double dist, bool valid);
    void knotsReady(int channel, const QVariantList& knots);
    void calibProgress(int channel, int step, int total, double progress, const QString& status);
    void calibFinished(int channel, bool success, const QString& message);
    void curveIoFinished(int channel, bool success, const QString& message);

private slots:
    void processLoopTick();

private:
    void initDrivers();
    bool initBackend(const std::string& uri);
    void extractLuma(const std::uint8_t* rgb, std::uint8_t* luma, std::size_t pixels) noexcept;

    struct ChannelDriverSlot {
        int driverType { 0 };
        std::unique_ptr<Transport::SerialTransport> serialTransport { nullptr };
        std::string portName { "IN-MEMORY" };
        std::uint32_t baudRate { 115200U };
        std::uint8_t address { 1U };
    };

    Video::AtomicTripleBuffer<LensFramePacket>& m_tripleBuffer;
    std::array<ChannelDriverSlot, 4> m_driverSlots { };
    LensControl::LensController m_controller { };
    std::unique_ptr<Video::IVideoDecoder> m_decoder { nullptr };
    SyntheticDefocusGenerator m_defocusGen { };

    QTimer* m_timer { nullptr };
    int m_activeChannel { 0 };
    bool m_isRunning { false };
    bool m_isPaused { false };
    bool m_isSynthetic { true };

    std::uint32_t m_frameWidth { 1280U };
    std::uint32_t m_frameHeight { 720U };
    double m_nominalFps { 30.0 };
    double m_playbackRate { 1.0 };
    std::string m_currentUri { "synthetic://siemens-star" };

    std::chrono::steady_clock::time_point m_lastTickTime { };
    LensControl::NormalizedRoi m_activeRoi { 64U, 64U, 128U, 128U };
    std::uint64_t m_frameCounter { 0U };
    LensControl::AutoFocusState m_lastAfState { LensControl::AutoFocusState::Idle };
    bool m_lastAfRunning { false };

    enum class CalibWizardState : std::uint8_t {
        Idle = 0U,
        SlewingZoom = 1U,
        SettlingOptics = 2U,
        WaitingAfSweep = 3U,
        RecordingKnot = 4U,
        Completed = 5U,
        Aborted = 6U
    };

    struct CalibWizardContext {
        CalibWizardState state { CalibWizardState::Idle };
        int channel { 0 };
        std::size_t totalKnots { 8U };
        std::size_t currentKnotIdx { 0U };
        std::uint16_t zoomMin { 1000U };
        std::uint16_t zoomMax { 60000U };
        float targetDistanceMeters { 0.0F };
        std::uint16_t currentTargetZoom { 0U };
        std::chrono::steady_clock::time_point settleStartTime { };
        std::chrono::milliseconds settleDelay { 200 };
    };

    void stepCalibWizard(const LensControl::LensStatus& status);

    CalibWizardContext m_calibContext { };
};

} // namespace LensApp
