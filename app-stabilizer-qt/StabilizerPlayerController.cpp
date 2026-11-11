/// @file StabilizerPlayerController.cpp
/// @brief Master QML controller bridging UI events with Video and Stabilizer subsystems.
#include "StabilizerPlayerController.h"

#include <cstdint>

#include <QMetaObject>
#include <QString>
#include <QThread>

#include "AtomicTripleBuffer.h"
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
        [this](const Stabilizer::StabilizerDiagnostics& /*diag*/, double measuredFps) {
            fps_ = measuredFps;
            emit telemetryUpdated();
        });

    connect(worker_, &StabilizerWorker::endOfStreamReached, this, [this]() {
        setStatusMessage(QStringLiteral("End of stream reached"));
        setPlaybackState(PlaybackState::Paused);
    });

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

void StabilizerPlayerController::updateStabilizerConfig(const Stabilizer::VideoStabilizerConfig& config)
{
    QMetaObject::invokeMethod(
        worker_, "updateConfig", Qt::QueuedConnection, Q_ARG(Stabilizer::VideoStabilizerConfig, config));
}

} // namespace StabilizerApp
