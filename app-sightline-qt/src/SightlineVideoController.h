#pragma once

/// @file SightlineVideoController.h
/// @brief Master video stream controller for Sightline SLA camera feeds and synthetic simulation.

#include "VideoQuickItem.h"
#include <Video/DecoderFactory.h>
#include <Video/DecoderTypes.h>
#include <Video/IVideoDecoder.h>

#include <QByteArray>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QThread>
#include <array>
#include <atomic>
#include <deque>
#include <memory>

namespace SightlineApp {

/// @class SightlineVideoController
/// @brief Manages RTSP/UDP stream ingestion, synthetic pattern fallback, frame pacing, and HUD telemetry.
class SightlineVideoController : public QObject {
    Q_OBJECT

    Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(
        NetworkChannel networkChannel READ networkChannel WRITE selectNetworkChannel NOTIFY networkChannelChanged)
    Q_PROPERTY(
        int activeNetworkChannel READ activeNetworkChannel WRITE selectNetworkChannelInt NOTIFY networkChannelChanged)
    Q_PROPERTY(RtspTransport transportMode READ transportMode WRITE selectTransportMode NOTIFY transportModeChanged)
    Q_PROPERTY(
        int activeTransportMode READ activeTransportMode WRITE selectTransportModeInt NOTIFY transportModeChanged)
    Q_PROPERTY(QString rtspUsername READ rtspUsername WRITE setRtspUsername NOTIFY rtspAuthChanged)
    Q_PROPERTY(QString rtspPassword READ rtspPassword WRITE setRtspPassword NOTIFY rtspAuthChanged)
    Q_PROPERTY(bool authEnabled READ authEnabled WRITE setAuthEnabled NOTIFY rtspAuthChanged)
    Q_PROPERTY(QString sanitizedSourceUri READ sanitizedSourceUri NOTIFY sourceUriChanged)
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
    Q_PROPERTY(int contrastMode READ contrastMode NOTIFY contrastModeChanged)
    Q_PROPERTY(int activePalette READ activePalette NOTIFY activePaletteChanged)
    Q_PROPERTY(QRect enhancementRoi READ enhancementRoi NOTIFY enhancementRoiChanged)
    Q_PROPERTY(bool isPaused READ isPaused WRITE pauseStream NOTIFY pausedChanged)
    Q_PROPERTY(quint64 currentFramePts READ currentFramePts NOTIFY currentFramePtsChanged)
    Q_PROPERTY(int bufferedFrameCount READ bufferedFrameCount NOTIFY bufferedFramesChanged)
    Q_PROPERTY(int scrubOffset READ scrubOffset WRITE setScrubOffset NOTIFY scrubOffsetChanged)

public:
    /// @enum PlaybackState
    /// @brief Current state of video decoder and network stream pipeline.
    enum class PlaybackState { Idle, Opening, Playing, Paused, Error };
    Q_ENUM(PlaybackState)

    /// @enum NetworkChannel
    /// @brief Logical RTSP outbound display channel per Sightline EAN-RTSP.
    enum class NetworkChannel { Net0 = 0, Net1 = 1, Legacy = 2, Custom = 3 };
    Q_ENUM(NetworkChannel)

    /// @enum RtspTransport
    /// @brief Transport protocol mode for RTSP streaming per Sightline EAN-RTSP.
    enum class RtspTransport { Auto = 0, Tcp = 1, Udp = 2, Multicast = 3 };
    Q_ENUM(RtspTransport)

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

    /// @brief Get active logical RTSP network channel.
    /// @return Selected NetworkChannel enum value.
    [[nodiscard]] NetworkChannel networkChannel() const noexcept;

    /// @brief Get active logical RTSP network channel as integer.
    /// @return Selected channel integer (0: Net0, 1: Net1, 2: Legacy, 3: Custom).
    [[nodiscard]] int activeNetworkChannel() const noexcept;

    /// @brief Get active RTSP transport mode.
    /// @return Active RtspTransport enum value.
    [[nodiscard]] RtspTransport transportMode() const noexcept;

    /// @brief Get active RTSP transport mode as integer for QML.
    /// @return Transport mode index (0: Auto, 1: TCP, 2: UDP, 3: Multicast).
    [[nodiscard]] int activeTransportMode() const noexcept;

    /// @brief Get configured RTSP username.
    /// @return Username string.
    [[nodiscard]] QString rtspUsername() const;

    /// @brief Get configured RTSP password.
    /// @return Password string.
    [[nodiscard]] QString rtspPassword() const;

    /// @brief Check if RTSP authentication is enabled.
    /// @return True if authentication is enabled.
    [[nodiscard]] bool authEnabled() const noexcept;

    /// @brief Get sanitized source URI with redacted password.
    /// @return URI string with hidden password.
    [[nodiscard]] QString sanitizedSourceUri() const;

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

    /// @brief Get active contrast enhancement mode.
    /// @return Mode index (0: Off, 1: Hist Eq, 2: CLAHE, 3: Scintillation).
    [[nodiscard]] int contrastMode() const noexcept;

    /// @brief Get active false-color palette index.
    /// @return Palette index (0: White-Hot, 1: Black-Hot, 2: Rainbow, 3: Ironbow, 4: User).
    [[nodiscard]] int activePalette() const noexcept;

    /// @brief Get active video enhancement ROI.
    /// @return Rectangle bounding active ROI.
    [[nodiscard]] QRect enhancementRoi() const;

    /// @struct TimestampedFrame
    /// @brief Buffered decoded video frame paired with MISB microsecond timestamp.
    struct TimestampedFrame {
        QImage frame {};
        quint64 ptsUs { 0U };
    };

    /// @brief Check if stream playback is currently paused.
    /// @return True if paused.
    [[nodiscard]] bool isPaused() const noexcept;

    /// @brief Get current frame presentation timestamp in microseconds.
    /// @return Microsecond PTS.
    [[nodiscard]] quint64 currentFramePts() const noexcept;

    /// @brief Alias for currentFramePts.
    /// @return Microsecond PTS.
    [[nodiscard]] quint64 currentPts() const noexcept;

    /// @brief Get count of frames retained in circular buffer.
    /// @return Number of buffered frames.
    [[nodiscard]] int bufferedFrameCount() const noexcept;

    /// @brief Get active scrub frame offset relative to live stream.
    /// @return Offset (0: Live, negative: historical frames).
    [[nodiscard]] int scrubOffset() const noexcept;

public slots:
    /// @brief Pause or resume live video stream ingestion.
    /// @param[in] pause True to pause stream, false to resume live playback.
    void pauseStream(bool pause);

    /// @brief Toggle paused playback state.
    void togglePause();

    /// @brief Scrub playback frame to a historical buffered frame.
    /// @param[in] offset Non-positive offset from latest frame (0: Live, -1: 1 frame ago, etc.).
    void setScrubOffset(int offset);

    /// @brief Set video stream URI.
    /// @param[in] uri RTSP, UDP, or mock URI.
    void setSourceUri(const QString& uri);

    /// @brief Select logical RTSP network channel.
    /// @param[in] channel Target NetworkChannel (Net0, Net1, Legacy, Custom).
    void selectNetworkChannel(NetworkChannel channel);

    /// @brief Select logical RTSP network channel by integer (for QML).
    /// @param[in] channel 0: Net0, 1: Net1, 2: Legacy, 3: Custom.
    void selectNetworkChannelInt(int channel);

    /// @brief Select RTSP transport mode.
    /// @param[in] mode Target RtspTransport (Auto, Tcp, Udp, Multicast).
    void selectTransportMode(RtspTransport mode);

    /// @brief Select RTSP transport mode by integer index (for QML).
    /// @param[in] mode 0: Auto, 1: TCP, 2: UDP, 3: Multicast.
    void selectTransportModeInt(int mode);

    /// @brief Set RTSP username.
    /// @param[in] user Username string.
    void setRtspUsername(const QString& user);

    /// @brief Set RTSP password.
    /// @param[in] pass Password string.
    void setRtspPassword(const QString& pass);

    /// @brief Enable or disable RTSP Digest authentication.
    /// @param[in] enabled True to enable authentication.
    void setAuthEnabled(bool enabled);

    /// @brief Configure RTSP credentials in a single call.
    /// @param[in] user Username.
    /// @param[in] pass Plaintext password.
    void setRtspCredentials(const QString& user, const QString& pass);

    /// @brief Clear configured RTSP credentials and disable authentication.
    void clearRtspCredentials();

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

    /// @brief Update enhancement mode and parameters for camera.
    /// @param[in] cam Camera index (0: EO, 1: IR).
    /// @param[in] mode Enhancement algorithm mode.
    /// @param[in] strength Enhancement strength (0..255).
    /// @param[in] blend Blend percentage (0..100).
    /// @param[in] sharpen Sharpen strength (0..255).
    /// @param[in] radius Kernel radius (1..15).
    void updateEnhancementMode(int cam, int mode, int strength, int blend, int sharpen, int radius);

    /// @brief Update histogram, contrast, and brightness parameters.
    /// @param[in] cam Camera index.
    /// @param[in] featureBased Enable feature-based equalization.
    /// @param[in] sqrtHist Enable square-root histogram scaling.
    /// @param[in] aveRate Rolling average temporal rate (0..255).
    /// @param[in] maxPct Maximum histogram peak clipping percentage.
    /// @param[in] brightness Offset level (-128..127).
    /// @param[in] contrast Contrast multiplier (-128..127).
    void updateHistogram(
        int cam, bool featureBased, bool sqrtHist, int aveRate, int maxPct, int brightness, int contrast);

    /// @brief Update false-color palette selection.
    /// @param[in] cam Camera index.
    /// @param[in] paletteIndex Palette enum (0: White-Hot, 1: Black-Hot, 2: Rainbow, 3: Ironbow, 4: User).
    void updateFalseColor(int cam, int paletteIndex);

    /// @brief Update user-defined false-color palette LUT data.
    /// @param[in] paletteIndex Palette ID (0..3).
    /// @param[in] yuvData Binary 256-color palette data.
    void updateUserPalette(int paletteIndex, const QByteArray& yuvData);

    /// @brief Update enhancement region of interest.
    /// @param[in] cam Camera index.
    /// @param[in] row Top row coordinate.
    /// @param[in] col Left column coordinate.
    /// @param[in] height ROI height.
    /// @param[in] width ROI width.
    void updateEnhancementRoi(int cam, int row, int col, int height, int width);

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

    /// @brief Emitted when logical RTSP network channel changes.
    void networkChannelChanged();

    /// @brief Emitted when RTSP transport mode changes.
    void transportModeChanged();

    /// @brief Emitted when RTSP authentication credentials change.
    void rtspAuthChanged();

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

    /// @brief Emitted when contrast enhancement mode changes.
    void contrastModeChanged();

    /// @brief Emitted when active palette index changes.
    void activePaletteChanged();

    /// @brief Emitted when enhancement ROI updates.
    void enhancementRoiChanged();

    /// @brief Emitted when stream pause state changes.
    void pausedChanged();

    /// @brief Emitted when current frame presentation timestamp changes.
    void currentFramePtsChanged();

    /// @brief Emitted when number of buffered frames updates.
    void bufferedFramesChanged();

    /// @brief Emitted when scrub offset updates.
    void scrubOffsetChanged();

private:
    struct EnhancementConfig {
        int mode { 0 };
        int strength { 128 };
        int blend { 100 };
        int sharpen { 0 };
        int radius { 1 };
        int brightness { 0 };
        int contrast { 0 };
        int paletteIndex { 0 };
        QRect roi {};
    };

    void workerLoop();
    void updateState(PlaybackState state, const QString& msg);
    [[nodiscard]] QString resolveSourceUri() const;
    void applyThermalLook(QImage& image);
    void applyEnhancement(QImage& image, int cam);
    void applyFalseColorLut(QImage& image, const QRect& targetRoi, int paletteIndex, int cam);
    void applyContrastBrightness(QImage& image, const QRect& targetRoi, int brightness, int contrast);
    void applyNativeSharpen(QImage& image, const QRect& targetRoi, int sharpen);
    void applyHistogramEq(QImage& image, const QRect& targetRoi, int blend);
    void generatePaletteTables();

    static constexpr std::size_t MaxRingBufferSize { 90U };
    std::deque<TimestampedFrame> m_frameRingBuffer {};
    mutable QMutex m_bufferMutex {};
    int m_scrubOffset { 0 };
    quint64 m_currentFramePts { 0U };

    mutable QMutex m_decoderMutex {};
    mutable QMutex m_snapshotMutex {};
    mutable QMutex m_enhancementMutex {};
    std::unique_ptr<Video::IVideoDecoder> m_decoder {};
    std::unique_ptr<QThread> m_thread {};

    std::atomic<bool> m_running { false };
    std::atomic<bool> m_paused { false };

    PlaybackState m_state { PlaybackState::Idle };
    NetworkChannel m_networkChannel { NetworkChannel::Net0 };
    RtspTransport m_transportMode { RtspTransport::Auto };
    QString m_rtspUsername {};
    QString m_rtspPassword {};
    bool m_authEnabled { false };
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

    std::array<EnhancementConfig, 4> m_enhancementConfigs {};
    std::array<QByteArray, 4> m_userPalettes {};
    std::array<std::array<QRgb, 256>, 4> m_presetPalettes {};
};

} // namespace SightlineApp
