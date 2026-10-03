/// @file SightlineQmlBridge.cpp
/// @brief Implementation of Sightline QML Bridge controller.

#include "SightlineQmlBridge.h"

#include <SightlineCore/SightlineCrc8.h>
#include <SightlineCore/SightlineProtocolParser.h>
#include <SightlineCore/modules/SightlineCompression.h>
#include <SightlineCore/modules/SightlineGeneral.h>
#include <SightlineCore/modules/SightlineNetwork.h>
#include <SightlineCore/modules/SightlineOverlay.h>
#include <SightlineCore/modules/SightlineStabilization.h>
#include <SightlineCore/modules/SightlineStabilizationBuilder.h>

#include <QDateTime>
#include <QFile>
#include <QHostAddress>
#include <QSettings>
#include <algorithm>

SightlineQmlBridge::SightlineQmlBridge(QObject* parent)
    : QObject(parent)
    , m_coolerTimer(std::make_unique<QTimer>(this))
    , m_recordingClockTimer(std::make_unique<QTimer>(this))
    , m_trackListModel(std::make_unique<TrackListModel>(this))
    , m_trafficLogModel(std::make_unique<TrafficLogModel>(this))
    , m_recordingFileListModel(std::make_unique<RecordingFileListModel>(this))
{
    connect(m_coolerTimer.get(), &QTimer::timeout, this, &SightlineQmlBridge::onCoolerTimerTick);
    connect(m_recordingClockTimer.get(), &QTimer::timeout, this, &SightlineQmlBridge::onRecordingClockTick);
}

SightlineQmlBridge::~SightlineQmlBridge()
{
    disconnectDevice();
}

bool SightlineQmlBridge::isConnected() const noexcept
{
    return m_device && m_device->isConnected();
}

QString SightlineQmlBridge::host() const
{
    return m_host;
}

void SightlineQmlBridge::setHost(const QString& host)
{
    if (m_host != host) {
        m_host = host;
        emit hostChanged();
    }
}

int SightlineQmlBridge::commandPort() const noexcept
{
    return m_commandPort;
}

void SightlineQmlBridge::setCommandPort(int port)
{
    if (m_commandPort != port) {
        m_commandPort = port;
        emit commandPortChanged();
    }
}

int SightlineQmlBridge::replyPort() const noexcept
{
    return m_replyPort;
}

void SightlineQmlBridge::setReplyPort(int port)
{
    if (m_replyPort != port) {
        m_replyPort = port;
        emit replyPortChanged();
    }
}

int SightlineQmlBridge::cpuLoadPercent() const noexcept
{
    return m_cpuLoadPercent;
}

int SightlineQmlBridge::coreTempC() const noexcept
{
    return m_coreTempC;
}

int SightlineQmlBridge::uptimeSeconds() const noexcept
{
    return m_uptimeSeconds;
}

QString SightlineQmlBridge::lastWarningMessage() const
{
    return m_lastWarningMessage;
}

QString SightlineQmlBridge::softwareVersion() const
{
    return m_softwareVersion;
}

TrackListModel* SightlineQmlBridge::trackListModel() const noexcept
{
    return m_trackListModel.get();
}

TrafficLogModel* SightlineQmlBridge::trafficLogModel() const noexcept
{
    return m_trafficLogModel.get();
}

int SightlineQmlBridge::activeContrastMode() const noexcept
{
    return static_cast<int>(m_cachedEnhancement[0].mode);
}

int SightlineQmlBridge::activePaletteIndex() const noexcept
{
    return m_activePaletteIndex[0];
}

QRect SightlineQmlBridge::enhancementRoi() const noexcept
{
    return m_cachedRoi[0];
}

QVariantList SightlineQmlBridge::activeOverlayIds() const
{
    return m_activeOverlayIds;
}

bool SightlineQmlBridge::isCoolerCountdownActive() const noexcept
{
    return m_coolerTimer && m_coolerTimer->isActive();
}

int SightlineQmlBridge::coolerCountdownRemaining() const noexcept
{
    return m_coolerRemaining;
}

bool SightlineQmlBridge::isRecordingActive() const noexcept
{
    return m_isRecordingActive;
}

int SightlineQmlBridge::freeStorageMB() const noexcept
{
    return m_freeStorageMB;
}

int SightlineQmlBridge::usedStorageMB() const noexcept
{
    return m_usedStorageMB;
}

double SightlineQmlBridge::storageUsagePercent() const noexcept
{
    return m_storageUsagePercent;
}

int SightlineQmlBridge::currentBitrateKbps() const noexcept
{
    return m_currentBitrateKbps;
}

int SightlineQmlBridge::droppedFrames() const noexcept
{
    return m_droppedFrames;
}

int SightlineQmlBridge::elapsedRecordingSec() const noexcept
{
    return m_elapsedRecordingSec;
}

QString SightlineQmlBridge::currentFilename() const
{
    return m_currentFilename;
}

QString SightlineQmlBridge::lastRecordingEvent() const
{
    return m_lastRecordingEvent;
}

QString SightlineQmlBridge::lastAckStatus() const
{
    return m_lastAckStatus;
}

RecordingFileListModel* SightlineQmlBridge::recordingFileListModel() const noexcept
{
    return m_recordingFileListModel.get();
}

int SightlineQmlBridge::encBitrateKbps() const noexcept
{
    return m_encBitrateKbps;
}

int SightlineQmlBridge::encGopInterval() const noexcept
{
    return m_encGopInterval;
}

int SightlineQmlBridge::encProfile() const noexcept
{
    return m_encProfile;
}

int SightlineQmlBridge::encRateControl() const noexcept
{
    return m_encRateControl;
}

int SightlineQmlBridge::encMinQp() const noexcept
{
    return m_encMinQp;
}

int SightlineQmlBridge::encMaxQp() const noexcept
{
    return m_encMaxQp;
}

int SightlineQmlBridge::encAirMb() const noexcept
{
    return m_encAirMb;
}

int SightlineQmlBridge::encSliceRows() const noexcept
{
    return m_encSliceRows;
}

int SightlineQmlBridge::netDisplayProtocol() const noexcept
{
    return m_netDisplayProtocol;
}

QString SightlineQmlBridge::netDisplayIp() const
{
    return m_netDisplayIp;
}

int SightlineQmlBridge::netDisplayPort() const noexcept
{
    return m_netDisplayPort;
}

int SightlineQmlBridge::netMaxPacket() const noexcept
{
    return m_netMaxPacket;
}

int SightlineQmlBridge::tcRateKbps() const noexcept
{
    return m_tcRateKbps;
}

int SightlineQmlBridge::tcBurstBytes() const noexcept
{
    return m_tcBurstBytes;
}

int SightlineQmlBridge::tcMtuBytes() const noexcept
{
    return m_tcMtuBytes;
}

bool SightlineQmlBridge::connectUdp(const QString& host, int cmdPort, int replyPort)
{
    const bool hostChangedVal { m_host != host };
    const bool cmdPortChangedVal { m_commandPort != cmdPort };
    const bool repPortChangedVal { m_replyPort != replyPort };

    m_host = host;
    m_commandPort = cmdPort;
    m_replyPort = replyPort;

    if (hostChangedVal) {
        emit hostChanged();
    }
    if (cmdPortChangedVal) {
        emit commandPortChanged();
    }
    if (repPortChangedVal) {
        emit replyPortChanged();
    }

    auto transport = std::make_shared<Transport::SightlineUdpTransport>(
        host.toStdString(), static_cast<std::uint16_t>(cmdPort), static_cast<std::uint16_t>(replyPort));

    m_device = std::make_unique<QSightlineDevice>(transport, this);

    connect(m_device.get(), &QSightlineDevice::connectionStateChanged, this, [this](bool ok) {
        emit connectionChanged();
        if (ok) {
            m_connectionTimer.restart();
            m_device->queryVersion();
            m_device->queryParameters(0x87U);
            m_device->enableSystemStatus(true);
        }
    });

    connect(m_device.get(), &QSightlineDevice::trackingPositionsReceived, this,
        &SightlineQmlBridge::handleTrackingPositions);
    connect(m_device.get(), &QSightlineDevice::trackingParametersReceived, this,
        &SightlineQmlBridge::handleTrackingParameters);
    connect(m_device.get(), &QSightlineDevice::extendedPositionsReceived, this,
        &SightlineQmlBridge::handleTrackingPositionsExtended);
    connect(m_device.get(), &QSightlineDevice::trackCoastingChanged, this,
        &SightlineQmlBridge::trackCoastingChanged);
    connect(m_device.get(), &QSightlineDevice::userWarningReceived, this, &SightlineQmlBridge::handleUserWarning);
    connect(m_device.get(), &QSightlineDevice::versionReceived, this, &SightlineQmlBridge::handleVersion);
    connect(m_device.get(), &QSightlineDevice::systemStatusReceived, this, &SightlineQmlBridge::handleSystemStatus);
    connect(
        m_device.get(), &QSightlineDevice::stabilizationReceived, this, &SightlineQmlBridge::handleStabilizationParams);
    connect(
        m_device.get(), &QSightlineDevice::registrationReceived, this, &SightlineQmlBridge::handleRegistrationParams);
    connect(m_device.get(), &QSightlineDevice::stabilizationBiasReceived, this,
        &SightlineQmlBridge::handleStabilizationBias);
    connect(m_device.get(), &QSightlineDevice::rawFrameReceived, this, &SightlineQmlBridge::handleRawFrame);
    connect(m_device.get(), &QSightlineDevice::overlayModeReceived, this, &SightlineQmlBridge::handleOverlayMode);
    connect(m_device.get(), &QSightlineDevice::overlayObjectsIdsReceived, this,
        &SightlineQmlBridge::handleOverlayObjectsIds);
    connect(m_device.get(), &QSightlineDevice::overlayObjectParamsReceived, this,
        &SightlineQmlBridge::handleOverlayObjectParams);
    connect(m_device.get(), &QSightlineDevice::logoParametersReceived, this, &SightlineQmlBridge::handleLogoParameters);
    connect(m_device.get(), &QSightlineDevice::detectionReceived, this, &SightlineQmlBridge::handleDetectionParams);
    connect(m_device.get(), &QSightlineDevice::advDetectionReceived, this, &SightlineQmlBridge::handleAdvDetection);
    connect(m_device.get(), &QSightlineDevice::detectionRoiReceived, this, &SightlineQmlBridge::handleDetectionROI);
    connect(
        m_device.get(), &QSightlineDevice::klvMetricFiltersReceived, this, &SightlineQmlBridge::handleKlvMetricFilters);
    connect(m_device.get(), &QSightlineDevice::commandAckReceived, this,
        &SightlineQmlBridge::handleCommandAck);
    connect(m_device.get(), &QSightlineDevice::recordingEventReceived, this,
        &SightlineQmlBridge::handleRecordingEvent);
    connect(m_device.get(), &QSightlineDevice::recordingStatusReceived, this,
        &SightlineQmlBridge::handleRecordingStatus);
    connect(m_device.get(), &QSightlineDevice::dirListingReplyReceived, this,
        &SightlineQmlBridge::handleDirListingReply);
    connect(m_device.get(), &QSightlineDevice::h264ParamsReceived, this,
        &SightlineQmlBridge::handleH264Params);
    connect(m_device.get(), &QSightlineDevice::ethernetDisplayReceived, this,
        &SightlineQmlBridge::handleEthernetDisplay);
    connect(m_device.get(), &QSightlineDevice::ethernetVideoReceived, this,
        &SightlineQmlBridge::handleEthernetVideo);
    connect(m_device.get(), &QSightlineDevice::networkParamsReceived, this,
        &SightlineQmlBridge::handleNetworkParams);
    connect(m_device.get(), &QSightlineDevice::networkListReceived, this,
        &SightlineQmlBridge::handleNetworkList);
    connect(m_device.get(), &QSightlineDevice::systemValueReceived, this,
        &SightlineQmlBridge::handleSystemValue);

    const bool started = m_device->start();
    emit connectionChanged();
    return started;
    emit connectionChanged();
    return started;
}

void SightlineQmlBridge::disconnectDevice()
{
    if (m_device) {
        m_device->stop();
        m_device.reset();
    }
    if (m_coolerTimer && m_coolerTimer->isActive()) {
        m_coolerTimer->stop();
        m_coolerRemaining = 0;
        emit coolerCountdownChanged();
    }
    if (m_recordingClockTimer && m_recordingClockTimer->isActive()) {
        m_recordingClockTimer->stop();
    }
    m_isRecordingActive = false;
    m_elapsedRecordingSec = 0;
    emit recordingActiveChanged(false);
    emit recordingClockChanged();
    if (m_recordingFileListModel) {
        m_recordingFileListModel->clear();
    }
    m_connectionTimer.invalidate();
    m_softwareVersion = tr("Disconnected");
    m_cpuLoadPercent = 0;
    m_coreTempC = 0;
    m_uptimeSeconds = 0;
    if (m_trackListModel) {
        m_trackListModel->clearTracks();
    }
    emit connectionChanged();
    emit versionReceived();
    emit systemStatusChanged();
}

// 1. Tracking
bool SightlineQmlBridge::startTracking(int cam, int col, int row, int w, int h, int flags)
{
    if (m_trackListModel) {
        Sightline::TrackCoordinate coord {};
        const bool isPrimary = ((flags & 0x01) != 0) || (flags == 0);
        std::uint8_t tid = 0;
        if (!isPrimary) {
            tid = static_cast<std::uint8_t>(m_trackListModel->rowCount() == 0 ? 1 : m_trackListModel->rowCount());
        }
        coord.trackId = tid;
        coord.centerCol = static_cast<double>(col);
        coord.centerRow = static_cast<double>(row);
        coord.width = static_cast<double>(w);
        coord.height = static_cast<double>(h);
        coord.confidence = 98U;
        coord.isPrimary = isPrimary;
        if (isPrimary) {
            m_trackListModel->setPrimaryTrack(tid);
        }
        m_trackListModel->addOrUpdateTrack(coord);
    }

    if (!isConnected()) {
        return true;
    }
    return m_device->startTracking(static_cast<quint8>(cam), static_cast<quint16>(col), static_cast<quint16>(row),
        static_cast<quint16>(w), static_cast<quint16>(h), static_cast<quint8>(flags));
}

bool SightlineQmlBridge::stopTracking(int cam, int trackId)
{
    if (m_trackListModel) {
        if (trackId < 0 || trackId >= 255) {
            m_trackListModel->clearTracks();
        } else {
            m_trackListModel->removeTrack(trackId);
        }
    }

    if (!isConnected()) {
        return true;
    }
    return m_device->stopTracking(static_cast<quint8>(cam), static_cast<quint8>(trackId));
}

bool SightlineQmlBridge::modifyTracking(int cam, int trackId, int mode, int flags)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->modifyTracking(
        static_cast<quint8>(cam), static_cast<quint8>(trackId), static_cast<quint8>(mode), static_cast<quint8>(flags));
}

bool SightlineQmlBridge::nudgeTracking(int cam, int deltaCol, int deltaRow)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->nudgeTracking(
        static_cast<quint8>(cam), static_cast<qint16>(deltaCol), static_cast<qint16>(deltaRow));
}

bool SightlineQmlBridge::designatePrimary(int cam, int trackId)
{
    if (m_trackListModel) {
        m_trackListModel->setPrimaryTrack(trackId);
    }

    if (!isConnected()) {
        return true;
    }
    return m_device->designatePrimary(static_cast<quint8>(cam), static_cast<quint8>(trackId));
}

bool SightlineQmlBridge::startPrecisionTrack(int cam, int col, int row, int w, int h, qint64 framePts)
{
    if (m_trackListModel) {
        Sightline::TrackCoordinate coord {};
        coord.trackId = 0U;
        coord.centerCol = static_cast<double>(col);
        coord.centerRow = static_cast<double>(row);
        coord.width = static_cast<double>(w);
        coord.height = static_cast<double>(h);
        coord.confidence = 98U;
        coord.isPrimary = true;
        m_trackListModel->setPrimaryTrack(0);
        m_trackListModel->addOrUpdateTrack(coord);
    }

    if (!isConnected()) {
        return true;
    }
    return m_device->startPrecisionTrack(static_cast<quint8>(cam), static_cast<quint16>(col),
        static_cast<quint16>(row), static_cast<quint16>(w), static_cast<quint16>(h),
        static_cast<quint64>(framePts));
}

bool SightlineQmlBridge::setForcedCoast(int cam, int trackId, int mode)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->setForcedCoast(
        static_cast<quint8>(cam), static_cast<quint8>(trackId), static_cast<Sightline::ForcedCoastingMode>(mode));
}

bool SightlineQmlBridge::reinitTrack(int cam, int trackId)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->reinitTrack(static_cast<quint8>(cam), static_cast<quint8>(trackId));
}

bool SightlineQmlBridge::resizeTrack(int cam, int trackId, int w, int h, bool assist)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->resizeTrack(static_cast<quint8>(cam), static_cast<quint8>(trackId),
        static_cast<quint16>(w), static_cast<quint16>(h), assist);
}

bool SightlineQmlBridge::cueTrackAt(int cam, int col, int row, int mode, int trackId)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->cueTrackAt(static_cast<quint8>(cam), static_cast<quint16>(col), static_cast<quint16>(row),
        static_cast<Sightline::ModifyMode>(mode), static_cast<quint8>(trackId < 0 ? 0xFFU : trackId));
}

bool SightlineQmlBridge::nudgeDisplayTrack(int cam, int deltaCol, int deltaRow)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->nudgeDisplayTrack(
        static_cast<quint8>(cam), static_cast<qint16>(deltaCol), static_cast<qint16>(deltaRow));
}

bool SightlineQmlBridge::setTrackingParameters(int cam, int mode, int flags,
    int maxMisses, int zoomSmoothing, int rollSmoothing,
    int maxPauseTime, int acqCol, int acqRow)
{
    if (cam < 0 || cam >= 4) {
        return false;
    }
    Sightline::MsgSetTrackingParameters params {};
    params.cameraIndex = static_cast<std::uint8_t>(cam);
    params.mode = static_cast<std::uint8_t>(mode);
    params.flags = static_cast<std::uint8_t>(flags);
    params.maxMisses = static_cast<std::uint8_t>(maxMisses);
    params.zoomSmoothing = static_cast<std::uint8_t>(zoomSmoothing);
    params.rollSmoothing = static_cast<std::uint8_t>(rollSmoothing);
    params.maxPauseTime = static_cast<std::uint8_t>(std::clamp(maxPauseTime, 0, 20));
    params.acquisitionSearchCol = static_cast<std::uint16_t>(acqCol);
    params.acquisitionSearchRow = static_cast<std::uint16_t>(acqRow);

    m_cachedTrackingParams[static_cast<std::size_t>(cam)] = params;

    if (!isConnected()) {
        return false;
    }
    return m_device->setTrackingParameters(params);
}

bool SightlineQmlBridge::queryTrackingParameters(int cam)
{
    (void)cam;
    return queryParameters(static_cast<int>(Sightline::MessageId::SetTrackingParameters));
}

// 2. Stabilization & Registration (EAN-Stabilization)
bool SightlineQmlBridge::setStabilization(int cam, int mode, int autoBias, int maxShift)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->setStabilization(static_cast<quint8>(cam), static_cast<quint8>(mode),
        static_cast<quint8>(autoBias), static_cast<quint8>(maxShift));
}

bool SightlineQmlBridge::setStabilizationFull(
    int cam, int mode, int rate, int maxDispOffset, int maxAngle, int maxStabOff, int edgeY, int edgeU, int edgeV)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetStabilizationParameters msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.mode = static_cast<std::uint8_t>(mode);
    msg.rate = static_cast<std::uint8_t>(rate);
    msg.translationLimit = static_cast<std::uint8_t>(maxDispOffset);
    msg.angleLimit = static_cast<std::uint8_t>(maxAngle);
    msg.maxStabOff = static_cast<std::uint8_t>(maxStabOff);
    msg.edgeY = static_cast<std::uint8_t>(edgeY);
    msg.edgeU = static_cast<std::uint8_t>(edgeU);
    msg.edgeV = static_cast<std::uint8_t>(edgeV);

    return m_device->setStabilization(msg);
}

bool SightlineQmlBridge::resetStabilization(int cam, int resetType)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->resetStabilization(static_cast<quint8>(cam), static_cast<quint8>(resetType));
}

bool SightlineQmlBridge::setRegistration(int cam, int maxTranslation, int maxRotation, int zoomRange, int left,
    int right, int top, int bottom, int updateRate, int flags)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetRegistrationParameters msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.maxTranslation = static_cast<std::uint16_t>(maxTranslation);
    msg.maxRotation = static_cast<std::uint8_t>(maxRotation);
    msg.zoomRange = static_cast<std::uint8_t>(zoomRange);
    msg.left = static_cast<std::uint16_t>(left);
    msg.right = static_cast<std::uint16_t>(right);
    msg.top = static_cast<std::uint16_t>(top);
    msg.bottom = static_cast<std::uint16_t>(bottom);
    msg.updateRate = static_cast<std::uint8_t>(updateRate);
    msg.flags = static_cast<std::uint8_t>(flags);

    return m_device->setRegistration(msg);
}

bool SightlineQmlBridge::setStabilizationBias(int cam, int biasCol, int biasRow, int autoBias, int updateRate)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->setStabilizationBias(static_cast<quint8>(cam), static_cast<qint16>(biasCol),
        static_cast<qint16>(biasRow), static_cast<quint8>(autoBias), static_cast<quint8>(updateRate));
}

bool SightlineQmlBridge::applyStabilizationPreset(int cam, int preset)
{
    if (!isConnected()) {
        return false;
    }
    const auto p = static_cast<Sightline::StabilizationPreset>(preset);
    const auto c = static_cast<std::uint8_t>(cam);

    const auto reg = Sightline::SightlineStabilizationBuilder::makeRegistrationPreset(p, c);
    const auto stab = Sightline::SightlineStabilizationBuilder::makeStabilizationPreset(p, c);
    const auto bias = Sightline::SightlineStabilizationBuilder::makeBiasPreset(p, c);

    const bool ok1 = m_device->setRegistration(reg);
    const bool ok2 = m_device->setStabilization(stab);
    const bool ok3 = m_device->setStabilizationBias(bias);
    return ok1 && ok2 && ok3;
}

int SightlineQmlBridge::setGimbalFeedforwardBias(
    int cam, double panLeft, double tiltUp, int hRes, int vRes, double hFov, double vFov, double fps)
{
    if (!isConnected()) {
        return 0;
    }
    const auto bias
        = Sightline::SightlineStabilizationBuilder::calcGimbalBias(panLeft, tiltUp, static_cast<std::uint16_t>(hRes),
            static_cast<std::uint16_t>(vRes), hFov, vFov, fps, static_cast<std::uint8_t>(cam));
    m_device->setStabilizationBias(bias);
    return static_cast<int>(bias.biasCol);
}

bool SightlineQmlBridge::setIgnoredEdgesOverlay(int cam, bool enable)
{
    if (!isConnected() || !m_device->device()) {
        return false;
    }
    Sightline::MsgSetOverlayMode mode {};
    mode.cameraIndex = static_cast<std::uint8_t>(cam);
    if (enable) {
        mode.graphics |= Sightline::OverlayGraphicsFlags::RegistrationIgnoreEdges;
    } else {
        mode.graphics &= static_cast<std::uint16_t>(~Sightline::OverlayGraphicsFlags::RegistrationIgnoreEdges);
    }
    return m_device->device()->setOverlayMode(mode);
}

// 3. Detection & AI Classification
bool SightlineQmlBridge::setDetectionParams(int cam, int mode, int threshold, int minSize, int maxSize)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetDetectionParameters msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.mode = static_cast<Sightline::DetectionMode>(mode);
    msg.threshold = static_cast<std::uint8_t>(threshold);
    msg.minTargetSize = static_cast<std::uint16_t>(minSize);
    msg.maxTargetSize = static_cast<std::uint16_t>(maxSize);
    return m_device->device()->setDetection(msg);
}

bool SightlineQmlBridge::setDetectionExtended(int cam, int detIdx, int mode, int sensMode, int threshold, int minSize,
    int maxSize, int bkgdThresh, int watchFrames, int suspScore)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetDetectionParameters msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.detectionIndex = static_cast<std::uint8_t>(detIdx);
    msg.mode = static_cast<Sightline::DetectionMode>(mode);
    msg.sensitivityMode = static_cast<Sightline::SensitivityMode>(sensMode);
    msg.threshold = static_cast<std::uint8_t>(threshold);
    msg.minTargetSize = static_cast<std::uint16_t>(minSize);
    msg.maxTargetSize = static_cast<std::uint16_t>(maxSize);
    msg.bkgdThreshold = static_cast<std::uint8_t>(bkgdThresh);
    msg.watchFrames = static_cast<std::uint8_t>(watchFrames);
    msg.suspiciousScore = static_cast<std::uint8_t>(suspScore);
    return m_device->setDetection(msg);
}

bool SightlineQmlBridge::setDetectionAdvanced(int cam, int updateRate, int surroundSize, int blobDir, bool use8Bit,
    int gasOriginal, int gasColor, int iouThresh, bool enableMtd, int downsample)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgAdvancedDetectionParameters msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.updateRate = static_cast<std::uint8_t>(updateRate);
    msg.surroundSize = static_cast<std::uint8_t>(surroundSize);
    msg.blobDirection = static_cast<Sightline::BlobDirection>(blobDir);
    msg.use8BitImages = use8Bit;
    msg.gasAddOriginal = static_cast<std::uint8_t>(gasOriginal);
    msg.gasColor = static_cast<std::uint8_t>(gasColor);
    msg.aiIouThreshold = static_cast<std::uint8_t>(iouThresh);
    msg.enableMtd = enableMtd;
    msg.downsample = static_cast<Sightline::DetectionDownsample>(downsample);
    return m_device->setAdvancedDetection(msg);
}

bool SightlineQmlBridge::setDetectionRoiLine(
    int cam, int detIdx, int roiIdx, int x1, int y1, int x2, int y2, int lineSide)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgDetectionROI msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.detectionIndex = static_cast<std::uint8_t>(detIdx);
    msg.roiIndex = static_cast<std::uint8_t>(roiIdx);
    msg.geometryMode = Sightline::RoiGeometryMode::DetectionLine;
    msg.lineLeftX = static_cast<std::uint16_t>(x1);
    msg.lineLeftY = static_cast<std::uint16_t>(y1);
    msg.lineRightX = static_cast<std::uint16_t>(x2);
    msg.lineRightY = static_cast<std::uint16_t>(y2);
    msg.lineSide = static_cast<Sightline::LineReportSide>(lineSide);
    return m_device->setDetectionROI(msg);
}

bool SightlineQmlBridge::setDetectionRoiGrid(int cam, int detIdx, int roiIdx, int blocksW, int blocksH,
    const QString& mask0, const QString& mask1, const QString& mask2, const QString& mask3, bool showRegions)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgDetectionROI msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.detectionIndex = static_cast<std::uint8_t>(detIdx);
    msg.roiIndex = static_cast<std::uint8_t>(roiIdx);
    msg.geometryMode = Sightline::RoiGeometryMode::MaskedGrid;
    msg.blocksWide = static_cast<std::uint8_t>(blocksW);
    msg.blocksHigh = static_cast<std::uint8_t>(blocksH);
    bool ok0 { false };
    bool ok1 { false };
    bool ok2 { false };
    bool ok3 { false };
    msg.gridMasks[0] = mask0.toULongLong(&ok0, 16);
    msg.gridMasks[1] = mask1.toULongLong(&ok1, 16);
    msg.gridMasks[2] = mask2.toULongLong(&ok2, 16);
    msg.gridMasks[3] = mask3.toULongLong(&ok3, 16);
    msg.showRegions = showRegions;
    return m_device->setDetectionROI(msg);
}

bool SightlineQmlBridge::triggerDetectionSnapshot(int cam, int detIdx)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->triggerDetectionSnapshot(static_cast<quint8>(cam), static_cast<quint8>(detIdx));
}

bool SightlineQmlBridge::setClassifierSettings(int cam, int model, const QString& customModel, int maxPerFrame,
    int minDims, int droneMode, int pad, int updateRate)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgClassifierConfig msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.model = static_cast<Sightline::PretrainedClassifierModel>(model);
    msg.customModelName = customModel.toStdString();
    msg.maxPerFrame = static_cast<std::uint8_t>(maxPerFrame);
    msg.minDimensions = static_cast<std::uint16_t>(minDims);
    msg.droneReporting = static_cast<Sightline::DroneReportingMode>(droneMode);
    msg.detectionPadding = static_cast<std::uint8_t>(pad);
    msg.updateRate = static_cast<std::uint8_t>(updateRate);
    return m_device->setClassifierConfig(msg);
}

bool SightlineQmlBridge::setComputeAssignment(bool useNpu, bool asyncInferencing)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->setComputeResources(useNpu, asyncInferencing);
}

bool SightlineQmlBridge::setKlvMetricBounds(int cam, double minW, double maxW, double minH, double maxH,
    bool aboveHorizon, bool belowHorizon, double minLat, double maxLat, double minLon, double maxLon)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgKlvMetricFilters msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.minTargetWidthM = static_cast<float>(minW);
    msg.maxTargetWidthM = static_cast<float>(maxW);
    msg.minTargetHeightM = static_cast<float>(minH);
    msg.maxTargetHeightM = static_cast<float>(maxH);
    msg.filterAboveHorizon = aboveHorizon;
    msg.filterBelowHorizon = belowHorizon;
    msg.minLatitude = minLat;
    msg.maxLatitude = maxLat;
    msg.minLongitude = minLon;
    msg.maxLongitude = maxLon;
    return m_device->setKlvMetricFilters(msg);
}

bool SightlineQmlBridge::queryDetection(int cam, int detIdx)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->queryDetectionParams(static_cast<quint8>(cam), static_cast<quint8>(detIdx));
}

bool SightlineQmlBridge::queryAdvDetection(int cam)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->queryAdvDetection(static_cast<quint8>(cam));
}

bool SightlineQmlBridge::queryDetectionROI(int cam, int roiIdx)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->queryDetectionROI(static_cast<quint8>(cam), static_cast<quint8>(roiIdx));
}

bool SightlineQmlBridge::queryKlvMetricFilters(int cam)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->queryKlvMetricFilters(static_cast<quint8>(cam));
}

bool SightlineQmlBridge::queryClassifierConfig(int cam)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->queryClassifierConfig(static_cast<quint8>(cam));
}

bool SightlineQmlBridge::customAIDetect(int cam, int modelId, int confThresh, int nmsThresh)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgCustomAIDetect msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.modelId = static_cast<std::uint8_t>(modelId);
    msg.confidenceThreshold = static_cast<std::uint8_t>(confThresh);
    msg.nmsThreshold = static_cast<std::uint8_t>(nmsThresh);
    return m_device->device()->customAIDetect(msg);
}

// 4. Focus & Lens
bool SightlineQmlBridge::sendLensCommand(int cam, int cmdType, int rateOrPos)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->sendLensCommand(
        static_cast<quint8>(cam), static_cast<quint8>(cmdType), static_cast<qint16>(rateOrPos));
}

// 5. Video & Compression
bool SightlineQmlBridge::setVideoParams(int cam, int format, int width, int height, int fps)
{
    if (!isConnected()) {
        return false;
    }
    static_cast<void>(format);
    static_cast<void>(fps);

    Sightline::MsgSetVideoParameters msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.autoChop = 0U;
    msg.deinterlace = 1U;
    msg.autoReset = 1U;
    bool success { m_device->device()->setVideoParams(msg) };

    if (width > 0 && height > 0) {
        Sightline::MsgSetDisplayParameters dispMsg {};
        dispMsg.displayIndex = 0U;
        dispMsg.cameraIndex = static_cast<std::uint8_t>(cam);
        dispMsg.displayWidth = static_cast<std::uint16_t>(width);
        dispMsg.displayHeight = static_cast<std::uint16_t>(height);
        const bool okDisplay { m_device->device()->setDisplayParams(dispMsg) };
        success = success && okDisplay;
    }

    return success;
}

bool SightlineQmlBridge::setH264Params(int stream, int bitrate, int gop, int quality, int rateCtrl)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetH264Parameters msg {};
    // Display ID: 0x0002 for Net0 (Stream 0), 0x0080 for Net1 (Stream 1)
    if (stream == 0) {
        msg.displayId = 0x0002U;
    } else if (stream == 1) {
        msg.displayId = 0x0080U;
    } else {
        msg.displayId = static_cast<std::uint16_t>(stream);
    }

    const auto targetBps = (bitrate > 0 && bitrate < 100000) ? static_cast<std::uint32_t>(bitrate) * 1000U
                                                             : static_cast<std::uint32_t>(std::max(0, bitrate));
    msg.targetBitrateBps = targetBps;
    msg.intraFrameInterval = static_cast<std::uint8_t>(std::clamp(gop, 0, 255));
    // Bitrate Mode: bit 4 is VBR (0x10) if rateCtrl == 1, CBR (0x00) otherwise. Profile: High (0x02).
    const std::uint8_t vbrBit = (rateCtrl == 1) ? 0x10U : 0x00U;
    msg.flags = static_cast<std::uint8_t>(vbrBit | 0x02U);
    msg.minQp = 0U;
    msg.maxQp = static_cast<std::uint8_t>(std::clamp(quality, 0, 51));
    return m_device->device()->setH264Params(msg);
}

bool SightlineQmlBridge::setH264ParamsEx(int stream, int bitrate, int gop, int profile, int rateCtrl,
    int minQp, int maxQp, int deblock, int airMb, int sliceRows)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetH264Parameters msg {};
    if (stream == 0) {
        msg.displayId = static_cast<std::uint16_t>(Sightline::NetworkDisplayId::Net0);
    } else if (stream == 1) {
        msg.displayId = static_cast<std::uint16_t>(Sightline::NetworkDisplayId::Net1);
    } else if (stream == 2) {
        msg.displayId = static_cast<std::uint16_t>(Sightline::NetworkDisplayId::Net2);
    } else {
        msg.displayId = static_cast<std::uint16_t>(stream);
    }

    const auto targetBps = (bitrate > 0 && bitrate < 100000) ? static_cast<std::uint32_t>(bitrate) * 1000U
                                                             : static_cast<std::uint32_t>(std::max(0, bitrate));
    msg.targetBitrateBps = targetBps;
    msg.intraFrameInterval = static_cast<std::uint8_t>(std::clamp(gop, 0, 255));
    msg.flags = Sightline::makeH264Flags(
        static_cast<Sightline::H264Profile>(std::clamp(profile, 0, 2)),
        static_cast<Sightline::BitrateControlMode>(std::clamp(rateCtrl, 0, 3)));
    msg.lfDisableIdc = static_cast<std::uint8_t>(std::clamp(deblock, 0, 2));
    msg.minQp = static_cast<std::uint8_t>(std::clamp(minQp, 0, 30));
    msg.maxQp = static_cast<std::uint8_t>(std::clamp(maxQp, 0, 51));
    msg.airMbPeriod = static_cast<std::uint8_t>(std::clamp(airMb, 0, 255));
    msg.sliceRefreshRowNumber = static_cast<std::uint8_t>(std::clamp(sliceRows, 0, 255));

    m_encBitrateKbps = bitrate;
    m_encGopInterval = gop;
    m_encProfile = profile;
    m_encRateControl = rateCtrl;
    m_encMinQp = minQp;
    m_encMaxQp = maxQp;
    m_encAirMb = airMb;
    m_encSliceRows = sliceRows;
    emit encParamsChanged();

    return m_device->setH264Params(msg);
}

bool SightlineQmlBridge::setEthernetDisplay(int stream, int protocol, const QString& ip, int port,
    int maxPacket, int maxRawPacket)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetEthernetDisplayParameters msg {};
    if (stream == 0) {
        msg.displayId = static_cast<std::uint16_t>(Sightline::NetworkDisplayId::Net0);
    } else if (stream == 1) {
        msg.displayId = static_cast<std::uint16_t>(Sightline::NetworkDisplayId::Net1);
    } else if (stream == 2) {
        msg.displayId = static_cast<std::uint16_t>(Sightline::NetworkDisplayId::Net2);
    } else {
        msg.displayId = static_cast<std::uint16_t>(stream);
    }

    msg.protocol = static_cast<std::uint8_t>(protocol);
    msg.ipAddress = QHostAddress(ip).toIPv4Address();
    msg.port = static_cast<std::uint16_t>(port);
    msg.maxPacket = static_cast<std::uint16_t>(maxPacket > 0 ? maxPacket : 1400);
    msg.maxRawPacket = static_cast<std::uint16_t>(maxRawPacket);

    m_netDisplayProtocol = protocol;
    m_netDisplayIp = ip;
    m_netDisplayPort = port;
    m_netMaxPacket = maxPacket;
    emit netDisplayChanged();

    return m_device->setEthernetDisplay(msg);
}

bool SightlineQmlBridge::setEthernetVideo(int stream, int frameStep, int frameSize,
    int customW, int customH, int quality, int foveal)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetEthernetVideoParameters msg {};
    if (stream == 0) {
        msg.displayId = static_cast<std::uint16_t>(Sightline::NetworkDisplayId::Net0);
    } else if (stream == 1) {
        msg.displayId = static_cast<std::uint16_t>(Sightline::NetworkDisplayId::Net1);
    } else if (stream == 2) {
        msg.displayId = static_cast<std::uint16_t>(Sightline::NetworkDisplayId::Net2);
    } else {
        msg.displayId = static_cast<std::uint16_t>(stream);
    }

    msg.frameStep = static_cast<std::uint8_t>(std::clamp(frameStep, 1, 255));
    msg.frameSize = static_cast<std::uint8_t>(frameSize);
    msg.customWide = static_cast<std::uint16_t>(customW);
    msg.customHigh = static_cast<std::uint16_t>(customH);
    msg.quality = static_cast<std::uint8_t>(std::clamp(quality, 0, 100));
    msg.foveal = static_cast<std::uint8_t>(foveal);

    emit netVideoChanged();
    return m_device->setEthernetVideo(msg);
}

bool SightlineQmlBridge::setTrafficControl(int rateKbps, int burstBytes, int mtuBytes)
{
    if (!isConnected()) {
        return false;
    }
    m_tcRateKbps = rateKbps;
    m_tcBurstBytes = burstBytes;
    m_tcMtuBytes = mtuBytes;
    emit tcStatusChanged();

    return m_device->setTrafficControl(
        static_cast<std::uint32_t>(std::max(0, rateKbps)),
        static_cast<std::uint32_t>(std::max(0, burstBytes)),
        static_cast<std::uint32_t>(std::max(0, mtuBytes)));
}

bool SightlineQmlBridge::resetTrafficControl()
{
    return setTrafficControl(0, 0, 0);
}

bool SightlineQmlBridge::applyLowBandwidth(int stream)
{
    if (!isConnected()) {
        return false;
    }
    const bool encOk = setH264ParamsEx(
        stream,
        100, // 100 kbps
        30,  // 30 frame GOP
        1,   // Main profile
        2,   // Constrained Bitrate (CBR)
        18,  // min QP
        42,  // max QP
        0,   // Deblocking enabled
        0,   // AIR
        0    // Slice rows
    );

    const bool vidOk = setEthernetVideo(
        stream,
        2, // Frame step 2 = 15 fps
        1, // Frame size 1 = 720p
        0, 0, 0, 0
    );

    return encOk && vidOk;
}

bool SightlineQmlBridge::isValidPort(int protocol, int port) const noexcept
{
    return Sightline::isValidTransportPort(
        static_cast<std::uint16_t>(port),
        static_cast<std::uint8_t>(protocol));
}

bool SightlineQmlBridge::isRtp(int protocol) const noexcept
{
    return Sightline::isRtpProtocol(static_cast<std::uint8_t>(protocol));
}

bool SightlineQmlBridge::queryEncoderParams(int stream)
{
    if (!isConnected()) {
        return false;
    }
    const std::uint16_t displayId = (stream == 1) ? 0x0080U : (stream == 2 ? 0x0200U : 0x0002U);
    return m_device->getH264Params(displayId);
}

bool SightlineQmlBridge::queryDisplayParams(int stream)
{
    if (!isConnected()) {
        return false;
    }
    const std::uint16_t displayId = (stream == 1) ? 0x0080U : (stream == 2 ? 0x0200U : 0x0002U);
    const bool dOk = m_device->getEthernetDisplay(displayId);
    const bool vOk = m_device->getEthernetVideo(displayId);
    return dOk || vOk;
}

bool SightlineQmlBridge::queryNetworkParams()
{
    if (!isConnected()) {
        return false;
    }
    const bool pOk = m_device->getNetworkParams(0U);
    const bool lOk = m_device->getNetworkList();
    return pOk || lOk;
}

bool SightlineQmlBridge::streamingControl(int stream, int action)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->device()->streamingControl(static_cast<std::uint8_t>(stream), static_cast<std::uint8_t>(action));
}

bool SightlineQmlBridge::setSDRecording(int state, int cam, const QString& prefix)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetSDRecordingParameters msg {};
    msg.recordingState = static_cast<std::uint8_t>(state);
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.filenamePrefix = prefix.toStdString();
    return m_device->device()->setSDRecording(msg);
}

bool SightlineQmlBridge::startRecordingV2(
    int cam, const QString& prefix, int format, int dest, int maxDurationSec, int maxBitrateKbps, bool autoSplit)
{
    static_cast<void>(format);
    static_cast<void>(maxDurationSec);
    static_cast<void>(maxBitrateKbps);

    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetFileRecordingParamsV2 msg {};
    static std::uint16_t s_recSeq { 1U };
    msg.sequenceId = s_recSeq++;
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.action = Sightline::RecordingAction::Start;
    msg.destination = static_cast<Sightline::StorageDestination>(std::clamp(dest, 0, 2));

    std::uint8_t flags = 0U;
    if (autoSplit) {
        flags |= static_cast<std::uint8_t>(Sightline::RecordingFlags::AllowNumericOverwrite);
    }
    msg.flags = flags;
    msg.baseFilename = prefix.toStdString();

    const bool ok = m_device->setFileRecordingV2(msg);
    if (ok) {
        m_isRecordingActive = true;
        m_elapsedRecordingSec = 0;
        if (!m_recordingClockTimer->isActive()) {
            m_recordingClockTimer->start(1000);
        }
        emit recordingActiveChanged(true);
        emit recordingClockChanged();
    }
    return ok;
}

bool SightlineQmlBridge::stopRecordingV2(int cam)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetFileRecordingParamsV2 msg {};
    static std::uint16_t s_stopSeq { 1000U };
    msg.sequenceId = s_stopSeq++;
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.action = Sightline::RecordingAction::Stop;

    const bool ok = m_device->setFileRecordingV2(msg);
    m_isRecordingActive = false;
    if (m_recordingClockTimer && m_recordingClockTimer->isActive()) {
        m_recordingClockTimer->stop();
    }
    emit recordingActiveChanged(false);
    return ok;
}

bool SightlineQmlBridge::captureSnapshotV2(
    int cam, const QString& prefix, int format, int quality, bool includeMetadata)
{
    static_cast<void>(includeMetadata);

    if (!isConnected()) {
        return false;
    }
    Sightline::MsgDoSnapShotV2 msg {};
    static std::uint16_t s_snapSeq { 2000U };
    msg.sequenceId = s_snapSeq++;
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.format = static_cast<Sightline::SnapshotFormat>(std::clamp(format, 0, 3));
    msg.qualityLevel = static_cast<std::uint8_t>(std::clamp(quality, 1, 100));
    msg.burstCount = 1U;
    msg.customFilename = prefix.toStdString();

    return m_device->doSnapshotV2(msg);
}

bool SightlineQmlBridge::requestDirectoryListing(int dest, int startIndex, int maxEntries, const QString& filter)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgGetDirectoryListing msg {};
    static std::uint16_t s_dirSeq { 3000U };
    msg.sequenceId = s_dirSeq++;
    msg.destination = static_cast<Sightline::StorageDestination>(std::clamp(dest, 0, 3));
    msg.startIndex = static_cast<std::uint32_t>(std::max(0, startIndex));
    msg.maxEntries = static_cast<std::uint16_t>(std::clamp(maxEntries, 1, 255));
    msg.pathFilter = filter.toStdString();

    return m_device->getDirectoryListing(msg);
}

bool SightlineQmlBridge::pinStorageFile(int dest, const QString& filename, bool pin)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgFileStorageManagement msg {};
    static std::uint16_t s_mgmtSeq { 4000U };
    msg.sequenceId = s_mgmtSeq++;
    msg.operation = pin ? Sightline::FileStorageOp::Pin : Sightline::FileStorageOp::Unpin;
    msg.destination = static_cast<Sightline::StorageDestination>(std::clamp(dest, 0, 3));
    msg.targetFilename = filename.toStdString();

    const bool ok = m_device->sendFileStorageMgmt(msg);
    if (ok && m_recordingFileListModel) {
        m_recordingFileListModel->setFilePinned(filename, pin);
    }
    return ok;
}

bool SightlineQmlBridge::deleteStorageFile(int dest, const QString& filename)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgFileStorageManagement msg {};
    static std::uint16_t s_delSeq { 5000U };
    msg.sequenceId = s_delSeq++;
    msg.operation = Sightline::FileStorageOp::Delete;
    msg.destination = static_cast<Sightline::StorageDestination>(std::clamp(dest, 0, 3));
    msg.targetFilename = filename.toStdString();

    const bool ok = m_device->sendFileStorageMgmt(msg);
    if (ok && m_recordingFileListModel) {
        m_recordingFileListModel->removeEntry(filename);
    }
    return ok;
}

QJsonObject SightlineQmlBridge::validateFilename(const QString& prefix)
{
    QJsonObject obj;
    const auto code = Sightline::RecordingValidator::checkFilename(prefix.toStdString(), 0U);
    const bool valid = (code == Sightline::RecordingStatusCode::Success);
    obj[QStringLiteral("valid")] = valid;
    QString errStr {};
    switch (code) {
    case Sightline::RecordingStatusCode::ErrNumericFilename:
        errStr = QStringLiteral("Warning: Filename cannot end in digits 0-9 without overwrite flag (Sightline rollover conflict)");
        break;
    case Sightline::RecordingStatusCode::ErrInvalidCharacters:
        errStr = QStringLiteral("Error: Filename contains invalid characters or exceeds 64 characters");
        break;
    default:
        break;
    }
    obj[QStringLiteral("error")] = errStr;
    return obj;
}

// 6. Blending & Enhancement
bool SightlineQmlBridge::setBlendParams(int cam1, int cam2, int mode, int alpha)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetBlendParameters msg {};
    msg.warpIndex = static_cast<std::uint8_t>(cam1);
    msg.fixedIndex = static_cast<std::uint8_t>(cam2);
    // Sightline blend mode is 1-based (1: Frame, 2: Thermal, 3: Night, 4: Color).
    const auto blendMode = (mode >= 1 && mode <= 4) ? static_cast<std::uint8_t>(mode)
                                                    : static_cast<std::uint8_t>(std::clamp(mode + 1, 1, 4));
    msg.mode = blendMode;
    msg.amt = static_cast<std::uint8_t>(std::clamp(alpha, 0, 255));
    return m_device->device()->setBlend(msg);
}

bool SightlineQmlBridge::setVideoEnhance(int cam, int contrast, int brightness, int sharpening, int clahe)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetVideoEnhancement msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.contrast = static_cast<std::uint8_t>(contrast);
    msg.brightness = static_cast<std::uint8_t>(brightness);
    msg.sharpening = static_cast<std::uint8_t>(sharpening);
    msg.claheEnable = static_cast<std::uint8_t>(clahe);
    return m_device->device()->setVideoEnhance(msg);
}

bool SightlineQmlBridge::setEnhancementMode(int cam, int mode, int strength, int blend, int sharpen, int radius)
{
    const int c = std::clamp(cam, 0, 3);
    auto& cfg = m_cachedEnhancement[c];
    cfg.cameraIndex = static_cast<std::uint8_t>(c);
    cfg.mode = static_cast<Sightline::ContrastMode>(mode);
    cfg.enhanceParam = static_cast<std::uint8_t>(std::clamp(strength, 0, 127));
    cfg.alphaBlend = static_cast<std::uint8_t>(std::clamp(blend, 0, 255));
    cfg.sharpening = static_cast<std::uint8_t>(std::clamp(sharpen, 0, 15));
    cfg.sharpenRadius = static_cast<std::uint8_t>(std::clamp(radius, 1, 3));

    emit enhancementModeChanged(c, mode, strength, blend, sharpen, radius);
    if (c == 0) {
        emit contrastModeChanged();
    }

    if (isConnected()) {
        return m_device->device()->setVideoEnhanceFull(cfg);
    }
    return true;
}

bool SightlineQmlBridge::setDenoiseParameters(int cam, int rate, bool motionMask, int motionMaskType)
{
    const int c = std::clamp(cam, 0, 3);
    auto& cfg = m_cachedEnhancement[c];
    cfg.cameraIndex = static_cast<std::uint8_t>(c);
    cfg.denoiseRate = static_cast<std::uint8_t>(std::clamp(rate, 0, 255));

    constexpr auto aerialMask = static_cast<std::uint8_t>(Sightline::EnhancementFlags::AerialMotionMask);
    constexpr auto staringMask = static_cast<std::uint8_t>(Sightline::EnhancementFlags::StaringMotionMask);

    if (motionMask) {
        if (motionMaskType == 1) {
            cfg.flags |= staringMask; // Staring mask
            cfg.flags = static_cast<std::uint8_t>(cfg.flags & ~aerialMask);
        } else {
            cfg.flags |= aerialMask; // Aerial mask
            cfg.flags = static_cast<std::uint8_t>(cfg.flags & ~staringMask);
        }
    } else {
        cfg.flags = static_cast<std::uint8_t>(cfg.flags & ~(aerialMask | staringMask));
    }

    emit denoiseChanged(c, rate, motionMask, motionMaskType);

    if (isConnected()) {
        return m_device->device()->setVideoEnhanceFull(cfg);
    }
    return true;
}

bool SightlineQmlBridge::setHistogramControls(
    int cam, bool featureBased, bool sqrtHist, int aveRate, int maxPct, int brightness, int contrast)
{
    const int c = std::clamp(cam, 0, 3);
    auto& cfg = m_cachedEnhancement[c];
    cfg.cameraIndex = static_cast<std::uint8_t>(c);
    constexpr auto featureMask = static_cast<std::uint8_t>(Sightline::EnhancementFlags::FeatureBasedHist);
    constexpr auto sqrtMask = static_cast<std::uint8_t>(Sightline::EnhancementFlags::SquareRootHist);

    if (featureBased) {
        cfg.flags |= featureMask;
    } else {
        cfg.flags = static_cast<std::uint8_t>(cfg.flags & ~featureMask);
    }
    if (sqrtHist) {
        cfg.flags |= sqrtMask;
    } else {
        cfg.flags = static_cast<std::uint8_t>(cfg.flags & ~sqrtMask);
    }
    cfg.histAveRate = static_cast<std::uint8_t>(std::clamp(aveRate, 0, 255));
    cfg.histMaxPctBin = static_cast<std::uint8_t>(std::clamp(maxPct, 0, 255));
    cfg.brightness = static_cast<std::uint8_t>(std::clamp(brightness, 0, 255));
    cfg.contrast = static_cast<std::uint8_t>(std::clamp(contrast, 0, 255));

    emit histogramChanged(c, featureBased, sqrtHist, aveRate, maxPct, brightness, contrast);

    if (isConnected()) {
        return m_device->device()->setVideoEnhanceFull(cfg);
    }
    return true;
}

bool SightlineQmlBridge::setScintillationMode(int cam, int mode)
{
    const int c = std::clamp(cam, 0, 3);
    auto& cfg = m_cachedEnhancement[c];
    cfg.cameraIndex = static_cast<std::uint8_t>(c);
    cfg.scintillation = static_cast<Sightline::ScintillationPreset>(std::clamp(mode, 0, 3));

    emit scintillationChanged(c, mode);

    if (isConnected()) {
        return m_device->device()->setVideoEnhanceFull(cfg);
    }
    return true;
}

bool SightlineQmlBridge::setGaussianAndLap(int cam, int gaussianBlur, int lapMinDiff, int colorEnhance)
{
    const int c = std::clamp(cam, 0, 3);
    auto& cfg = m_cachedEnhancement[c];
    cfg.cameraIndex = static_cast<std::uint8_t>(c);
    cfg.gaussianBlur = static_cast<std::uint8_t>(std::clamp(gaussianBlur, 0, 6));
    cfg.lapMinDiff = static_cast<std::uint8_t>(std::clamp(lapMinDiff, 0, 255));
    cfg.colorEnhance = static_cast<std::uint8_t>(std::clamp(colorEnhance, 0, 255));

    emit gaussianAndLapChanged(c, gaussianBlur, lapMinDiff, colorEnhance);

    if (isConnected()) {
        return m_device->device()->setVideoEnhanceFull(cfg);
    }
    return true;
}

bool SightlineQmlBridge::setEnhancementRoi(int cam, int row, int col, int height, int width)
{
    const int c = std::clamp(cam, 0, 3);
    auto& cfg = m_cachedEnhancement[c];
    cfg.cameraIndex = static_cast<std::uint8_t>(c);
    cfg.roiRow = static_cast<std::uint16_t>(row);
    cfg.roiCol = static_cast<std::uint16_t>(col);
    cfg.roiHigh = static_cast<std::uint16_t>(height);
    cfg.roiWide = static_cast<std::uint16_t>(width);

    m_cachedRoi[c] = QRect(col, row, width, height);

    emit enhancementRoiUpdated(c, row, col, height, width);
    if (c == 0) {
        emit enhancementRoiChanged();
    }

    if (isConnected()) {
        return m_device->device()->setVideoEnhanceFull(cfg);
    }
    return true;
}

bool SightlineQmlBridge::setCustomConvolution(int cam, int kernelSize, const QVariantList& weights, bool normalize)
{
    Q_UNUSED(kernelSize);
    const int c = std::clamp(cam, 0, 3);
    auto& cfg = m_cachedEnhancement[c];
    cfg.cameraIndex = static_cast<std::uint8_t>(c);
    cfg.normalizeKernel = normalize;
    cfg.customKernel.clear();
    cfg.customKernel.reserve(static_cast<std::size_t>(weights.size()));
    for (const auto& w : weights) {
        cfg.customKernel.push_back(static_cast<std::int8_t>(w.toInt()));
    }

    if (isConnected()) {
        return m_device->device()->setVideoEnhanceFull(cfg);
    }
    return true;
}

bool SightlineQmlBridge::setFalseColorPalette(int cam, int paletteIndex)
{
    const int c = std::clamp(cam, 0, 3);
    m_activePaletteIndex[c] = paletteIndex;

    emit falseColorPaletteChanged(c, paletteIndex);
    if (c == 0) {
        emit paletteIndexChanged();
    }

    if (isConnected()) {
        return m_device->device()->setFalseColor(
            static_cast<std::uint8_t>(c), static_cast<Sightline::FalseColorPalette>(paletteIndex));
    }
    return true;
}

bool SightlineQmlBridge::uploadUserPalette(int paletteIndex, const QByteArray& yuvData)
{
    if (yuvData.size() < 768) {
        return false;
    }
    const int idx = std::clamp(paletteIndex, 0, 3);
    m_activeUserPalette[idx] = yuvData.left(768);

    emit userPaletteUploaded(idx, m_activeUserPalette[idx]);

    if (!isConnected()) {
        return true;
    }
    Sightline::MsgUserPalette msg {};
    msg.paletteIndex = static_cast<std::uint8_t>(idx);
    msg.lutData.reserve(768U);
    for (int i = 0; i < 768; ++i) {
        msg.lutData.push_back(static_cast<std::uint8_t>(yuvData.at(i)));
    }
    return m_device->device()->setUserPalette(msg);
}

QByteArray SightlineQmlBridge::loadUserPaletteFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray {};
    }
    return file.readAll();
}

bool SightlineQmlBridge::saveUserPaletteFile(const QString& filePath, const QByteArray& yuvData)
{
    if (yuvData.size() < 768) {
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(yuvData.constData(), 768) == 768;
}

bool SightlineQmlBridge::setLensDistortion(int cam, double k1, double k2, double centerOffsetX, double centerOffsetY)
{
    const int c = std::clamp(cam, 0, 3);
    m_lensK1[c] = k1;
    m_lensK2[c] = k2;
    m_lensCenterX[c] = centerOffsetX;
    m_lensCenterY[c] = centerOffsetY;

    emit lensDistortionUpdated(c, k1, k2, centerOffsetX, centerOffsetY);

    if (!isConnected()) {
        return true;
    }
    Sightline::MsgSetLensParameters msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(c);
    return m_device->device()->setLensParams(msg);
}

bool SightlineQmlBridge::saveEnhancementPreset(const QString& name, const QVariantMap& settings)
{
    if (name.trimmed().isEmpty()) {
        return false;
    }
    QSettings s;
    s.beginGroup(QStringLiteral("EnhancePresets"));
    s.setValue(name.trimmed(), settings);
    s.endGroup();
    return true;
}

QVariantMap SightlineQmlBridge::loadEnhancementPreset(const QString& name)
{
    QSettings s;
    s.beginGroup(QStringLiteral("EnhancePresets"));
    const auto val = s.value(name.trimmed()).toMap();
    s.endGroup();
    return val;
}

QStringList SightlineQmlBridge::getEnhancementPresets()
{
    QSettings s;
    s.beginGroup(QStringLiteral("EnhancePresets"));
    const auto keys = s.childKeys();
    s.endGroup();
    return keys;
}

// Backwards compatibility wrappers
bool SightlineQmlBridge::setEnhanceFull(int cam, int mode, int sharpen, int blend, int enhanceParam, int denoise,
    int flags, int histAveRate, int histMaxPct, int roiRow, int roiCol, int roiHigh, int roiWide, int gaussian,
    int lapMinDiff, int colorEnhance, int brightness, int contrast, int scintillation, int sharpenRadius)
{
    setEnhancementMode(cam, mode, enhanceParam, blend, sharpen, sharpenRadius);
    setDenoiseParameters(cam, denoise, (flags & 0x11) != 0, (flags & (1 << 4)) != 0 ? 1 : 0);
    setHistogramControls(
        cam, (flags & (1 << 1)) != 0, (flags & (1 << 2)) != 0, histAveRate, histMaxPct, brightness, contrast);
    setGaussianAndLap(cam, gaussian, lapMinDiff, colorEnhance);
    setEnhancementRoi(cam, roiRow, roiCol, roiHigh, roiWide);
    setScintillationMode(cam, scintillation);
    return true;
}

bool SightlineQmlBridge::setCustomConvolution(int cam, const QVariantList& weights, bool normalize)
{
    return setCustomConvolution(cam, 3, weights, normalize);
}

bool SightlineQmlBridge::setFalseColor(int cam, int paletteIndex)
{
    return setFalseColorPalette(cam, paletteIndex);
}

bool SightlineQmlBridge::setUserPaletteLut(int paletteIndex, const QVariantList& yuvValues)
{
    if (yuvValues.size() < 768) {
        return false;
    }
    QByteArray bytes;
    bytes.reserve(768);
    for (int i = 0; i < 768; ++i) {
        bytes.append(static_cast<char>(yuvValues[i].toInt() & 0xFF));
    }
    return uploadUserPalette(paletteIndex, bytes);
}

QVariantList SightlineQmlBridge::loadPaletteFile(const QString& filePath)
{
    const QByteArray data = loadUserPaletteFile(filePath);
    QVariantList list;
    list.reserve(data.size());
    for (int i = 0; i < data.size(); ++i) {
        list.append(static_cast<int>(static_cast<std::uint8_t>(data[i])));
    }
    return list;
}

bool SightlineQmlBridge::savePaletteFile(const QString& filePath, const QVariantList& yuvValues)
{
    QByteArray bytes;
    bytes.reserve(yuvValues.size());
    for (int i = 0; i < yuvValues.size(); ++i) {
        bytes.append(static_cast<char>(yuvValues[i].toInt() & 0xFF));
    }
    return saveUserPaletteFile(filePath, bytes);
}

bool SightlineQmlBridge::saveEnhancePreset(const QString& name, const QVariantMap& settings)
{
    return saveEnhancementPreset(name, settings);
}

QVariantMap SightlineQmlBridge::loadEnhancePreset(const QString& name)
{
    return loadEnhancementPreset(name);
}

QStringList SightlineQmlBridge::getEnhancePresets()
{
    return getEnhancementPresets();
}

bool SightlineQmlBridge::setNoise3D(int cam, int enable, int temporal, int spatial)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgNoise3D msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.enable = static_cast<std::uint8_t>(enable);
    msg.temporalStrength = static_cast<std::uint8_t>(temporal);
    msg.spatialStrength = static_cast<std::uint8_t>(spatial);
    return m_device->device()->setNoise3D(msg);
}

// 6. Overlays & Graphic Primitives (Module 0x62 & 0x9C)
bool SightlineQmlBridge::setOverlayMode(int cam, int primaryReticle, int secondaryReticle, int graphicsMask)
{
    if (!isConnected()) {
        emit overlayModeReceived(cam, primaryReticle, secondaryReticle, graphicsMask);
        return true;
    }
    Sightline::MsgSetOverlayMode msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.primaryReticle = static_cast<std::uint8_t>(primaryReticle);
    msg.secondaryReticle = static_cast<std::uint8_t>(secondaryReticle);
    msg.graphics = static_cast<std::uint16_t>(graphicsMask);
    return m_device->setOverlayMode(msg);
}

bool SightlineQmlBridge::getOverlayMode(int cam)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->getOverlayMode(static_cast<quint8>(cam));
}

bool SightlineQmlBridge::drawCross(
    int cam, int objId, int x, int y, int size, int fgColor, int thickness, bool originUpperLeft)
{
    if (!m_activeOverlayIds.contains(objId)) {
        m_activeOverlayIds.append(objId);
        emit activeOverlayIdsChanged();
    }
    if (!isConnected()) {
        return true;
    }
    return m_device->drawCross(static_cast<quint8>(cam), static_cast<quint8>(objId), static_cast<qint16>(x),
        static_cast<qint16>(y), static_cast<quint16>(size), static_cast<Sightline::OverlayPaletteColor>(fgColor),
        static_cast<quint16>(thickness), originUpperLeft);
}

bool SightlineQmlBridge::drawRectangle(int cam, int objId, int x, int y, int w, int h, bool filled, int fgColor,
    int bgColor, int alpha, int thickness, bool originUpperLeft)
{
    if (!m_activeOverlayIds.contains(objId)) {
        m_activeOverlayIds.append(objId);
        emit activeOverlayIdsChanged();
    }
    if (!isConnected()) {
        return true;
    }
    return m_device->drawRectangle(static_cast<quint8>(cam), static_cast<quint8>(objId), static_cast<qint16>(x),
        static_cast<qint16>(y), static_cast<quint16>(w), static_cast<quint16>(h), filled,
        static_cast<Sightline::OverlayPaletteColor>(fgColor), static_cast<Sightline::OverlayPaletteColor>(bgColor),
        static_cast<quint8>(alpha), static_cast<quint16>(thickness), originUpperLeft);
}

bool SightlineQmlBridge::drawText(int cam, int objId, int x, int y, const QString& text, int fontId, int fgColor,
    int bgColor, int hScale, int vScale, bool originUpperLeft)
{
    if (!m_activeOverlayIds.contains(objId)) {
        m_activeOverlayIds.append(objId);
        emit activeOverlayIdsChanged();
    }
    if (!isConnected()) {
        return true;
    }
    return m_device->drawText(static_cast<quint8>(cam), static_cast<quint8>(objId), static_cast<qint16>(x),
        static_cast<qint16>(y), text, static_cast<Sightline::OverlayFontId>(fontId),
        static_cast<Sightline::OverlayPaletteColor>(fgColor), static_cast<Sightline::OverlayPaletteColor>(bgColor),
        static_cast<quint8>(hScale), static_cast<quint8>(vScale), originUpperLeft);
}

bool SightlineQmlBridge::drawKlvField(int cam, int objId, int x, int y, int fieldTag, int formatType,
    const QString& formatString, int fontId, int fgColor, bool originUpperLeft)
{
    if (!m_activeOverlayIds.contains(objId)) {
        m_activeOverlayIds.append(objId);
        emit activeOverlayIdsChanged();
    }
    if (!isConnected()) {
        return true;
    }
    return m_device->drawKlvField(static_cast<quint8>(cam), static_cast<quint8>(objId), static_cast<qint16>(x),
        static_cast<qint16>(y), static_cast<Sightline::KlvFieldTag>(fieldTag),
        static_cast<Sightline::KlvFormatType>(formatType), formatString, static_cast<Sightline::OverlayFontId>(fontId),
        static_cast<Sightline::OverlayPaletteColor>(fgColor), originUpperLeft);
}

bool SightlineQmlBridge::drawBlackout(int cam, int objId, int width, int height)
{
    if (!m_activeOverlayIds.contains(objId)) {
        m_activeOverlayIds.append(objId);
        emit activeOverlayIdsChanged();
    }
    if (!isConnected()) {
        return true;
    }
    return m_device->drawBlackout(static_cast<quint8>(cam), static_cast<quint8>(objId), static_cast<quint16>(width),
        static_cast<quint16>(height));
}

bool SightlineQmlBridge::destroyOverlay(int cam, int objId)
{
    m_activeOverlayIds.removeAll(objId);
    emit activeOverlayIdsChanged();
    if (!isConnected()) {
        return true;
    }
    return m_device->destroyOverlay(static_cast<quint8>(cam), static_cast<quint8>(objId));
}

bool SightlineQmlBridge::destroyAllOverlays(int cam)
{
    m_activeOverlayIds.clear();
    emit activeOverlayIdsChanged();
    if (!isConnected()) {
        return true;
    }
    return m_device->destroyAllOverlays(static_cast<quint8>(cam));
}

bool SightlineQmlBridge::setLogoParameters(int cam, int opacity, int offsetX, int offsetY)
{
    if (!isConnected()) {
        emit logoParametersReceived(cam, opacity, offsetX, offsetY);
        return true;
    }
    Sightline::MsgLogoParameters msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.logoOpacity = static_cast<std::uint8_t>(opacity);
    msg.offsetX = static_cast<std::uint16_t>(offsetX);
    msg.offsetY = static_cast<std::uint16_t>(offsetY);
    return m_device->setLogoParameters(msg);
}

bool SightlineQmlBridge::getLogoParameters(int cam)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->getLogoParameters(static_cast<quint8>(cam));
}

bool SightlineQmlBridge::setUserFont(int slotIndex, const QString& fontPath)
{
    if (!isConnected()) {
        return true;
    }
    return m_device->setUserFont(static_cast<quint8>(slotIndex), fontPath);
}

bool SightlineQmlBridge::getOverlayObjectsIds(int cam)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->getOverlayObjectsIds(static_cast<quint8>(cam));
}

bool SightlineQmlBridge::getOverlayObjectParams(int objId)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->getOverlayObjectParams(static_cast<quint8>(objId));
}

bool SightlineQmlBridge::startCoolerCountdown(int cam, int durationSeconds)
{
    const int duration = (durationSeconds <= 0) ? 15 : durationSeconds;
    m_coolerCamera = cam;
    m_coolerRemaining = duration;

    // EAN Section 10: Step 1 - Draw blackout box (objId=1, 640x480 black)
    drawBlackout(cam, 1, 640, 480);

    // EAN Section 10: Step 2 - Draw countdown text (objId=2)
    const QString text = QString(QStringLiteral("Imager cooling down %1s")).arg(m_coolerRemaining);
    drawText(cam, 2, 20, 20, text, 0, 0, 14, 32, 32, true);

    if (m_coolerTimer) {
        m_coolerTimer->start(1000);
    }
    emit coolerCountdownChanged();
    return true;
}

void SightlineQmlBridge::cancelCoolerCountdown()
{
    if (m_coolerTimer && m_coolerTimer->isActive()) {
        m_coolerTimer->stop();
    }
    destroyOverlay(m_coolerCamera, 1);
    destroyOverlay(m_coolerCamera, 2);
    m_coolerRemaining = 0;
    emit coolerCountdownChanged();
    emit coolerCountdownFinished();
}

// 7. Telemetry & Metadata
bool SightlineQmlBridge::setReportingMode(int cam, int period, int flags)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->device()->setReportingMode(
        static_cast<std::uint8_t>(cam), static_cast<std::uint8_t>(period), static_cast<std::uint8_t>(flags));
}

bool SightlineQmlBridge::setMetadata(double lat, double lon, double alt, double heading, double pitch, double roll)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetMetadataValues msg {};
    msg.platformLatitudeDeg = lat;
    msg.platformLongitudeDeg = lon;
    msg.platformAltitudeMeters = alt;
    msg.platformHeadingDeg = heading;
    msg.platformPitchDeg = pitch;
    msg.platformRollDeg = roll;
    return m_device->device()->setMetadata(msg);
}

bool SightlineQmlBridge::setCursorOnTarget(int enable, int port, const QString& uid, const QString& type)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgCursorOnTarget msg {};
    msg.enable = static_cast<std::uint8_t>(enable);
    msg.broadcastPort = static_cast<std::uint16_t>(port);
    msg.uid = uid.toStdString();
    msg.cotType = type.toStdString();
    return m_device->device()->transport()->sendData(Sightline::SightlineProtocolBuilder::buildCursorOnTarget(msg));
}

// 8. System & Raw Inspection
bool SightlineQmlBridge::saveParameters(int commitType)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->saveParameters(static_cast<quint8>(commitType));
}

bool SightlineQmlBridge::resetParameters(int resetType)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->resetParameters(static_cast<quint8>(resetType));
}

bool SightlineQmlBridge::queryVersion()
{
    if (!isConnected()) {
        return false;
    }
    return m_device->queryVersion();
}

bool SightlineQmlBridge::sendRawHex(const QString& hexString)
{
    if (!isConnected()) {
        return false;
    }
    const QByteArray rawBytes = QByteArray::fromHex(hexString.toUtf8());
    if (rawBytes.isEmpty()) {
        return false;
    }
    return m_device->sendRawPacket(rawBytes);
}

bool SightlineQmlBridge::queryParameters(int queryId)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->device()->queryParameters(static_cast<std::uint8_t>(queryId));
}

void SightlineQmlBridge::queryModuleParameters(int tabIndex)
{
    if (!isConnected()) {
        return;
    }

    switch (tabIndex) {
    case 0: // Tracking
        queryParameters(static_cast<int>(Sightline::MessageId::SetTrackingParameters)); // 0x0C
        queryParameters(static_cast<int>(Sightline::MessageId::CoordinateReportingMode)); // 0x0B
        break;
    case 1: // Stabilization
        m_device->getStabilization(0U);
        m_device->getRegistration(0U);
        m_device->getStabilizationBias(0U);
        break;
    case 2: // Detection
        queryParameters(static_cast<int>(Sightline::MessageId::SetDetectionParameters)); // 0x2D
        queryParameters(static_cast<int>(Sightline::MessageId::SetVMTI)); // 0x84
        break;
    case 3: // Classification
        queryParameters(static_cast<int>(Sightline::MessageId::CustomAIDetect)); // 0xBA
        queryParameters(static_cast<int>(Sightline::MessageId::TrackingMultiClass)); // 0xBD
        break;
    case 4: // Landing Aid
        queryParameters(static_cast<int>(Sightline::MessageId::LandingAid)); // 0x81
        break;
    case 5: // Capture
        queryParameters(static_cast<int>(Sightline::MessageId::SetVideoParameters)); // 0x10
        queryParameters(static_cast<int>(Sightline::MessageId::CameraCapabilities)); // 0xBB
        break;
    case 6: // Display
        queryParameters(static_cast<int>(Sightline::MessageId::SetDisplayParameters)); // 0x16
        queryParameters(static_cast<int>(Sightline::MessageId::VideoDisplay)); // 0xA4
        break;
    case 7: // Enhancement
        queryParameters(static_cast<int>(Sightline::MessageId::SetVideoEnhancementParameters)); // 0x21
        queryParameters(static_cast<int>(Sightline::MessageId::Noise3D)); // 0xAF
        break;
    case 8: // Compression
        queryEncoderParams(0);
        queryEncoderParams(1);
        queryDisplayParams(0);
        queryDisplayParams(1);
        queryParameters(static_cast<int>(Sightline::MessageId::StreamingControl)); // 0x90
        queryParameters(static_cast<int>(Sightline::MessageId::DecoderParameters)); // 0x99
        break;
    case 9: // Blending
        queryParameters(static_cast<int>(Sightline::MessageId::SetBlendParameters)); // 0x2B
        queryParameters(static_cast<int>(Sightline::MessageId::BlendAlign)); // 0xB9
        break;
    case 10: // Overlays
        queryParameters(static_cast<int>(Sightline::MessageId::SetOverlayMode)); // 0x06
        queryParameters(static_cast<int>(Sightline::MessageId::LogoParameters)); // 0x9B
        if (m_device) {
            m_device->getOverlayObjectsIds(0U);
        }
        break;
    case 11: // Focus & Lens
        queryParameters(static_cast<int>(Sightline::MessageId::SetLensParameters)); // 0x6E
        break;
    case 12: // NUC Calibration
        queryParameters(static_cast<int>(Sightline::MessageId::NucParameters)); // 0x35
        queryParameters(static_cast<int>(Sightline::MessageId::DeadPixel)); // 0xA8
        break;
    case 13: // Telemetry
        queryParameters(0x13); // Metadata values
        queryParameters(0x64); // Telemetry destination
        break;
    case 14: // KLV Metadata
        queryParameters(static_cast<int>(Sightline::MessageId::CurrentMetadataRate)); // 0x8D / 0x62
        break;
    case 15: // Recording
        queryParameters(0x5F); // SnapShot status
        break;
    case 16: // Network
        queryNetworkParams();
        queryParameters(static_cast<int>(Sightline::MessageId::SetSystemValue)); // 0x92 (traffic control)
        break;
    case 17: // Serial Port
        queryParameters(static_cast<int>(Sightline::MessageId::SetPortConfiguration)); // 0x3E
        queryParameters(static_cast<int>(Sightline::MessageId::GPIO)); // 0xB6
        break;
    case 18: // General System
        queryVersion(); // 0x00
        queryParameters(static_cast<int>(Sightline::MessageId::SystemStatusMode)); // 0x80
        break;
    default:
        break;
    }

    emit moduleQueryDispatched(tabIndex);
}

void SightlineQmlBridge::handleTrackingPositions(const Sightline::MsgTrackingPositions& pos)
{
    if (m_trackListModel) {
        m_trackListModel->updateTracks(pos.tracks);
    }
}

void SightlineQmlBridge::handleTrackingPositionsExtended(const Sightline::MsgTrackingPositionsExtended& ext)
{
    if (m_trackListModel) {
        m_trackListModel->updateTracks(ext.tracks);
    }
}

void SightlineQmlBridge::handleTrackingParameters(const Sightline::MsgSetTrackingParameters& p)
{
    const int cam = static_cast<int>(p.cameraIndex);
    if (cam >= 0 && cam < 4) {
        m_cachedTrackingParams[static_cast<std::size_t>(cam)] = p;
    }
    emit trackingParametersReceived(cam, static_cast<int>(p.mode), static_cast<int>(p.flags),
        static_cast<int>(p.maxMisses), static_cast<int>(p.zoomSmoothing),
        static_cast<int>(p.rollSmoothing), static_cast<int>(p.maxPauseTime),
        static_cast<int>(p.acquisitionSearchCol), static_cast<int>(p.acquisitionSearchRow));
}

void SightlineQmlBridge::handleUserWarning(const Sightline::MsgUserWarningMessage& warn)
{
    m_lastWarningMessage = QString::fromStdString(warn.message);
    emit warningReceived();
}

void SightlineQmlBridge::handleVersion(const Sightline::MsgVersionNumber& ver)
{
    m_softwareVersion = QString::fromStdString(ver.versionString);
    if (m_softwareVersion.isEmpty()) {
        m_softwareVersion = QString("v%1.%2.%3").arg(ver.softwareMajor).arg(ver.softwareMinor).arg(ver.softwareRelease);
    }
    m_coreTempC = static_cast<int>(ver.degreesC);
    emit versionReceived();
    emit systemStatusChanged();
}

void SightlineQmlBridge::handleSystemStatus(const Sightline::MsgSystemStatusMessage& stat)
{
    m_cpuLoadPercent = static_cast<int>(stat.cpuLoadPercent);
    m_coreTempC = static_cast<int>(stat.coreTempC);
    if (m_connectionTimer.isValid()) {
        m_uptimeSeconds = static_cast<int>(m_connectionTimer.elapsed() / 1000);
    }
    emit systemStatusChanged();
}

void SightlineQmlBridge::handleRawFrame(bool isTx, const QByteArray& data)
{
    TrafficLogEntry entry {};
    entry.timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    entry.isTx = isTx;
    entry.length = static_cast<int>(data.size());

    const std::vector<std::uint8_t> stdPacket(reinterpret_cast<const std::uint8_t*>(data.constData()),
        reinterpret_cast<const std::uint8_t*>(data.constData()) + data.size());

    const auto id = Sightline::SightlineProtocolParser::identifyMessage(stdPacket);
    entry.messageName = QString::fromUtf8(Sightline::messageIdToString(id).data());

    if (data.size() >= 4 && static_cast<std::uint8_t>(data[0]) == Sightline::HeaderByte1
        && static_cast<std::uint8_t>(data[1]) == Sightline::HeaderByte2) {
        std::size_t hLen = 3U;
        if ((static_cast<std::uint8_t>(data[2]) & 0x80U) != 0U) {
            hLen = 4U;
        }
        if (stdPacket.size() > hLen) {
            const std::uint8_t computedCrc
                = Sightline::SightlineCrc8::compute(stdPacket.data() + hLen, stdPacket.size() - hLen - 1U);
            entry.crcOk = (computedCrc == stdPacket.back());
        }
    }

    entry.hexPayload = data.toHex(' ').toUpper();
    m_trafficLogModel->addEntry(entry);
}

void SightlineQmlBridge::handleStabilizationParams(const Sightline::MsgSetStabilizationParameters& p)
{
    emit stabilizationChanged(p.cameraIndex, p.mode, p.rate, p.translationLimit, p.angleLimit, p.maxStabOff);
}

void SightlineQmlBridge::handleRegistrationParams(const Sightline::MsgSetRegistrationParameters& p)
{
    emit registrationChanged(
        p.cameraIndex, p.maxTranslation, p.maxRotation, p.zoomRange, p.left, p.right, p.top, p.bottom, p.updateRate);
}

void SightlineQmlBridge::handleStabilizationBias(const Sightline::MsgSetStabilizationBias& b)
{
    emit stabilizationBiasChanged(b.cameraIndex, b.biasCol, b.biasRow, b.autoBias, b.updateRate);
}

void SightlineQmlBridge::handleOverlayMode(const Sightline::MsgSetOverlayMode& m)
{
    emit overlayModeReceived(m.cameraIndex, m.primaryReticle, m.secondaryReticle, m.graphics);
}

void SightlineQmlBridge::handleOverlayObjectsIds(const Sightline::MsgCurrentOverlayObjectsIds& ids)
{
    m_activeOverlayIds.clear();
    const auto active = ids.getActiveObjectIds();
    for (const auto id : active) {
        m_activeOverlayIds.append(static_cast<int>(id));
    }
    emit activeOverlayIdsChanged();
}

void SightlineQmlBridge::handleOverlayObjectParams(const Sightline::MsgCurrentOverlayObjectParameters& p)
{
    emit overlayObjectParamsReceived(p.objectId, static_cast<int>(p.type), p.a, p.b);
}

void SightlineQmlBridge::handleLogoParameters(const Sightline::MsgLogoParameters& l)
{
    emit logoParametersReceived(l.cameraIndex, l.logoOpacity, l.offsetX, l.offsetY);
}

void SightlineQmlBridge::handleDetectionParams(const Sightline::MsgSetDetectionParameters& det)
{
    emit detectionParamsReceived(det.cameraIndex, det.detectionIndex, static_cast<int>(det.mode),
        static_cast<int>(det.sensitivityMode), det.threshold, det.minTargetSize, det.maxTargetSize);
}

void SightlineQmlBridge::handleAdvDetection(const Sightline::MsgAdvancedDetectionParameters& adv)
{
    emit advDetectionReceived(adv.cameraIndex, adv.updateRate, adv.surroundSize, static_cast<int>(adv.blobDirection),
        adv.use8BitImages, adv.gasAddOriginal, adv.gasColor, adv.aiIouThreshold, adv.enableMtd,
        static_cast<int>(adv.downsample));
}

void SightlineQmlBridge::handleDetectionROI(const Sightline::MsgDetectionROI& roi)
{
    emit detectionRoiReceived(roi.cameraIndex, roi.detectionIndex, roi.roiIndex, static_cast<int>(roi.geometryMode),
        roi.lineLeftX, roi.lineLeftY, roi.lineRightX, roi.lineRightY, static_cast<int>(roi.lineSide));
}

void SightlineQmlBridge::handleKlvMetricFilters(const Sightline::MsgKlvMetricFilters& filters)
{
    emit klvMetricFiltersReceived(filters.cameraIndex, filters.minTargetWidthM, filters.maxTargetWidthM,
        filters.minTargetHeightM, filters.maxTargetHeightM, filters.filterAboveHorizon, filters.filterBelowHorizon,
        filters.minLatitude, filters.maxLatitude, filters.minLongitude, filters.maxLongitude);
}

void SightlineQmlBridge::onCoolerTimerTick()
{
    if (m_coolerRemaining > 0) {
        --m_coolerRemaining;
        emit coolerCountdownChanged();
        if (m_coolerRemaining > 0) {
            const QString text = QString(QStringLiteral("Imager cooling down %1s")).arg(m_coolerRemaining);
            drawText(m_coolerCamera, 2, 20, 20, text, 0, 0, 14, 32, 32, true);
        } else {
            if (m_coolerTimer) {
                m_coolerTimer->stop();
            }
            destroyOverlay(m_coolerCamera, 1);
            destroyOverlay(m_coolerCamera, 2);
            emit coolerCountdownFinished();
        }
    } else {
        if (m_coolerTimer) {
            m_coolerTimer->stop();
        }
    }
}

void SightlineQmlBridge::handleCommandAck(const Sightline::MsgCommandAck& ack)
{
    QString statusDesc = QStringLiteral("Success");
    switch (ack.statusCode) {
    case Sightline::RecordingStatusCode::Success:
        statusDesc = QStringLiteral("Success (0x00)");
        break;
    case Sightline::RecordingStatusCode::ErrMalformedPayload:
        statusDesc = QStringLiteral("Error: Malformed Payload (0x01)");
        break;
    case Sightline::RecordingStatusCode::ErrMediaUnavailable:
        statusDesc = QStringLiteral("Error: Media Unavailable (0x02)");
        break;
    case Sightline::RecordingStatusCode::ErrMediaReadOnly:
        statusDesc = QStringLiteral("Error: Media Read-Only (0x03)");
        break;
    case Sightline::RecordingStatusCode::ErrInsufficientStorage:
        statusDesc = QStringLiteral("Error: Insufficient Storage (0x04)");
        break;
    case Sightline::RecordingStatusCode::ErrNumericFilename:
        statusDesc = QStringLiteral("Error: Numeric Filename Conflict (0x05)");
        break;
    case Sightline::RecordingStatusCode::ErrInvalidCharacters:
        statusDesc = QStringLiteral("Error: Invalid Characters (0x06)");
        break;
    case Sightline::RecordingStatusCode::ErrBusyFlushing:
        statusDesc = QStringLiteral("Error: Storage Busy Flushing (0x07)");
        break;
    case Sightline::RecordingStatusCode::ErrChannelUnsupported:
        statusDesc = QStringLiteral("Error: Channel Unsupported (0x08)");
        break;
    default:
        statusDesc = QStringLiteral("Status 0x%1").arg(static_cast<int>(ack.statusCode), 2, 16, QChar('0'));
        break;
    }
    m_lastAckStatus = QStringLiteral("ACK Seq %1 [%2]").arg(ack.sequenceId).arg(statusDesc);
    emit commandAckReceived(ack.sequenceId, static_cast<int>(ack.statusCode), statusDesc);
}

void SightlineQmlBridge::handleRecordingEvent(const Sightline::MsgFileRecordingEvent& ev)
{
    QString evTypeStr = QStringLiteral("Event");
    switch (ev.eventType) {
    case Sightline::RecordingEventType::Started:
        evTypeStr = QStringLiteral("Started");
        m_isRecordingActive = true;
        emit recordingActiveChanged(true);
        break;
    case Sightline::RecordingEventType::Stopped:
        evTypeStr = QStringLiteral("Stopped");
        m_isRecordingActive = false;
        if (m_recordingClockTimer && m_recordingClockTimer->isActive()) {
            m_recordingClockTimer->stop();
        }
        emit recordingActiveChanged(false);
        break;
    case Sightline::RecordingEventType::FileSplit:
        evTypeStr = QStringLiteral("Split / Rollover");
        break;
    case Sightline::RecordingEventType::LowWatermark:
        evTypeStr = QStringLiteral("Low Storage Watermark");
        break;
    case Sightline::RecordingEventType::CriticalStorage:
        evTypeStr = QStringLiteral("Critical Storage (< 2%)");
        break;
    case Sightline::RecordingEventType::BufferOverrun:
        evTypeStr = QStringLiteral("Buffer Overrun!");
        break;
    case Sightline::RecordingEventType::SnapshotSaved:
        evTypeStr = QStringLiteral("Snapshot Saved");
        break;
    case Sightline::RecordingEventType::FifoPruned:
        evTypeStr = QStringLiteral("FIFO Pruned Oldest");
        break;
    default:
        break;
    }

    m_lastRecordingEvent = QStringLiteral("[%1] %2 %3")
                               .arg(evTypeStr)
                               .arg(QString::fromStdString(ev.eventPayload))
                               .arg(ev.freeStorageMB > 0 ? QStringLiteral("(%1 MB free)").arg(ev.freeStorageMB) : QString());
    emit recordingEventReceived(m_lastRecordingEvent);
}

void SightlineQmlBridge::handleRecordingStatus(const Sightline::MsgCurrentRecordingStatusV2& stat)
{
    m_isRecordingActive = (stat.recordingState != 0U);
    m_currentBitrateKbps = static_cast<int>(stat.currentBitrateKbps);
    m_droppedFrames = static_cast<int>(stat.droppedFrames);
    m_freeStorageMB = static_cast<int>(stat.freeStorageMB);
    m_currentFilename = QString::fromStdString(stat.activeFilename);

    const quint64 usedMB = stat.totalBytesWritten / (1024ULL * 1024ULL);
    m_usedStorageMB = static_cast<int>(usedMB);
    const quint64 totalMB = usedMB + stat.freeStorageMB;
    if (totalMB > 0ULL) {
        m_storageUsagePercent = (static_cast<double>(usedMB) / static_cast<double>(totalMB)) * 100.0;
    }

    emit recordingStatusChanged();
}

void SightlineQmlBridge::handleDirListingReply(const Sightline::MsgDirectoryListingReply& rep)
{
    if (m_recordingFileListModel) {
        if (rep.startIndex == 0U) {
            m_recordingFileListModel->updateEntries(rep.entries);
        } else {
            m_recordingFileListModel->appendEntries(rep.entries);
        }
    }
}

void SightlineQmlBridge::onRecordingClockTick()
{
    if (m_isRecordingActive) {
        m_elapsedRecordingSec++;
        emit recordingClockChanged();
    }
}

void SightlineQmlBridge::handleH264Params(const Sightline::MsgSetH264Parameters& p)
{
    m_encBitrateKbps = static_cast<int>(p.targetBitrateBps / 1000U);
    m_encGopInterval = static_cast<int>(p.intraFrameInterval);
    m_encProfile = static_cast<int>(p.flags & 0x03U);
    m_encRateControl = static_cast<int>((p.flags >> 4) & 0x03U);
    m_encMinQp = static_cast<int>(p.minQp);
    m_encMaxQp = static_cast<int>(p.maxQp);
    m_encAirMb = static_cast<int>(p.airMbPeriod);
    m_encSliceRows = static_cast<int>(p.sliceRefreshRowNumber);

    emit encParamsChanged();
    const int streamIdx = (p.displayId == 0x0080U) ? 1 : (p.displayId == 0x0200U ? 2 : 0);
    emit encoderParamsReceived(streamIdx, m_encBitrateKbps, m_encGopInterval, p.flags, m_encMinQp, m_encMaxQp);
}

void SightlineQmlBridge::handleEthernetDisplay(const Sightline::MsgSetEthernetDisplayParameters& p)
{
    m_netDisplayProtocol = static_cast<int>(p.protocol);
    m_netDisplayIp = QHostAddress(p.ipAddress).toString();
    m_netDisplayPort = static_cast<int>(p.port);
    m_netMaxPacket = static_cast<int>(p.maxPacket);

    emit netDisplayChanged();
    const int streamIdx = (p.displayId == 0x0080U) ? 1 : (p.displayId == 0x0200U ? 2 : 0);
    emit displayParamsReceived(streamIdx, m_netDisplayProtocol, m_netDisplayIp, m_netDisplayPort, m_netMaxPacket);
}

void SightlineQmlBridge::handleEthernetVideo(const Sightline::MsgSetEthernetVideoParameters& p)
{
    static_cast<void>(p);
    emit netVideoChanged();
}

void SightlineQmlBridge::handleNetworkParams(const Sightline::MsgSetNetworkParameters& p)
{
    static_cast<void>(p);
}

void SightlineQmlBridge::handleNetworkList(const Sightline::MsgCurrentNetworkList& l)
{
    static_cast<void>(l);
}

void SightlineQmlBridge::handleSystemValue(const Sightline::MsgSystemValue& val)
{
    if (val.systemValueId == Sightline::MsgSystemValue::TrafficControl) {
        m_tcRateKbps = static_cast<int>(val.value);
        m_tcBurstBytes = static_cast<int>(val.value1);
        m_tcMtuBytes = static_cast<int>(val.value2);
        emit tcStatusChanged();
        emit trafficControlReceived(m_tcRateKbps, m_tcBurstBytes, m_tcMtuBytes);
    }
}

