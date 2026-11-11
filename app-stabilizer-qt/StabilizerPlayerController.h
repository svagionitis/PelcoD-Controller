#pragma once

/// @file StabilizerPlayerController.h
/// @brief Master QML controller bridging UI events with Video and Stabilizer subsystems.

#include <cstdint>

#include <QObject>
#include <QString>
#include <QThread>

#include "AtomicTripleBuffer.h"
#include "StabilizerFramePacket.h"
#include "StabilizerTypes.h"
#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

class StabilizerWorker;

/// @class StabilizerPlayerController
/// @brief High-level ViewModel managing playback state, stream source, and stabilization telemetry.
class StabilizerPlayerController : public QObject {
    Q_OBJECT

    Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(QString sourceUri READ sourceUri WRITE setSourceUri NOTIFY sourceUriChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoDimensionsChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoDimensionsChanged)
    Q_PROPERTY(double fps READ fps NOTIFY telemetryUpdated)

public:
    /// @enum PlaybackState
    /// @brief Playback lifecycle states.
    enum class PlaybackState : std::uint8_t { Idle = 0U, Opening, Playing, Paused, Error };
    Q_ENUM(PlaybackState)

    /// @brief Constructor initializing default controller state and launching worker thread.
    /// @param parent Optional parent QObject.
    explicit StabilizerPlayerController(QObject* parent = nullptr);

    /// @brief Destructor safely stopping and joining worker thread.
    ~StabilizerPlayerController() override;

    StabilizerPlayerController(const StabilizerPlayerController&) = delete;
    StabilizerPlayerController& operator=(const StabilizerPlayerController&) = delete;
    StabilizerPlayerController(StabilizerPlayerController&&) = delete;
    StabilizerPlayerController& operator=(StabilizerPlayerController&&) = delete;

    /// @brief Current playback state.
    /// @return Active PlaybackState enum value.
    [[nodiscard]] PlaybackState playbackState() const noexcept;

    /// @brief Active source URI, file path, or device identifier.
    /// @return Current source URI string.
    [[nodiscard]] QString sourceUri() const;

    /// @brief Sets active source URI, file path, or device identifier.
    /// @param uri New source URI.
    void setSourceUri(const QString& uri);

    /// @brief Human-readable status or diagnostic message.
    /// @return Current status message string.
    [[nodiscard]] QString statusMessage() const;

    /// @brief Video frame width in pixels.
    /// @return Frame width (0 if no frame).
    [[nodiscard]] int videoWidth() const noexcept;

    /// @brief Video frame height in pixels.
    /// @return Frame height (0 if no frame).
    [[nodiscard]] int videoHeight() const noexcept;

    /// @brief Current frame rate in frames per second.
    /// @return Current FPS metric.
    [[nodiscard]] double fps() const noexcept;

    /// @brief Accesses underlying lock-free triple buffer primitive.
    /// @return Mutable reference to triple buffer.
    [[nodiscard]] Video::AtomicTripleBuffer<StabilizerFramePacket>& tripleBuffer() noexcept;

    /// @brief Requests playback start.
    Q_INVOKABLE void play();

    /// @brief Requests playback pause.
    Q_INVOKABLE void pause();

    /// @brief Requests playback stop and stream teardown.
    Q_INVOKABLE void stop();

    /// @brief Advances playback by exactly one video frame.
    Q_INVOKABLE void stepFrame();

    /// @brief Seeks to target presentation timestamp in seconds.
    /// @param timestampSec Target time in seconds.
    Q_INVOKABLE void seek(double timestampSec);

    /// @brief Sets playback speed scaling multiplier.
    /// @param rate Multiplier [0.25 .. 4.0].
    Q_INVOKABLE void setPlaybackRate(double rate);

    /// @brief Dispatches updated video stabilization parameters to worker engine.
    /// @param config New stabilization configuration.
    void updateStabilizerConfig(const Stabilizer::VideoStabilizerConfig& config);

signals:
    /// @brief Emitted when playback state transitions.
    void playbackStateChanged();

    /// @brief Emitted when source URI changes.
    void sourceUriChanged();

    /// @brief Emitted when status message changes.
    void statusMessageChanged();

    /// @brief Emitted when decoded frame dimensions change.
    void videoDimensionsChanged();

    /// @brief Emitted when telemetry or FPS updates.
    void telemetryUpdated();

    /// @brief Emitted when a new frame is ready in the triple buffer for display.
    void frameReady();

private:
    void setPlaybackState(PlaybackState state);
    void setStatusMessage(const QString& msg);

    Video::AtomicTripleBuffer<StabilizerFramePacket> tripleBuffer_ { };
    QThread workerThread_ { };
    StabilizerWorker* worker_ { nullptr };

    PlaybackState playbackState_ { PlaybackState::Idle };
    QString sourceUri_ { };
    QString statusMessage_ { QStringLiteral("Ready") };
    int videoWidth_ { 0 };
    int videoHeight_ { 0 };
    double fps_ { 0.0 };
};

} // namespace StabilizerApp
