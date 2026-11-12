#pragma once

/// @file StabilizerPlayerController.h
/// @brief Master QML controller bridging UI events with Video and Stabilizer subsystems.

#include <cstdint>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariantList>

#include "AtomicTripleBuffer.h"
#include "PresetManager.h"
#include "StabilizerConfigModel.h"
#include "StabilizerFramePacket.h"
#include "StabilizerTypes.h"
#include "TelemetryRingBuffer.h"
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

    // Sub-models
    Q_PROPERTY(StabilizerConfigModel* configModel READ configModel CONSTANT)
    Q_PROPERTY(TelemetryRingBuffer* telemetryRingBuffer READ telemetryRingBuffer CONSTANT)
    Q_PROPERTY(PresetManager* presetManager READ presetManager CONSTANT)

    // Real-time diagnostics
    Q_PROPERTY(int detectedFeatures READ detectedFeatures NOTIFY telemetryUpdated)
    Q_PROPERTY(int trackedFeatures READ trackedFeatures NOTIFY telemetryUpdated)
    Q_PROPERTY(int inlierCount READ inlierCount NOTIFY telemetryUpdated)
    Q_PROPERTY(double inlierRatio READ inlierRatio NOTIFY telemetryUpdated)
    Q_PROPERTY(double totalLatencyMs READ totalLatencyMs NOTIFY telemetryUpdated)
    Q_PROPERTY(int pyramidLatencyUs READ pyramidLatencyUs NOTIFY telemetryUpdated)
    Q_PROPERTY(int featureDetectLatencyUs READ featureDetectLatencyUs NOTIFY telemetryUpdated)
    Q_PROPERTY(int opticalFlowLatencyUs READ opticalFlowLatencyUs NOTIFY telemetryUpdated)
    Q_PROPERTY(int motionModelLatencyUs READ motionModelLatencyUs NOTIFY telemetryUpdated)
    Q_PROPERTY(int trajectoryFilterLatencyUs READ trajectoryFilterLatencyUs NOTIFY telemetryUpdated)
    Q_PROPERTY(int warpLatencyUs READ warpLatencyUs NOTIFY telemetryUpdated)
    Q_PROPERTY(QString engineStateText READ engineStateText NOTIFY telemetryUpdated)

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
    [[nodiscard]] PlaybackState playbackState() const noexcept;

    /// @brief Active source URI, file path, or device identifier.
    [[nodiscard]] QString sourceUri() const;

    /// @brief Sets active source URI, file path, or device identifier.
    /// @param uri New source URI.
    void setSourceUri(const QString& uri);

    /// @brief Human-readable status or diagnostic message.
    [[nodiscard]] QString statusMessage() const;

    /// @brief Video frame width in pixels.
    [[nodiscard]] int videoWidth() const noexcept;

    /// @brief Video frame height in pixels.
    [[nodiscard]] int videoHeight() const noexcept;

    /// @brief Current frame rate in frames per second.
    [[nodiscard]] double fps() const noexcept;

    /// @brief Accesses configuration sub-model.
    [[nodiscard]] StabilizerConfigModel* configModel() noexcept;

    /// @brief Accesses telemetry history ring buffer.
    [[nodiscard]] TelemetryRingBuffer* telemetryRingBuffer() noexcept;

    /// @brief Accesses preset manager.
    [[nodiscard]] PresetManager* presetManager() noexcept;

    // Diagnostic getters
    [[nodiscard]] int detectedFeatures() const noexcept;
    [[nodiscard]] int trackedFeatures() const noexcept;
    [[nodiscard]] int inlierCount() const noexcept;
    [[nodiscard]] double inlierRatio() const noexcept;
    [[nodiscard]] double totalLatencyMs() const noexcept;
    [[nodiscard]] int pyramidLatencyUs() const noexcept;
    [[nodiscard]] int featureDetectLatencyUs() const noexcept;
    [[nodiscard]] int opticalFlowLatencyUs() const noexcept;
    [[nodiscard]] int motionModelLatencyUs() const noexcept;
    [[nodiscard]] int trajectoryFilterLatencyUs() const noexcept;
    [[nodiscard]] int warpLatencyUs() const noexcept;
    [[nodiscard]] QString engineStateText() const;

    /// @brief Gets elements of 2x3 affine matrix [a11, a12, tx, a21, a22, ty].
    /// @param matrixType 0 for raw, 1 for smoothed, 2 for correction.
    [[nodiscard]] Q_INVOKABLE QVariantList getMatrixElements(int matrixType) const;

    /// @brief Accesses underlying lock-free triple buffer primitive.
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

    /// @brief Enumerate local hardware video capture devices.
    [[nodiscard]] Q_INVOKABLE QStringList availableHardwareDevices() const;

    /// @brief Load named preset profile.
    /// @param name Preset name.
    Q_INVOKABLE void loadPreset(const QString& name);

    /// @brief Export active configuration to JSON file.
    Q_INVOKABLE bool exportConfig(const QString& filePath);

    /// @brief Import configuration from JSON file.
    Q_INVOKABLE bool importConfig(const QString& filePath);

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

    StabilizerConfigModel configModel_ { };
    TelemetryRingBuffer telemetryRingBuffer_ { };
    PresetManager presetManager_ { };

    PlaybackState playbackState_ { PlaybackState::Idle };
    QString sourceUri_ { };
    QString statusMessage_ { QStringLiteral("Ready") };
    int videoWidth_ { 0 };
    int videoHeight_ { 0 };
    double fps_ { 0.0 };

    // Diagnostics cache
    Stabilizer::StabilizerDiagnostics lastDiagnostics_ { };
};

} // namespace StabilizerApp
