#pragma once

/// @file SightlineVideoController.h
/// @brief Master video stream controller for Sightline SLA camera feeds and synthetic simulation.

#include "VideoQuickItem.h"
#include <Video/DecoderFactory.h>
#include <Video/DecoderTypes.h>
#include <Video/IVideoDecoder.h>

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <atomic>
#include <memory>

namespace SightlineApp {

/// @class SightlineVideoController
/// @brief Manages RTSP/UDP stream ingestion, synthetic pattern fallback, frame pacing, and HUD telemetry.
class SightlineVideoController : public QObject {
    Q_OBJECT

    Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(QString sourceUri READ sourceUri WRITE setSourceUri NOTIFY sourceUriChanged)
    Q_PROPERTY(int activeCamera READ activeCamera WRITE selectCamera NOTIFY activeCameraChanged)
    Q_PROPERTY(bool isSynthetic READ isSynthetic WRITE setSyntheticMode NOTIFY syntheticChanged)
    Q_PROPERTY(double fps READ fps NOTIFY statsUpdated)
    Q_PROPERTY(double avgDecodeTimeMs READ avgDecodeTimeMs NOTIFY statsUpdated)
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY statsUpdated)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY statsUpdated)
    Q_PROPERTY(double bitrateKbps READ bitrateKbps NOTIFY statsUpdated)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QStringList availableBackends READ availableBackends CONSTANT)
    Q_PROPERTY(int backendIndex READ backendIndex WRITE setBackendIndex NOTIFY backendIndexChanged)
    Q_PROPERTY(bool pipEnabled READ pipEnabled WRITE setPipEnabled NOTIFY pipEnabledChanged)
    Q_PROPERTY(int pipCamera READ pipCamera NOTIFY pipCameraChanged)

public:
    /// @enum PlaybackState
    /// @brief Current state of video decoder and network stream pipeline.
    enum class PlaybackState { Idle, Opening, Playing, Paused, Error };
    Q_ENUM(PlaybackState)

    /// @brief Constructor.
    /// @param[in] parent Optional parent QObject.
    explicit SightlineVideoController(QObject* parent = nullptr);

    /// @brief Destructor.
    ~SightlineVideoController() override;

    /// @brief Get current playback state.
    /// @return Active PlaybackState enum value.
    [[nodiscard]] PlaybackState playbackState() const noexcept;

    /// @brief Get current video stream URI.
    /// @return Configured source URI.
    [[nodiscard]] QString sourceUri() const;

    /// @brief Get active camera index (0: EO Daylight, 1: IR Thermal).
    /// @return Selected camera index.
    [[nodiscard]] int activeCamera() const noexcept;

    /// @brief Get synthetic test pattern simulation flag.
    /// @return True if synthetic generation is active.
    [[nodiscard]] bool isSynthetic() const noexcept;

    /// @brief Get measured frame rate.
    /// @return Current frames per second.
    [[nodiscard]] double fps() const noexcept;

    /// @brief Get average decode latency in milliseconds.
    /// @return Latency in ms.
    [[nodiscard]] double avgDecodeTimeMs() const noexcept;

    /// @brief Get current frame width in pixels.
    /// @return Width in pixels.
    [[nodiscard]] int frameWidth() const noexcept;

    /// @brief Get current frame height in pixels.
    /// @return Height in pixels.
    [[nodiscard]] int frameHeight() const noexcept;

    /// @brief Get estimated video stream bitrate in kilobits per second.
    /// @return Bitrate in kbps.
    [[nodiscard]] double bitrateKbps() const noexcept;

    /// @brief Get user-facing status message string.
    /// @return Status string.
    [[nodiscard]] QString statusMessage() const;

    /// @brief List of compiled decoder backends (FFmpeg, GStreamer, Mock).
    /// @return List of backend names.
    [[nodiscard]] QStringList availableBackends() const;

    /// @brief Index of selected backend.
    /// @return 0-based backend index.
    [[nodiscard]] int backendIndex() const noexcept;

    /// @brief Select active backend index.
    /// @param[in] index Index into availableBackends list.
    void setBackendIndex(int index);

    /// @brief Check if Picture-in-Picture thumbnail mode is active.
    /// @return True if PIP is enabled.
    [[nodiscard]] bool pipEnabled() const noexcept;

    /// @brief Get camera index used for the Picture-in-Picture feed.
    /// @return Alternate camera index (0 or 1).
    [[nodiscard]] int pipCamera() const noexcept;

public slots:
    /// @brief Set video stream URI.
    /// @param[in] uri RTSP, UDP, or mock URI.
    void setSourceUri(const QString& uri);

    /// @brief Start or resume video decoding pipeline.
    void startStream();

    /// @brief Stop active decoding pipeline and release network resources.
    void stopStream();

    /// @brief Restart current stream pipeline.
    void restartStream();

    /// @brief Select active sensor camera (0: EO, 1: IR).
    /// @param[in] camIndex 0 for EO, 1 for IR.
    void selectCamera(int camIndex);

    /// @brief Enable or disable synthetic test pattern generator.
    /// @param[in] enabled True for synthetic simulation, false for live stream.
    void setSyntheticMode(bool enabled);

    /// @brief Enable or disable Picture-in-Picture thumbnail.
    /// @param[in] enabled PIP enable flag.
    void setPipEnabled(bool enabled);

    /// @brief Swap primary and PIP inset camera feeds.
    void swapPipFeeds();

    /// @brief Capture high-resolution snapshot of current video frame.
    /// @param[in] filePath Target file path, or empty for automatic timestamp.
    /// @return Absolute path where image was saved, or empty on failure.
    QString takeSnapshot(const QString& filePath = QString {});

    /// @brief Attach presentation QML VideoQuickItem to controller.
    /// @param[in] item Target VideoQuickItem instance.
    void attachVideoItem(VideoQuickItem* item);

    /// @brief Attach secondary presentation QML VideoQuickItem for PIP.
    /// @param[in] item Target VideoQuickItem instance.
    void attachPipVideoItem(VideoQuickItem* item);

    /// @brief Adapt stream source to matching target IP address.
    /// @param[in] host Target IP address.
    void updateHostAddress(const QString& host);

signals:
    /// @brief Emitted when a new frame is decoded.
    /// @param[in] frame Decoded QImage frame.
    void frameDecoded(const QImage& frame);

    /// @brief Emitted when a new PIP frame is decoded.
    /// @param[in] frame Decoded secondary QImage frame.
    void pipFrameDecoded(const QImage& frame);

    /// @brief Emitted when playback state transitions.
    void playbackStateChanged();

    /// @brief Emitted when source URI changes.
    void sourceUriChanged();

    /// @brief Emitted when active camera index changes.
    void activeCameraChanged();

    /// @brief Emitted when synthetic mode toggles.
    void syntheticChanged();

    /// @brief Emitted when PIP enabled status changes.
    void pipEnabledChanged();

    /// @brief Emitted when PIP camera channel changes.
    void pipCameraChanged();

    /// @brief Emitted when video statistics update.
    void statsUpdated();

    /// @brief Emitted when status message updates.
    void statusMessageChanged();

    /// @brief Emitted when backend selection changes.
    void backendIndexChanged();

    /// @brief Emitted after a successful snapshot capture.
    /// @param[in] path Absolute path of saved screenshot.
    void snapshotTaken(const QString& path);

private:
    void workerLoop();
    void updateState(PlaybackState state, const QString& msg);
    [[nodiscard]] QString resolveSourceUri() const;
    void applyThermalLook(QImage& image);

    mutable QMutex m_decoderMutex {};
    mutable QMutex m_snapshotMutex {};
    std::unique_ptr<Video::IVideoDecoder> m_decoder {};
    std::unique_ptr<QThread> m_thread {};

    std::atomic<bool> m_running { false };
    std::atomic<bool> m_paused { false };

    PlaybackState m_state { PlaybackState::Idle };
    QString m_sourceUri {};
    QString m_hostAddress { QStringLiteral("127.0.0.1") };
    int m_activeCamera { 0 };
    bool m_isSynthetic { true };
    int m_backendIndex { 0 };
    bool m_pipEnabled { false };

    double m_fps { 0.0 };
    double m_avgDecodeTimeMs { 0.0 };
    int m_frameWidth { 0 };
    int m_frameHeight { 0 };
    double m_bitrateKbps { 0.0 };
    QString m_statusMessage { QStringLiteral("Idle") };

    QImage m_lastFrameCopy {};
};

} // namespace SightlineApp
