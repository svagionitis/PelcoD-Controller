#include "QVideoStreamWorker.h"
#include "DecoderFactory.h"

#include <QElapsedTimer>
#include <glog/logging.h>

namespace PelcoDQt {

using Video::BackendType;
using Video::DecoderFactory;
using Video::DecoderPerformanceStats;
using Video::DeviceType;
using Video::FrameInfo;
using Video::PixelFormat;
using Video::StreamState;
using Video::VideoMetadata;

QVideoStreamWorker::QVideoStreamWorker(QObject* parent)
    : QThread(parent)
{
}

QVideoStreamWorker::~QVideoStreamWorker()
{
    stopPlayback();
}

void QVideoStreamWorker::openStream(const QString& source, BackendType backend, DeviceType device)
{
    stopPlayback();

    QMutexLocker locker(&m_mutex);
    m_source = source;
    m_backend = backend;
    m_device = device;
    m_stopRequested = false;
    m_pauseRequested = false;

    start();
}

void QVideoStreamWorker::stopPlayback()
{
    {
        QMutexLocker locker(&m_mutex);
        m_stopRequested = true;
        m_pauseRequested = false;
        m_condition.wakeAll();
    }

    if (isRunning()) {
        wait();
    }

    QMutexLocker locker(&m_mutex);
    m_state = StreamState::Disconnected;
}

void QVideoStreamWorker::pausePlayback()
{
    QMutexLocker locker(&m_mutex);
    m_pauseRequested = true;
    m_state = StreamState::Paused;
    emit streamStatusChanged(m_state, tr("Stream paused"));
}

void QVideoStreamWorker::resumePlayback()
{
    QMutexLocker locker(&m_mutex);
    m_pauseRequested = false;
    m_state = StreamState::Streaming;
    m_condition.wakeAll();
    emit streamStatusChanged(m_state, tr("Stream resumed"));
}

bool QVideoStreamWorker::isStreaming() const
{
    QMutexLocker locker(&m_mutex);
    return (m_state == StreamState::Streaming);
}

StreamState QVideoStreamWorker::streamState() const
{
    QMutexLocker locker(&m_mutex);
    return m_state;
}

void QVideoStreamWorker::setLoopPlayback(bool loop)
{
    QMutexLocker locker(&m_mutex);
    m_loopPlayback = loop;
}

bool QVideoStreamWorker::isLoopPlayback() const
{
    QMutexLocker locker(&m_mutex);
    return m_loopPlayback;
}

void QVideoStreamWorker::seekTo(double timestampSeconds)
{
    QMutexLocker locker(&m_mutex);
    m_requestedSeekPos = timestampSeconds;
    m_condition.wakeAll();
}

void QVideoStreamWorker::addFrameProcessor(std::shared_ptr<Video::IFrameProcessor> processor)
{
    QMutexLocker locker(&m_mutex);
    if (processor) {
        m_processors.push_back(processor);
        if (m_decoder) {
            m_decoder->addFrameProcessor(processor);
        }
    }
}

void QVideoStreamWorker::clearFrameProcessors()
{
    QMutexLocker locker(&m_mutex);
    m_processors.clear();
    if (m_decoder) {
        m_decoder->clearFrameProcessors();
    }
}

void QVideoStreamWorker::run()
{
    QString currentSource;
    BackendType currentBackend;
    DeviceType currentDevice;

    {
        QMutexLocker locker(&m_mutex);
        currentSource = m_source;
        currentBackend = m_backend;
        currentDevice = m_device;
        m_state = StreamState::Connecting;
    }

    emit streamStatusChanged(StreamState::Connecting, tr("Connecting to %1...").arg(currentSource));

    m_decoder = DecoderFactory::create(currentBackend);
    if (!m_decoder) {
        {
            QMutexLocker locker(&m_mutex);
            m_state = StreamState::Error;
        }
        emit streamStatusChanged(StreamState::Error, tr("Requested decoder backend is not available"));
        return;
    }

    const std::string sourceStr = currentSource.toStdString();
    if (!m_decoder->initialize(sourceStr, PixelFormat::RGB24, 0, currentDevice)) {
        {
            QMutexLocker locker(&m_mutex);
            m_state = StreamState::Error;
        }
        emit streamStatusChanged(StreamState::Error, tr("Failed to initialize video stream: %1").arg(currentSource));
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        for (const auto& proc : m_processors) {
            m_decoder->addFrameProcessor(proc);
        }
    }

    const VideoMetadata meta = m_decoder->getVideoMetadata();
    {
        QMutexLocker locker(&m_mutex);
        m_state = StreamState::Streaming;
    }

    emit streamStatusChanged(StreamState::Streaming,
        tr("Connected. Streaming %1x%2 @ %3 FPS").arg(meta.width).arg(meta.height).arg(meta.frameRate, 0, 'f', 1));
    emit streamMetadataReady(meta.width, meta.height, meta.frameRate, QString::fromStdString(meta.codecName));

    QElapsedTimer fpsTimer;
    fpsTimer.start();
    int frameCount = 0;
    const double targetFrameIntervalMs = (meta.frameRate > 0.0) ? (1000.0 / meta.frameRate) : 33.3;

    QElapsedTimer framePacer;
    framePacer.start();

    while (true) {
        double seekTarget = -1.0;
        bool shouldLoop = true;

        {
            QMutexLocker locker(&m_mutex);
            if (m_stopRequested) {
                break;
            }
            while (m_pauseRequested && !m_stopRequested) {
                m_condition.wait(&m_mutex);
            }
            if (m_stopRequested) {
                break;
            }
            if (m_requestedSeekPos >= 0.0) {
                seekTarget = m_requestedSeekPos;
                m_requestedSeekPos = -1.0;
            }
            shouldLoop = m_loopPlayback;
        }

        if (seekTarget >= 0.0 && m_decoder) {
            m_decoder->seek(seekTarget);
        }

        const bool ok = m_decoder->decodeNextFrame();
        if (!ok) {
            if (shouldLoop && (meta.duration > 0.0 || currentBackend == BackendType::Mock)) {
                // Loop finite video back to start
                m_decoder->seek(0.0);
                continue;
            }

            QMutexLocker locker(&m_mutex);
            if (m_stopRequested) {
                break;
            }
            m_state = StreamState::Disconnected;
            emit streamStatusChanged(StreamState::Disconnected, tr("Stream disconnected or reached EOF"));
            break;
        }

        const FrameInfo frame = m_decoder->getRawFrameData();
        if (frame.data != nullptr && frame.width > 0 && frame.height > 0) {
            // Construct QImage with deep copy to safely transfer across Qt thread boundaries
            const QImage img(frame.data, frame.width, frame.height, frame.width * 3, QImage::Format_RGB888);
            emit frameReady(img.copy(), frame.timestamp, frame.decodeTimeMs);

            if (meta.duration > 0.0) {
                emit playbackPositionChanged(frame.timestamp, meta.duration);
            }

            ++frameCount;
            const qint64 elapsedMs = fpsTimer.elapsed();
            if (elapsedMs >= 1000) {
                const double currentFps = (static_cast<double>(frameCount) * 1000.0) / static_cast<double>(elapsedMs);
                const DecoderPerformanceStats stats = m_decoder->getPerformanceStats();
                emit statsUpdated(currentFps, stats.averageDecodeTimeMs);
                fpsTimer.restart();
                frameCount = 0;
            }
        }

        // Rate pacing for file streams and synthetic generators
        const qint64 frameElapsedMs = framePacer.elapsed();
        if (meta.duration > 0.0 || currentBackend == BackendType::Mock) {
            if (frameElapsedMs < static_cast<qint64>(targetFrameIntervalMs)) {
                msleep(static_cast<unsigned long>(static_cast<qint64>(targetFrameIntervalMs) - frameElapsedMs));
            }
        } else {
            // For live RTSP streams, yield briefly if processing is instantaneous
            if (frameElapsedMs < 2) {
                msleep(1);
            }
        }
        framePacer.restart();
    }

    if (m_decoder) {
        m_decoder->close();
        m_decoder.reset();
    }

    {
        QMutexLocker locker(&m_mutex);
        m_state = StreamState::Disconnected;
    }
    emit streamStatusChanged(StreamState::Disconnected, tr("Stream closed"));
}

} // namespace PelcoDQt
