#pragma once

/// @file VideoPlayerController.h
/// @brief Master QML controller bridging UI events with Video and VideoFilters subsystems.

#include "DecoderFactory.h"
#include "DecoderTypes.h"
#include "DeviceEnumerator.h"
#include "IVideoDecoder.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <memory>
#include <vector>

namespace VideoApp {

class VideoQuickItem;

/// @class VideoPlayerController
/// @brief Manages background decoding thread, backend selection, dynamic filter pipeline, and telemetry.
class VideoPlayerController : public QObject {
    Q_OBJECT

    // --- State & Source Properties ---
    Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(QString sourceUri READ sourceUri WRITE setSourceUri NOTIFY sourceUriChanged)
    Q_PROPERTY(int backendIndex READ backendIndex WRITE setBackendIndex NOTIFY backendIndexChanged)
    Q_PROPERTY(int deviceIndex READ deviceIndex WRITE setDeviceIndex NOTIFY deviceIndexChanged)
    Q_PROPERTY(bool isLoopPlayback READ isLoopPlayback WRITE setLoopPlayback NOTIFY loopPlaybackChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

    // --- Enums & Catalogs ---
    Q_PROPERTY(QStringList availableBackends READ availableBackends CONSTANT)
    Q_PROPERTY(QVariantList availableDevices READ availableDevices NOTIFY availableDevicesChanged)

    // --- Timeline Properties ---
    Q_PROPERTY(double positionSeconds READ positionSeconds NOTIFY positionSecondsChanged)
    Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY durationSecondsChanged)
    Q_PROPERTY(bool isSeekable READ isSeekable NOTIFY isSeekableChanged)

    // --- Real-Time Performance & Telemetry ---
    Q_PROPERTY(double fps READ fps NOTIFY statsUpdated)
    Q_PROPERTY(double avgDecodeTimeMs READ avgDecodeTimeMs NOTIFY statsUpdated)
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY statsUpdated)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY statsUpdated)
    Q_PROPERTY(QString codecName READ codecName NOTIFY statsUpdated)
    Q_PROPERTY(QString pixelFormat READ pixelFormat NOTIFY statsUpdated)
    Q_PROPERTY(qint64 totalFrames READ totalFrames NOTIFY statsUpdated)
    Q_PROPERTY(qint64 droppedFrames READ droppedFrames NOTIFY statsUpdated)
    Q_PROPERTY(double bitrateKbps READ bitrateKbps NOTIFY statsUpdated)

    // --- Filter Controls ---
    Q_PROPERTY(bool dehazeEnabled READ dehazeEnabled WRITE setDehazeEnabled NOTIFY filterConfigChanged)
    Q_PROPERTY(
        bool stabilizationEnabled READ stabilizationEnabled WRITE setStabilizationEnabled NOTIFY filterConfigChanged)
    Q_PROPERTY(bool claheEnabled READ claheEnabled WRITE setClaheEnabled NOTIFY filterConfigChanged)
    Q_PROPERTY(bool denoiseEnabled READ denoiseEnabled WRITE setDenoiseEnabled NOTIFY filterConfigChanged)
    Q_PROPERTY(bool sharpenEnabled READ sharpenEnabled WRITE setSharpenEnabled NOTIFY filterConfigChanged)
    Q_PROPERTY(bool edgeDetectEnabled READ edgeDetectEnabled WRITE setEdgeDetectEnabled NOTIFY filterConfigChanged)
    Q_PROPERTY(int whiteBalanceMode READ whiteBalanceMode WRITE setWhiteBalanceMode NOTIFY filterConfigChanged)

    Q_PROPERTY(int falseColorPalette READ falseColorPalette WRITE setFalseColorPalette NOTIFY filterConfigChanged)
    Q_PROPERTY(int isothermPreset READ isothermPreset WRITE setIsothermPreset NOTIFY filterConfigChanged)
    Q_PROPERTY(
        bool hotspotTrackerEnabled READ hotspotTrackerEnabled WRITE setHotspotTrackerEnabled NOTIFY filterConfigChanged)

    Q_PROPERTY(bool mtiMotionEnabled READ mtiMotionEnabled WRITE setMtiMotionEnabled NOTIFY filterConfigChanged)
    Q_PROPERTY(int opticalFlowMode READ opticalFlowMode WRITE setOpticalFlowMode NOTIFY filterConfigChanged)
    Q_PROPERTY(bool heatmapEnabled READ heatmapEnabled WRITE setHeatmapEnabled NOTIFY filterConfigChanged)

    Q_PROPERTY(int reticleStyle READ reticleStyle WRITE setReticleStyle NOTIFY filterConfigChanged)
    Q_PROPERTY(bool tripwireEnabled READ tripwireEnabled WRITE setTripwireEnabled NOTIFY filterConfigChanged)
    Q_PROPERTY(int privacyMaskMode READ privacyMaskMode WRITE setPrivacyMaskMode NOTIFY filterConfigChanged)
    Q_PROPERTY(bool watermarkEnabled READ watermarkEnabled WRITE setWatermarkEnabled NOTIFY filterConfigChanged)
    Q_PROPERTY(
        bool telemetryOsdEnabled READ telemetryOsdEnabled WRITE setTelemetryOsdEnabled NOTIFY filterConfigChanged)

public:
    /// @enum PlaybackState
    /// @brief Operational state of the video playback controller.
    enum class PlaybackState { Idle, Opening, Playing, Paused, Error };
    Q_ENUM(PlaybackState)

    /// @brief Constructor.
    /// @param[in] parent Optional QObject parent.
    explicit VideoPlayerController(QObject* parent = nullptr);

    /// @brief Destructor. Safely joins worker thread.
    ~VideoPlayerController() override;

    // Getters
    [[nodiscard]] PlaybackState playbackState() const noexcept;
    [[nodiscard]] QString sourceUri() const;
    void setSourceUri(const QString& uri);

    [[nodiscard]] int backendIndex() const noexcept;
    void setBackendIndex(int index);

    [[nodiscard]] int deviceIndex() const noexcept;
    void setDeviceIndex(int index);

    [[nodiscard]] bool isLoopPlayback() const noexcept;
    void setLoopPlayback(bool loop);

    [[nodiscard]] QString statusMessage() const;

    [[nodiscard]] QStringList availableBackends() const;
    [[nodiscard]] QVariantList availableDevices() const;

    [[nodiscard]] double positionSeconds() const noexcept;
    [[nodiscard]] double durationSeconds() const noexcept;
    [[nodiscard]] bool isSeekable() const noexcept;

    [[nodiscard]] double fps() const noexcept;
    [[nodiscard]] double avgDecodeTimeMs() const noexcept;
    [[nodiscard]] int frameWidth() const noexcept;
    [[nodiscard]] int frameHeight() const noexcept;
    [[nodiscard]] QString codecName() const;
    [[nodiscard]] QString pixelFormat() const;
    [[nodiscard]] qint64 totalFrames() const noexcept;
    [[nodiscard]] qint64 droppedFrames() const noexcept;
    [[nodiscard]] double bitrateKbps() const noexcept;

    // Filter getters & setters
    [[nodiscard]] bool dehazeEnabled() const noexcept;
    void setDehazeEnabled(bool v);
    [[nodiscard]] bool stabilizationEnabled() const noexcept;
    void setStabilizationEnabled(bool v);
    [[nodiscard]] bool claheEnabled() const noexcept;
    void setClaheEnabled(bool v);
    [[nodiscard]] bool denoiseEnabled() const noexcept;
    void setDenoiseEnabled(bool v);
    [[nodiscard]] bool sharpenEnabled() const noexcept;
    void setSharpenEnabled(bool v);
    [[nodiscard]] bool edgeDetectEnabled() const noexcept;
    void setEdgeDetectEnabled(bool v);
    [[nodiscard]] int whiteBalanceMode() const noexcept;
    void setWhiteBalanceMode(int v);

    [[nodiscard]] int falseColorPalette() const noexcept;
    void setFalseColorPalette(int v);
    [[nodiscard]] int isothermPreset() const noexcept;
    void setIsothermPreset(int v);
    [[nodiscard]] bool hotspotTrackerEnabled() const noexcept;
    void setHotspotTrackerEnabled(bool v);

    [[nodiscard]] bool mtiMotionEnabled() const noexcept;
    void setMtiMotionEnabled(bool v);
    [[nodiscard]] int opticalFlowMode() const noexcept;
    void setOpticalFlowMode(int v);
    [[nodiscard]] bool heatmapEnabled() const noexcept;
    void setHeatmapEnabled(bool v);

    [[nodiscard]] int reticleStyle() const noexcept;
    void setReticleStyle(int v);
    [[nodiscard]] bool tripwireEnabled() const noexcept;
    void setTripwireEnabled(bool v);
    [[nodiscard]] int privacyMaskMode() const noexcept;
    void setPrivacyMaskMode(int v);
    [[nodiscard]] bool watermarkEnabled() const noexcept;
    void setWatermarkEnabled(bool v);
    [[nodiscard]] bool telemetryOsdEnabled() const noexcept;
    void setTelemetryOsdEnabled(bool v);

public slots:
    /// @brief Starts video playback using configured source, backend, and hardware device.
    void startPlayback();

    /// @brief Stops playback and shuts down decoding engine.
    void stopPlayback();

    /// @brief Pauses active playback.
    void pausePlayback();

    /// @brief Resumes paused playback.
    void resumePlayback();

    /// @brief Seeks to target timestamp in seconds (files only).
    /// @param[in] seconds Target presentation timestamp.
    void seek(double seconds);

    /// @brief Re-enumerates connected V4L2/USB video capture devices.
    void refreshDevices();

    /// @brief Connects video output to a QQuickItem surface.
    /// @param[in] item Target VideoQuickItem pointer.
    void attachVideoItem(VideoApp::VideoQuickItem* item);

    /// @brief Saves the current video frame snapshot to image file.
    /// @param[in] filePath Target path (or empty for auto-generated in Pictures).
    /// @return Saved image filepath, or empty string on failure.
    QString takeSnapshot(const QString& filePath = QString());

signals:
    void playbackStateChanged();
    void sourceUriChanged();
    void backendIndexChanged();
    void deviceIndexChanged();
    void loopPlaybackChanged();
    void statusMessageChanged();
    void availableDevicesChanged();
    void positionSecondsChanged();
    void durationSecondsChanged();
    void isSeekableChanged();
    void statsUpdated();
    void filterConfigChanged();
    void frameDecoded(const QImage& frame);

private:
    void workerLoop();
    void configureFilterPipeline(Video::IVideoDecoder* decoder);
    void updateState(PlaybackState state, const QString& message = QString());

    PlaybackState m_state { PlaybackState::Idle };
    QString m_sourceUri { "mock://test" };
    int m_backendIndex { 0 }; // 0: FFmpeg, 1: GStreamer, 2: Mock
    int m_deviceIndex { 0 }; // 0: CPU, 1: CUDA, 2: VAAPI
    bool m_isLoopPlayback { true };
    QString m_statusMessage { tr("Ready") };

    QVariantList m_devicesList {};

    double m_positionSeconds { 0.0 };
    double m_durationSeconds { 0.0 };
    bool m_isSeekable { false };

    double m_fps { 0.0 };
    double m_avgDecodeTimeMs { 0.0 };
    int m_frameWidth { 0 };
    int m_frameHeight { 0 };
    QString m_codecName { "-" };
    QString m_pixelFormat { "-" };
    qint64 m_totalFrames { 0 };
    qint64 m_droppedFrames { 0 };
    double m_bitrateKbps { 0.0 };

    // Filter configuration states
    bool m_dehaze { false };
    bool m_stabilization { false };
    bool m_clahe { false };
    bool m_denoise { false };
    bool m_sharpen { false };
    bool m_edgeDetect { false };
    int m_whiteBalance { 0 }; // 0: None, 1: GrayWorld, 2: WhitePatch

    int m_falseColor { 0 }; // 0: None, 1: WhiteHot, 2: BlackHot, 3: Iron256, 4: Jet, 5: Turbo, etc.
    int m_isotherm { 0 }; // 0: None, 1: HumanBody, 2: HighHeat, 3: Custom
    bool m_hotspotTracker { false };

    bool m_mtiMotion { false };
    int m_opticalFlow { 0 }; // 0: None, 1: VectorArrows, 2: ColorFlow
    bool m_heatmap { false };

    int m_reticleStyle { 0 }; // 0: None, 1: Crosshair, 2: MilDot, 3: Stadiametric, 4: CornerBrackets
    bool m_tripwire { false };
    int m_privacyMode { 0 }; // 0: None, 1: Blackout, 2: Blur, 3: Mosaic
    bool m_watermark { false };
    bool m_telemetryOsd { false };

    std::atomic<bool> m_running { false };
    std::atomic<bool> m_paused { false };
    std::atomic<bool> m_filtersDirty { false };
    std::atomic<double> m_seekRequested { -1.0 };

    std::unique_ptr<QThread> m_thread { nullptr };
    mutable QMutex m_decoderMutex {};
    std::unique_ptr<Video::IVideoDecoder> m_decoder { nullptr };

    mutable QMutex m_snapshotMutex {};
    QImage m_lastFrameCopy {};
};

} // namespace VideoApp
