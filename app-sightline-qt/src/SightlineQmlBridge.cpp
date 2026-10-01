/// @file SightlineQmlBridge.cpp
/// @brief Implementation of Sightline QML Bridge controller.

#include "SightlineQmlBridge.h"

#include <SightlineCore/SightlineCrc8.h>
#include <SightlineCore/SightlineProtocolParser.h>

#include <QDateTime>
#include <algorithm>

SightlineQmlBridge::SightlineQmlBridge(QObject* parent)
    : QObject(parent)
    , m_trackListModel(std::make_unique<TrackListModel>(this))
    , m_trafficLogModel(std::make_unique<TrafficLogModel>(this))
{
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
    connect(m_device.get(), &QSightlineDevice::rawFrameReceived, this, &SightlineQmlBridge::handleRawFrame);

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
    if (!isConnected()) {
        return false;
    }
    return m_device->startTracking(static_cast<quint8>(cam), static_cast<quint16>(col), static_cast<quint16>(row),
        static_cast<quint16>(w), static_cast<quint16>(h), static_cast<quint8>(flags));
}

bool SightlineQmlBridge::stopTracking(int cam, int trackId)
{
    if (!isConnected()) {
        return false;
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
    if (!isConnected()) {
        return false;
    }
    return m_device->designatePrimary(static_cast<quint8>(cam), static_cast<quint8>(trackId));
}

// 2. Stabilization
bool SightlineQmlBridge::setStabilization(int cam, int mode, int autoBias, int maxShift)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->setStabilization(static_cast<quint8>(cam), static_cast<quint8>(mode),
        static_cast<quint8>(autoBias), static_cast<quint8>(maxShift));
}

bool SightlineQmlBridge::resetStabilization(int cam)
{
    if (!isConnected()) {
        return false;
    }
    return m_device->resetStabilization(static_cast<quint8>(cam));
}

// 3. Detection & AI Classification
bool SightlineQmlBridge::setDetectionParams(int cam, int mode, int threshold, int minSize, int maxSize)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetDetectionParameters msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.mode = static_cast<std::uint8_t>(mode);
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
        queryParameters(static_cast<int>(Sightline::MessageId::SetStabilizationParameters)); // 0x02
        queryParameters(static_cast<int>(Sightline::MessageId::SetStabilizationBias)); // 0x12
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
