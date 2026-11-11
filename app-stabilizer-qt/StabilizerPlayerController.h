#pragma once

/// @file StabilizerPlayerController.h
/// @brief Master QML controller bridging UI events with Video and Stabilizer subsystems.

#include <cstdint>

#include <QObject>
#include <QString>

namespace StabilizerApp {

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

    /// @brief Constructor initializing default controller state.
    /// @param parent Optional parent QObject.
    explicit StabilizerPlayerController(QObject* parent = nullptr);

    /// @brief Destructor.
    ~StabilizerPlayerController() override = default;

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

    /// @brief Requests playback start.
    Q_INVOKABLE void play();

    /// @brief Requests playback pause.
    Q_INVOKABLE void pause();

    /// @brief Requests playback stop and stream teardown.
    Q_INVOKABLE void stop();

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

private:
    PlaybackState playbackState_ { PlaybackState::Idle };
    QString sourceUri_ { };
    QString statusMessage_ { QStringLiteral("Ready") };
    int videoWidth_ { 0 };
    int videoHeight_ { 0 };
    double fps_ { 0.0 };
};

} // namespace StabilizerApp
