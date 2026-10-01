/// @file SightlineVideoController.cpp
/// @brief Implementation of master video controller for Sightline SLA camera feeds.

#include "SightlineVideoController.h"

#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <algorithm>
#include <chrono>

namespace SightlineApp {

SightlineVideoController::SightlineVideoController(QObject* parent)
    : QObject(parent)
{
    // Auto-start initial video feed (defaulting to synthetic simulator)
    startStream();
}

SightlineVideoController::~SightlineVideoController()
{
    stopStream();
}

SightlineVideoController::PlaybackState SightlineVideoController::playbackState() const noexcept
{
    return m_state;
}

QString SightlineVideoController::sourceUri() const
{
    return m_sourceUri;
}

void SightlineVideoController::setSourceUri(const QString& uri)
{
    const QString trimmed { uri.trimmed() };
    if (m_sourceUri != trimmed) {
        m_sourceUri = trimmed;
        emit sourceUriChanged();
        if (m_running.load() && !m_isSynthetic) {
            restartStream();
        }
    }
}

int SightlineVideoController::activeCamera() const noexcept
{
    return m_activeCamera;
}

bool SightlineVideoController::isSynthetic() const noexcept
{
    return m_isSynthetic;
}

double SightlineVideoController::fps() const noexcept
{
    return m_fps;
}

double SightlineVideoController::avgDecodeTimeMs() const noexcept
{
    return m_avgDecodeTimeMs;
}

int SightlineVideoController::frameWidth() const noexcept
{
    return m_frameWidth;
}

int SightlineVideoController::frameHeight() const noexcept
{
    return m_frameHeight;
}

double SightlineVideoController::bitrateKbps() const noexcept
{
    return m_bitrateKbps;
}

QString SightlineVideoController::statusMessage() const
{
    return m_statusMessage;
}

QStringList SightlineVideoController::availableBackends() const
{
    QStringList list {};
    const auto backends { Video::DecoderFactory::availableBackends() };
    for (const auto b : backends) {
        switch (b) {
        case Video::BackendType::FFmpeg:
            list.append(QStringLiteral("FFmpeg"));
            break;
        case Video::BackendType::GStreamer:
            list.append(QStringLiteral("GStreamer"));
            break;
        case Video::BackendType::Mock:
            list.append(QStringLiteral("Mock/Synthetic"));
            break;
        }
    }
    return list;
}

int SightlineVideoController::backendIndex() const noexcept
{
    return m_backendIndex;
}

void SightlineVideoController::setBackendIndex(int index)
{
    if (m_backendIndex != index) {
        m_backendIndex = index;
        emit backendIndexChanged();
        if (m_running.load() && !m_isSynthetic) {
            restartStream();
        }
    }
}

void SightlineVideoController::selectCamera(int camIndex)
{
    const int clamped { (camIndex == 1) ? 1 : 0 };
    if (m_activeCamera != clamped) {
        m_activeCamera = clamped;
        emit activeCameraChanged();
        emit pipCameraChanged();
        if (!m_sourceUri.isEmpty() && m_sourceUri.contains(QStringLiteral(":554/net"))) {
            m_sourceUri = QStringLiteral("rtsp://%1:554/net%2").arg(m_hostAddress).arg(m_activeCamera);
            emit sourceUriChanged();
        }
        if (m_running.load() && !m_isSynthetic) {
            restartStream();
        }
    }
}

bool SightlineVideoController::pipEnabled() const noexcept
{
    return m_pipEnabled;
}

int SightlineVideoController::pipCamera() const noexcept
{
    return (m_activeCamera == 0) ? 1 : 0;
}

void SightlineVideoController::setPipEnabled(bool enabled)
{
    if (m_pipEnabled != enabled) {
        m_pipEnabled = enabled;
        emit pipEnabledChanged();
    }
}

void SightlineVideoController::swapPipFeeds()
{
    selectCamera((m_activeCamera == 0) ? 1 : 0);
}

void SightlineVideoController::setSyntheticMode(bool enabled)
{
    if (m_isSynthetic != enabled) {
        m_isSynthetic = enabled;
        emit syntheticChanged();
        restartStream();
    }
}

void SightlineVideoController::updateHostAddress(const QString& host)
{
    const QString trimmed { host.trimmed() };
    if (!trimmed.isEmpty() && m_hostAddress != trimmed) {
        const QString oldHost { m_hostAddress };
        m_hostAddress = trimmed;
        if (!m_sourceUri.isEmpty() && m_sourceUri.contains(oldHost)) {
            m_sourceUri.replace(oldHost, m_hostAddress);
            emit sourceUriChanged();
        } else if (m_sourceUri.contains(QStringLiteral("rtsp://127.0.0.1"))) {
            m_sourceUri.replace(QStringLiteral("127.0.0.1"), m_hostAddress);
            emit sourceUriChanged();
        }
        if (!m_isSynthetic && m_running.load()) {
            restartStream();
        }
    }
}

void SightlineVideoController::attachVideoItem(VideoQuickItem* item)
{
    if (item != nullptr) {
        connect(
            this, &SightlineVideoController::frameDecoded, item, &VideoQuickItem::updateFrame, Qt::QueuedConnection);
    }
}

void SightlineVideoController::attachPipVideoItem(VideoQuickItem* item)
{
    if (item != nullptr) {
        connect(
            this, &SightlineVideoController::pipFrameDecoded, item, &VideoQuickItem::updateFrame, Qt::QueuedConnection);
    }
}

QString SightlineVideoController::resolveSourceUri() const
{
    if (m_isSynthetic) {
        return QStringLiteral("mock://synthetic");
    }
    if (!m_sourceUri.trimmed().isEmpty()) {
        QString uri { m_sourceUri.trimmed() };
        if (uri.contains(QStringLiteral("rtsp://127.0.0.1")) && m_hostAddress != QStringLiteral("127.0.0.1")) {
            uri.replace(QStringLiteral("127.0.0.1"), m_hostAddress);
        }
        return uri;
    }
    return QStringLiteral("rtsp://%1:554/net%2").arg(m_hostAddress).arg(m_activeCamera);
}

void SightlineVideoController::startStream()
{
    stopStream();

    updateState(PlaybackState::Opening, tr("Connecting to sensor stream..."));

    m_running.store(true);
    m_paused.store(false);

    m_thread = std::unique_ptr<QThread>(QThread::create([this]() { workerLoop(); }));
    m_thread->start();
}

void SightlineVideoController::stopStream()
{
    m_running.store(false);
    m_paused.store(false);

    if (m_thread && m_thread->isRunning()) {
        m_thread->wait(1500);
        if (m_thread->isRunning()) {
            m_thread->terminate();
            m_thread->wait(500);
        }
    }
    m_thread.reset();

    {
        QMutexLocker locker(&m_decoderMutex);
        m_decoder.reset();
    }

    m_fps = 0.0;
    m_avgDecodeTimeMs = 0.0;
    m_bitrateKbps = 0.0;
    emit statsUpdated();

    updateState(PlaybackState::Idle, tr("Stream Disconnected"));
}

void SightlineVideoController::restartStream()
{
    startStream();
}

QString SightlineVideoController::takeSnapshot(const QString& filePath)
{
    QImage snap {};
    {
        QMutexLocker locker(&m_snapshotMutex);
        if (m_lastFrameCopy.isNull()) {
            return QString {};
        }
        snap = m_lastFrameCopy.copy();
    }

    QString targetPath { filePath };
    if (targetPath.isEmpty()) {
        const QString picturesDir { QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) };
        const QString timestamp { QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")) };
        targetPath = QStringLiteral("%1/Sightline_Cam%2_%3.png").arg(picturesDir).arg(m_activeCamera).arg(timestamp);
    }

    if (snap.save(targetPath, "PNG")) {
        emit snapshotTaken(targetPath);
        return targetPath;
    }
    return QString {};
}

void SightlineVideoController::updateState(PlaybackState state, const QString& msg)
{
    m_state = state;
    m_statusMessage = msg;
    emit playbackStateChanged();
    emit statusMessageChanged();
}

void SightlineVideoController::applyThermalLook(QImage& image)
{
    if (image.format() != QImage::Format_RGB888) {
        return;
    }

    const int height { image.height() };
    const int width { image.width() };

    for (int y = 0; y < height; ++y) {
        auto* scanLine { image.scanLine(y) };
        for (int x = 0; x < width; ++x) {
            const int offset { x * 3 };
            const int r { scanLine[offset] };
            const int g { scanLine[offset + 1] };
            const int b { scanLine[offset + 2] };

            // Luminance weighting
            const int lum { (r * 77 + g * 150 + b * 29) >> 8 };

            // FLIR White-Hot palette with subtle tactical gradient
            scanLine[offset] = static_cast<uchar>(std::min(255, lum + 8));
            scanLine[offset + 1] = static_cast<uchar>(lum);
            scanLine[offset + 2] = static_cast<uchar>(std::max(0, lum - 12));
        }
    }
}

void SightlineVideoController::workerLoop()
{
    Video::BackendType backend { Video::BackendType::Mock };
    if (!m_isSynthetic) {
        const auto available { Video::DecoderFactory::availableBackends() };
        if (m_backendIndex >= 0 && m_backendIndex < static_cast<int>(available.size())) {
            backend = available[static_cast<std::size_t>(m_backendIndex)];
        } else if (!available.empty()) {
            backend = available.front();
        }
    }

    auto dec { Video::DecoderFactory::create(backend) };
    if (!dec) {
        updateState(PlaybackState::Error, tr("Requested decoder backend is unavailable"));
        m_running.store(false);
        return;
    }

    const QString effectiveUri { resolveSourceUri() };

    if (!dec->initialize(effectiveUri.toStdString(), Video::PixelFormat::RGB24, 0, Video::DeviceType::CPU)) {
        if (!m_isSynthetic) {
            updateState(PlaybackState::Error, tr("Cannot connect to %1").arg(effectiveUri));
            m_running.store(false);
            return;
        }
    }

    {
        QMutexLocker locker(&m_decoderMutex);
        m_decoder = std::move(dec);
    }

    const auto meta { m_decoder->getVideoMetadata() };
    m_frameWidth = meta.width > 0 ? meta.width : 1920;
    m_frameHeight = meta.height > 0 ? meta.height : 1080;

    const QString stateMsg { m_isSynthetic ? tr("Synthetic Pattern Active") : tr("Live Stream Connected") };
    updateState(PlaybackState::Playing, stateMsg);

    using clock = std::chrono::steady_clock;
    auto lastStatsTime { clock::now() };
    int framesCount { 0 };
    double decodeAccumMs { 0.0 };

    while (m_running.load()) {
        if (m_paused.load()) {
            QThread::msleep(20);
            continue;
        }

        const auto startDecode { clock::now() };
        bool success { false };
        Video::FrameInfo frameInfo {};

        {
            QMutexLocker locker(&m_decoderMutex);
            if (m_decoder) {
                success = m_decoder->decodeNextFrame();
                if (success) {
                    frameInfo = m_decoder->getRawFrameData();
                }
            }
        }

        const auto endDecode { clock::now() };
        const double decodeMs { std::chrono::duration<double, std::milli>(endDecode - startDecode).count() };
        decodeAccumMs += decodeMs;

        if (!success) {
            // Live stream packet starvation or disconnect -> sleep and retry
            QThread::msleep(20);
            continue;
        }

        if (frameInfo.data != nullptr && frameInfo.width > 0 && frameInfo.height > 0) {
            const QImage rawImg(
                frameInfo.data, frameInfo.width, frameInfo.height, frameInfo.width * 3, QImage::Format_RGB888);

            QImage frameCopy { rawImg.copy() };

            if (m_isSynthetic && m_activeCamera == 1) {
                applyThermalLook(frameCopy);
            }

            {
                QMutexLocker locker(&m_snapshotMutex);
                m_lastFrameCopy = frameCopy;
            }

            emit frameDecoded(frameCopy);

            // Picture-in-Picture secondary frame emission
            if (m_pipEnabled) {
                QImage pipCopy { rawImg.copy() };
                const int pipCam { pipCamera() };
                if (m_isSynthetic && pipCam == 1) {
                    applyThermalLook(pipCopy);
                }
                emit pipFrameDecoded(pipCopy);
            }

            ++framesCount;

            m_frameWidth = frameInfo.width;
            m_frameHeight = frameInfo.height;
        }

        // Frame rate pacing for synthetic generation (~30 FPS -> 33.3ms)
        if (m_isSynthetic) {
            const double sleepTargetMs { 33.3 - decodeMs };
            if (sleepTargetMs > 1.0) {
                QThread::msleep(static_cast<unsigned long>(sleepTargetMs));
            }
        }

        // Periodic statistical sampling (every 500ms)
        const auto now { clock::now() };
        const auto elapsedMs { std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStatsTime).count() };

        if (elapsedMs >= 500) {
            m_fps = (framesCount * 1000.0) / static_cast<double>(elapsedMs);
            m_avgDecodeTimeMs = framesCount > 0 ? (decodeAccumMs / framesCount) : 0.0;
            // Approximate uncompressed RGB bitrate estimate
            m_bitrateKbps = (m_fps * m_frameWidth * m_frameHeight * 3 * 8) / 1000.0;

            emit statsUpdated();

            framesCount = 0;
            decodeAccumMs = 0.0;
            lastStatsTime = now;
        }
    }
}

} // namespace SightlineApp
