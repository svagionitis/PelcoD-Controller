#pragma once

/// @file LensWorker.h
/// @brief Dedicated background worker managing video ingestion, optical simulation, and LensController.

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <memory>
#include <string>

#include <QObject>
#include <QString>
#include <QTimer>

#include "AtomicTripleBuffer.h"
#include "IVideoDecoder.h"
#include "LensController.h"
#include "LensFramePacket.h"
#include "LensTypes.h"
#include "MockLensDriver.h"
#include "SyntheticDefocusGenerator.h"

namespace LensApp {

/// @class LensWorker
/// @brief Worker object executing on dedicated QThread for continuous ingestion and lens actuation.
class LensWorker : public QObject {
    Q_OBJECT

public:
    explicit LensWorker(Video::AtomicTripleBuffer<LensFramePacket>& tripleBuffer, QObject* parent = nullptr);
    ~LensWorker() override;

    LensWorker(const LensWorker&) = delete;
    LensWorker& operator=(const LensWorker&) = delete;
    LensWorker(LensWorker&&) = delete;
    LensWorker& operator=(LensWorker&&) = delete;

    [[nodiscard]] bool isRunning() const noexcept;
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

signals:
    void workerInitialized();
    void frameReady();
    void sourceOpened(int width, int height, double fps);
    void playbackStateChanged(int state);
    void statusUpdated(int focus, int zoom, int limits, int mode, bool afActive);
    void telemetryReady(double metricScore, int channel);
    void errorOccurred(const QString& message);

private slots:
    void processLoopTick();

private:
    void initDrivers();
    bool initBackend(const std::string& uri);
    void extractLuma(const std::uint8_t* rgb, std::uint8_t* luma, std::size_t pixels) noexcept;

    Video::AtomicTripleBuffer<LensFramePacket>& m_tripleBuffer;
    LensControl::LensController m_controller { };
    std::unique_ptr<Video::IVideoDecoder> m_decoder { nullptr };
    SyntheticDefocusGenerator m_defocusGen { };

    std::unique_ptr<LensControl::MockLensDriver> m_mockDrivers[4] { };

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
};

} // namespace LensApp
