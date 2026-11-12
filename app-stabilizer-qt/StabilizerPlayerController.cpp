/// @file StabilizerPlayerController.cpp
/// @brief Master QML controller bridging UI events with Video and Stabilizer subsystems.

#include "StabilizerPlayerController.h"

#include <cstdint>

#include <QMetaObject>
#include <QString>
#include <QThread>

#include "AtomicTripleBuffer.h"
#include "DeviceEnumerator.h"
#include "StabilizerFramePacket.h"
#include "StabilizerWorker.h"
#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

StabilizerPlayerController::StabilizerPlayerController(QObject* parent)
    : QObject(parent)
    , worker_ { new StabilizerWorker(tripleBuffer_) }
{
    worker_->moveToThread(&workerThread_);

    connect(&workerThread_, &QThread::finished, worker_, &QObject::deleteLater);

    connect(worker_, &StabilizerWorker::frameReady, this, &StabilizerPlayerController::frameReady);

    connect(worker_, &StabilizerWorker::sourceOpened, this, [this](int w, int h, double nominalFps) {
        videoWidth_ = w;
        videoHeight_ = h;
        fps_ = nominalFps;
        emit videoDimensionsChanged();
        emit telemetryUpdated();
        setStatusMessage(QStringLiteral("Streaming %1x%2 @ %3 FPS").arg(w).arg(h).arg(nominalFps, 0, 'f', 1));
        setPlaybackState(PlaybackState::Playing);
    });

    connect(worker_, &StabilizerWorker::errorOccurred, this, [this](const QString& err) {
        setStatusMessage(err);
        setPlaybackState(PlaybackState::Error);
    });

    connect(worker_, &StabilizerWorker::telemetryUpdated, this,
        [this](const Stabilizer::StabilizerDiagnostics& diag, double measuredFps) {
            fps_ = measuredFps;
            lastDiagnostics_ = diag;
            telemetryRingBuffer_.appendSample(diag);
            emit telemetryUpdated();
        });

    connect(worker_, &StabilizerWorker::endOfStreamReached, this, [this]() {
        setStatusMessage(QStringLiteral("End of stream reached"));
        setPlaybackState(PlaybackState::Paused);
    });

    connect(&configModel_, &StabilizerConfigModel::configChanged, this,
        [this]() { updateStabilizerConfig(configModel_.toConfig()); });

    workerThread_.start();
    QMetaObject::invokeMethod(worker_, &StabilizerWorker::initWorker);
}

StabilizerPlayerController::~StabilizerPlayerController()
{
    if (workerThread_.isRunning()) {
        QMetaObject::invokeMethod(worker_, &StabilizerWorker::stop, Qt::BlockingQueuedConnection);
        workerThread_.quit();
        workerThread_.wait();
    }
}

StabilizerPlayerController::PlaybackState StabilizerPlayerController::playbackState() const noexcept
{
    return playbackState_;
}

void StabilizerPlayerController::setPlaybackState(PlaybackState state)
{
    if (playbackState_ != state) {
        playbackState_ = state;
        emit playbackStateChanged();
    }
}

QString StabilizerPlayerController::sourceUri() const
{
    return sourceUri_;
}

void StabilizerPlayerController::setSourceUri(const QString& uri)
{
    if (sourceUri_ != uri) {
        sourceUri_ = uri;
        emit sourceUriChanged();

        if (!sourceUri_.isEmpty()) {
            setPlaybackState(PlaybackState::Opening);
            setStatusMessage(QStringLiteral("Opening source: ") + sourceUri_);
            QMetaObject::invokeMethod(worker_, "openSource", Qt::QueuedConnection, Q_ARG(QString, sourceUri_));
        } else {
            stop();
        }
    }
}

QString StabilizerPlayerController::statusMessage() const
{
    return statusMessage_;
}

void StabilizerPlayerController::setStatusMessage(const QString& msg)
{
    if (statusMessage_ != msg) {
        statusMessage_ = msg;
        emit statusMessageChanged();
    }
}

int StabilizerPlayerController::videoWidth() const noexcept
{
    return videoWidth_;
}

int StabilizerPlayerController::videoHeight() const noexcept
{
    return videoHeight_;
}

double StabilizerPlayerController::fps() const noexcept
{
    return fps_;
}

StabilizerConfigModel* StabilizerPlayerController::configModel() noexcept
{
    return &configModel_;
}

TelemetryRingBuffer* StabilizerPlayerController::telemetryRingBuffer() noexcept
{
    return &telemetryRingBuffer_;
}

PresetManager* StabilizerPlayerController::presetManager() noexcept
{
    return &presetManager_;
}

int StabilizerPlayerController::detectedFeatures() const noexcept
{
    return static_cast<int>(lastDiagnostics_.detectedFeatures);
}

int StabilizerPlayerController::trackedFeatures() const noexcept
{
    return static_cast<int>(lastDiagnostics_.trackedFeatures);
}

int StabilizerPlayerController::inlierCount() const noexcept
{
    return static_cast<int>(lastDiagnostics_.inlierCount);
}

double StabilizerPlayerController::inlierRatio() const noexcept
{
    return static_cast<double>(lastDiagnostics_.inlierRatio);
}

double StabilizerPlayerController::totalLatencyMs() const noexcept
{
    return static_cast<double>(lastDiagnostics_.latencyUs.totalFrameUs) / 1000.0;
}

int StabilizerPlayerController::pyramidLatencyUs() const noexcept
{
    return static_cast<int>(lastDiagnostics_.latencyUs.pyramidGenUs);
}

int StabilizerPlayerController::featureDetectLatencyUs() const noexcept
{
    return static_cast<int>(lastDiagnostics_.latencyUs.featureDetectUs);
}

int StabilizerPlayerController::opticalFlowLatencyUs() const noexcept
{
    return static_cast<int>(lastDiagnostics_.latencyUs.opticalFlowUs);
}

int StabilizerPlayerController::motionModelLatencyUs() const noexcept
{
    return static_cast<int>(lastDiagnostics_.latencyUs.motionModelUs);
}

int StabilizerPlayerController::trajectoryFilterLatencyUs() const noexcept
{
    return static_cast<int>(lastDiagnostics_.latencyUs.trajectoryFilterUs);
}

int StabilizerPlayerController::warpLatencyUs() const noexcept
{
    return static_cast<int>(lastDiagnostics_.latencyUs.warpSynthesisUs);
}

QString StabilizerPlayerController::engineStateText() const
{
    switch (lastDiagnostics_.state) {
    case Stabilizer::EngineState::Initializing:
        return QStringLiteral("INITIALIZING");
    case Stabilizer::EngineState::Tracking:
        return QStringLiteral("TRACKING");
    case Stabilizer::EngineState::Degraded:
        return QStringLiteral("DEGRADED");
    case Stabilizer::EngineState::Resetting:
        return QStringLiteral("RESETTING");
    case Stabilizer::EngineState::Uninitialized:
    default:
        return QStringLiteral("UNINITIALIZED");
    }
}

QVariantList StabilizerPlayerController::getMatrixElements(int matrixType) const
{
    const auto& mat = (matrixType == 0) ? lastDiagnostics_.rawTransform
        : (matrixType == 1)             ? lastDiagnostics_.smoothedTransform
                                        : lastDiagnostics_.correctionTransform;
    return { static_cast<double>(mat.a11), static_cast<double>(mat.a12), static_cast<double>(mat.tx),
        static_cast<double>(mat.a21), static_cast<double>(mat.a22), static_cast<double>(mat.ty) };
}

Video::AtomicTripleBuffer<StabilizerFramePacket>& StabilizerPlayerController::tripleBuffer() noexcept
{
    return tripleBuffer_;
}

void StabilizerPlayerController::play()
{
    if (playbackState_ != PlaybackState::Playing) {
        setPlaybackState(PlaybackState::Playing);
        setStatusMessage(QStringLiteral("Playing"));
        QMetaObject::invokeMethod(worker_, &StabilizerWorker::play, Qt::QueuedConnection);
    }
}

void StabilizerPlayerController::pause()
{
    if (playbackState_ == PlaybackState::Playing) {
        setPlaybackState(PlaybackState::Paused);
        setStatusMessage(QStringLiteral("Paused"));
        QMetaObject::invokeMethod(worker_, &StabilizerWorker::pause, Qt::QueuedConnection);
    }
}

void StabilizerPlayerController::stop()
{
    setPlaybackState(PlaybackState::Idle);
    setStatusMessage(QStringLiteral("Stopped"));
    QMetaObject::invokeMethod(worker_, &StabilizerWorker::stop, Qt::QueuedConnection);
}

void StabilizerPlayerController::stepFrame()
{
    setPlaybackState(PlaybackState::Paused);
    setStatusMessage(QStringLiteral("Stepping frame"));
    QMetaObject::invokeMethod(worker_, &StabilizerWorker::stepFrame, Qt::QueuedConnection);
}

void StabilizerPlayerController::seek(double timestampSec)
{
    setStatusMessage(QStringLiteral("Seeking to %1 s").arg(timestampSec, 0, 'f', 2));
    QMetaObject::invokeMethod(worker_, "seek", Qt::QueuedConnection, Q_ARG(double, timestampSec));
}

void StabilizerPlayerController::setPlaybackRate(double rate)
{
    QMetaObject::invokeMethod(worker_, "setPlaybackRate", Qt::QueuedConnection, Q_ARG(double, rate));
}

QStringList StabilizerPlayerController::availableHardwareDevices() const
{
    QStringList result;
    const auto devices = Video::DeviceEnumerator::enumerateDevices();
    for (const auto& dev : devices) {
        result.append(QString::fromStdString(dev.name));
    }
    return result;
}

void StabilizerPlayerController::loadPreset(const QString& name)
{
    Stabilizer::VideoStabilizerConfig cfg { };
    if (presetManager_.getPreset(name, cfg)) {
        configModel_.loadFromConfig(cfg);
        setStatusMessage(QStringLiteral("Loaded preset: ") + name);
    }
}

bool StabilizerPlayerController::exportConfig(const QString& filePath)
{
    return PresetManager::exportJson(filePath, configModel_.toConfig());
}

bool StabilizerPlayerController::importConfig(const QString& filePath)
{
    Stabilizer::VideoStabilizerConfig cfg { };
    if (PresetManager::importJson(filePath, cfg)) {
        configModel_.loadFromConfig(cfg);
        setStatusMessage(QStringLiteral("Imported config from: ") + filePath);
        return true;
    }
    return false;
}

void StabilizerPlayerController::updateStabilizerConfig(const Stabilizer::VideoStabilizerConfig& config)
{
    QMetaObject::invokeMethod(
        worker_, "updateConfig", Qt::QueuedConnection, Q_ARG(Stabilizer::VideoStabilizerConfig, config));
}

} // namespace StabilizerApp
