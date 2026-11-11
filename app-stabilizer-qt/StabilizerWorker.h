#pragma once

/// @file StabilizerWorker.h
/// @brief High-priority background decoding and video stabilization worker.

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QTimer>

#include "AtomicTripleBuffer.h"
#include "DecoderFactory.h"
#include "DecoderTypes.h"
#include "IVideoDecoder.h"
#include "StabilizerFramePacket.h"
#include "SyntheticJitterGenerator.h"
#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

/// @class StabilizerWorker
/// @brief Worker object executing on dedicated QThread for continuous ingestion and stabilization.
class StabilizerWorker : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor.
    /// @param tripleBuffer Reference to lock-free triple buffer primitive.
    /// @param parent Optional parent QObject.
    explicit StabilizerWorker(
        Video::AtomicTripleBuffer<StabilizerFramePacket>& tripleBuffer, QObject* parent = nullptr);

    /// @brief Destructor.
    ~StabilizerWorker() override;

    StabilizerWorker(const StabilizerWorker&) = delete;
    StabilizerWorker& operator=(const StabilizerWorker&) = delete;
    StabilizerWorker(StabilizerWorker&&) = delete;
    StabilizerWorker& operator=(StabilizerWorker&&) = delete;

    /// @brief Current frame width in pixels.
    [[nodiscard]] std::uint32_t frameWidth() const noexcept;

    /// @brief Current frame height in pixels.
    [[nodiscard]] std::uint32_t frameHeight() const noexcept;

    /// @brief Whether worker is currently actively decoding.
    [[nodiscard]] bool isRunning() const noexcept;

public slots:
    /// @brief Initializes timers on the worker thread.
    void initWorker();

    /// @brief Opens a video source URI and prepares playback.
    /// @param uri Source URI, file path, device string, or synthetic scheme.
    void openSource(const QString& uri);

    /// @brief Starts or resumes playback loop.
    void play();

    /// @brief Pauses continuous playback loop.
    void pause();

    /// @brief Stops playback, tears down active decoder, and clears buffers.
    void stop();

    /// @brief Advances playback by exactly one video frame.
    void stepFrame();

    /// @brief Seeks to target timestamp in seconds (file sources only).
    /// @param timestampSec Target presentation timestamp.
    void seek(double timestampSec);

    /// @brief Sets playback speed multiplier [0.25 .. 4.0].
    /// @param rate Speed multiplier.
    void setPlaybackRate(double rate);

    /// @brief Updates video stabilization configuration parameters on the fly.
    /// @param config New stabilization configuration.
    void updateConfig(const Stabilizer::VideoStabilizerConfig& config);

    /// @brief Internal processing tick triggered by loop timer.
    void processTick();

signals:
    /// @brief Emitted when a new frame pair has been published to the triple buffer.
    void frameReady();

    /// @brief Emitted when stream metadata (resolution, FPS) is resolved.
    void sourceOpened(int width, int height, double fps);

    /// @brief Emitted when an unrecoverable decoding error occurs.
    void errorOccurred(const QString& errorMsg);

    /// @brief Emitted periodically with updated stage execution latencies and stats.
    void telemetryUpdated(const Stabilizer::StabilizerDiagnostics& diagnostics, double fps);

    /// @brief Emitted when end of stream is reached (file sources).
    void endOfStreamReached();

private:
    bool initBackend(const std::string& uri);
    void preallocateBuffers(std::uint32_t w, std::uint32_t h);
    void updateTimerInterval();

    Video::AtomicTripleBuffer<StabilizerFramePacket>& tripleBuffer_;

    std::unique_ptr<Video::IVideoDecoder> decoder_ { nullptr };
    SyntheticJitterGenerator syntheticGen_ { };
    bool isSyntheticSource_ { false };

    Stabilizer::VideoStabilizerEngine engine_ { };
    Stabilizer::VideoStabilizerConfig pendingConfig_ { };
    std::atomic<bool> hasPendingConfig_ { false };

    std::vector<std::uint8_t> lumaInputBuffer_ { };
    std::vector<std::uint8_t> lumaOutputBuffer_ { };

    QTimer* timer_ { nullptr };

    std::atomic<bool> isRunning_ { false };
    std::atomic<bool> isPaused_ { false };
    std::atomic<double> playbackRate_ { 1.0 };

    std::string currentUri_ { };
    std::uint32_t frameWidth_ { 0U };
    std::uint32_t frameHeight_ { 0U };
    double nominalFps_ { 30.0 };
    std::uint64_t frameCounter_ { 0U };

    std::chrono::steady_clock::time_point lastFrameTime_ { };
    std::chrono::steady_clock::time_point fpsWindowStart_ { };
    std::uint32_t fpsFrameCount_ { 0U };
    double measuredFps_ { 0.0 };
};

} // namespace StabilizerApp
