/// @file StabilizerPlayerController.cpp
/// @brief Master QML controller bridging UI events with Video and Stabilizer subsystems.

#include "StabilizerPlayerController.h"

#include <cstdint>

namespace StabilizerApp {

StabilizerPlayerController::StabilizerPlayerController(QObject* parent)
    : QObject(parent)
{
}

StabilizerPlayerController::PlaybackState StabilizerPlayerController::playbackState() const noexcept
{
    return playbackState_;
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
    }
}

QString StabilizerPlayerController::statusMessage() const
{
    return statusMessage_;
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

void StabilizerPlayerController::play()
{
    if (playbackState_ != PlaybackState::Playing) {
        playbackState_ = PlaybackState::Playing;
        statusMessage_ = QStringLiteral("Playing");
        emit playbackStateChanged();
        emit statusMessageChanged();
    }
}

void StabilizerPlayerController::pause()
{
    if (playbackState_ == PlaybackState::Playing) {
        playbackState_ = PlaybackState::Paused;
        statusMessage_ = QStringLiteral("Paused");
        emit playbackStateChanged();
        emit statusMessageChanged();
    }
}

void StabilizerPlayerController::stop()
{
    if (playbackState_ != PlaybackState::Idle) {
        playbackState_ = PlaybackState::Idle;
        statusMessage_ = QStringLiteral("Stopped");
        emit playbackStateChanged();
        emit statusMessageChanged();
    }
}

} // namespace StabilizerApp
