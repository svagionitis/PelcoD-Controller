/// @file LensPlayerController.cpp
#include "LensPlayerController.h"

#include <cstdint>

#include <algorithm>

#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QThread>

#include "LensConfigModel.h"
#include "LensWorker.h"

namespace LensApp {

LensPlayerController::LensPlayerController(QObject* parent)
    : QObject(parent)
    , m_configModel(new LensConfigModel(this))
{
    setupWorkerThread();
}

LensPlayerController::~LensPlayerController()
{
    if (m_workerThread.isRunning()) {
        QMetaObject::invokeMethod(m_worker, &LensWorker::stopWorker, Qt::BlockingQueuedConnection);
        m_workerThread.quit();
        m_workerThread.wait();
    }
}

int LensPlayerController::activeChannel() const noexcept
{
    return m_activeChannel;
}

int LensPlayerController::lensMode() const noexcept
{
    return m_lensMode;
}

QString LensPlayerController::statusMessage() const
{
    return m_statusMessage;
}

int LensPlayerController::currentFocus() const noexcept
{
    return m_currentFocus;
}

int LensPlayerController::currentZoom() const noexcept
{
    return m_currentZoom;
}

int LensPlayerController::focusMin() const noexcept
{
    return m_focusMin;
}

int LensPlayerController::focusMax() const noexcept
{
    return m_focusMax;
}

int LensPlayerController::zoomMin() const noexcept
{
    return m_zoomMin;
}

int LensPlayerController::zoomMax() const noexcept
{
    return m_zoomMax;
}

int LensPlayerController::limitFlags() const noexcept
{
    return m_limitFlags;
}

bool LensPlayerController::isAfRunning() const noexcept
{
    return m_isAfRunning;
}

LensConfigModel* LensPlayerController::configModel() const noexcept
{
    return m_configModel;
}

void LensPlayerController::setActiveChannel(int channel)
{
    const int clamped = std::clamp(channel, 0, 3);
    if (m_activeChannel != clamped) {
        m_activeChannel = clamped;
        emit activeChannelChanged();
        emit requestSetChannel(m_activeChannel);
    }
}

void LensPlayerController::setLensMode(int mode)
{
    const int clamped = std::clamp(mode, 0, 5);
    if (m_lensMode != clamped) {
        m_lensMode = clamped;
        emit lensModeChanged();
        emit requestSetMode(m_activeChannel, m_lensMode);
    }
}

void LensPlayerController::moveFocusStep(int steps)
{
    emit requestMoveFocus(m_activeChannel, steps);
}

void LensPlayerController::moveZoomStep(int steps)
{
    emit requestMoveZoom(m_activeChannel, steps);
}

void LensPlayerController::setFocusTarget(int position)
{
    const int clamped = std::clamp(position, m_focusMin, m_focusMax);
    emit requestSetFocus(m_activeChannel, clamped);
}

void LensPlayerController::setZoomTarget(int position)
{
    const int clamped = std::clamp(position, m_zoomMin, m_zoomMax);
    emit requestSetZoom(m_activeChannel, clamped);
}

void LensPlayerController::setFocusSpeed(int speed)
{
    const int clamped = std::clamp(speed, 0, 255);
    emit requestSetFocusSpeed(m_activeChannel, clamped);
}

void LensPlayerController::setZoomSpeed(int speed)
{
    const int clamped = std::clamp(speed, 0, 255);
    emit requestSetZoomSpeed(m_activeChannel, clamped);
}

void LensPlayerController::stopMotion()
{
    emit requestStopMotion(m_activeChannel);
}

void LensPlayerController::emergencyStop()
{
    emit requestEmergencyStop();
    m_statusMessage = QStringLiteral("EMERGENCY STOP EXECUTED");
    emit statusMessageChanged();
}

void LensPlayerController::onStatusUpdated(int focus, int zoom, int limits, int mode, bool afActive)
{
    bool changed = false;

    if (m_currentFocus != focus) {
        m_currentFocus = focus;
        changed = true;
    }
    if (m_currentZoom != zoom) {
        m_currentZoom = zoom;
        changed = true;
    }
    if (m_limitFlags != limits) {
        m_limitFlags = limits;
        changed = true;
    }
    if (m_lensMode != mode) {
        m_lensMode = mode;
        emit lensModeChanged();
    }
    if (m_isAfRunning != afActive) {
        m_isAfRunning = afActive;
        emit afStateChanged();
    }

    if (changed) {
        emit telemetryUpdated();
    }
}

void LensPlayerController::setupWorkerThread()
{
    m_worker = new LensWorker();
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(this, &LensPlayerController::requestMoveFocus, m_worker, &LensWorker::onMoveFocusStep);
    connect(this, &LensPlayerController::requestMoveZoom, m_worker, &LensWorker::onMoveZoomStep);
    connect(this, &LensPlayerController::requestSetFocus, m_worker, &LensWorker::onSetFocusTarget);
    connect(this, &LensPlayerController::requestSetZoom, m_worker, &LensWorker::onSetZoomTarget);
    connect(this, &LensPlayerController::requestSetFocusSpeed, m_worker, &LensWorker::onSetFocusSpeed);
    connect(this, &LensPlayerController::requestSetZoomSpeed, m_worker, &LensWorker::onSetZoomSpeed);
    connect(this, &LensPlayerController::requestStopMotion, m_worker, &LensWorker::onStopMotion);
    connect(this, &LensPlayerController::requestEmergencyStop, m_worker, &LensWorker::onEmergencyStop);
    connect(this, &LensPlayerController::requestSetChannel, m_worker, &LensWorker::onSetActiveChannel);
    connect(this, &LensPlayerController::requestSetMode, m_worker, &LensWorker::onSetLensMode);

    connect(m_worker, &LensWorker::statusUpdated, this, &LensPlayerController::onStatusUpdated);

    m_workerThread.start();
    QMetaObject::invokeMethod(m_worker, &LensWorker::initWorker);
}

} // namespace LensApp
