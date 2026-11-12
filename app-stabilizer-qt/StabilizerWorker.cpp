/// @file StabilizerWorker.cpp
/// @brief High-priority background decoding and video stabilization worker.
#include "StabilizerWorker.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <string>

#include <QMetaObject>
#include <QTimer>

#include "AtomicTripleBuffer.h"
#include "DecoderFactory.h"
#include "DecoderTypes.h"
#include "IVideoDecoder.h"
#include "RgbWarpUtility.h"
#include "StabilizerFramePacket.h"
#include "StabilizerTypes.h"
#include "SyntheticJitterGenerator.h"
#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

StabilizerWorker::StabilizerWorker(Video::AtomicTripleBuffer<StabilizerFramePacket>& tripleBuffer, QObject* parent)
    : QObject(parent)
    , tripleBuffer_(tripleBuffer)
{
}

StabilizerWorker::~StabilizerWorker()
{
    stop();
}

std::uint32_t StabilizerWorker::frameWidth() const noexcept
{
    return frameWidth_;
}

std::uint32_t StabilizerWorker::frameHeight() const noexcept
{
    return frameHeight_;
}

bool StabilizerWorker::isRunning() const noexcept
{
    return isRunning_.load();
}

void StabilizerWorker::initWorker()
{
    if (timer_ == nullptr) {
        timer_ = new QTimer(this);
        timer_->setTimerType(Qt::PreciseTimer);
        connect(timer_, &QTimer::timeout, this, &StabilizerWorker::processTick);
    }
}

void StabilizerWorker::openSource(const QString& uri)
{
    initWorker();
    stop();

    const std::string u = uri.trimmed().toStdString();
    currentUri_ = u;

    if (u.empty()) {
        return;
    }

    if (!initBackend(u)) {
        emit errorOccurred(QStringLiteral("Failed to open source: ") + uri);
        return;
    }

    play();
}

bool StabilizerWorker::initBackend(const std::string& uri)
{
    if ((uri.rfind("synthetic://", 0U) == 0U) || (uri.rfind("shaker://", 0U) == 0U)) {
        isSyntheticSource_ = true;
        syntheticGen_.reset();
        frameWidth_ = 1280U;
        frameHeight_ = 720U;
        nominalFps_ = 30.0;
        preallocateBuffers(frameWidth_, frameHeight_);
        emit sourceOpened(static_cast<int>(frameWidth_), static_cast<int>(frameHeight_), nominalFps_);
        return true;
    }

    if (uri.rfind("mock://", 0U) == 0U) {
        isSyntheticSource_ = false;
        decoder_ = Video::DecoderFactory::create(Video::BackendType::Mock);
        if ((decoder_ == nullptr) || !decoder_->initialize(uri, Video::PixelFormat::RGB24)) {
            return false;
        }
        const Video::VideoMetadata meta = decoder_->getVideoMetadata();
        frameWidth_ = static_cast<std::uint32_t>((meta.width > 0) ? meta.width : 1280);
        frameHeight_ = static_cast<std::uint32_t>((meta.height > 0) ? meta.height : 720);
        nominalFps_ = (meta.frameRate > 0.0) ? meta.frameRate : 30.0;
        preallocateBuffers(frameWidth_, frameHeight_);
        emit sourceOpened(static_cast<int>(frameWidth_), static_cast<int>(frameHeight_), nominalFps_);
        return true;
    }

    // Default backend: Native FFmpeg decoder for files, RTSP, UDP, and direct capture devices
    isSyntheticSource_ = false;
    decoder_ = Video::DecoderFactory::create(Video::BackendType::FFmpeg);
    if ((decoder_ == nullptr) || !decoder_->initialize(uri, Video::PixelFormat::RGB24)) {
        return false;
    }

    const Video::VideoMetadata meta = decoder_->getVideoMetadata();
    frameWidth_ = static_cast<std::uint32_t>((meta.width > 0) ? meta.width : 1920);
    frameHeight_ = static_cast<std::uint32_t>((meta.height > 0) ? meta.height : 1080);
    nominalFps_ = (meta.frameRate > 0.0) ? meta.frameRate : 30.0;

    preallocateBuffers(frameWidth_, frameHeight_);
    emit sourceOpened(static_cast<int>(frameWidth_), static_cast<int>(frameHeight_), nominalFps_);
    return true;
}

void StabilizerWorker::preallocateBuffers(std::uint32_t w, std::uint32_t h)
{
    for (auto& slot : tripleBuffer_.getSlots()) {
        slot.preallocate(w, h);
    }

    const std::size_t pixelCount = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    lumaInputBuffer_.resize(pixelCount);
    lumaOutputBuffer_.resize(pixelCount);

    Stabilizer::VideoStabilizerConfig cfg;
    cfg.dimensions = Stabilizer::FrameDimensions { w, h, w };
    static_cast<void>(engine_.configure(cfg));

    lastFrameTime_ = std::chrono::steady_clock::time_point { };
    fpsWindowStart_ = std::chrono::steady_clock::now();
    fpsFrameCount_ = 0U;
    measuredFps_ = nominalFps_;
}

void StabilizerWorker::updateTimerInterval()
{
    if (timer_ == nullptr) {
        return;
    }
    const double rate = playbackRate_.load();
    const double effectiveFps = nominalFps_ * ((rate > 0.0) ? rate : 1.0);
    const int intervalMs = std::max(1, static_cast<int>(1000.0 / effectiveFps));
    timer_->setInterval(intervalMs);
}

void StabilizerWorker::play()
{
    initWorker();
    isRunning_ = true;
    isPaused_ = false;
    updateTimerInterval();
    if (timer_ != nullptr) {
        timer_->start();
    }
}

void StabilizerWorker::pause()
{
    isPaused_ = true;
    if (timer_ != nullptr) {
        timer_->stop();
    }
}

void StabilizerWorker::stop()
{
    isRunning_ = false;
    isPaused_ = false;
    if (timer_ != nullptr) {
        timer_->stop();
    }
    if (decoder_ != nullptr) {
        decoder_->close();
        decoder_.reset();
    }
    engine_.reset();
    tripleBuffer_.reset();
}

void StabilizerWorker::stepFrame()
{
    if (isPaused_.load()) {
        processTick();
    }
}

void StabilizerWorker::seek(double timestampSec)
{
    if (decoder_ != nullptr) {
        decoder_->seek(timestampSec);
        if (isPaused_.load()) {
            processTick();
        }
    }
}

void StabilizerWorker::setPlaybackRate(double rate)
{
    playbackRate_ = std::clamp(rate, 0.25, 4.0);
    updateTimerInterval();
}

void StabilizerWorker::updateConfig(const Stabilizer::VideoStabilizerConfig& config)
{
    pendingConfig_ = config;
    pendingConfig_.dimensions = Stabilizer::FrameDimensions { frameWidth_, frameHeight_, frameWidth_ };
    hasPendingConfig_ = true;
}

void StabilizerWorker::processTick()
{
    if (!isRunning_.load() && !isPaused_.load()) {
        return;
    }

    if (hasPendingConfig_.load()) {
        static_cast<void>(engine_.configure(pendingConfig_));
        hasPendingConfig_ = false;
    }

    const auto now = std::chrono::steady_clock::now();
    double dt = 1.0 / nominalFps_;
    if (lastFrameTime_.time_since_epoch().count() > 0) {
        dt = std::chrono::duration<double>(now - lastFrameTime_).count();
    }
    lastFrameTime_ = now;
    if ((dt <= 0.0) || (dt > 0.2)) {
        dt = 1.0 / nominalFps_;
    }

    // Measure rolling FPS
    ++fpsFrameCount_;
    const double elapsedSec = std::chrono::duration<double>(now - fpsWindowStart_).count();
    if (elapsedSec >= 1.0) {
        measuredFps_ = static_cast<double>(fpsFrameCount_) / elapsedSec;
        fpsFrameCount_ = 0U;
        fpsWindowStart_ = now;
    }

    auto& writeSlot = tripleBuffer_.getWriteBuffer();
    if ((writeSlot.width != frameWidth_) || (writeSlot.height != frameHeight_)) {
        writeSlot.preallocate(frameWidth_, frameHeight_);
    }

    double pts = 0.0;

    if (isSyntheticSource_) {
        pts = syntheticGen_.generateNextFrame(writeSlot.rawRgb.data(), dt);
        writeSlot.timestampSec = pts;
    } else {
        if (decoder_ == nullptr) {
            return;
        }
        if (!decoder_->decodeNextFrame()) {
            emit endOfStreamReached();
            return;
        }
        const Video::FrameInfo info = decoder_->getRawFrameData();
        if ((info.data != nullptr) && (info.size > 0U)) {
            const std::size_t copyBytes = std::min(writeSlot.rawRgb.size(), info.size);
            std::memcpy(writeSlot.rawRgb.data(), info.data, copyBytes);
        }
        pts = info.timestamp;
        writeSlot.timestampSec = pts;
    }

    // Extract grayscale luma (Y) for feature detection and tracking
    RgbWarpUtility::extractLuma(writeSlot.rawRgb.data(), lumaInputBuffer_.data(), frameWidth_, frameHeight_);

    const Stabilizer::FrameView inLumaView(
        lumaInputBuffer_.data(), Stabilizer::FrameDimensions { frameWidth_, frameHeight_, frameWidth_ });

    Stabilizer::FrameView outLumaView(
        lumaOutputBuffer_.data(), Stabilizer::FrameDimensions { frameWidth_, frameHeight_, frameWidth_ });

    // Execute core stabilization engine
    const bool procOk = engine_.processFrame(inLumaView, outLumaView, static_cast<float>(dt));
    if (!procOk) {
        return;
    }

    const Stabilizer::StabilizerDiagnostics diag = engine_.diagnostics();
    writeSlot.diagnostics = diag;

    // Populate visual landmark coordinates and motion vectors
    std::array<Stabilizer::VisualLandmark, StabilizerFramePacket::MaxVisualFeatures> rawLandmarks { };
    const std::size_t numLandmarks = engine_.copyLandmarks(rawLandmarks.data(), rawLandmarks.size());
    writeSlot.featureCount = numLandmarks;
    for (std::size_t i = 0U; i < numLandmarks; ++i) {
        writeSlot.features[i].x = rawLandmarks[i].currPt.x;
        writeSlot.features[i].y = rawLandmarks[i].currPt.y;
        writeSlot.features[i].vx = rawLandmarks[i].motion.dx;
        writeSlot.features[i].vy = rawLandmarks[i].motion.dy;
        writeSlot.features[i].isInlier = rawLandmarks[i].isInlier;
    }

    // Apply affine correction warp to full-color RGB24 frame
    static_cast<void>(RgbWarpUtility::warpRgb24(writeSlot.rawRgb.data(), writeSlot.stabilizedRgb.data(), frameWidth_,
        frameHeight_, writeSlot.stride, diag.correctionTransform, engine_.config().warp));

    // Atomically publish finished frame to consumer
    writeSlot.frameNumber = ++frameCounter_;
    tripleBuffer_.publishWriteBuffer();

    emit frameReady();
    emit telemetryUpdated(diag, measuredFps_);
}

} // namespace StabilizerApp
