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
    , m_telemetryBuffer(new LensTelemetryRingBuffer(this))
    , m_presetManager(new LensPresetManager(this))
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

bool LensPlayerController::zoomTrackEnabled() const noexcept
{
    return m_zoomTrackEnabled;
}

int LensPlayerController::interpolationMode() const noexcept
{
    return m_interpolationMode;
}

int LensPlayerController::activeCurveIndex() const noexcept
{
    return m_activeCurveIndex;
}

int LensPlayerController::curveCount() const noexcept
{
    return m_curveCount;
}

double LensPlayerController::activeCurveDistance() const noexcept
{
    return m_activeCurveDistance;
}

bool LensPlayerController::isCurveValid() const noexcept
{
    return m_isCurveValid;
}

QVariantList LensPlayerController::activeCurveKnots() const
{
    return m_activeCurveKnots;
}

bool LensPlayerController::isCalibrating() const noexcept
{
    return m_isCalibrating;
}

int LensPlayerController::calibStep() const noexcept
{
    return m_calibStep;
}

int LensPlayerController::calibTotalSteps() const noexcept
{
    return m_calibTotalSteps;
}

double LensPlayerController::calibProgress() const noexcept
{
    return m_calibProgress;
}

QString LensPlayerController::calibStatusText() const
{
    return m_calibStatusText;
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

LensTelemetryRingBuffer* LensPlayerController::telemetryBuffer() const noexcept
{
    return m_telemetryBuffer;
}

LensPresetManager* LensPlayerController::presetManager() const noexcept
{
    return m_presetManager;
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
        if (m_presetManager) {
            m_presetManager->setActiveChannel(m_activeChannel);
        }
        emit activeChannelChanged();
        emit requestSetChannel(m_activeChannel);

        emit driverTypeChanged();
        emit portBindingChanged();
        emit cameraAddressChanged();
        emit speedsChanged();
        emit requestActiveCurve(m_activeChannel, m_activeCurveIndex);
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

void LensPlayerController::setZoomTrackEnabled(bool enabled)
{
    if (m_zoomTrackEnabled != enabled) {
        m_zoomTrackEnabled = enabled;
        emit requestZoomTrack(m_activeChannel, enabled);
        emit zoomTrackChanged();
    }
}

void LensPlayerController::setInterpolationMode(int mode)
{
    if (m_interpolationMode != mode) {
        m_interpolationMode = mode;
        emit requestInterpMode(m_activeChannel, mode);
        emit interpolationModeChanged();
    }
}

void LensPlayerController::setActiveCurveIndex(int index)
{
    if (m_activeCurveIndex != index) {
        m_activeCurveIndex = index;
        emit requestActiveCurve(m_activeChannel, index);
        emit activeCurveChanged();
    }
}

void LensPlayerController::setActiveCurveDistance(double meters)
{
    m_activeCurveDistance = meters;
    emit requestCurveDist(m_activeChannel, m_activeCurveIndex, meters);
    emit activeCurveChanged();
}

void LensPlayerController::addKnot(int zoom, int focus)
{
    QVariantMap knot;
    knot[QStringLiteral("zoom")] = zoom;
    knot[QStringLiteral("focus")] = focus;
    knot[QStringLiteral("slope")] = 0.0;
    m_activeCurveKnots.append(knot);
    emit knotsUpdated();
    emit requestAddKnot(m_activeChannel, zoom, focus);
}

void LensPlayerController::updateKnot(int index, int zoom, int focus)
{
    if (index >= 0 && index < m_activeCurveKnots.size()) {
        QVariantMap knot = m_activeCurveKnots[index].toMap();
        knot[QStringLiteral("zoom")] = zoom;
        knot[QStringLiteral("focus")] = focus;
        m_activeCurveKnots[index] = knot;
        emit knotsUpdated();
    }
    emit requestUpdateKnot(m_activeChannel, index, zoom, focus);
}

void LensPlayerController::removeKnot(int index)
{
    if (index >= 0 && index < m_activeCurveKnots.size()) {
        m_activeCurveKnots.removeAt(index);
        emit knotsUpdated();
    }
    emit requestRemoveKnot(m_activeChannel, index);
}

void LensPlayerController::clearKnots()
{
    m_activeCurveKnots.clear();
    emit knotsUpdated();
    emit requestClearKnots(m_activeChannel);
}

void LensPlayerController::sortKnots()
{
    emit requestSortKnots(m_activeChannel);
}

void LensPlayerController::startCalibWizard(int knots, int zMin, int zMax, double dist)
{
    m_isCalibrating = true;
    m_calibStep = 1;
    m_calibTotalSteps = knots;
    m_calibProgress = 0.0;
    m_calibStatusText = QStringLiteral("Initializing wizard...");
    emit calibStateChanged();
    emit calibProgressChanged();
    emit requestStartCalib(m_activeChannel, knots, zMin, zMax, dist);
}

void LensPlayerController::cancelCalibWizard()
{
    emit requestCancelCalib(m_activeChannel);
    m_isCalibrating = false;
    m_calibStatusText = QStringLiteral("Cancelled");
    emit calibStateChanged();
    emit calibProgressChanged();
}

void LensPlayerController::loadTrackCurves(const QString& path)
{
    emit requestLoadTrack(m_activeChannel, path);
}

void LensPlayerController::saveTrackCurves(const QString& path)
{
    emit requestSaveTrack(m_activeChannel, path);
}

void LensPlayerController::exportJsonProfile(const QString& path)
{
    emit requestExportJson(m_activeChannel, path);
}

void LensPlayerController::importJsonProfile(const QString& path)
{
    emit requestImportJson(m_activeChannel, path);
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

void LensPlayerController::recallPreset(int id)
{
    if (!m_presetManager) {
        return;
    }
    LensPreset preset { };
    if (!m_presetManager->getPreset(id, preset)) {
        return;
    }

    setFocusSpeed(preset.focusSpeed);
    setZoomSpeed(preset.zoomSpeed);
    setFocusTarget(preset.focusPosition);
    setZoomTarget(preset.zoomPosition);
    setAfAlgorithm(preset.afAlgorithm);
    setFocusMetric(preset.focusMetric);

    m_statusMessage = QStringLiteral("Recalled Preset %1 ('%2')").arg(id).arg(preset.name);
    emit statusMessageChanged();
}

void LensPlayerController::saveCurrentAsPreset(int id, const QString& name)
{
    if (!m_presetManager) {
        return;
    }
    const auto chIdx = static_cast<std::size_t>(std::clamp(m_activeChannel, 0, 3));
    m_presetManager->savePreset(id, name, m_currentFocus, m_currentZoom, m_driverStates[chIdx].focusSpeed,
        m_driverStates[chIdx].zoomSpeed, m_afAlgorithm, m_focusMetric);
    m_statusMessage = QStringLiteral("Saved Preset %1 ('%2')").arg(id).arg(name);
    emit statusMessageChanged();
}

void LensPlayerController::deletePreset(int id)
{
    if (!m_presetManager) {
        return;
    }
    m_presetManager->deletePreset(id);
    m_statusMessage = QStringLiteral("Cleared Preset %1").arg(id);
    emit statusMessageChanged();
}

void LensPlayerController::resetPresetsToDefault()
{
    if (!m_presetManager) {
        return;
    }
    m_presetManager->resetFactoryDefaults();
    m_statusMessage = QStringLiteral("Restored factory default presets");
    emit statusMessageChanged();
}

void LensPlayerController::exportPresetsJson(const QString& path)
{
    if (!m_presetManager) {
        return;
    }
    const bool ok = m_presetManager->exportJson(path);
    m_statusMessage = ok ? QStringLiteral("Exported presets to: %1").arg(path)
                         : QStringLiteral("Failed to export presets to: %1").arg(path);
    emit statusMessageChanged();
}

void LensPlayerController::importPresetsJson(const QString& path)
{
    if (!m_presetManager) {
        return;
    }
    const bool ok = m_presetManager->importJson(path);
    m_statusMessage = ok ? QStringLiteral("Imported presets from: %1").arg(path)
                         : QStringLiteral("Failed to import presets from: %1").arg(path);
    emit statusMessageChanged();
}

void LensPlayerController::exportTelemetryCsv(const QString& path)
{
    if (!m_telemetryBuffer) {
        return;
    }
    const bool ok = m_telemetryBuffer->exportCsv(path);
    m_statusMessage = ok ? QStringLiteral("Exported telemetry CSV to: %1").arg(path)
                         : QStringLiteral("Failed to export telemetry CSV to: %1").arg(path);
    emit statusMessageChanged();
}

void LensPlayerController::clearTelemetryBuffer()
{
    if (m_telemetryBuffer) {
        m_telemetryBuffer->clear();
    }
    m_statusMessage = QStringLiteral("Telemetry buffer cleared");
    emit statusMessageChanged();
}

void LensPlayerController::onTelemetrySnapshotReady(const LensApp::TelemetrySample& sample)
{
    if (m_telemetryBuffer) {
        m_telemetryBuffer->appendSample(sample);
    }
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

void LensPlayerController::onZoomTrackState(int channel, bool enabled)
{
    if (m_activeChannel == channel && m_zoomTrackEnabled != enabled) {
        m_zoomTrackEnabled = enabled;
        emit zoomTrackChanged();
    }
}

void LensPlayerController::onCurveSetUpdated(int channel, int curveCount, int activeIndex, double dist, bool valid)
{
    if (m_activeChannel == channel) {
        m_curveCount = curveCount;
        m_activeCurveIndex = activeIndex;
        m_activeCurveDistance = dist;
        m_isCurveValid = valid;
        emit curveSetChanged();
        emit activeCurveChanged();
    }
}

void LensPlayerController::onKnotsReady(int channel, const QVariantList& knots)
{
    if (m_activeChannel == channel) {
        m_activeCurveKnots = knots;
        emit knotsUpdated();
    }
}

void LensPlayerController::onCalibProgress(int channel, int step, int total, double progress, const QString& status)
{
    if (m_activeChannel == channel) {
        m_isCalibrating = true;
        m_calibStep = step;
        m_calibTotalSteps = total;
        m_calibProgress = progress;
        m_calibStatusText = status;
        emit calibProgressChanged();
        emit calibStateChanged();
    }
}

void LensPlayerController::onCalibFinished(int channel, bool success, const QString& message)
{
    if (m_activeChannel == channel) {
        m_isCalibrating = false;
        m_calibProgress = success ? 1.0 : 0.0;
        m_calibStatusText = message;
        emit calibProgressChanged();
        emit calibStateChanged();
    }
}

void LensPlayerController::onCurveIoFinished(int channel, bool success, const QString& message)
{
    if (m_activeChannel == channel) {
        m_statusMessage = message;
        emit statusMessageChanged();
        emit curveIoResult(success, message);
    }
}

void LensPlayerController::setupWorkerThread()
{
    qRegisterMetaType<QVariantList>("QVariantList");
    qRegisterMetaType<LensApp::TelemetrySample>("LensApp::TelemetrySample");
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

    // Parfocal & Calibration wiring (Phase 5)
    connect(this, &LensPlayerController::requestZoomTrack, m_worker, &LensWorker::onSetZoomTrack);
    connect(this, &LensPlayerController::requestInterpMode, m_worker, &LensWorker::onSetInterpMode);
    connect(this, &LensPlayerController::requestActiveCurve, m_worker, &LensWorker::onSetActiveCurve);
    connect(this, &LensPlayerController::requestCurveDist, m_worker, &LensWorker::onSetCurveDistance);
    connect(this, &LensPlayerController::requestAddKnot, m_worker, &LensWorker::onAddKnot);
    connect(this, &LensPlayerController::requestUpdateKnot, m_worker, &LensWorker::onUpdateKnot);
    connect(this, &LensPlayerController::requestRemoveKnot, m_worker, &LensWorker::onRemoveKnot);
    connect(this, &LensPlayerController::requestClearKnots, m_worker, &LensWorker::onClearKnots);
    connect(this, &LensPlayerController::requestSortKnots, m_worker, &LensWorker::onSortKnots);
    connect(this, &LensPlayerController::requestStartCalib, m_worker, &LensWorker::onStartCalib);
    connect(this, &LensPlayerController::requestCancelCalib, m_worker, &LensWorker::onCancelCalib);
    connect(this, &LensPlayerController::requestLoadTrack, m_worker, &LensWorker::onLoadTrackFile);
    connect(this, &LensPlayerController::requestSaveTrack, m_worker, &LensWorker::onSaveTrackFile);
    connect(this, &LensPlayerController::requestImportJson, m_worker, &LensWorker::onImportJsonProfile);
    connect(this, &LensPlayerController::requestExportJson, m_worker, &LensWorker::onExportJsonProfile);

    // Feedback signals from worker
    connect(m_worker, &LensWorker::statusUpdated, this, &LensPlayerController::onStatusUpdated);
    connect(m_worker, &LensWorker::telemetryReady, this, &LensPlayerController::onTelemetryReady);
    connect(m_worker, &LensWorker::telemetrySampleReady, this, &LensPlayerController::onTelemetrySnapshotReady);
    connect(m_worker, &LensWorker::afStateUpdated, this, &LensPlayerController::onAfStateUpdated);
    connect(m_worker, &LensWorker::peakFitted, this, &LensPlayerController::onPeakFitted);
    connect(m_worker, &LensWorker::samplesReady, this, &LensPlayerController::onSamplesReady);
    connect(m_worker, &LensWorker::sourceOpened, this, &LensPlayerController::onSourceOpened);
    connect(m_worker, &LensWorker::playbackStateChanged, this, &LensPlayerController::onPlaybackStateChanged);
    connect(m_worker, &LensWorker::frameReady, this, &LensPlayerController::frameReady);
    connect(m_worker, &LensWorker::driverBound, this, &LensPlayerController::onDriverBound);

    connect(m_worker, &LensWorker::zoomTrackState, this, &LensPlayerController::onZoomTrackState);
    connect(m_worker, &LensWorker::curveSetUpdated, this, &LensPlayerController::onCurveSetUpdated);
    connect(m_worker, &LensWorker::knotsReady, this, &LensPlayerController::onKnotsReady);
    connect(m_worker, &LensWorker::calibProgress, this, &LensPlayerController::onCalibProgress);
    connect(m_worker, &LensWorker::calibFinished, this, &LensPlayerController::onCalibFinished);
    connect(m_worker, &LensWorker::curveIoFinished, this, &LensPlayerController::onCurveIoFinished);

    m_workerThread.start();
}

} // namespace LensApp
