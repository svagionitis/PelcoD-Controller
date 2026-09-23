/// @file VideoPlayerController.cpp
/// @brief Master QML controller bridging UI events with Video and VideoFilters subsystems.

#include "VideoPlayerController.h"
#include "VideoQuickItem.h"

#if defined(PELCOD_HAS_FILTERS)
#include "VideoFilters.h"
#endif

#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <algorithm>
#include <chrono>

namespace VideoApp {

VideoPlayerController::VideoPlayerController(QObject* parent)
    : QObject(parent)
{
    refreshDevices();
}

VideoPlayerController::~VideoPlayerController()
{
    stopPlayback();
}

VideoPlayerController::PlaybackState VideoPlayerController::playbackState() const noexcept
{
    return m_state;
}

QString VideoPlayerController::sourceUri() const
{
    return m_sourceUri;
}

void VideoPlayerController::setSourceUri(const QString& uri)
{
    if (m_sourceUri != uri) {
        m_sourceUri = uri;
        emit sourceUriChanged();
    }
}

int VideoPlayerController::backendIndex() const noexcept
{
    return m_backendIndex;
}

void VideoPlayerController::setBackendIndex(int index)
{
    if (m_backendIndex != index) {
        m_backendIndex = index;
        emit backendIndexChanged();
    }
}

int VideoPlayerController::deviceIndex() const noexcept
{
    return m_deviceIndex;
}

void VideoPlayerController::setDeviceIndex(int index)
{
    if (m_deviceIndex != index) {
        m_deviceIndex = index;
        emit deviceIndexChanged();
    }
}

bool VideoPlayerController::isLoopPlayback() const noexcept
{
    return m_isLoopPlayback;
}

void VideoPlayerController::setLoopPlayback(bool loop)
{
    if (m_isLoopPlayback != loop) {
        m_isLoopPlayback = loop;
        emit loopPlaybackChanged();
    }
}

QString VideoPlayerController::statusMessage() const
{
    return m_statusMessage;
}

QStringList VideoPlayerController::availableBackends() const
{
    QStringList list;
    const auto backends = Video::DecoderFactory::availableBackends();
    for (const auto b : backends) {
        switch (b) {
        case Video::BackendType::FFmpeg:
            list.append(tr("FFmpeg (Hardware/Software)"));
            break;
        case Video::BackendType::GStreamer:
            list.append(tr("GStreamer Pipeline"));
            break;
        case Video::BackendType::Mock:
            list.append(tr("Mock Synthetic Generator"));
            break;
        }
    }
    if (list.isEmpty()) {
        list.append(tr("Mock Synthetic Generator"));
    }
    return list;
}

QVariantList VideoPlayerController::availableDevices() const
{
    return m_devicesList;
}

double VideoPlayerController::positionSeconds() const noexcept
{
    return m_positionSeconds;
}

double VideoPlayerController::durationSeconds() const noexcept
{
    return m_durationSeconds;
}

bool VideoPlayerController::isSeekable() const noexcept
{
    return m_isSeekable;
}

double VideoPlayerController::fps() const noexcept
{
    return m_fps;
}

double VideoPlayerController::avgDecodeTimeMs() const noexcept
{
    return m_avgDecodeTimeMs;
}

int VideoPlayerController::frameWidth() const noexcept
{
    return m_frameWidth;
}

int VideoPlayerController::frameHeight() const noexcept
{
    return m_frameHeight;
}

QString VideoPlayerController::codecName() const
{
    return m_codecName;
}

QString VideoPlayerController::pixelFormat() const
{
    return m_pixelFormat;
}

qint64 VideoPlayerController::totalFrames() const noexcept
{
    return m_totalFrames;
}

qint64 VideoPlayerController::droppedFrames() const noexcept
{
    return m_droppedFrames;
}

double VideoPlayerController::bitrateKbps() const noexcept
{
    return m_bitrateKbps;
}

// --- Filter Getters & Setters ---
bool VideoPlayerController::dehazeEnabled() const noexcept
{
    return m_dehaze;
}
void VideoPlayerController::setDehazeEnabled(bool v)
{
    if (m_dehaze != v) {
        m_dehaze = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::stabilizationEnabled() const noexcept
{
    return m_stabilization;
}
void VideoPlayerController::setStabilizationEnabled(bool v)
{
    if (m_stabilization != v) {
        m_stabilization = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::claheEnabled() const noexcept
{
    return m_clahe;
}
void VideoPlayerController::setClaheEnabled(bool v)
{
    if (m_clahe != v) {
        m_clahe = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::denoiseEnabled() const noexcept
{
    return m_denoise;
}
void VideoPlayerController::setDenoiseEnabled(bool v)
{
    if (m_denoise != v) {
        m_denoise = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::sharpenEnabled() const noexcept
{
    return m_sharpen;
}
void VideoPlayerController::setSharpenEnabled(bool v)
{
    if (m_sharpen != v) {
        m_sharpen = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::edgeDetectEnabled() const noexcept
{
    return m_edgeDetect;
}
void VideoPlayerController::setEdgeDetectEnabled(bool v)
{
    if (m_edgeDetect != v) {
        m_edgeDetect = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::whiteBalanceMode() const noexcept
{
    return m_whiteBalance;
}
void VideoPlayerController::setWhiteBalanceMode(int v)
{
    if (m_whiteBalance != v) {
        m_whiteBalance = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::brightness() const noexcept
{
    return m_brightness;
}
void VideoPlayerController::setBrightness(int v)
{
    if (m_brightness != v) {
        m_brightness = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

double VideoPlayerController::contrast() const noexcept
{
    return m_contrast;
}
void VideoPlayerController::setContrast(double v)
{
    if (std::abs(m_contrast - v) > 0.001) {
        m_contrast = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

double VideoPlayerController::gamma() const noexcept
{
    return m_gamma;
}
void VideoPlayerController::setGamma(double v)
{
    if (std::abs(m_gamma - v) > 0.001) {
        m_gamma = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::colorToneMode() const noexcept
{
    return m_colorToneMode;
}
void VideoPlayerController::setColorToneMode(int v)
{
    if (m_colorToneMode != v) {
        m_colorToneMode = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::colorTintPreset() const noexcept
{
    return m_colorTintPreset;
}
void VideoPlayerController::setColorTintPreset(int v)
{
    if (m_colorTintPreset != v) {
        m_colorTintPreset = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

double VideoPlayerController::colorEnhanceFactor() const noexcept
{
    return m_colorEnhanceFactor;
}
void VideoPlayerController::setColorEnhanceFactor(double v)
{
    if (std::abs(m_colorEnhanceFactor - v) > 0.001) {
        m_colorEnhanceFactor = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::histogramEqMode() const noexcept
{
    return m_histogramEqMode;
}
void VideoPlayerController::setHistogramEqMode(int v)
{
    if (m_histogramEqMode != v) {
        m_histogramEqMode = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::vignetteEnabled() const noexcept
{
    return m_vignette;
}
void VideoPlayerController::setVignetteEnabled(bool v)
{
    if (m_vignette != v) {
        m_vignette = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::thresholdEnabled() const noexcept
{
    return m_threshold;
}
void VideoPlayerController::setThresholdEnabled(bool v)
{
    if (m_threshold != v) {
        m_threshold = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

double VideoPlayerController::thresholdValue() const noexcept
{
    return m_thresholdValue;
}
void VideoPlayerController::setThresholdValue(double v)
{
    if (std::abs(m_thresholdValue - v) > 0.001) {
        m_thresholdValue = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::falseColorPalette() const noexcept
{
    return m_falseColor;
}
void VideoPlayerController::setFalseColorPalette(int v)
{
    if (m_falseColor != v) {
        m_falseColor = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::isothermPreset() const noexcept
{
    return m_isotherm;
}
void VideoPlayerController::setIsothermPreset(int v)
{
    if (m_isotherm != v) {
        m_isotherm = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::hotspotTrackerEnabled() const noexcept
{
    return m_hotspotTracker;
}
void VideoPlayerController::setHotspotTrackerEnabled(bool v)
{
    if (m_hotspotTracker != v) {
        m_hotspotTracker = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::mtiMotionEnabled() const noexcept
{
    return m_mtiMotion;
}
void VideoPlayerController::setMtiMotionEnabled(bool v)
{
    if (m_mtiMotion != v) {
        m_mtiMotion = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::opticalFlowMode() const noexcept
{
    return m_opticalFlow;
}
void VideoPlayerController::setOpticalFlowMode(int v)
{
    if (m_opticalFlow != v) {
        m_opticalFlow = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::heatmapEnabled() const noexcept
{
    return m_heatmap;
}
void VideoPlayerController::setHeatmapEnabled(bool v)
{
    if (m_heatmap != v) {
        m_heatmap = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::reticleStyle() const noexcept
{
    return m_reticleStyle;
}
void VideoPlayerController::setReticleStyle(int v)
{
    if (m_reticleStyle != v) {
        m_reticleStyle = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::tripwireEnabled() const noexcept
{
    return m_tripwire;
}
void VideoPlayerController::setTripwireEnabled(bool v)
{
    if (m_tripwire != v) {
        m_tripwire = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::privacyMaskMode() const noexcept
{
    return m_privacyMode;
}
void VideoPlayerController::setPrivacyMaskMode(int v)
{
    if (m_privacyMode != v) {
        m_privacyMode = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::watermarkEnabled() const noexcept
{
    return m_watermark;
}
void VideoPlayerController::setWatermarkEnabled(bool v)
{
    if (m_watermark != v) {
        m_watermark = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::telemetryOsdEnabled() const noexcept
{
    return m_telemetryOsd;
}
void VideoPlayerController::setTelemetryOsdEnabled(bool v)
{
    if (m_telemetryOsd != v) {
        m_telemetryOsd = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::streamHealthOsdEnabled() const noexcept
{
    return m_streamHealthOsd;
}
void VideoPlayerController::setStreamHealthOsdEnabled(bool v)
{
    if (m_streamHealthOsd != v) {
        m_streamHealthOsd = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::streamHealthOsdStyle() const noexcept
{
    return m_streamHealthOsdStyle;
}
void VideoPlayerController::setStreamHealthOsdStyle(int v)
{
    if (m_streamHealthOsdStyle != v) {
        m_streamHealthOsdStyle = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

int VideoPlayerController::streamHealthOsdPosition() const noexcept
{
    return m_streamHealthOsdPosition;
}
void VideoPlayerController::setStreamHealthOsdPosition(int v)
{
    if (m_streamHealthOsdPosition != v) {
        m_streamHealthOsdPosition = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

bool VideoPlayerController::textOverlayEnabled() const noexcept
{
    return m_textOverlay;
}
void VideoPlayerController::setTextOverlayEnabled(bool v)
{
    if (m_textOverlay != v) {
        m_textOverlay = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

QString VideoPlayerController::textOverlayString() const
{
    return m_textOverlayString;
}
void VideoPlayerController::setTextOverlayString(const QString& v)
{
    if (m_textOverlayString != v) {
        m_textOverlayString = v;
        m_filtersDirty.store(true);
        emit filterConfigChanged();
    }
}

void VideoPlayerController::refreshDevices()
{
    m_devicesList.clear();
    const auto devices = Video::DeviceEnumerator::enumerateDevices();
    for (const auto& d : devices) {
        QVariantMap map;
        map[QStringLiteral("name")] = QString::fromStdString(d.name);
        map[QStringLiteral("path")] = QString::fromStdString(d.path);
        map[QStringLiteral("busInfo")] = QString::fromStdString(d.description);
        m_devicesList.append(map);
    }
    emit availableDevicesChanged();
}

void VideoPlayerController::attachVideoItem(VideoApp::VideoQuickItem* item)
{
    if (item != nullptr) {
        connect(this, &VideoPlayerController::frameDecoded, item, &VideoQuickItem::updateFrame, Qt::QueuedConnection);
    }
}

void VideoPlayerController::startPlayback()
{
    stopPlayback();

    updateState(PlaybackState::Opening, tr("Initializing decoder backend..."));

    // Map backend index
    const auto available = Video::DecoderFactory::availableBackends();
    Video::BackendType backend = Video::BackendType::Mock;
    if (m_backendIndex >= 0 && m_backendIndex < static_cast<int>(available.size())) {
        backend = available[static_cast<std::size_t>(m_backendIndex)];
    }

    // Map hardware acceleration device
    Video::DeviceType device = Video::DeviceType::CPU;
    if (m_deviceIndex == 1) {
        device = Video::DeviceType::CUDA;
    } else if (m_deviceIndex == 2) {
        device = Video::DeviceType::VAAPI;
    }

    auto dec = Video::DecoderFactory::create(backend);
    if (!dec) {
        updateState(PlaybackState::Error, tr("Requested decoder backend is unavailable"));
        return;
    }

    QString effectiveSource = m_sourceUri.trimmed();
    if (effectiveSource.isEmpty()) {
        effectiveSource = QStringLiteral("mock://test");
    }

    configureFilterPipeline(dec.get());

    if (!dec->initialize(effectiveSource.toStdString(), Video::PixelFormat::RGB24, 0, device)) {
        updateState(PlaybackState::Error, tr("Failed to open video source: %1").arg(effectiveSource));
        return;
    }

    {
        QMutexLocker locker(&m_decoderMutex);
        m_decoder = std::move(dec);
    }

    const auto meta = m_decoder->getVideoMetadata();
    m_codecName = QString::fromStdString(meta.codecName);
    m_pixelFormat = (meta.format == Video::PixelFormat::RGB24) ? QStringLiteral("RGB24") : QStringLiteral("BGR24");
    m_durationSeconds = meta.duration;
    m_isSeekable = meta.duration > 0.0;
    m_totalFrames = 0;
    m_droppedFrames = 0;

    emit durationSecondsChanged();
    emit isSeekableChanged();

    m_running.store(true);
    m_paused.store(false);
    m_seekRequested.store(-1.0);

    updateState(PlaybackState::Playing, tr("Streaming"));

    m_thread = std::unique_ptr<QThread>(QThread::create([this]() { workerLoop(); }));
    m_thread->start();
}

void VideoPlayerController::stopPlayback()
{
    m_running.store(false);
    m_paused.store(false);

    if (m_thread && m_thread->isRunning()) {
        m_thread->wait(2000);
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
    m_positionSeconds = 0.0;
    emit positionSecondsChanged();
    emit statsUpdated();

    updateState(PlaybackState::Idle, tr("Stopped"));
}

void VideoPlayerController::pausePlayback()
{
    if (m_state == PlaybackState::Playing) {
        m_paused.store(true);
        updateState(PlaybackState::Paused, tr("Paused"));
    }
}

void VideoPlayerController::resumePlayback()
{
    if (m_state == PlaybackState::Paused) {
        m_paused.store(false);
        updateState(PlaybackState::Playing, tr("Playing"));
    }
}

void VideoPlayerController::seek(double seconds)
{
    if (m_isSeekable && seconds >= 0.0) {
        m_seekRequested.store(seconds);
    }
}

QString VideoPlayerController::takeSnapshot(const QString& filePath)
{
    QImage snap;
    {
        QMutexLocker locker(&m_snapshotMutex);
        if (m_lastFrameCopy.isNull()) {
            return QString();
        }
        snap = m_lastFrameCopy.copy();
    }

    QString targetPath = filePath;
    if (targetPath.isEmpty()) {
        const QString picturesDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
        targetPath = QString("%1/Snapshot_%2.png").arg(picturesDir, timestamp);
    }

    if (snap.save(targetPath, "PNG")) {
        return targetPath;
    }
    return QString();
}

void VideoPlayerController::updateState(PlaybackState state, const QString& message)
{
    m_state = state;
    m_statusMessage = message;
    emit playbackStateChanged();
    emit statusMessageChanged();
}

void VideoPlayerController::workerLoop()
{
    using clock = std::chrono::steady_clock;
    auto lastStatsTime = clock::now();
    int framesInInterval = 0;
    double decodeAccumMs = 0.0;

    while (m_running.load()) {
        if (m_filtersDirty.exchange(false)) {
            QMutexLocker locker(&m_decoderMutex);
            if (m_decoder) {
                configureFilterPipeline(m_decoder.get());
            }
        }

        const double reqSeek = m_seekRequested.exchange(-1.0);
        if (reqSeek >= 0.0) {
            QMutexLocker locker(&m_decoderMutex);
            if (m_decoder) {
                m_decoder->seek(reqSeek);
            }
        }

        if (m_paused.load()) {
            QThread::msleep(20);
            continue;
        }

        const auto startDecode = clock::now();
        bool success = false;
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

        const auto endDecode = clock::now();
        const double decodeMs = std::chrono::duration<double, std::milli>(endDecode - startDecode).count();

        if (!success) {
            if (m_isLoopPlayback && m_isSeekable) {
                QMutexLocker locker(&m_decoderMutex);
                if (m_decoder) {
                    m_decoder->seek(0.0);
                }
                QThread::msleep(10);
                continue;
            }
            QThread::msleep(15);
            continue;
        }

        if (frameInfo.data != nullptr && frameInfo.width > 0 && frameInfo.height > 0) {
            // Ingest and emit decoded frame
            const QImage rawImg(
                frameInfo.data, frameInfo.width, frameInfo.height, frameInfo.width * 3, QImage::Format_RGB888);

            QImage frameCopy = rawImg.copy();

            {
                QMutexLocker locker(&m_snapshotMutex);
                m_lastFrameCopy = frameCopy;
            }

            emit frameDecoded(frameCopy);
            ++m_totalFrames;
            ++framesInInterval;
            decodeAccumMs += decodeMs;

            m_positionSeconds = frameInfo.timestamp;
        }

        // Periodic telemetry calculation (every 250ms)
        const auto now = clock::now();
        const double elapsedSec = std::chrono::duration<double>(now - lastStatsTime).count();
        if (elapsedSec >= 0.25) {
            m_fps = static_cast<double>(framesInInterval) / elapsedSec;
            m_avgDecodeTimeMs = framesInInterval > 0 ? (decodeAccumMs / framesInInterval) : 0.0;
            m_frameWidth = frameInfo.width;
            m_frameHeight = frameInfo.height;

            framesInInterval = 0;
            decodeAccumMs = 0.0;
            lastStatsTime = now;

            emit positionSecondsChanged();
            emit statsUpdated();
        }

        // Frame rate pacing for local files or synthetic generator
        if (decodeMs < 30.0) {
            QThread::msleep(static_cast<unsigned long>(std::max(1.0, 32.0 - decodeMs)));
        }
    }
}

void VideoPlayerController::configureFilterPipeline(Video::IVideoDecoder* decoder)
{
    if (decoder == nullptr) {
        return;
    }

    decoder->clearFrameProcessors();

#if defined(PELCOD_HAS_FILTERS)
    // 1. Stabilization & Denoise
    if (m_stabilization) {
        decoder->addFrameProcessor(std::make_shared<Video::ImageStabilizationFilter>(0.8, 30.0, 0.04));
    }
    if (m_denoise) {
        decoder->addFrameProcessor(std::make_shared<Video::TemporalDenoiseFilter>(0.5, 30.0));
    }

    // 2. Optical & Tonal Enhancements
    if (m_dehaze) {
        decoder->addFrameProcessor(std::make_shared<Video::DarkChannelDehazeFilter>(0.85, 9, 0.1));
    }
    if (m_whiteBalance == 1) {
        decoder->addFrameProcessor(
            std::make_shared<Video::WhiteBalanceFilter>(Video::WhiteBalanceFilter::Mode::GrayWorld, 1.0));
    } else if (m_whiteBalance == 2) {
        decoder->addFrameProcessor(
            std::make_shared<Video::WhiteBalanceFilter>(Video::WhiteBalanceFilter::Mode::WhitePatch, 1.0));
    }
    if (m_clahe) {
        decoder->addFrameProcessor(std::make_shared<Video::ClaheFilter>(2.5, 8, 8));
    }
    if (m_sharpen) {
        decoder->addFrameProcessor(std::make_shared<Video::SharpenFilter>(0.6, 1.0));
    }
    if (m_edgeDetect) {
        decoder->addFrameProcessor(std::make_shared<Video::EdgeDetectionFilter>(50.0, 150.0));
    }

    // Brightness & Contrast
    if (m_brightness != 0 || std::abs(m_contrast - 1.0) > 0.01) {
        decoder->addFrameProcessor(std::make_shared<Video::BrightnessContrastFilter>(m_contrast, m_brightness));
    }
    // Gamma Correction
    if (std::abs(m_gamma - 1.0) > 0.01) {
        decoder->addFrameProcessor(std::make_shared<Video::GammaCorrectionFilter>(m_gamma));
    }
    // Color Saturation Boost
    if (std::abs(m_colorEnhanceFactor - 1.0) > 0.05) {
        decoder->addFrameProcessor(std::make_shared<Video::ColorEnhanceFilter>(m_colorEnhanceFactor));
    }
    // Color Tone Modes (0: Normal, 1: Grayscale, 2: Inverted Negative, 3: Sepia)
    if (m_colorToneMode == 1) {
        decoder->addFrameProcessor(std::make_shared<Video::GrayscaleFilter>());
    } else if (m_colorToneMode == 2) {
        decoder->addFrameProcessor(std::make_shared<Video::InvertColorsFilter>());
    } else if (m_colorToneMode == 3) {
        decoder->addFrameProcessor(std::make_shared<Video::SepiaFilter>());
    }
    // Color Tint Presets (0: None, 1: NVG Green, 2: Marine Blue, 3: Tactical Amber)
    if (m_colorTintPreset == 1) {
        decoder->addFrameProcessor(std::make_shared<Video::ColorTintFilter>(0.2, 1.4, 0.2));
    } else if (m_colorTintPreset == 2) {
        decoder->addFrameProcessor(std::make_shared<Video::ColorTintFilter>(0.2, 0.6, 1.5));
    } else if (m_colorTintPreset == 3) {
        decoder->addFrameProcessor(std::make_shared<Video::ColorTintFilter>(1.4, 1.0, 0.3));
    }
    // Adaptive Histogram Equalization
    if (m_histogramEqMode > 0) {
        const auto mode = static_cast<Video::HistogramEqualizationFilter::Mode>(m_histogramEqMode - 1);
        decoder->addFrameProcessor(std::make_shared<Video::HistogramEqualizationFilter>(mode));
    }
    // Vignette
    if (m_vignette) {
        decoder->addFrameProcessor(std::make_shared<Video::VignetteFilter>());
    }
    // Binary Threshold
    if (m_threshold) {
        decoder->addFrameProcessor(std::make_shared<Video::ThresholdFilter>(m_thresholdValue));
    }

    // 3. Thermal Analytics
    if (m_falseColor > 0) {
        const auto p = static_cast<Video::FalseColorPalette>(m_falseColor - 1);
        decoder->addFrameProcessor(std::make_shared<Video::FalseColorFilter>(p));
    }
    if (m_isotherm > 0) {
        auto isoFilter = std::make_shared<Video::IsothermFilter>();
        const auto iso = static_cast<Video::IsothermFilter::Preset>(m_isotherm - 1);
        isoFilter->setPreset(iso);
        decoder->addFrameProcessor(isoFilter);
    }
    if (m_hotspotTracker) {
        decoder->addFrameProcessor(std::make_shared<Video::HotspotTrackerFilter>(220, 15));
    }

    // 4. Motion & Computer Vision
    if (m_mtiMotion) {
        decoder->addFrameProcessor(std::make_shared<Video::MovingTargetIndicatorFilter>(0.05, 30, 20.0));
    }
    if (m_opticalFlow > 0) {
        const auto m = (m_opticalFlow == 1) ? Video::OpticalFlowFieldFilter::DisplayMode::VectorArrows
                                            : Video::OpticalFlowFieldFilter::DisplayMode::ColorFlow;
        decoder->addFrameProcessor(std::make_shared<Video::OpticalFlowFieldFilter>(m, 16, 2.0));
    }
    if (m_heatmap) {
        decoder->addFrameProcessor(std::make_shared<Video::MotionHeatmapFilter>(0.05, 0.96));
    }

    // 5. Tactical Overlays & OSD
    if (m_reticleStyle > 0) {
        const auto s = static_cast<Video::TacticalReticleOverlayFilter::Style>(m_reticleStyle - 1);
        decoder->addFrameProcessor(std::make_shared<Video::TacticalReticleOverlayFilter>(
            s, Video::TacticalReticleOverlayFilter::Color::TacticalGreen, 1.0));
    }
    if (m_tripwire) {
        decoder->addFrameProcessor(std::make_shared<Video::PerimeterTripwireFilter>(
            0.1, 0.5, 0.9, 0.5, Video::PerimeterTripwireFilter::Direction::Bidirectional));
    }
    if (m_privacyMode > 0) {
        const auto mode = static_cast<Video::PrivacyMaskFilter::ConcealmentMode>(m_privacyMode - 1);
        decoder->addFrameProcessor(std::make_shared<Video::PrivacyMaskFilter>(mode));
    }
    if (m_watermark) {
        decoder->addFrameProcessor(std::make_shared<Video::TimestampWatermarkFilter>(
            Video::TimestampWatermarkFilter::Position::TopLeft, "VIDEO-HUB-01"));
    }
    if (m_telemetryOsd) {
        decoder->addFrameProcessor(std::make_shared<Video::TelemetryOsdFilter>());
    }

    // Stream Health OSD & Monitor Watchdog
    if (m_streamHealthOsd) {
        if (!m_streamHealthMonitor) {
            Video::StreamHealthConfig cfg {};
            cfg.nominalFps = 30.0;
            m_streamHealthMonitor = std::make_shared<Video::StreamHealthMonitor>(cfg);
        }
        decoder->addFrameProcessor(m_streamHealthMonitor);

        const auto style = static_cast<Video::StreamHealthOsdFilter::Style>(std::clamp(m_streamHealthOsdStyle, 0, 2));
        const auto pos
            = static_cast<Video::StreamHealthOsdFilter::Position>(std::clamp(m_streamHealthOsdPosition, 0, 3));
        auto healthOsd = std::make_shared<Video::StreamHealthOsdFilter>(pos, style);
        healthOsd->bindMonitor(m_streamHealthMonitor);
        decoder->addFrameProcessor(healthOsd);
    }
    // Custom Text Overlay Banner
    if (m_textOverlay && !m_textOverlayString.isEmpty()) {
        decoder->addFrameProcessor(
            std::make_shared<Video::TextOverlayFilter>(m_textOverlayString.toStdString(), 24, 40, 1.0));
    }
#endif
}

} // namespace VideoApp
