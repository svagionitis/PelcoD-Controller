/// @file SightlineDevice.cpp
/// @brief Implementation of Sightline SLA high-level controller and message dispatcher.

#include "SightlineDevice.h"
#include "modules/SightlineTrackingBuilder.h"

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

bool SightlineDevice::startPrecisionTrack(
    std::uint8_t cameraIndex, std::uint16_t col, std::uint16_t row,
    std::uint16_t width, std::uint16_t height, std::uint64_t framePtsUs,
    std::uint8_t flags)
{
    return sendPacket(SightlineTrackingBuilder::buildStartPrecision(
        cameraIndex, col, row, width, height, framePtsUs, flags));
}

bool SightlineDevice::setForcedCoast(
    std::uint8_t cameraIndex, std::uint8_t trackId, ForcedCoastingMode mode)
{
    return sendPacket(SightlineTrackingBuilder::buildForcedCoasting(
        cameraIndex, trackId, mode));
}

bool SightlineDevice::reinitTrack(std::uint8_t cameraIndex, std::uint8_t trackId)
{
    return sendPacket(SightlineTrackingBuilder::buildModifyTrackIndex(
        cameraIndex, trackId, TrackIndexAction::Reinitialize));
}

bool SightlineDevice::resizeTrack(
    std::uint8_t cameraIndex, std::uint8_t trackId,
    std::uint16_t width, std::uint16_t height, bool acqAssist)
{
    const auto action = acqAssist ? TrackIndexAction::ResizeWithAcquisitionAssist
                                  : TrackIndexAction::ResizeNoAcquisitionAssist;
    return sendPacket(SightlineTrackingBuilder::buildModifyTrackIndex(
        cameraIndex, trackId, action, width, height));
}

bool SightlineDevice::cueTrackAt(
    std::uint8_t cameraIndex, std::uint16_t col, std::uint16_t row,
    ModifyMode mode, std::uint8_t trackId)
{
    return sendPacket(SightlineTrackingBuilder::buildModifyTrackingMode(
        cameraIndex, col, row, mode, trackId));
}

bool SightlineDevice::nudgeTracking(std::uint8_t cameraIndex, std::int16_t deltaCol, std::int16_t deltaRow)
{
    MsgNudgeTrackingCoordinate msg {};
    msg.cameraIndex = cameraIndex;
    msg.deltaCol = deltaCol;
    msg.deltaRow = deltaRow;

    return sendPacket(SightlineProtocolBuilder::buildNudgeTracking(msg));
}

bool SightlineDevice::nudgeDisplayTrack(
    std::uint8_t cameraIndex, std::int16_t deltaCol, std::int16_t deltaRow)
{
    return sendPacket(SightlineTrackingBuilder::buildNudgeTrackingRotated(
        cameraIndex, deltaCol, deltaRow, NudgeCoordinateMode::DisplayCoordinates));
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

bool SightlineDevice::setAdvancedDetection(const MsgAdvancedDetectionParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetAdvDetectionParams(msg));
}

bool SightlineDevice::setDetectionROI(const MsgDetectionROI& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetDetectionROI(msg));
}

bool SightlineDevice::setVMTI(const MsgSetVMTI& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetVMTI(msg));
}

bool SightlineDevice::triggerDetectionSnapshot(std::uint8_t cameraIndex, std::uint8_t detectionIndex)
{
    MsgDoDetectSnapShot msg {};
    msg.cameraIndex = cameraIndex;
    msg.detectionIndex = detectionIndex;
    return sendPacket(SightlineProtocolBuilder::buildDoDetectSnapShot(msg));
}

bool SightlineDevice::setKlvMetricFilters(const MsgKlvMetricFilters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetKlvMetricFilters(msg));
}

bool SightlineDevice::setClassifierConfig(const MsgClassifierConfig& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetClassifierConfig(msg));
}

bool SightlineDevice::setComputeResources(bool useNpu, bool asyncInferencing)
{
    MsgSystemValue npuVal {};
    npuVal.systemValueId = 0x3CU; // NPU_CONTROL
    npuVal.value = useNpu ? 1U : 0U;

    MsgSystemValue asyncVal {};
    asyncVal.systemValueId = 0x3DU; // CLASSIFY_ASYNC
    asyncVal.value = asyncInferencing ? 1U : 0U;

    const bool ok1 { sendPacket(SightlineProtocolBuilder::buildSetSystemValue(npuVal)) };
    const bool ok2 { sendPacket(SightlineProtocolBuilder::buildSetSystemValue(asyncVal)) };
    return ok1 && ok2;
}

bool SightlineDevice::queryDetectionParams(std::uint8_t cameraIndex, std::uint8_t detIdx)
{
    return sendPacket(SightlineProtocolBuilder::buildGetDetectionParams(cameraIndex, detIdx));
}

bool SightlineDevice::queryAdvDetection(std::uint8_t cameraIndex)
{
    return sendPacket(SightlineProtocolBuilder::buildGetAdvDetectionParams(cameraIndex));
}

bool SightlineDevice::queryDetectionROI(std::uint8_t cameraIndex, std::uint8_t roiIndex)
{
    return sendPacket(SightlineProtocolBuilder::buildGetDetectionROI(cameraIndex, roiIndex));
}

bool SightlineDevice::queryVMTI(std::uint8_t cameraIndex)
{
    return sendPacket(SightlineProtocolBuilder::buildGetVMTI(cameraIndex));
}

bool SightlineDevice::queryTrackingPixelStats(std::uint8_t cameraIndex, std::uint8_t trackId)
{
    return sendPacket(SightlineProtocolBuilder::buildGetTrackingPixelStats(cameraIndex, trackId));
}

bool SightlineDevice::queryKlvMetricFilters(std::uint8_t cameraIndex)
{
    return sendPacket(SightlineProtocolBuilder::buildGetKlvMetricFilters(cameraIndex));
}

bool SightlineDevice::queryClassifierConfig(std::uint8_t cameraIndex)
{
    return sendPacket(SightlineProtocolBuilder::buildGetClassifierConfig(cameraIndex));
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

bool SightlineDevice::resetStabilization(std::uint8_t cameraIndex, std::uint8_t resetType)
{
    MsgResetStabilizationParameters msg {};
    msg.cameraIndex = cameraIndex;
    msg.resetType = resetType;

    return sendPacket(SightlineProtocolBuilder::buildResetStabilization(msg));
}

bool SightlineDevice::setStabilizationBias(const MsgSetStabilizationBias& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetStabilizationBias(msg));
}

bool SightlineDevice::setStabilizationBias(std::uint8_t cameraIndex, std::int16_t biasCol, std::int16_t biasRow,
    std::uint8_t autoBias, std::uint8_t updateRate)
{
    MsgSetStabilizationBias msg {};
    msg.cameraIndex = cameraIndex;
    msg.biasCol = biasCol;
    msg.biasRow = biasRow;
    msg.autoBias = autoBias;
    msg.updateRate = updateRate;

    return setStabilizationBias(msg);
}

bool SightlineDevice::setRegistration(const MsgSetRegistrationParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetRegistration(msg));
}

bool SightlineDevice::getStabilization(std::uint8_t cameraIndex)
{
    return sendPacket(SightlineProtocolBuilder::buildGetStabilization(cameraIndex));
}

bool SightlineDevice::getRegistration(std::uint8_t cameraIndex)
{
    return sendPacket(SightlineProtocolBuilder::buildGetRegistration(cameraIndex));
}

bool SightlineDevice::getStabilizationBias(std::uint8_t cameraIndex)
{
    return sendPacket(SightlineProtocolBuilder::buildGetStabilizationBias(cameraIndex));
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

bool SightlineDevice::getOverlayMode(std::uint8_t cameraIndex)
{
    return sendPacket(SightlineProtocolBuilder::buildGetOverlayMode(cameraIndex));
}

bool SightlineDevice::drawOverlay(const MsgDrawOverlay& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildDrawOverlay(msg));
}

bool SightlineDevice::drawOverlayBatch(const std::vector<MsgDrawOverlay>& objects)
{
    return sendPacket(SightlineProtocolBuilder::buildDrawOverlayBatch(objects));
}

bool SightlineDevice::drawCross(std::uint8_t cameraIndex, std::uint8_t objectId, std::int16_t centerX,
    std::int16_t centerY, std::uint16_t size, OverlayPaletteColor fgColor, std::uint16_t thickness,
    bool originUpperLeft)
{
    return drawOverlay(SightlineProtocolBuilder::makeCrossOverlay(
        cameraIndex, objectId, centerX, centerY, size, fgColor, thickness, originUpperLeft));
}

bool SightlineDevice::drawRectangle(std::uint8_t cameraIndex, std::uint8_t objectId, std::int16_t x, std::int16_t y,
    std::uint16_t width, std::uint16_t height, bool filled, OverlayPaletteColor fgColor, OverlayPaletteColor bgColor,
    std::uint8_t alpha, std::uint16_t thickness, bool originUpperLeft)
{
    return drawOverlay(SightlineProtocolBuilder::makeRectangleOverlay(
        cameraIndex, objectId, x, y, width, height, filled, fgColor, bgColor, alpha, thickness, originUpperLeft));
}

bool SightlineDevice::drawText(std::uint8_t cameraIndex, std::uint8_t objectId, std::int16_t x, std::int16_t y,
    const std::string& text, OverlayFontId fontId, OverlayPaletteColor fgColor, OverlayPaletteColor bgColor,
    std::uint8_t hScale, std::uint8_t vScale, bool originUpperLeft)
{
    return drawOverlay(SightlineProtocolBuilder::makeTextOverlay(
        cameraIndex, objectId, x, y, text, fontId, fgColor, bgColor, hScale, vScale, originUpperLeft));
}

bool SightlineDevice::drawKlvField(std::uint8_t cameraIndex, std::uint8_t objectId, std::int16_t x, std::int16_t y,
    KlvFieldTag fieldTag, KlvFormatType formatType, const std::string& formatString, OverlayFontId fontId,
    OverlayPaletteColor fgColor, bool originUpperLeft)
{
    return drawOverlay(SightlineProtocolBuilder::makeKlvFieldOverlay(
        cameraIndex, objectId, x, y, fieldTag, formatType, formatString, fontId, fgColor, originUpperLeft));
}

bool SightlineDevice::drawBlackout(
    std::uint8_t cameraIndex, std::uint8_t objectId, std::uint16_t width, std::uint16_t height)
{
    return drawOverlay(SightlineProtocolBuilder::makeBlackoutOverlay(cameraIndex, objectId, width, height));
}

bool SightlineDevice::destroyOverlay(std::uint8_t cameraIndex, std::uint8_t objectId)
{
    return drawOverlay(SightlineProtocolBuilder::makeDestroyOverlay(cameraIndex, objectId));
}

bool SightlineDevice::destroyAllOverlays(std::uint8_t cameraIndex)
{
    return drawOverlay(SightlineProtocolBuilder::makeDestroyOverlay(cameraIndex, 0U));
}

bool SightlineDevice::drawObject(const MsgDrawObject& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildDrawObject(msg));
}

bool SightlineDevice::setLogoParameters(const MsgLogoParameters& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildSetLogoParameters(msg));
}

bool SightlineDevice::getLogoParameters(std::uint8_t cameraIndex)
{
    return sendPacket(SightlineProtocolBuilder::buildGetLogoParameters(cameraIndex));
}

bool SightlineDevice::setUserFont(const MsgUserFont& msg)
{
    return sendPacket(SightlineProtocolBuilder::buildUserFont(msg));
}

bool SightlineDevice::setUserFont(std::uint8_t slotIndex, const std::string& fontFileName)
{
    MsgUserFont msg {};
    msg.userFontIndex = slotIndex;
    msg.fontFileName = fontFileName;
    return setUserFont(msg);
}

bool SightlineDevice::getOverlayObjectsIds(std::uint8_t cameraIndex)
{
    return sendPacket(SightlineProtocolBuilder::buildGetOverlayObjectsIds(cameraIndex));
}

bool SightlineDevice::getOverlayObjectParams(std::uint8_t objectId)
{
    return sendPacket(SightlineProtocolBuilder::buildGetOverlayObjectParams(objectId));
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

void SightlineDevice::setStabilizationCallback(StabilizationCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_stabilizationCallback = std::move(cb);
}

void SightlineDevice::setRegistrationCallback(RegistrationCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_registrationCallback = std::move(cb);
}

void SightlineDevice::setStabilizationBiasCallback(StabilizationBiasCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_stabilizationBiasCallback = std::move(cb);
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

std::optional<MsgSetStabilizationParameters> SightlineDevice::lastStabilization() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastStabilization;
}

std::optional<MsgSetRegistrationParameters> SightlineDevice::lastRegistration() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastRegistration;
}

std::optional<MsgSetStabilizationBias> SightlineDevice::lastStabilizationBias() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastStabilizationBias;
}

void SightlineDevice::setOverlayCallback(OverlayModeCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_overlayModeCallback = std::move(cb);
}

void SightlineDevice::setObjectsIdsCallback(OverlayObjectsIdsCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_objectsIdsCallback = std::move(cb);
}

void SightlineDevice::setObjectParamsCallback(OverlayObjectParamsCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_objectParamsCallback = std::move(cb);
}

void SightlineDevice::setLogoCallback(LogoParametersCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_logoCallback = std::move(cb);
}

void SightlineDevice::setDetectionCallback(DetectionCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_detectionCallback = std::move(cb);
}

void SightlineDevice::setAdvDetectionCallback(AdvDetectionCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_advDetectionCallback = std::move(cb);
}

void SightlineDevice::setDetectionRoiCallback(DetectionRoiCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_detectionRoiCallback = std::move(cb);
}

void SightlineDevice::setKlvMetricFiltersCb(KlvMetricFiltersCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_klvMetricFiltersCallback = std::move(cb);
}

std::optional<MsgSetOverlayMode> SightlineDevice::lastOverlayMode() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastOverlayMode;
}

std::optional<MsgCurrentOverlayObjectsIds> SightlineDevice::lastOverlayObjectsIds() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastOverlayObjectsIds;
}

std::optional<MsgLogoParameters> SightlineDevice::lastLogoParameters() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastLogoParameters;
}

std::optional<MsgSetDetectionParameters> SightlineDevice::lastDetectionParams() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastDetectionParams;
}

std::optional<MsgAdvancedDetectionParameters> SightlineDevice::lastAdvDetection() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastAdvDetection;
}

std::optional<MsgDetectionROI> SightlineDevice::lastDetectionROI() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastDetectionROI;
}

std::optional<MsgKlvMetricFilters> SightlineDevice::lastKlvMetricFilters() const
{
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_lastKlvMetricFilters;
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
    case MessageId::CurrentStabilizationParameters:
    case MessageId::SetStabilizationParameters: {
        MsgSetStabilizationParameters stab {};
        if (SightlineProtocolParser::parseStabilizationParams(packet, stab)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastStabilization = stab;
            }
            StabilizationCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_stabilizationCallback;
            }
            if (cb) {
                cb(stab);
            }
        }
        break;
    }
    case MessageId::RegistrationParameters:
    case MessageId::CurrentRegistrationParameters:
    case MessageId::SetRegistrationParameters: {
        MsgSetRegistrationParameters reg {};
        if (SightlineProtocolParser::parseRegistration(packet, reg)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastRegistration = reg;
            }
            RegistrationCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_registrationCallback;
            }
            if (cb) {
                cb(reg);
            }
        }
        break;
    }
    case MessageId::StabilizationBias:
    case MessageId::CurrentStabilizationBias:
    case MessageId::SetStabilizationBias: {
        MsgSetStabilizationBias bias {};
        if (SightlineProtocolParser::parseStabilizationBias(packet, bias)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastStabilizationBias = bias;
            }
            StabilizationBiasCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_stabilizationBiasCallback;
            }
            if (cb) {
                cb(bias);
            }
        }
        break;
    }
    case MessageId::CurrentOverlayMode:
    case MessageId::SetOverlayMode: {
        MsgSetOverlayMode mode {};
        if (SightlineProtocolParser::parseOverlayMode(packet, mode)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastOverlayMode = mode;
            }
            OverlayModeCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_overlayModeCallback;
            }
            if (cb) {
                cb(mode);
            }
        }
        break;
    }
    case MessageId::CurrentOverlayObjectsIds: {
        MsgCurrentOverlayObjectsIds ids {};
        if (SightlineProtocolParser::parseOverlayObjectsIds(packet, ids)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastOverlayObjectsIds = ids;
            }
            OverlayObjectsIdsCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_objectsIdsCallback;
            }
            if (cb) {
                cb(ids);
            }
        }
        break;
    }
    case MessageId::CurrentOverlayObjectParameters: {
        MsgCurrentOverlayObjectParameters params {};
        if (SightlineProtocolParser::parseOverlayObjectParams(packet, params)) {
            OverlayObjectParamsCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_objectParamsCallback;
            }
            if (cb) {
                cb(params);
            }
        }
        break;
    }
    case MessageId::LogoParameters: {
        MsgLogoParameters logo {};
        if (SightlineProtocolParser::parseLogoParameters(packet, logo)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastLogoParameters = logo;
            }
            LogoParametersCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_logoCallback;
            }
            if (cb) {
                cb(logo);
            }
        }
        break;
    }
    case MessageId::CurrentDetectionParameters:
    case MessageId::SetDetectionParameters: {
        MsgSetDetectionParameters det {};
        if (SightlineProtocolParser::parseDetectionParams(packet, det)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastDetectionParams = det;
            }
            DetectionCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_detectionCallback;
            }
            if (cb) {
                cb(det);
            }
        }
        break;
    }
    case MessageId::CurrentAdvancedDetectionParameters:
    case MessageId::SetAdvancedDetectionParameters: {
        MsgAdvancedDetectionParameters adv {};
        if (SightlineProtocolParser::parseAdvDetectionParams(packet, adv)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastAdvDetection = adv;
            }
            AdvDetectionCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_advDetectionCallback;
            }
            if (cb) {
                cb(adv);
            }
        }
        break;
    }
    case MessageId::CurrentDetectionRegionOfInterestParameters:
    case MessageId::SetDetectionRegionOfInterestParameters: {
        MsgDetectionROI roi {};
        if (SightlineProtocolParser::parseDetectionROI(packet, roi)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastDetectionROI = roi;
            }
            DetectionRoiCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_detectionRoiCallback;
            }
            if (cb) {
                cb(roi);
            }
        }
        break;
    }
    case MessageId::KlvClassFilters: {
        MsgKlvMetricFilters filters {};
        if (SightlineProtocolParser::parseKlvMetricFilters(packet, filters)) {
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                m_lastKlvMetricFilters = filters;
            }
            KlvMetricFiltersCallback cb {};
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                cb = m_klvMetricFiltersCallback;
            }
            if (cb) {
                cb(filters);
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
