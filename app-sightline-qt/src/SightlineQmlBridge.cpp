/// @file SightlineQmlBridge.cpp
/// @brief Implementation of Sightline QML Bridge controller.

#include "SightlineQmlBridge.h"

#include <SightlineCore/SightlineCrc8.h>
#include <SightlineCore/SightlineProtocolParser.h>

#include <QDateTime>

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
    disconnectDevice();

    m_host = host;
    m_commandPort = cmdPort;
    m_replyPort = replyPort;

    auto transport = std::make_shared<Transport::SightlineUdpTransport>(
        host.toStdString(), static_cast<std::uint16_t>(cmdPort), static_cast<std::uint16_t>(replyPort));

    m_device = std::make_unique<QSightlineDevice>(transport, this);

    connect(m_device.get(), &QSightlineDevice::connectionStateChanged, this, [this](bool ok) {
        emit connectionChanged();
        if (ok) {
            m_device->queryVersion();
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
    m_softwareVersion = tr("Disconnected");
    m_trackListModel->clearTracks();
    emit connectionChanged();
    emit versionReceived();
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
    Sightline::MsgSetVideoParameters msg {};
    msg.cameraIndex = static_cast<std::uint8_t>(cam);
    msg.inputFormat = static_cast<std::uint8_t>(format);
    msg.width = static_cast<std::uint16_t>(width);
    msg.height = static_cast<std::uint16_t>(height);
    msg.frameRate = static_cast<std::uint8_t>(fps);
    return m_device->device()->setVideoParams(msg);
}

bool SightlineQmlBridge::setH264Params(int stream, int bitrate, int gop, int quality, int rateCtrl)
{
    if (!isConnected()) {
        return false;
    }
    Sightline::MsgSetH264Parameters msg {};
    msg.streamIndex = static_cast<std::uint8_t>(stream);
    msg.targetBitrateBps = static_cast<std::uint32_t>(bitrate);
    msg.gopLength = static_cast<std::uint16_t>(gop);
    msg.qualityLevel = static_cast<std::uint8_t>(quality);
    msg.rateControl = static_cast<std::uint8_t>(rateCtrl);
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
    msg.primaryCamera = static_cast<std::uint8_t>(cam1);
    msg.secondaryCamera = static_cast<std::uint8_t>(cam2);
    msg.blendMode = static_cast<std::uint8_t>(mode);
    msg.alphaPercent = static_cast<std::uint8_t>(alpha);
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
        m_softwareVersion
            = QString("SLA Firmware v%1.%2.%3").arg(ver.softwareMajor).arg(ver.softwareMinor).arg(ver.softwarePatch);
    }
    emit versionReceived();
}

void SightlineQmlBridge::handleSystemStatus(const Sightline::MsgSystemStatusMessage& stat)
{
    m_cpuLoadPercent = static_cast<int>(stat.cpuLoadPercent / 10U);
    m_coreTempC = static_cast<int>(stat.coreTempC);
    m_uptimeSeconds = static_cast<int>(stat.uptimeSeconds);
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
