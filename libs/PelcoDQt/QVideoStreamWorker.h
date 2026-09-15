#pragma once

/// @file QVideoStreamWorker.h
/// @brief Asynchronous Qt worker thread driving video decoder backends.

#include "DecoderTypes.h"
#include "IVideoDecoder.h"

#include <QImage>
#include <QMutex>
#include <QString>
#include <QThread>
#include <QWaitCondition>
#include <memory>

namespace PelcoDQt {

/// @class QVideoStreamWorker
/// @brief Dedicated background thread running the IVideoDecoder decoding loop.
class QVideoStreamWorker : public QThread {
    Q_OBJECT

public:
    /// @brief Constructor.
    /// @param[in] parent Optional QObject parent.
    explicit QVideoStreamWorker(QObject* parent = nullptr);

    /// @brief Destructor. Safely interrupts and joins thread execution.
    ~QVideoStreamWorker() override;

    /// @brief Opens and starts playback of a video stream or file.
    /// @param[in] source URL (RTSP, file, or mock pattern).
    /// @param[in] backend Requested decoding backend.
    /// @param[in] device Hardware acceleration device.
    void openStream(const QString& source,
                    PelcoD::Video::BackendType backend = PelcoD::Video::BackendType::FFmpeg,
                    PelcoD::Video::DeviceType device = PelcoD::Video::DeviceType::CPU);

    /// @brief Stops decoding and terminates worker loop.
    void stopPlayback();

    /// @brief Pauses decoding loop.
    void pausePlayback();

    /// @brief Resumes paused decoding loop.
    void resumePlayback();

    /// @brief Checks whether the worker is currently streaming.
    /// @return True if streaming.
    [[nodiscard]] bool isStreaming() const;

    /// @brief Gets active stream state.
    /// @return Current StreamState.
    [[nodiscard]] PelcoD::Video::StreamState streamState() const;

signals:
    /// @brief Emitted when a video frame is decoded and ready for display.
    /// @param frame Decoded QImage in Format_RGB888.
    /// @param pts Presentation timestamp in seconds.
    /// @param decodeLatencyMs Frame decode duration in milliseconds.
    void frameReady(const QImage& frame, double pts, double decodeLatencyMs);

    /// @brief Emitted when the stream connection lifecycle state changes.
    /// @param state New stream state.
    /// @param message Descriptive status message.
    void streamStatusChanged(PelcoD::Video::StreamState state, const QString& message);

    /// @brief Emitted once stream properties are parsed.
    /// @param width Frame width in pixels.
    /// @param height Frame height in pixels.
    /// @param fps Video framerate.
    /// @param codec Codec description.
    void streamMetadataReady(int width, int height, double fps, const QString& codec);

    /// @brief Periodic telemetry update with streaming metrics.
    /// @param fps Measured render/decode frames per second.
    /// @param avgDecodeMs Rolling average decode time in milliseconds.
    void statsUpdated(double fps, double avgDecodeMs);

protected:
    void run() override;

private:
    mutable QMutex m_mutex;
    QWaitCondition m_condition;

    QString m_source;
    PelcoD::Video::BackendType m_backend { PelcoD::Video::BackendType::FFmpeg };
    PelcoD::Video::DeviceType m_device { PelcoD::Video::DeviceType::CPU };

    std::unique_ptr<PelcoD::Video::IVideoDecoder> m_decoder;

    PelcoD::Video::StreamState m_state { PelcoD::Video::StreamState::Disconnected };
    bool m_stopRequested { false };
    bool m_pauseRequested { false };
};

} // namespace PelcoDQt

namespace PelcoD::Video {
using QVideoStreamWorker = PelcoDQt::QVideoStreamWorker;
}
