/// @file SightlineDevice.cpp
/// @brief Implementation of Sightline SLA high-level controller and message dispatcher.

#include "SightlineDevice.h"

namespace Sightline {

SightlineDevice::SightlineDevice(std::shared_ptr<Transport::ITransport> transport, std::size_t maxAccumulatorBuffer)
    : m_transport(std::move(transport))
    , m_accumulator(maxAccumulatorBuffer)
{
}

SightlineDevice::~SightlineDevice()
{
    stop();
}

bool SightlineDevice::start()
{
    if (m_started.exchange(true)) {
        return true;
    }

    if (!m_transport) {
        m_started.store(false);
        return false;
    }

    m_transport->setDataCallback([this](const std::vector<std::uint8_t>& data) { handleIncomingBytes(data); });

    if (!m_transport->isOpen()) {
        if (!m_transport->open()) {
            m_started.store(false);
            return false;
        }
    }

    return true;
}

void SightlineDevice::stop()
{
    if (!m_started.exchange(false)) {
        return;
    }

    if (m_transport) {
        m_transport->setDataCallback(nullptr);
        m_transport->close();
    }

    m_accumulator.clear();
}

bool SightlineDevice::isConnected() const noexcept
{
    return m_transport && m_transport->isOpen();
}

std::shared_ptr<Transport::ITransport> SightlineDevice::transport() const noexcept
{
    return m_transport;
}

Transport::TransportStatsSnapshot SightlineDevice::getTransportStats() const
{
    if (m_transport) {
        return m_transport->getStats();
    }
    return {};
}

// ==============================================================================
// 1. Tracking Commands
// ==============================================================================

bool SightlineDevice::startTracking(std::uint8_t cameraIndex, std::uint16_t col, std::uint16_t row, std::uint16_t width,
    std::uint16_t height, std::uint8_t flags)
{
    MsgStartTracking msg {};
    msg.cameraIndex = cameraIndex;
    msg.centerCol = col;
    msg.centerRow = row;
    msg.width = width;
    msg.height = height;
    msg.flags = flags;

    return sendPacket(SightlineProtocolBuilder::buildStartTracking(msg));
}

bool SightlineDevice::stopTracking(std::uint8_t cameraIndex, std::uint8_t trackId)
{
    if (trackId == 0xFFU) {
        MsgStopTracking msg {};
        msg.cameraIndex = cameraIndex;
        msg.trackId = 0xFFU;
        return sendPacket(SightlineProtocolBuilder::buildStopTracking(msg));
    }

    MsgModifyTrackIndex modifyMsg {};
    modifyMsg.trackIndex = trackId;
    modifyMsg.flags = 0U; // 0 = Stop track
    modifyMsg.cameraIndex = cameraIndex;
    return sendPacket(SightlineProtocolBuilder::buildModifyTrackIndex(modifyMsg));
}

bool SightlineDevice::modifyTracking(
    std::uint8_t cameraIndex, std::uint8_t trackId, std::uint8_t mode, std::uint8_t flags)
{
    MsgModifyTracking msg {};
    msg.cameraIndex = cameraIndex;
    msg.trackId = trackId;
    msg.mode = mode;
    msg.flags = flags;

    return sendPacket(SightlineProtocolBuilder::buildModifyTracking(msg));
}

bool SightlineDevice::nudgeTracking(std::uint8_t cameraIndex, std::int16_t deltaCol, std::int16_t deltaRow)
{
    MsgNudgeTrackingCoordinate msg {};
    msg.cameraIndex = cameraIndex;
    msg.deltaCol = deltaCol;
    msg.deltaRow = deltaRow;

    return sendPacket(SightlineProtocolBuilder::buildNudgeTracking(msg));
}

bool SightlineDevice::setReportingMode(std::uint8_t cameraIndex, std::uint8_t framePeriod, std::uint8_t reportingFlags)
{
    MsgCoordinateReportingMode msg {};
    msg.cameraIndex = cameraIndex;
    msg.framePeriod = framePeriod;
    msg.reportingFlags = reportingFlags;

    return sendPacket(SightlineProtocolBuilder::buildSetReportingMode(msg));
}

bool SightlineDevice::designatePrimary(std::uint8_t cameraIndex, std::uint8_t trackId)
{
    MsgDesignateSelectedTrackPrimary msg {};
    msg.cameraIndex = cameraIndex;
    msg.trackId = trackId;

    return sendPacket(SightlineProtocolBuilder::buildDesignatePrimary(msg));
}

bool SightlineDevice::shiftTrack(
    std::uint8_t cameraIndex, std::uint8_t trackId, std::int16_t shiftCol, std::int16_t shiftRow)
{
    MsgShiftSelectedTrack msg {};
    msg.cameraIndex = cameraIndex;
    msg.trackId = trackId;
    msg.shiftCol = shiftCol;
    msg.shiftRow = shiftRow;

    return sendPacket(SightlineProtocolBuilder::buildShiftSelectedTrack(msg));
}

bool SightlineDevice::stopTrack(std::uint8_t cameraIndex, std::uint8_t trackId)
{
    MsgStopSelectedTrack msg {};
    msg.cameraIndex = cameraIndex;
    msg.trackId = trackId;

    return sendPacket(SightlineProtocolBuilder::buildStopSelectedTrack(msg));
}

bool SightlineDevice::setDetection(const MsgSetDetectionParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetDetectionParams(msg));
}

bool SightlineDevice::customAIDetect(const MsgCustomAIDetect& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildCustomAIDetect(msg));
}

// ==============================================================================
// 2. Stabilization Commands
// ==============================================================================

bool SightlineDevice::setStabilization(const MsgSetStabilizationParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetStabilization(msg));
}

bool SightlineDevice::setStabilization(
    std::uint8_t cameraIndex, std::uint8_t mode, std::uint8_t rate, std::uint8_t maxStabOff)
{
    MsgSetStabilizationParameters msg {};
    msg.cameraIndex = cameraIndex;
    msg.mode = mode;
    msg.rate = rate;
    msg.maxStabOff = maxStabOff;

    return sendPacket(SightlineProtocolBuilder::buildSetStabilization(msg));
}

bool SightlineDevice::resetStabilization(std::uint8_t cameraIndex)
{
    MsgResetStabilizationParameters msg {};
    msg.cameraIndex = cameraIndex;

    return sendPacket(SightlineProtocolBuilder::buildResetStabilization(msg));
}

bool SightlineDevice::setStabilizationBias(
    std::uint8_t cameraIndex, std::int16_t biasCol, std::int16_t biasRow, std::int16_t biasRotation)
{
    MsgSetStabilizationBias msg {};
    msg.cameraIndex = cameraIndex;
    msg.biasCol = biasCol;
    msg.biasRow = biasRow;
    msg.biasRotation = biasRotation;

    return sendPacket(SightlineProtocolBuilder::buildSetStabilizationBias(msg));
}

bool SightlineDevice::setBlend(const MsgSetBlendParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetBlendParameters(msg));
}

bool SightlineDevice::setNoise3D(const MsgNoise3D& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetNoise3D(msg));
}

// ==============================================================================
// 3. Video Pipeline & Display Commands
// ==============================================================================

bool SightlineDevice::setVideoParams(const MsgSetVideoParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetVideoParameters(msg));
}

bool SightlineDevice::setVideoMode(const MsgSetVideoMode& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetVideoMode(msg));
}

bool SightlineDevice::setVideoEnhance(const MsgSetVideoEnhancement& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetVideoEnhance(msg));
}

bool SightlineDevice::setVideoEnhanceFull(const MsgSetVideoEnhancementFull& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetVideoEnhanceFull(msg));
}

bool SightlineDevice::setFalseColor(std::uint8_t cameraIndex, FalseColorPalette palette)
{
    return sendPacket(SightlineProtocolBuilder::buildSetFalseColor(cameraIndex, palette));
}

bool SightlineDevice::setUserPalette(const MsgUserPalette& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetUserPalette(msg));
}

bool SightlineDevice::setDisplayParams(const MsgSetDisplayParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetDisplayParams(msg));
}

bool SightlineDevice::setEthernetVideo(const MsgSetEthernetVideoParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetEthernetVideo(msg));
}

bool SightlineDevice::setEthernetDisplay(const MsgSetEthernetDisplayParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetEthernetDisplay(msg));
}

bool SightlineDevice::setH264Params(const MsgSetH264Parameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetH264Parameters(msg));
}

bool SightlineDevice::setSDRecording(const MsgSetSDRecordingParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetSDRecording(msg));
}

bool SightlineDevice::streamingControl(std::uint8_t streamIndex, std::uint8_t action)
{
    MsgStreamingControl msg {};
    msg.streamIndex = streamIndex;
    msg.action = action;

    return sendPacket(SightlineProtocolBuilder::buildStreamingControl(msg));
}

// ==============================================================================
// 4. Metadata & KLV Commands
// ==============================================================================

bool SightlineDevice::setMetadata(const MsgSetMetadataValues& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetMetadataValues(msg));
}

bool SightlineDevice::setMetadataRate(std::uint8_t metadataType, std::uint8_t ratePeriod)
{
    MsgSetMetadataRate msg {};
    msg.metadataType = metadataType;
    msg.ratePeriod = ratePeriod;

    return sendPacket(SightlineProtocolBuilder::buildSetMetadataRate(msg));
}

bool SightlineDevice::setTelemetryDest(const MsgSetTelemetryDestination& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetTelemetryDest(msg));
}

// ==============================================================================
// 5. Lens & Optics Commands
// ==============================================================================

bool SightlineDevice::sendLensCommand(std::uint8_t cameraIndex, std::uint8_t commandType, std::int16_t rateOrPosition)
{
    MsgLensCommand msg {};
    msg.cameraIndex = cameraIndex;
    msg.commandType = commandType;
    msg.rateOrPosition = rateOrPosition;

    return sendPacket(SightlineProtocolBuilder::buildLensCommand(msg));
}

bool SightlineDevice::setFocusParams(const MsgFocusParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildFocusParameters(msg));
}

bool SightlineDevice::setLensParams(const MsgSetLensParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetLensParameters(msg));
}

bool SightlineDevice::setGPIO(const MsgGPIO& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildGPIO(msg));
}

// ==============================================================================
// 6. Overlays & Graphics Commands
// ==============================================================================

bool SightlineDevice::setOverlayMode(const MsgSetOverlayMode& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetOverlayMode(msg));
}

bool SightlineDevice::drawObject(const MsgDrawObject& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildDrawObject(msg));
}

bool SightlineDevice::drawOverlay(const MsgDrawOverlay& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildDrawOverlay(msg));
}

// ==============================================================================
// 7. System & Maintenance Commands
// ==============================================================================

bool SightlineDevice::saveParameters(std::uint8_t commitType)
{
    MsgSaveParameters msg {};
    msg.commitType = commitType;
    return sendPacket(SightlineProtocolBuilder::buildSaveParameters(msg));
}

bool SightlineDevice::resetParameters(std::uint8_t resetType)
{
    MsgResetAllParameters msg {};
    msg.resetType = resetType;
    return sendPacket(SightlineProtocolBuilder::buildResetAllParameters(msg));
}

bool SightlineDevice::setNetwork(const MsgSetNetworkParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetNetworkParameters(msg));
}

bool SightlineDevice::setPortConfig(const MsgSetPortConfiguration& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetPortConfiguration(msg));
}

bool SightlineDevice::queryVersion()
{
    return sendPacket(SightlineProtocolBuilder::buildGetVersionNumber());
}

bool SightlineDevice::enableSystemStatus(bool enable)
{
    MsgSystemStatusMode mode {};
    mode.systemStatusBits = enable ? 0x0001U : 0x0000U;
    mode.systemDebugBits = 0U;
    return sendPacket(SightlineProtocolBuilder::buildSystemStatusMode(mode));
}

bool SightlineDevice::queryParameters(std::uint8_t queryId)
{
    return sendPacket(SightlineProtocolBuilder::buildGetParameters(queryId));
}

// ==============================================================================
// 8. Observers & Telemetry Subscriptions
// ==============================================================================

void SightlineDevice::setTrackingCallback(TrackingCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_trackingCallback = std::move(cb);
}

void SightlineDevice::setExtendedPositionsCallback(ExtendedPositionsCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_extendedPositionsCallback = std::move(cb);
}

void SightlineDevice::setWarningCallback(WarningCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_warningCallback = std::move(cb);
}

void SightlineDevice::setVersionCallback(VersionCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_versionCallback = std::move(cb);
}

void SightlineDevice::setSystemStatusCallback(SystemStatusCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_systemStatusCallback = std::move(cb);
}

void SightlineDevice::setRawTrafficCallback(RawTrafficCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_rawTrafficCallback = std::move(cb);
}

std::optional<MsgTrackingPositions> SightlineDevice::lastTrackingPositions() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastPositions;
}

std::optional<MsgVersionNumber> SightlineDevice::lastVersion() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastVersion;
}

std::optional<MsgSystemStatusMessage> SightlineDevice::lastSystemStatus() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastStatus;
}

void SightlineDevice::handleIncomingBytes(const std::vector<std::uint8_t>& data)
{
    const auto packets = m_accumulator.push(data);
    for (const auto& packet : packets) {
        dispatchPacket(packet);
    }
}

void SightlineDevice::dispatchPacket(const std::vector<std::uint8_t>& packet)
{
    RawTrafficCallback rawCb {};
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        rawCb = m_rawTrafficCallback;
    }
    if (rawCb) {
        rawCb(false, packet);
    }

    const auto id = SightlineProtocolParser::identifyMessage(packet);
    switch (id) {
    case MessageId::TrackingPositions: {
        MsgTrackingPositions trackPositions {};
        if (SightlineProtocolParser::parseTrackingPositions(packet, trackPositions)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastPositions = trackPositions;
            }
            TrackingCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_trackingCallback;
            }
            if (cb) {
                cb(trackPositions);
            }
        }
        break;
    }
    case MessageId::TrackingPositionsExtended: {
        MsgTrackingPositionsExtended extPositions {};
        if (SightlineProtocolParser::parsePositionsExtended(packet, extPositions)) {
            ExtendedPositionsCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_extendedPositionsCallback;
            }
            if (cb) {
                cb(extPositions);
            }
        }
        break;
    }
    case MessageId::UserWarningMessage: {
        MsgUserWarningMessage warning {};
        if (SightlineProtocolParser::parseUserWarning(packet, warning)) {
            WarningCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_warningCallback;
            }
            if (cb) {
                cb(warning);
            }
        }
        break;
    }
    case MessageId::GetVersionNumber:
    case MessageId::VersionNumber: {
        MsgVersionNumber version {};
        if (SightlineProtocolParser::parseVersionNumber(packet, version)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastVersion = version;
            }
            VersionCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_versionCallback;
            }
            if (cb) {
                cb(version);
            }
        }
        break;
    }
    case MessageId::SystemStatusMessage: {
        MsgSystemStatusMessage status {};
        if (SightlineProtocolParser::parseSystemStatus(packet, status)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastStatus = status;
            }
            SystemStatusCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_systemStatusCallback;
            }
            if (cb) {
                cb(status);
            }
        }
        break;
    }
    default:
        break;
    }
}

bool SightlineDevice::sendPacket(const std::vector<std::uint8_t>& packet)
{
    if (!m_transport || !m_transport->isOpen()) {
        return false;
    }

    RawTrafficCallback rawCb {};
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        rawCb = m_rawTrafficCallback;
    }
    if (rawCb) {
        rawCb(true, packet);
    }

    return m_transport->sendData(packet);
}

} // namespace Sightline
