/// @file LensPlayerController.cpp
/// @brief Master QML ViewModel bridging UI events with LensWorker and LensController subsystems.
#include "LensPlayerController.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>

#include <QMetaObject>
#include <QRectF>
#include <QString>
#include <QThread>

#include "AtomicTripleBuffer.h"
#include "LensConfigModel.h"
#include "LensFramePacket.h"
#include "LensWorker.h"
#include "SerialTransport.h"

namespace LensApp {

LensPlayerController::LensPlayerController(QObject* parent)
    : QObject(parent)
    , m_configModel(new LensConfigModel(this))
{
    setupWorkerThread();
    scanSerialPorts();
}

LensPlayerController::~LensPlayerController()
{
    if (m_workerThread.isRunning()) {
        QMetaObject::invokeMethod(m_worker, &LensWorker::stopWorker, Qt::BlockingQueuedConnection);
        m_workerThread.quit();
        m_workerThread.wait(1000);
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

double LensPlayerController::latestMetricScore() const noexcept
{
    return m_latestMetricScore;
}

QString LensPlayerController::sourceUri() const
{
    return m_sourceUri;
}

int LensPlayerController::playbackState() const noexcept
{
    return m_playbackState;
}

double LensPlayerController::currentFps() const noexcept
{
    return m_currentFps;
}

double LensPlayerController::currentPts() const noexcept
{
    return m_currentPts;
}

bool LensPlayerController::isSynthetic() const noexcept
{
    return m_isSynthetic;
}

int LensPlayerController::syntheticPlane() const noexcept
{
    return m_syntheticPlane;
}

double LensPlayerController::syntheticNoise() const noexcept
{
    return m_syntheticNoise;
}

QRectF LensPlayerController::roiRect() const noexcept
{
    return m_roiRect;
}

bool LensPlayerController::showPeaking() const noexcept
{
    return m_showPeaking;
}

bool LensPlayerController::showGrid() const noexcept
{
    return m_showGrid;
}

bool LensPlayerController::showReticle() const noexcept
{
    return m_showReticle;
}

bool LensPlayerController::showRoi() const noexcept
{
    return m_showRoi;
}

int LensPlayerController::activeDriverType() const noexcept
{
    return m_driverStates[static_cast<std::size_t>(m_activeChannel)].driverType;
}

QString LensPlayerController::driverName() const
{
    return m_driverStates[static_cast<std::size_t>(m_activeChannel)].driverName;
}

QString LensPlayerController::boundPortName() const
{
    return m_driverStates[static_cast<std::size_t>(m_activeChannel)].portName;
}

int LensPlayerController::boundBaudRate() const noexcept
{
    return m_driverStates[static_cast<std::size_t>(m_activeChannel)].baudRate;
}

int LensPlayerController::cameraAddress() const noexcept
{
    return m_driverStates[static_cast<std::size_t>(m_activeChannel)].cameraAddress;
}

QStringList LensPlayerController::availablePorts() const
{
    return m_availablePorts;
}

bool LensPlayerController::isPortConnected() const noexcept
{
    return m_driverStates[static_cast<std::size_t>(m_activeChannel)].isConnected;
}

int LensPlayerController::focusSpeed() const noexcept
{
    return m_driverStates[static_cast<std::size_t>(m_activeChannel)].focusSpeed;
}

int LensPlayerController::zoomSpeed() const noexcept
{
    return m_driverStates[static_cast<std::size_t>(m_activeChannel)].zoomSpeed;
}

LensConfigModel* LensPlayerController::configModel() const noexcept
{
    return m_configModel;
}

Video::AtomicTripleBuffer<LensFramePacket>& LensPlayerController::tripleBuffer() noexcept
{
    return m_tripleBuffer;
}

void LensPlayerController::setActiveChannel(int channel)
{
    const int clamped = std::clamp(channel, 0, 3);
    if (m_activeChannel != clamped) {
        m_activeChannel = clamped;
        emit activeChannelChanged();
        emit driverTypeChanged();
        emit portBindingChanged();
        emit cameraAddressChanged();
        emit speedsChanged();
        emit requestSetChannel(clamped);
    }
}

void LensPlayerController::setLensMode(int mode)
{
    if (m_lensMode != mode) {
        m_lensMode = mode;
        emit lensModeChanged();
        emit requestSetMode(m_activeChannel, mode);
    }
}

void LensPlayerController::setSourceUri(const QString& uri)
{
    if (m_sourceUri != uri) {
        m_sourceUri = uri;
        emit sourceUriChanged();
        openSource(uri);
    }
}

void LensPlayerController::openSource(const QString& uri)
{
    m_sourceUri = uri;
    emit sourceUriChanged();
    m_isSynthetic = (uri.startsWith(QStringLiteral("synthetic://")) || (uri == QStringLiteral("synthetic")));
    emit sourceModeChanged();
    emit requestOpenSource(uri);
}

void LensPlayerController::play()
{
    emit requestPlay();
}

void LensPlayerController::pause()
{
    emit requestPause();
}

void LensPlayerController::stop()
{
    emit requestStop();
}

void LensPlayerController::stepFrame()
{
    emit requestStepFrame();
}

void LensPlayerController::seek(double seconds)
{
    emit requestSeek(seconds);
}

void LensPlayerController::setPlaybackRate(double rate)
{
    emit requestRate(rate);
}

void LensPlayerController::setSyntheticPlane(int plane)
{
    const int clamped = std::clamp(plane, 0, 65535);
    if (m_syntheticPlane != clamped) {
        m_syntheticPlane = clamped;
        emit syntheticPlaneChanged();
        emit requestPlane(clamped);
    }
}

void LensPlayerController::setSyntheticNoise(double sigma)
{
    const double clamped = std::clamp(sigma, 0.0, 25.0);
    if (m_syntheticNoise != clamped) {
        m_syntheticNoise = clamped;
        emit syntheticNoiseChanged();
        emit requestNoise(clamped);
    }
}

void LensPlayerController::setSyntheticPattern(int pattern)
{
    emit requestPattern(pattern);
}

void LensPlayerController::setRoiRect(const QRectF& rect)
{
    if (m_roiRect != rect) {
        m_roiRect = rect;
        emit roiRectChanged();
        setRoi(rect.x(), rect.y(), rect.width(), rect.height());
    }
}

void LensPlayerController::setRoi(double x, double y, double w, double h)
{
    const double cx = std::clamp(x, 0.0, 0.95);
    const double cy = std::clamp(y, 0.0, 0.95);
    const double cw = std::clamp(w, 0.05, 1.0 - cx);
    const double ch = std::clamp(h, 0.05, 1.0 - cy);

    const QRectF newRect(cx, cy, cw, ch);
    if (m_roiRect != newRect) {
        m_roiRect = newRect;
        emit roiRectChanged();
    }
    emit requestRoi(cx, cy, cw, ch);
}

void LensPlayerController::setShowPeaking(bool show)
{
    if (m_showPeaking != show) {
        m_showPeaking = show;
        emit showPeakingChanged();
    }
}

void LensPlayerController::setShowGrid(bool show)
{
    if (m_showGrid != show) {
        m_showGrid = show;
        emit showGridChanged();
    }
}

void LensPlayerController::setShowReticle(bool show)
{
    if (m_showReticle != show) {
        m_showReticle = show;
        emit showReticleChanged();
    }
}

void LensPlayerController::setShowRoi(bool show)
{
    if (m_showRoi != show) {
        m_showRoi = show;
        emit showRoiChanged();
    }
}

void LensPlayerController::moveFocusStep(int steps)
{
    m_currentFocus = std::clamp(m_currentFocus + steps, m_focusMin, m_focusMax);
    emit telemetryUpdated();
    emit requestMoveFocus(m_activeChannel, steps);
}

void LensPlayerController::moveZoomStep(int steps)
{
    m_currentZoom = std::clamp(m_currentZoom + steps, m_zoomMin, m_zoomMax);
    emit telemetryUpdated();
    emit requestMoveZoom(m_activeChannel, steps);
}

void LensPlayerController::setFocusTarget(int position)
{
    m_currentFocus = std::clamp(position, m_focusMin, m_focusMax);
    emit telemetryUpdated();
    emit requestSetFocus(m_activeChannel, position);
}

void LensPlayerController::setZoomTarget(int position)
{
    m_currentZoom = std::clamp(position, m_zoomMin, m_zoomMax);
    emit telemetryUpdated();
    emit requestSetZoom(m_activeChannel, position);
}

void LensPlayerController::setFocusSpeed(int speed)
{
    m_driverStates[static_cast<std::size_t>(m_activeChannel)].focusSpeed = speed;
    emit speedsChanged();
    emit requestSetFocusSpeed(m_activeChannel, speed);
}

void LensPlayerController::setZoomSpeed(int speed)
{
    m_driverStates[static_cast<std::size_t>(m_activeChannel)].zoomSpeed = speed;
    emit speedsChanged();
    emit requestSetZoomSpeed(m_activeChannel, speed);
}

void LensPlayerController::stopMotion()
{
    emit requestStopMotion(m_activeChannel);
}

void LensPlayerController::emergencyStop()
{
    m_statusMessage = QStringLiteral("EMERGENCY STOP EXECUTED");
    emit statusMessageChanged();
    emit emergencyStopOccurred();
    emit requestEmergencyStop();
}

void LensPlayerController::setActiveDriverType(int type)
{
    const auto chIdx = static_cast<std::size_t>(m_activeChannel);
    if (m_driverStates[chIdx].driverType != type) {
        m_driverStates[chIdx].driverType = type;
        emit driverTypeChanged();
    }
}

void LensPlayerController::setCameraAddress(int address)
{
    const auto chIdx = static_cast<std::size_t>(m_activeChannel);
    if (m_driverStates[chIdx].cameraAddress != address) {
        m_driverStates[chIdx].cameraAddress = address;
        emit cameraAddressChanged();
    }
}

void LensPlayerController::scanSerialPorts()
{
    const auto ports = Transport::SerialTransport::enumeratePorts();
    m_availablePorts.clear();
    for (const auto& p : ports) {
        m_availablePorts.append(QString::fromStdString(p));
    }
    if (m_availablePorts.isEmpty()) {
#ifdef _WIN32
        m_availablePorts << QStringLiteral("COM1") << QStringLiteral("COM2") << QStringLiteral("COM3")
                         << QStringLiteral("COM4");
#else
        m_availablePorts << QStringLiteral("/dev/ttyUSB0") << QStringLiteral("/dev/ttyUSB1")
                         << QStringLiteral("/dev/ttyS0");
#endif
    }
    emit portsScanned();
}

void LensPlayerController::bindMockDriver()
{
    emit requestBindMock(m_activeChannel);
}

void LensPlayerController::bindPelcoDriver(int portIdx, int baud, int address)
{
    emit requestBindPelco(m_activeChannel, portIdx, baud, address);
}

void LensPlayerController::bindViscaDriver(int portIdx, int baud, int address)
{
    emit requestBindVisca(m_activeChannel, portIdx, baud, address);
}

void LensPlayerController::loadDriverPlugin(const QString& path, int portIdx, int baud)
{
    emit requestLoadDll(m_activeChannel, path, portIdx, baud);
}

void LensPlayerController::onDriverBound(
    int channel, int driverType, const QString& portName, int baud, int address, bool success, const QString& error)
{
    if (channel >= 0 && channel < 4) {
        auto& state = m_driverStates[static_cast<std::size_t>(channel)];
        state.driverType = driverType;
        switch (driverType) {
        case 0:
            state.driverName = QStringLiteral("MOCK");
            break;
        case 1:
            state.driverName = QStringLiteral("PELCO-D");
            break;
        case 2:
            state.driverName = QStringLiteral("VISCA");
            break;
        case 3:
            state.driverName = QStringLiteral("DYNAMIC DLL");
            break;
        default:
            state.driverName = QStringLiteral("UNKNOWN");
            break;
        }
        state.portName = portName;
        state.baudRate = baud;
        state.cameraAddress = address;
        state.isConnected = success;

        if (channel == m_activeChannel) {
            emit driverTypeChanged();
            emit portBindingChanged();
            emit cameraAddressChanged();
            if (!success && !error.isEmpty()) {
                m_statusMessage = QStringLiteral("Driver error: ") + error;
                emit statusMessageChanged();
            } else {
                m_statusMessage = state.driverName + QStringLiteral(" bound on ") + portName;
                emit statusMessageChanged();
            }
        }
    }
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

void LensPlayerController::onTelemetryReady(double metricScore, int channel)
{
    if (channel == m_activeChannel) {
        m_latestMetricScore = metricScore;
        emit telemetryUpdated();
    }
}

void LensPlayerController::onSourceOpened(int width, int height, double fps)
{
    m_currentFps = fps;
    emit telemetryUpdated();

    m_statusMessage = QStringLiteral("Opened: %1x%2 @ %3 fps").arg(width).arg(height).arg(fps, 0, 'f', 1);
    emit statusMessageChanged();
}

void LensPlayerController::onPlaybackStateChanged(int state)
{
    if (m_playbackState != state) {
        m_playbackState = state;
        emit playbackStateChanged();
    }
}

void LensPlayerController::setupWorkerThread()
{
    m_worker = new LensWorker(m_tripleBuffer);
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::started, m_worker, &LensWorker::initWorker);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Media & Ingestion wiring
    connect(this, &LensPlayerController::requestOpenSource, m_worker, &LensWorker::openSource);
    connect(this, &LensPlayerController::requestPlay, m_worker, &LensWorker::play);
    connect(this, &LensPlayerController::requestPause, m_worker, &LensWorker::pause);
    connect(this, &LensPlayerController::requestStop, m_worker, &LensWorker::stop);
    connect(this, &LensPlayerController::requestStepFrame, m_worker, &LensWorker::stepFrame);
    connect(this, &LensPlayerController::requestSeek, m_worker, &LensWorker::seek);
    connect(this, &LensPlayerController::requestRate, m_worker, &LensWorker::setPlaybackRate);

    // Synthetic Defocus & ROI wiring
    connect(this, &LensPlayerController::requestPlane, m_worker, &LensWorker::setSyntheticTarget);
    connect(this, &LensPlayerController::requestNoise, m_worker, &LensWorker::setSyntheticNoise);
    connect(this, &LensPlayerController::requestPattern, m_worker, &LensWorker::setSyntheticPattern);
    connect(this, &LensPlayerController::requestRoi, m_worker, &LensWorker::setRoi);

    // Axis Motion wiring
    connect(this, &LensPlayerController::requestSetChannel, m_worker, &LensWorker::onSetActiveChannel);
    connect(this, &LensPlayerController::requestSetMode, m_worker, &LensWorker::onSetLensMode);
    connect(this, &LensPlayerController::requestMoveFocus, m_worker, &LensWorker::onMoveFocusStep);
    connect(this, &LensPlayerController::requestMoveZoom, m_worker, &LensWorker::onMoveZoomStep);
    connect(this, &LensPlayerController::requestSetFocus, m_worker, &LensWorker::onSetFocusTarget);
    connect(this, &LensPlayerController::requestSetZoom, m_worker, &LensWorker::onSetZoomTarget);
    connect(this, &LensPlayerController::requestSetFocusSpeed, m_worker, &LensWorker::onSetFocusSpeed);
    connect(this, &LensPlayerController::requestSetZoomSpeed, m_worker, &LensWorker::onSetZoomSpeed);
    connect(this, &LensPlayerController::requestStopMotion, m_worker, &LensWorker::onStopMotion);
    connect(this, &LensPlayerController::requestEmergencyStop, m_worker, &LensWorker::onEmergencyStop);

    // Driver Binding wiring
    connect(this, &LensPlayerController::requestBindMock, m_worker, &LensWorker::onBindMock);
    connect(this, &LensPlayerController::requestBindPelco, m_worker, &LensWorker::onBindPelco);
    connect(this, &LensPlayerController::requestBindVisca, m_worker, &LensWorker::onBindVisca);
    connect(this, &LensPlayerController::requestLoadDll, m_worker, &LensWorker::onLoadDll);

    // Feedback signals from worker
    connect(m_worker, &LensWorker::statusUpdated, this, &LensPlayerController::onStatusUpdated);
    connect(m_worker, &LensWorker::telemetryReady, this, &LensPlayerController::onTelemetryReady);
    connect(m_worker, &LensWorker::sourceOpened, this, &LensPlayerController::onSourceOpened);
    connect(m_worker, &LensWorker::playbackStateChanged, this, &LensPlayerController::onPlaybackStateChanged);
    connect(m_worker, &LensWorker::frameReady, this, &LensPlayerController::frameReady);
    connect(m_worker, &LensWorker::driverBound, this, &LensPlayerController::onDriverBound);

    m_workerThread.start();
}

} // namespace LensApp
