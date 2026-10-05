/// @file SightlineVideoController.cpp
/// @brief Implementation of master video controller for Sightline SLA camera feeds.

#include "SightlineVideoController.h"

#include <Transport/SdpGenerator.h>

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>
#include <chrono>

namespace SightlineApp {

SightlineVideoController::SightlineVideoController(QObject* parent)
    : QObject(parent)
{
    generatePaletteTables();
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
    return m_sourceUri.isEmpty() ? resolveSourceUri() : m_sourceUri;
}

SightlineVideoController::NetworkChannel SightlineVideoController::networkChannel() const noexcept
{
    return m_networkChannel;
}

int SightlineVideoController::activeNetworkChannel() const noexcept
{
    return static_cast<int>(m_networkChannel);
}

void SightlineVideoController::setSourceUri(const QString& uri)
{
    const QString trimmed { uri.trimmed() };
    if (m_sourceUri != trimmed) {
        m_sourceUri = trimmed;
        if (m_sourceUri.endsWith(QStringLiteral("/net0"))) {
            m_networkChannel = NetworkChannel::Net0;
        } else if (m_sourceUri.endsWith(QStringLiteral("/net1"))) {
            m_networkChannel = NetworkChannel::Net1;
        } else if (m_sourceUri.endsWith(QStringLiteral(":554/")) || m_sourceUri.endsWith(QStringLiteral(":554"))) {
            m_networkChannel = NetworkChannel::Legacy;
        } else if (!m_sourceUri.isEmpty()) {
            m_networkChannel = NetworkChannel::Custom;
        }
        emit networkChannelChanged();
        emit sourceUriChanged();
        if (m_running.load() && !m_isSynthetic) {
            restartStream();
        }
    }
}

void SightlineVideoController::selectNetworkChannel(NetworkChannel channel)
{
    if (m_networkChannel != channel) {
        m_networkChannel = channel;
        m_sourceUri = resolveSourceUri();
        emit networkChannelChanged();
        emit sourceUriChanged();
        if (m_running.load() && !m_isSynthetic) {
            restartStream();
        }
    }
}

void SightlineVideoController::selectNetworkChannelInt(int channel)
{
    NetworkChannel ch { NetworkChannel::Net0 };
    if (channel == 1) {
        ch = NetworkChannel::Net1;
    } else if (channel == 2) {
        ch = NetworkChannel::Legacy;
    } else if (channel == 3) {
        ch = NetworkChannel::Custom;
    }
    selectNetworkChannel(ch);
}

SightlineVideoController::RtspTransport SightlineVideoController::transportMode() const noexcept
{
    return m_transportMode;
}

int SightlineVideoController::activeTransportMode() const noexcept
{
    return static_cast<int>(m_transportMode);
}

void SightlineVideoController::selectTransportMode(RtspTransport mode)
{
    if (m_transportMode != mode) {
        m_transportMode = mode;
        emit transportModeChanged();
        if (m_running.load() && !m_isSynthetic) {
            restartStream();
        }
    }
}

void SightlineVideoController::selectTransportModeInt(int mode)
{
    RtspTransport transport { RtspTransport::Auto };
    if (mode == 1) {
        transport = RtspTransport::Tcp;
    } else if (mode == 2) {
        transport = RtspTransport::Udp;
    } else if (mode == 3) {
        transport = RtspTransport::Multicast;
    }
    selectTransportMode(transport);
}

QString SightlineVideoController::rtspUsername() const
{
    return m_rtspUsername;
}

QString SightlineVideoController::rtspPassword() const
{
    return m_rtspPassword;
}

bool SightlineVideoController::authEnabled() const noexcept
{
    return m_authEnabled;
}

QString SightlineVideoController::sanitizedSourceUri() const
{
    const QString uri { sourceUri() };
    const qsizetype schemeIdx { uri.indexOf(QStringLiteral("://")) };
    if (schemeIdx < 0) {
        return uri;
    }
    const qsizetype atIdx { uri.indexOf(QLatin1Char('@'), schemeIdx + 3) };
    if (atIdx < 0) {
        return uri;
    }
    const qsizetype colonIdx { uri.indexOf(QLatin1Char(':'), schemeIdx + 3) };
    if (colonIdx >= 0 && colonIdx < atIdx) {
        return uri.left(colonIdx + 1) + QStringLiteral("***") + uri.mid(atIdx);
    }
    return uri;
}

void SightlineVideoController::setRtspUsername(const QString& user)
{
    if (m_rtspUsername != user) {
        m_rtspUsername = user;
        emit rtspAuthChanged();
    }
}

void SightlineVideoController::setRtspPassword(const QString& pass)
{
    if (m_rtspPassword != pass) {
        m_rtspPassword = pass;
        emit rtspAuthChanged();
    }
}

void SightlineVideoController::setAuthEnabled(bool enabled)
{
    if (m_authEnabled != enabled) {
        m_authEnabled = enabled;
        emit rtspAuthChanged();
        if (m_running.load() && !m_isSynthetic) {
            restartStream();
        }
    }
}

void SightlineVideoController::setRtspCredentials(const QString& user, const QString& pass)
{
    m_rtspUsername = user;
    m_rtspPassword = pass;
    m_authEnabled = !user.isEmpty();
    emit rtspAuthChanged();
    if (m_running.load() && !m_isSynthetic) {
        restartStream();
    }
}

void SightlineVideoController::clearRtspCredentials()
{
    m_rtspUsername.clear();
    m_rtspPassword.clear();
    m_authEnabled = false;
    emit rtspAuthChanged();
    if (m_running.load() && !m_isSynthetic) {
        restartStream();
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
    const int clamped { (camIndex >= 0 && camIndex <= 3) ? camIndex : 0 };
    if (m_activeCamera != clamped) {
        m_activeCamera = clamped;
        emit activeCameraChanged();
        emit pipCameraChanged();
        // Note: Per Sightline EAN-RTSP, physical camera selection is decoupled
        // from the logical network transmission channel (net0 / net1).
        // Camera switching updates local enhancements, telemetry, and PIP without
        // mutating the active RTSP stream mount point.
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

int SightlineVideoController::contrastMode() const noexcept
{
    QMutexLocker locker(&m_enhancementMutex);
    return m_enhancementConfigs[static_cast<std::size_t>(m_activeCamera)].mode;
}

int SightlineVideoController::activePalette() const noexcept
{
    QMutexLocker locker(&m_enhancementMutex);
    return m_enhancementConfigs[static_cast<std::size_t>(m_activeCamera)].paletteIndex;
}

QRect SightlineVideoController::enhancementRoi() const
{
    QMutexLocker locker(&m_enhancementMutex);
    return m_enhancementConfigs[static_cast<std::size_t>(m_activeCamera)].roi;
}

bool SightlineVideoController::isPaused() const noexcept
{
    return m_paused.load();
}

quint64 SightlineVideoController::currentFramePts() const noexcept
{
    QMutexLocker locker(&m_bufferMutex);
    return m_currentFramePts;
}

quint64 SightlineVideoController::currentPts() const noexcept
{
    return currentFramePts();
}

int SightlineVideoController::bufferedFrameCount() const noexcept
{
    QMutexLocker locker(&m_bufferMutex);
    return static_cast<int>(m_frameRingBuffer.size());
}

int SightlineVideoController::scrubOffset() const noexcept
{
    QMutexLocker locker(&m_bufferMutex);
    return m_scrubOffset;
}

void SightlineVideoController::pauseStream(bool pause)
{
    if (m_paused.load() != pause) {
        m_paused.store(pause);
        if (!pause) {
            QMutexLocker locker(&m_bufferMutex);
            m_scrubOffset = 0;
            if (!m_frameRingBuffer.empty()) {
                m_currentFramePts = m_frameRingBuffer.back().ptsUs;
            }
            emit scrubOffsetChanged();
            emit currentFramePtsChanged();
        }
        emit pausedChanged();
    }
}

void SightlineVideoController::togglePause()
{
    pauseStream(!m_paused.load());
}

void SightlineVideoController::setScrubOffset(int offset)
{
    QImage scrubImg {};
    quint64 scrubPts { 0U };
    {
        QMutexLocker locker(&m_bufferMutex);
        if (m_frameRingBuffer.empty()) {
            return;
        }

        const int maxBack { -static_cast<int>(m_frameRingBuffer.size()) + 1 };
        const int clamped { std::clamp(offset, maxBack, 0) };
        m_scrubOffset = clamped;

        const std::size_t idx { static_cast<std::size_t>(static_cast<int>(m_frameRingBuffer.size()) - 1 + clamped) };
        if (idx < m_frameRingBuffer.size()) {
            scrubImg = m_frameRingBuffer[idx].frame;
            scrubPts = m_frameRingBuffer[idx].ptsUs;
            m_currentFramePts = scrubPts;
        }
    }

    if (!scrubImg.isNull()) {
        {
            QMutexLocker locker(&m_snapshotMutex);
            m_lastFrameCopy = scrubImg;
        }
        emit frameDecoded(scrubImg);
        emit currentFramePtsChanged();
        emit scrubOffsetChanged();
    }
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
        if (m_networkChannel != NetworkChannel::Custom) {
            m_sourceUri = resolveSourceUri();
            emit sourceUriChanged();
        } else if (!m_sourceUri.isEmpty() && m_sourceUri.contains(oldHost)) {
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

void SightlineVideoController::updateEnhancementMode(
    int cam, int mode, int strength, int blend, int sharpen, int radius)
{
    if (cam < 0 || cam >= 4) {
        return;
    }
    {
        QMutexLocker locker(&m_enhancementMutex);
        auto& cfg { m_enhancementConfigs[static_cast<std::size_t>(cam)] };
        cfg.mode = mode;
        cfg.strength = strength;
        cfg.blend = blend;
        cfg.sharpen = sharpen;
        cfg.radius = radius;
    }
    if (cam == m_activeCamera) {
        emit contrastModeChanged();
    }
}

void SightlineVideoController::updateHistogram(
    int cam, bool featureBased, bool sqrtHist, int aveRate, int maxPct, int brightness, int contrast)
{
    Q_UNUSED(featureBased);
    Q_UNUSED(sqrtHist);
    Q_UNUSED(aveRate);
    Q_UNUSED(maxPct);
    if (cam < 0 || cam >= 4) {
        return;
    }
    {
        QMutexLocker locker(&m_enhancementMutex);
        auto& cfg { m_enhancementConfigs[static_cast<std::size_t>(cam)] };
        cfg.brightness = brightness;
        cfg.contrast = contrast;
    }
}

void SightlineVideoController::updateFalseColor(int cam, int paletteIndex)
{
    if (cam < 0 || cam >= 4) {
        return;
    }
    {
        QMutexLocker locker(&m_enhancementMutex);
        m_enhancementConfigs[static_cast<std::size_t>(cam)].paletteIndex = paletteIndex;
    }
    if (cam == m_activeCamera) {
        emit activePaletteChanged();
    }
}

void SightlineVideoController::updateUserPalette(int paletteIndex, const QByteArray& yuvData)
{
    if (paletteIndex < 0 || paletteIndex >= 4) {
        return;
    }
    QMutexLocker locker(&m_enhancementMutex);
    m_userPalettes[static_cast<std::size_t>(paletteIndex)] = yuvData;
}

void SightlineVideoController::updateEnhancementRoi(int cam, int row, int col, int height, int width)
{
    if (cam < 0 || cam >= 4) {
        return;
    }
    {
        QMutexLocker locker(&m_enhancementMutex);
        m_enhancementConfigs[static_cast<std::size_t>(cam)].roi = QRect(col, row, width, height);
    }
    if (cam == m_activeCamera) {
        emit enhancementRoiChanged();
    }
}

QString SightlineVideoController::resolveSourceUri() const
{
    if (m_isSynthetic) {
        return QStringLiteral("mock://synthetic");
    }
    if (m_networkChannel == NetworkChannel::Custom && !m_sourceUri.trimmed().isEmpty()) {
        QString uri { m_sourceUri.trimmed() };
        if (uri.contains(QStringLiteral("rtsp://127.0.0.1")) && m_hostAddress != QStringLiteral("127.0.0.1")) {
            uri.replace(QStringLiteral("127.0.0.1"), m_hostAddress);
        }
        return uri;
    }
    if (m_networkChannel == NetworkChannel::Net1) {
        return QStringLiteral("rtsp://%1:554/net1").arg(m_hostAddress);
    }
    if (m_networkChannel == NetworkChannel::Legacy) {
        return QStringLiteral("rtsp://%1:554/").arg(m_hostAddress);
    }
    return QStringLiteral("rtsp://%1:554/net0").arg(m_hostAddress);
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

    {
        QMutexLocker locker(&m_bufferMutex);
        m_frameRingBuffer.clear();
        m_scrubOffset = 0;
        m_currentFramePts = 0U;
    }
    emit bufferedFramesChanged();
    emit currentFramePtsChanged();
    emit scrubOffsetChanged();

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

bool SightlineVideoController::exportCurrentStreamSdp(const QString& destinationPath)
{
    QString filePath { destinationPath };
    if (filePath.startsWith(QStringLiteral("file://"))) {
        filePath = QUrl(filePath).toLocalFile();
    }
    if (filePath.isEmpty()) {
        return false;
    }

    Transport::SdpStreamParams params {};
    params.sessionName = "Sightline Video Stream";
    params.originAddress = "127.0.0.1";

    const QUrl uri { m_sourceUri };
    if (uri.isValid()) {
        if (!uri.host().isEmpty()) {
            params.destinationIp = uri.host().toStdString();
        }
        if (uri.port() > 0) {
            params.destinationPort = static_cast<std::uint16_t>(uri.port());
        }
    }

    if (params.destinationIp.empty()) {
        params.destinationIp = "127.0.0.1";
    }
    if (params.destinationPort == 0U) {
        params.destinationPort = 15004U;
    }

    params.isMulticast = Transport::SdpGenerator::isMulticast(params.destinationIp);
    params.ttl = 15U;
    params.clockRate = 90000U;

    if (m_sourceUri.startsWith(QStringLiteral("udp://"))) {
        params.transportProtocol = Transport::SdpProtocol::Udp;
        params.payloadType = Transport::SdpPayloadType::Mpeg2Ts;
    } else {
        params.transportProtocol = Transport::SdpProtocol::RtpAvp;
        params.payloadType = Transport::SdpPayloadType::H264;
    }

    if (m_frameWidth > 0 && m_frameHeight > 0) {
        params.frameWidth = m_frameWidth;
        params.frameHeight = m_frameHeight;
    }
    if (m_bitrateKbps > 0.0) {
        params.bitrateKbps = static_cast<int>(m_bitrateKbps);
    }

    return Transport::SdpGenerator::saveToFile(filePath.toStdString(), params);
}

void SightlineVideoController::updateState(PlaybackState state, const QString& msg)
{
    m_state = state;
    m_statusMessage = msg;
    emit playbackStateChanged();
    emit statusMessageChanged();
}

void SightlineVideoController::generatePaletteTables()
{
    // Palette 0: White-Hot (grayscale)
    for (int i = 0; i < 256; ++i) {
        m_presetPalettes[0][static_cast<std::size_t>(i)] = qRgb(i, i, i);
    }

    // Palette 1: Black-Hot (inverted grayscale)
    for (int i = 0; i < 256; ++i) {
        m_presetPalettes[1][static_cast<std::size_t>(i)] = qRgb(255 - i, 255 - i, 255 - i);
    }

    // Palette 2: Rainbow (blue -> cyan -> green -> yellow -> red)
    for (int i = 0; i < 256; ++i) {
        const int hue { static_cast<int>((255 - i) * 240.0 / 255.0) };
        m_presetPalettes[2][static_cast<std::size_t>(i)] = QColor::fromHsv(hue, 255, 255).rgb();
    }

    // Palette 3: FLIR Ironbow tactical gradient
    for (int i = 0; i < 256; ++i) {
        int r { 0 };
        int g { 0 };
        int b { 0 };
        if (i < 64) {
            const double t { i / 64.0 };
            r = static_cast<int>(t * 70.0);
            g = 0;
            b = static_cast<int>(32.0 + t * (115.0 - 32.0));
        } else if (i < 128) {
            const double t { (i - 64) / 64.0 };
            r = static_cast<int>(70.0 + t * (210.0 - 70.0));
            g = static_cast<int>(t * 30.0);
            b = static_cast<int>(115.0 + t * (10.0 - 115.0));
        } else if (i < 192) {
            const double t { (i - 128) / 64.0 };
            r = static_cast<int>(210.0 + t * (255.0 - 210.0));
            g = static_cast<int>(30.0 + t * (170.0 - 30.0));
            b = static_cast<int>(10.0 - t * 10.0);
        } else {
            const double t { (i - 192) / 63.0 };
            r = 255;
            g = static_cast<int>(170.0 + t * (255.0 - 170.0));
            b = static_cast<int>(t * 255.0);
        }
        m_presetPalettes[3][static_cast<std::size_t>(i)]
            = qRgb(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
    }
}

void SightlineVideoController::applyThermalLook(QImage& image)
{
    applyEnhancement(image, 1);
}

void SightlineVideoController::applyEnhancement(QImage& image, int cam)
{
    if (image.format() != QImage::Format_RGB888 || cam < 0 || cam >= 4) {
        return;
    }

    EnhancementConfig cfg {};
    {
        QMutexLocker locker(&m_enhancementMutex);
        cfg = m_enhancementConfigs[static_cast<std::size_t>(cam)];
    }

    QRect targetRoi { image.rect() };
    if (cfg.roi.isValid() && !cfg.roi.isEmpty()) {
        targetRoi = cfg.roi.intersected(image.rect());
    }
    if (targetRoi.isEmpty()) {
        return;
    }

    // 1. False-color thermal mapping (thermal camera default or custom palette)
    if (cam == 1 || cfg.paletteIndex > 0) {
        applyFalseColorLut(image, targetRoi, cfg.paletteIndex, cam);
    }

    // 2. Histogram Equalization (Mode 1: Hist Eq, Mode 2: CLAHE)
    if (cfg.mode == 1 || cfg.mode == 2) {
        applyHistogramEq(image, targetRoi, cfg.blend);
    }

    // 3. Brightness and Contrast
    if (cfg.brightness != 0 || cfg.contrast != 0) {
        applyContrastBrightness(image, targetRoi, cfg.brightness, cfg.contrast);
    }

    // 4. Native Sharpening
    if (cfg.sharpen > 0) {
        applyNativeSharpen(image, targetRoi, cfg.sharpen);
    }
}

void SightlineVideoController::applyFalseColorLut(QImage& image, const QRect& targetRoi, int paletteIndex, int cam)
{
    const int top { targetRoi.top() };
    const int bottom { targetRoi.bottom() };
    const int left { targetRoi.left() };
    const int right { targetRoi.right() };

    const QRgb* lut { nullptr };
    std::array<QRgb, 256> userTable {};

    if (paletteIndex >= 0 && paletteIndex < 4) {
        lut = m_presetPalettes[static_cast<std::size_t>(paletteIndex)].data();
    } else if (paletteIndex == 4) {
        QMutexLocker locker(&m_enhancementMutex);
        const auto& data { m_userPalettes[static_cast<std::size_t>(cam)] };
        if (data.size() >= 256 * 3) {
            for (int i = 0; i < 256; ++i) {
                const int y { static_cast<uchar>(data[i * 3]) };
                const int u { static_cast<uchar>(data[i * 3 + 1]) - 128 };
                const int v { static_cast<uchar>(data[i * 3 + 2]) - 128 };
                const int r { std::clamp(static_cast<int>(y + 1.402 * v), 0, 255) };
                const int g { std::clamp(static_cast<int>(y - 0.344136 * u - 0.714136 * v), 0, 255) };
                const int b { std::clamp(static_cast<int>(y + 1.772 * u), 0, 255) };
                userTable[static_cast<std::size_t>(i)] = qRgb(r, g, b);
            }
            lut = userTable.data();
        } else {
            lut = m_presetPalettes[0].data();
        }
    }

    if (lut == nullptr) {
        return;
    }

    for (int y = top; y <= bottom; ++y) {
        auto* scanLine { image.scanLine(y) };
        for (int x = left; x <= right; ++x) {
            const int offset { x * 3 };
            const int r { scanLine[offset] };
            const int g { scanLine[offset + 1] };
            const int b { scanLine[offset + 2] };
            const int lum { (r * 77 + g * 150 + b * 29) >> 8 };
            const QRgb mapped { lut[lum] };
            scanLine[offset] = static_cast<uchar>(qRed(mapped));
            scanLine[offset + 1] = static_cast<uchar>(qGreen(mapped));
            scanLine[offset + 2] = static_cast<uchar>(qBlue(mapped));
        }
    }
}

void SightlineVideoController::applyContrastBrightness(
    QImage& image, const QRect& targetRoi, int brightness, int contrast)
{
    std::array<uchar, 256> table {};
    const double factor { (259.0 * (contrast + 255.0)) / (255.0 * (259.0 - contrast)) };
    for (int i = 0; i < 256; ++i) {
        const int val { static_cast<int>(factor * (i - 128) + 128 + brightness) };
        table[static_cast<std::size_t>(i)] = static_cast<uchar>(std::clamp(val, 0, 255));
    }

    const int top { targetRoi.top() };
    const int bottom { targetRoi.bottom() };
    const int left { targetRoi.left() };
    const int right { targetRoi.right() };

    for (int y = top; y <= bottom; ++y) {
        auto* scanLine { image.scanLine(y) };
        for (int x = left; x <= right; ++x) {
            const int offset { x * 3 };
            scanLine[offset] = table[scanLine[offset]];
            scanLine[offset + 1] = table[scanLine[offset + 1]];
            scanLine[offset + 2] = table[scanLine[offset + 2]];
        }
    }
}

void SightlineVideoController::applyHistogramEq(QImage& image, const QRect& targetRoi, int blend)
{
    const int top { targetRoi.top() };
    const int bottom { targetRoi.bottom() };
    const int left { targetRoi.left() };
    const int right { targetRoi.right() };
    const int totalPixels { targetRoi.width() * targetRoi.height() };
    if (totalPixels <= 0) {
        return;
    }

    std::array<int, 256> hist {};
    for (int y = top; y <= bottom; ++y) {
        const auto* scanLine { image.constScanLine(y) };
        for (int x = left; x <= right; ++x) {
            const int offset { x * 3 };
            const int lum { (scanLine[offset] * 77 + scanLine[offset + 1] * 150 + scanLine[offset + 2] * 29) >> 8 };
            ++hist[static_cast<std::size_t>(lum)];
        }
    }

    std::array<uchar, 256> cdfMap {};
    int accum { 0 };
    for (int i = 0; i < 256; ++i) {
        accum += hist[static_cast<std::size_t>(i)];
        const int mapped { (accum * 255) / totalPixels };
        cdfMap[static_cast<std::size_t>(i)] = static_cast<uchar>(std::clamp(mapped, 0, 255));
    }

    const int bNorm { std::clamp(blend, 0, 100) };
    const int origWeight { 100 - bNorm };

    for (int y = top; y <= bottom; ++y) {
        auto* scanLine { image.scanLine(y) };
        for (int x = left; x <= right; ++x) {
            const int offset { x * 3 };
            const int r { scanLine[offset] };
            const int g { scanLine[offset + 1] };
            const int b { scanLine[offset + 2] };
            const int lum { (r * 77 + g * 150 + b * 29) >> 8 };
            const int eqLum { cdfMap[static_cast<std::size_t>(lum)] };

            if (lum > 0) {
                const int blendedLum { (eqLum * bNorm + lum * origWeight) / 100 };
                scanLine[offset] = static_cast<uchar>(std::clamp((r * blendedLum) / lum, 0, 255));
                scanLine[offset + 1] = static_cast<uchar>(std::clamp((g * blendedLum) / lum, 0, 255));
                scanLine[offset + 2] = static_cast<uchar>(std::clamp((b * blendedLum) / lum, 0, 255));
            }
        }
    }
}

void SightlineVideoController::applyNativeSharpen(QImage& image, const QRect& targetRoi, int sharpen)
{
    if (sharpen <= 0) {
        return;
    }
    const int top { std::max(1, targetRoi.top()) };
    const int bottom { std::min(image.height() - 2, targetRoi.bottom()) };
    const int left { std::max(1, targetRoi.left()) };
    const int right { std::min(image.width() - 2, targetRoi.right()) };

    if (top > bottom || left > right) {
        return;
    }

    const int strength { std::clamp(sharpen, 1, 255) };
    const int factorFixed { (strength * 128) / 255 };

    const QImage copy { image.copy(left - 1, top - 1, (right - left + 3), (bottom - top + 3)) };

    for (int y = top; y <= bottom; ++y) {
        auto* outLine { image.scanLine(y) };
        const int localY { y - (top - 1) };
        const auto* linePrev { copy.constScanLine(localY - 1) };
        const auto* lineCur { copy.constScanLine(localY) };
        const auto* lineNext { copy.constScanLine(localY + 1) };

        for (int x = left; x <= right; ++x) {
            const int localX { (x - (left - 1)) * 3 };
            const int offset { x * 3 };

            for (int c = 0; c < 3; ++c) {
                const int curVal { lineCur[localX + c] };
                const int upVal { linePrev[localX + c] };
                const int downVal { lineNext[localX + c] };
                const int leftVal { lineCur[localX - 3 + c] };
                const int rightVal { lineCur[localX + 3 + c] };

                const int laplacian { (4 * curVal) - upVal - downVal - leftVal - rightVal };
                const int sharpened { curVal + ((laplacian * factorFixed) >> 8) };
                outLine[offset + c] = static_cast<uchar>(std::clamp(sharpened, 0, 255));
            }
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

    dec->setRtspTransport(static_cast<Video::RtspTransportMode>(m_transportMode));
    if (m_authEnabled && !m_rtspUsername.isEmpty()) {
        dec->setCredentials(m_rtspUsername.toStdString(), m_rtspPassword.toStdString());
    } else {
        dec->clearCredentials();
    }

    if (!dec->initialize(effectiveUri.toStdString(), Video::PixelFormat::RGB24, 0, Video::DeviceType::CPU)) {
        if (!m_isSynthetic) {
            updateState(PlaybackState::Error, tr("Cannot connect to %1").arg(sanitizedSourceUri()));
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

            if (m_isSynthetic) {
                applyEnhancement(frameCopy, m_activeCamera);
            }

            {
                QMutexLocker locker(&m_snapshotMutex);
                m_lastFrameCopy = frameCopy;
            }

            quint64 framePtsUs { 0U };
            if (frameInfo.timestamp > 0.0) {
                framePtsUs = static_cast<quint64>(frameInfo.timestamp * 1'000'000.0);
            } else {
                framePtsUs = static_cast<quint64>(std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                                                      .count());
            }

            {
                QMutexLocker locker(&m_bufferMutex);
                if (m_frameRingBuffer.size() >= MaxRingBufferSize) {
                    m_frameRingBuffer.pop_front();
                }
                m_frameRingBuffer.push_back(TimestampedFrame { frameCopy, framePtsUs });
                if (!m_paused.load()) {
                    m_currentFramePts = framePtsUs;
                }
            }
            emit bufferedFramesChanged();
            if (!m_paused.load()) {
                emit currentFramePtsChanged();
            }

            emit frameDecoded(frameCopy);

            // Picture-in-Picture secondary frame emission
            if (m_pipEnabled) {
                QImage pipCopy { rawImg.copy() };
                const int pipCam { pipCamera() };
                if (m_isSynthetic) {
                    applyEnhancement(pipCopy, pipCam);
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
