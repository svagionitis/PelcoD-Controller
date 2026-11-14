/// @file LensPlayerController.cpp
/// @brief Master QML ViewModel bridging UI events with LensWorker and LensController subsystems.
#include "LensPlayerController.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cmath>

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

double LensPlayerController::latestMetricScore() const noexcept
{
    return m_latestMetricScore;
}

bool LensPlayerController::isAfRunning() const noexcept
{
    return m_isAfRunning;
}

int LensPlayerController::afState() const noexcept
{
    return m_afState;
}

QString LensPlayerController::afStateString() const
{
    switch (m_afState) {
    case 0:
        return QStringLiteral("IDLE");
    case 1:
        return QStringLiteral("INITIALIZING");
    case 2:
        return QStringLiteral("COARSE SWEEP");
    case 3:
        return QStringLiteral("MID SWEEP");
    case 4:
        return QStringLiteral("FINE SWEEP");
    case 5:
        return QStringLiteral("REVERSING");
    case 6:
        return QStringLiteral("SLEW TO PEAK");
    case 7:
        return QStringLiteral("PEAK CONFIRMED");
    case 8:
        return QStringLiteral("FAILED");
    default:
        return QStringLiteral("UNKNOWN");
    }
}

int LensPlayerController::afAlgorithm() const noexcept
{
    return m_afAlgorithm;
}

int LensPlayerController::focusMetric() const noexcept
{
    return m_focusMetric;
}

bool LensPlayerController::cafEnabled() const noexcept
{
    return m_cafEnabled;
}

double LensPlayerController::cafThreshold() const noexcept
{
    return m_cafThreshold;
}

double LensPlayerController::peakSummit() const noexcept
{
    return m_peakSummit;
}

double LensPlayerController::peakScore() const noexcept
{
    return m_peakScore;
}

double LensPlayerController::peakFwhm() const noexcept
{
    return m_peakFwhm;
}

double LensPlayerController::peakCurvature() const noexcept
{
    return m_peakCurvature;
}

double LensPlayerController::peakR2() const noexcept
{
    return m_peakR2;
}

bool LensPlayerController::isPeakValid() const noexcept
{
    return m_isPeakValid;
}

QVariantList LensPlayerController::samplePoints() const
{
    return m_samplePoints;
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

int LensPlayerController::peakingThreshold() const noexcept
{
    return m_peakingThreshold;
}

int LensPlayerController::peakingColorIndex() const noexcept
{
    return m_peakingColorIndex;
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

bool LensPlayerController::isRoiLockedToTrack() const noexcept
{
    return m_isRoiLockedToTrack;
}

int LensPlayerController::activeDriverType() const noexcept
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    return m_driverStates[idx].driverType;
}

QString LensPlayerController::driverName() const
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    return m_driverStates[idx].driverName;
}

QString LensPlayerController::boundPortName() const
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    return m_driverStates[idx].portName;
}

int LensPlayerController::boundBaudRate() const noexcept
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    return m_driverStates[idx].baudRate;
}

int LensPlayerController::cameraAddress() const noexcept
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    return m_driverStates[idx].cameraAddress;
}

QStringList LensPlayerController::availablePorts() const
{
    return m_availablePorts;
}

bool LensPlayerController::isPortConnected() const noexcept
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    return m_driverStates[idx].isConnected;
}

int LensPlayerController::focusSpeed() const noexcept
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    return m_driverStates[idx].focusSpeed;
}

int LensPlayerController::zoomSpeed() const noexcept
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    return m_driverStates[idx].zoomSpeed;
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
        emit requestSetChannel(m_activeChannel);

        emit driverTypeChanged();
        emit portBindingChanged();
        emit cameraAddressChanged();
        emit speedsChanged();
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

    m_isSynthetic = uri.startsWith(QStringLiteral("synthetic://"));
    emit sourceModeChanged();

    m_playbackState = 1; // Opening
    emit playbackStateChanged();

    m_statusMessage = QStringLiteral("Opening: %1").arg(uri);
    emit statusMessageChanged();

    emit requestOpenSource(uri);
}

void LensPlayerController::play()
{
    m_playbackState = 2; // Playing
    emit playbackStateChanged();
    emit requestPlay();
}

void LensPlayerController::pause()
{
    m_playbackState = 3; // Paused
    emit playbackStateChanged();
    emit requestPause();
}

void LensPlayerController::stop()
{
    m_playbackState = 0; // Idle
    emit playbackStateChanged();
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
        emit requestPlane(m_syntheticPlane);
    }
}

void LensPlayerController::setSyntheticNoise(double sigma)
{
    if (std::abs(m_syntheticNoise - sigma) > 0.001) {
        m_syntheticNoise = sigma;
        emit syntheticNoiseChanged();
        emit requestNoise(m_syntheticNoise);
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
    }
}

void LensPlayerController::setRoi(double x, double y, double w, double h)
{
    setRoiRect(QRectF(x, y, w, h));
    emit requestRoi(x, y, w, h);
}

void LensPlayerController::setShowPeaking(bool show)
{
    if (m_showPeaking != show) {
        m_showPeaking = show;
        emit showPeakingChanged();
    }
}

void LensPlayerController::setPeakingThreshold(int thresh)
{
    const int clamped = std::clamp(thresh, 10, 300);
    if (m_peakingThreshold != clamped) {
        m_peakingThreshold = clamped;
        emit peakingThresholdChanged();
    }
}

void LensPlayerController::setPeakingColorIndex(int idx)
{
    const int clamped = std::clamp(idx, 0, 2);
    if (m_peakingColorIndex != clamped) {
        m_peakingColorIndex = clamped;
        emit peakingColorChanged();
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

void LensPlayerController::setIsRoiLockedToTrack(bool locked)
{
    if (m_isRoiLockedToTrack != locked) {
        m_isRoiLockedToTrack = locked;
        emit trackerLockChanged();
    }
}

void LensPlayerController::applyRoiPreset(int presetIdx)
{
    QRectF target;
    switch (presetIdx) {
    case 0:
        target = QRectF(0.0, 0.0, 1.0, 1.0);
        break;
    case 1:
        target = QRectF(0.25, 0.25, 0.50, 0.50);
        break;
    case 2:
        target = QRectF(0.375, 0.375, 0.25, 0.25);
        break;
    case 3:
        target = QRectF(0.10, 0.60, 0.80, 0.30);
        break;
    case 4:
        target = QRectF(0.10, 0.10, 0.80, 0.30);
        break;
    default:
        target = QRectF(0.25, 0.25, 0.50, 0.50);
        break;
    }

    setRoiRect(target);
    setRoi(target.x(), target.y(), target.width(), target.height());
}

void LensPlayerController::setTrackerTarget(double nx, double ny, double nw, double nh)
{
    if (m_isRoiLockedToTrack) {
        constexpr double kMargin = 0.20;
        const double cx = std::clamp(nx - (kMargin * nw), 0.0, 0.95);
        const double cy = std::clamp(ny - (kMargin * nh), 0.0, 0.95);
        const double cw = std::clamp(nw * (1.0 + (2.0 * kMargin)), 0.05, 1.0 - cx);
        const double ch = std::clamp(nh * (1.0 + (2.0 * kMargin)), 0.05, 1.0 - cy);
        setRoiRect(QRectF(cx, cy, cw, ch));
        setRoi(cx, cy, cw, ch);
    }
    emit requestTrackerRoi(m_activeChannel, nx, ny, nw, nh);
}

void LensPlayerController::moveFocusStep(int steps)
{
    if (steps == 0) {
        return;
    }
    const int newPos = std::clamp(m_currentFocus + steps, m_focusMin, m_focusMax);
    m_currentFocus = newPos;
    emit telemetryUpdated();
    emit requestMoveFocus(m_activeChannel, steps);
}

void LensPlayerController::moveZoomStep(int steps)
{
    if (steps == 0) {
        return;
    }
    const int newPos = std::clamp(m_currentZoom + steps, m_zoomMin, m_zoomMax);
    m_currentZoom = newPos;
    emit telemetryUpdated();
    emit requestMoveZoom(m_activeChannel, steps);
}

void LensPlayerController::setFocusTarget(int position)
{
    const int clamped = std::clamp(position, m_focusMin, m_focusMax);
    m_currentFocus = clamped;
    emit telemetryUpdated();
    emit requestSetFocus(m_activeChannel, clamped);
}

void LensPlayerController::setZoomTarget(int position)
{
    const int clamped = std::clamp(position, m_zoomMin, m_zoomMax);
    m_currentZoom = clamped;
    emit telemetryUpdated();
    emit requestSetZoom(m_activeChannel, clamped);
}

void LensPlayerController::setFocusSpeed(int speed)
{
    const int clamped = std::clamp(speed, 0, 255);
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    m_driverStates[idx].focusSpeed = clamped;
    emit speedsChanged();
    emit requestSetFocusSpeed(m_activeChannel, clamped);
}

void LensPlayerController::setZoomSpeed(int speed)
{
    const int clamped = std::clamp(speed, 0, 255);
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    m_driverStates[idx].zoomSpeed = clamped;
    emit speedsChanged();
    emit requestSetZoomSpeed(m_activeChannel, clamped);
}

void LensPlayerController::stopMotion()
{
    emit requestStopMotion(m_activeChannel);
}

void LensPlayerController::emergencyStop()
{
    emit requestEmergencyStop();
    m_statusMessage = QStringLiteral("EMERGENCY STOP ACTIVATED");
    emit statusMessageChanged();
    emit emergencyStopOccurred();
}

void LensPlayerController::triggerAutoFocus()
{
    emit requestTriggerAf(m_activeChannel, m_afAlgorithm, m_focusMetric);
    m_statusMessage = QStringLiteral("Autofocus triggered");
    emit statusMessageChanged();
}

void LensPlayerController::abortAutoFocus()
{
    emit requestAbortAf(m_activeChannel);
    m_statusMessage = QStringLiteral("Autofocus aborted");
    emit statusMessageChanged();
}

void LensPlayerController::setAfAlgorithm(int algorithm)
{
    if (m_afAlgorithm != algorithm) {
        m_afAlgorithm = algorithm;
        emit afConfigChanged();
    }
}

void LensPlayerController::setFocusMetric(int metric)
{
    if (m_focusMetric != metric) {
        m_focusMetric = metric;
        emit afConfigChanged();
        emit requestFocusMetric(m_activeChannel, metric);
    }
}

void LensPlayerController::setCafEnabled(bool enabled)
{
    if (m_cafEnabled != enabled) {
        m_cafEnabled = enabled;
        emit cafToggled();
        emit requestCaf(m_activeChannel, enabled, m_cafThreshold);
    }
}

void LensPlayerController::setCafThreshold(double threshold)
{
    const double clamped = std::clamp(threshold, 0.05, 0.50);
    if (std::abs(m_cafThreshold - clamped) > 0.001) {
        m_cafThreshold = clamped;
        emit cafConfigChanged();
        if (m_cafEnabled) {
            emit requestCaf(m_activeChannel, true, m_cafThreshold);
        }
    }
}

void LensPlayerController::setActiveDriverType(int type)
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    m_driverStates[idx].driverType = type;
    switch (type) {
    case 0:
        m_driverStates[idx].driverName = QStringLiteral("MOCK (IN-MEMORY)");
        bindMockDriver();
        break;
    case 1:
        m_driverStates[idx].driverName = QStringLiteral("PELCO-D");
        break;
    case 2:
        m_driverStates[idx].driverName = QStringLiteral("VISCA");
        break;
    case 3:
        m_driverStates[idx].driverName = QStringLiteral("DYNAMIC PLUGIN");
        break;
    default:
        break;
    }
    emit driverTypeChanged();
}

void LensPlayerController::setCameraAddress(int address)
{
    const int clamped = std::clamp(address, 1, 255);
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    m_driverStates[idx].cameraAddress = clamped;
    emit cameraAddressChanged();
}

void LensPlayerController::scanSerialPorts()
{
    m_availablePorts.clear();
#ifdef _WIN32
    for (int i = 1; i <= 32; ++i) {
        m_availablePorts.append(QStringLiteral("COM%1").arg(i));
    }
#else
    for (int i = 0; i < 8; ++i) {
        m_availablePorts.append(QStringLiteral("/dev/ttyUSB%1").arg(i));
    }
#endif
    emit portsScanned();
}

void LensPlayerController::bindMockDriver()
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    m_driverStates[idx].driverType = 0;
    m_driverStates[idx].driverName = QStringLiteral("MOCK");
    m_driverStates[idx].portName = QStringLiteral("IN-MEMORY");
    m_driverStates[idx].isConnected = true;
    emit driverTypeChanged();
    emit portBindingChanged();
    emit requestBindMock(m_activeChannel);
}

void LensPlayerController::bindPelcoDriver(int portIdx, int baud, int address)
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    m_driverStates[idx].driverType = 1;
    m_driverStates[idx].driverName = QStringLiteral("PELCO-D");
    m_driverStates[idx].baudRate = baud;
    m_driverStates[idx].cameraAddress = address;

    if (portIdx >= 0 && portIdx < m_availablePorts.size()) {
        m_driverStates[idx].portName = m_availablePorts.at(portIdx);
    }
    emit driverTypeChanged();
    emit portBindingChanged();
    emit cameraAddressChanged();
    emit requestBindPelco(m_activeChannel, portIdx, baud, address);
}

void LensPlayerController::bindViscaDriver(int portIdx, int baud, int address)
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    m_driverStates[idx].driverType = 2;
    m_driverStates[idx].driverName = QStringLiteral("VISCA");
    m_driverStates[idx].baudRate = baud;
    m_driverStates[idx].cameraAddress = address;

    if (portIdx >= 0 && portIdx < m_availablePorts.size()) {
        m_driverStates[idx].portName = m_availablePorts.at(portIdx);
    }
    emit driverTypeChanged();
    emit portBindingChanged();
    emit cameraAddressChanged();
    emit requestBindVisca(m_activeChannel, portIdx, baud, address);
}

void LensPlayerController::loadDriverPlugin(const QString& path, int portIdx, int baud)
{
    const auto idx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    m_driverStates[idx].driverType = 3;
    m_driverStates[idx].driverName = QStringLiteral("PLUGIN: %1").arg(path);
    m_driverStates[idx].baudRate = baud;

    if (portIdx >= 0 && portIdx < m_availablePorts.size()) {
        m_driverStates[idx].portName = m_availablePorts.at(portIdx);
    }
    emit driverTypeChanged();
    emit portBindingChanged();
    emit requestLoadDll(m_activeChannel, path, portIdx, baud);
}

void LensPlayerController::onDriverBound(
    int channel, int driverType, const QString& portName, int baud, int address, bool success, const QString& error)
{
    if (channel < 0 || channel >= 4) {
        return;
    }
    const auto idx = static_cast<std::size_t>(channel);
    m_driverStates[idx].driverType = driverType;
    m_driverStates[idx].portName = portName;
    m_driverStates[idx].baudRate = baud;
    m_driverStates[idx].cameraAddress = address;
    m_driverStates[idx].isConnected = success;

    if (channel == m_activeChannel) {
        emit driverTypeChanged();
        emit portBindingChanged();
        emit cameraAddressChanged();
    }

    if (!success && !error.isEmpty()) {
        m_statusMessage = QStringLiteral("Driver Error (Ch %1): %2").arg(channel).arg(error);
    } else {
        m_statusMessage = QStringLiteral("Channel %1 Bound to %2").arg(channel).arg(portName);
    }
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

void LensPlayerController::onAfStateUpdated(int state, bool isRunning)
{
    bool changed = false;
    if (m_afState != state) {
        m_afState = state;
        changed = true;
    }
    if (m_isAfRunning != isRunning) {
        m_isAfRunning = isRunning;
        changed = true;
    }
    if (changed) {
        emit afStateChanged();
    }
}

void LensPlayerController::onPeakFitted(double vertex, double score, double curvature, double r2, bool valid)
{
    m_peakSummit = vertex;
    m_peakScore = score;
    m_peakCurvature = curvature;
    m_peakR2 = r2;
    m_isPeakValid = valid;
    emit peakFitUpdated();
}

void LensPlayerController::onSamplesReady(const QVariantList& samples)
{
    m_samplePoints = samples;
    emit samplesUpdated();
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

    // Autofocus wiring (Phase 4)
    connect(this, &LensPlayerController::requestTriggerAf, m_worker, &LensWorker::onTriggerAf);
    connect(this, &LensPlayerController::requestAbortAf, m_worker, &LensWorker::onAbortAf);
    connect(this, &LensPlayerController::requestCaf, m_worker, &LensWorker::onSetCaf);
    connect(this, &LensPlayerController::requestTrackerRoi, m_worker, &LensWorker::onSetTrackerRoi);
    connect(this, &LensPlayerController::requestFocusMetric, m_worker, &LensWorker::onSetFocusMetric);

    // Driver Binding wiring
    connect(this, &LensPlayerController::requestBindMock, m_worker, &LensWorker::onBindMock);
    connect(this, &LensPlayerController::requestBindPelco, m_worker, &LensWorker::onBindPelco);
    connect(this, &LensPlayerController::requestBindVisca, m_worker, &LensWorker::onBindVisca);
    connect(this, &LensPlayerController::requestLoadDll, m_worker, &LensWorker::onLoadDll);

    // Feedback signals from worker
    connect(m_worker, &LensWorker::statusUpdated, this, &LensPlayerController::onStatusUpdated);
    connect(m_worker, &LensWorker::telemetryReady, this, &LensPlayerController::onTelemetryReady);
    connect(m_worker, &LensWorker::afStateUpdated, this, &LensPlayerController::onAfStateUpdated);
    connect(m_worker, &LensWorker::peakFitted, this, &LensPlayerController::onPeakFitted);
    connect(m_worker, &LensWorker::samplesReady, this, &LensPlayerController::onSamplesReady);
    connect(m_worker, &LensWorker::sourceOpened, this, &LensPlayerController::onSourceOpened);
    connect(m_worker, &LensWorker::playbackStateChanged, this, &LensPlayerController::onPlaybackStateChanged);
    connect(m_worker, &LensWorker::frameReady, this, &LensPlayerController::frameReady);
    connect(m_worker, &LensWorker::driverBound, this, &LensPlayerController::onDriverBound);

    m_workerThread.start();
}

} // namespace LensApp
