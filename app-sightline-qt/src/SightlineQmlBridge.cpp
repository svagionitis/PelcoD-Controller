/// @file SightlineQmlBridge.cpp
/// @brief Implementation of Sightline QML Bridge controller.

#include "SightlineQmlBridge.h"

#include <SightlineCore/SightlineCrc8.h>
#include <SightlineCore/SightlineProtocolParser.h>
#include <SightlineCore/modules/SightlineOverlay.h>
#include <SightlineCore/modules/SightlineStabilization.h>
#include <SightlineCore/modules/SightlineStabilizationBuilder.h>

#include <QDateTime>
#include <QFile>
#include <QSettings>
#include <algorithm>

SightlineQmlBridge::SightlineQmlBridge(QObject* parent)
    : QObject(parent)
    , m_coolerTimer(std::make_unique<QTimer>(this))
    , m_trackListModel(std::make_unique<TrackListModel>(this))
    , m_trafficLogModel(std::make_unique<TrafficLogModel>(this))
{
    connect(m_coolerTimer.get(), &QTimer::timeout, this, &SightlineQmlBridge::onCoolerTimerTick);
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

    const bool started = m_device->start();
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
        queryParameters(static_cast<int>(Sightline::MessageId::SetH264Parameters)); // 0x23
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
        queryParameters(static_cast<int>(Sightline::MessageId::SetNetworkParameters)); // 0x1C
        queryParameters(0x66); // Network list
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
    m_trackListModel->updateTracks(pos.tracks);
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
