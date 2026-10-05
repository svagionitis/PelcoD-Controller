/// @file SightlineProtocolParser.cpp
/// @brief Facade implementation delegating to modular domain parsers.

#include "SightlineProtocolParser.h"
#include "SightlineFraming.h"
#include "modules/SightlineBlendingParser.h"
#include "modules/SightlineCaptureParser.h"
#include "modules/SightlineClassificationParser.h"
#include "modules/SightlineCompressionParser.h"
#include "modules/SightlineDetectionParser.h"
#include "modules/SightlineDisplayParser.h"
#include "modules/SightlineEnhancementParser.h"
#include "modules/SightlineFocusParser.h"
#include "modules/SightlineGeneralParser.h"
#include "modules/SightlineKlvParser.h"
#include "modules/SightlineLandingParser.h"
#include "modules/SightlineNetworkParser.h"
#include "modules/SightlineNucParser.h"
#include "modules/SightlineOverlayParser.h"
#include "modules/SightlineRecordingParser.h"
#include "modules/SightlineSerialParser.h"
#include "modules/SightlineStabilizationParser.h"
#include "modules/SightlineTelemetryParser.h"
#include "modules/SightlineTrackingParser.h"

namespace Sightline {

std::size_t SightlineProtocolParser::getHeaderLength(ByteView packet) noexcept
{
    return SightlineFraming::getHeaderLength(packet);
}

MessageId SightlineProtocolParser::identifyMessage(ByteView packet) noexcept
{
    return SightlineFraming::identifyMessage(packet);
}

ByteView SightlineProtocolParser::extractPayload(ByteView packet) noexcept
{
    return SightlineFraming::extractPayload(packet);
}

// ==============================================================================
// 1. System, Diagnostic & Configuration Deserializers (General module)
// ==============================================================================

bool SightlineProtocolParser::parseVersionNumber(const std::vector<std::uint8_t>& packet, MsgVersionNumber& out)
{
    return SightlineGeneralParser::parseVersionNumber(packet, out);
}

bool SightlineProtocolParser::parseUserWarning(const std::vector<std::uint8_t>& packet, MsgUserWarningMessage& out)
{
    return SightlineGeneralParser::parseUserWarning(packet, out);
}

bool SightlineProtocolParser::parseSystemStatus(const std::vector<std::uint8_t>& packet, MsgSystemStatusMessage& out)
{
    return SightlineGeneralParser::parseSystemStatus(packet, out);
}

bool SightlineProtocolParser::parseCurrentConfiguration(
    const std::vector<std::uint8_t>& packet, MsgCurrentConfiguration& out)
{
    return SightlineGeneralParser::parseCurrentConfiguration(packet, out);
}

// ==============================================================================
// 2. Video Tracking & Motion Telemetry Deserializers (Tracking module)
// ==============================================================================

bool SightlineProtocolParser::parseTrackingPosition(const std::vector<std::uint8_t>& packet, MsgTrackingPosition& out)
{
    return SightlineTrackingParser::parseTrackingPosition(packet, out);
}

bool SightlineProtocolParser::parseTrackingPositions(const std::vector<std::uint8_t>& packet, MsgTrackingPositions& out)
{
    return SightlineTrackingParser::parseTrackingPositions(packet, out);
}

bool SightlineProtocolParser::parsePositionsExtended(
    const std::vector<std::uint8_t>& packet, MsgTrackingPositionsExtended& out)
{
    return SightlineTrackingParser::parsePositionsExtended(packet, out);
}

bool SightlineProtocolParser::parseTrackTrails(const std::vector<std::uint8_t>& packet, MsgTrackTrails& out)
{
    return SightlineTrackingParser::parseTrackTrails(packet, out);
}

// ==============================================================================
// 3. Configuration State Deserializers (Stabilization, Capture, Compression, Focus, KLV)
// ==============================================================================

bool SightlineProtocolParser::parseStabilizationParams(
    const std::vector<std::uint8_t>& packet, MsgSetStabilizationParameters& out)
{
    return SightlineStabilizationParser::parseStabilizationParams(packet, out);
}

bool SightlineProtocolParser::parseStabilizationBias(
    const std::vector<std::uint8_t>& packet, MsgSetStabilizationBias& out)
{
    return SightlineStabilizationParser::parseStabilizationBias(packet, out);
}

bool SightlineProtocolParser::parseRegistration(
    const std::vector<std::uint8_t>& packet, MsgSetRegistrationParameters& out)
{
    return SightlineStabilizationParser::parseRegistration(packet, out);
}

bool SightlineProtocolParser::parseVideoParameters(const std::vector<std::uint8_t>& packet, MsgSetVideoParameters& out)
{
    return SightlineCaptureParser::parseVideoParameters(packet, out);
}

bool SightlineProtocolParser::parseDigitalCameraParams(ByteView packet, MsgDigitalCameraParameters& out)
{
    return SightlineCaptureParser::parseDigitalCameraParams(packet, out);
}

bool SightlineProtocolParser::parseVideoEnhance(const std::vector<std::uint8_t>& packet, MsgSetVideoEnhancement& out)
{
    return SightlineEnhancementParser::parseVideoEnhance(packet, out);
}

bool SightlineProtocolParser::parseVideoEnhanceFull(
    const std::vector<std::uint8_t>& packet, MsgSetVideoEnhancementFull& out)
{
    return SightlineEnhancementParser::parseVideoEnhanceFull(packet, out);
}

bool SightlineProtocolParser::parseH264Parameters(const std::vector<std::uint8_t>& packet, MsgSetH264Parameters& out)
{
    return SightlineCompressionParser::parseH264Parameters(packet, out);
}

bool SightlineProtocolParser::parseFocusStats(const std::vector<std::uint8_t>& packet, MsgFocusParameters& out)
{
    return SightlineFocusParser::parseFocusStats(packet, out);
}

bool SightlineProtocolParser::parseMetadataValues(const std::vector<std::uint8_t>& packet, MsgSetMetadataValues& out)
{
    return SightlineKlvParser::parseMetadataValues(packet, out);
}

bool SightlineProtocolParser::parseTrackingParameters(
    const std::vector<std::uint8_t>& packet, MsgSetTrackingParameters& out)
{
    return SightlineTrackingParser::parseTrackingParameters(packet, out);
}

bool SightlineProtocolParser::parseLensParameters(const std::vector<std::uint8_t>& packet, MsgSetLensParameters& out)
{
    return SightlineFocusParser::parseLensParameters(packet, out);
}

bool SightlineProtocolParser::parseSystemStatusMode(const std::vector<std::uint8_t>& packet, MsgSystemStatusMode& out)
{
    return SightlineGeneralParser::parseSystemStatusMode(packet, out);
}

bool SightlineProtocolParser::parseMetadataStaticValues(
    const std::vector<std::uint8_t>& packet, MsgMetadataStaticValues& out)
{
    return SightlineKlvParser::parseMetadataStaticValues(packet, out);
}

bool SightlineProtocolParser::parseMetadataRate(const std::vector<std::uint8_t>& packet, MsgSetMetadataRate& out)
{
    return SightlineKlvParser::parseMetadataRate(packet, out);
}

bool SightlineProtocolParser::parseCurrentRate(const std::vector<std::uint8_t>& packet, MsgCurrentMetadataRate& out)
{
    return SightlineKlvParser::parseCurrentRate(packet, out);
}

bool SightlineProtocolParser::parseCurrentValues(
    const std::vector<std::uint8_t>& packet, MsgCurrentMetadataValues& out)
{
    return SightlineKlvParser::parseCurrentValues(packet, out);
}

bool SightlineProtocolParser::parseFrameValues(
    const std::vector<std::uint8_t>& packet, MsgSetMetadataFrameValues& out)
{
    return SightlineKlvParser::parseFrameValues(packet, out);
}

bool SightlineProtocolParser::parseCurrentFrameValues(
    const std::vector<std::uint8_t>& packet, MsgCurrentMetadataFrameValues& out)
{
    return SightlineKlvParser::parseCurrentFrameValues(packet, out);
}

bool SightlineProtocolParser::parseTagData(const std::vector<std::uint8_t>& packet, MsgTagData& out)
{
    return SightlineKlvParser::parseTagData(packet, out);
}

bool SightlineProtocolParser::parseTagDataRate(const std::vector<std::uint8_t>& packet, MsgTagDataRate& out)
{
    return SightlineKlvParser::parseTagDataRate(packet, out);
}

bool SightlineProtocolParser::parseTagSourceSelector(
    const std::vector<std::uint8_t>& packet, MsgTagSourceSelector& out)
{
    return SightlineKlvParser::parseTagSourceSelector(packet, out);
}

bool SightlineProtocolParser::parseCursorOnTarget(
    const std::vector<std::uint8_t>& packet, MsgCursorOnTarget& out)
{
    return SightlineKlvParser::parseCursorOnTarget(packet, out);
}

bool SightlineProtocolParser::parseVmtiChips(const std::vector<std::uint8_t>& packet, MsgVmtiChips& out)
{
    return SightlineKlvParser::parseVmtiChips(packet, out);
}

bool SightlineProtocolParser::parseVmtiFields(const std::vector<std::uint8_t>& packet, MsgVmtiFields& out)
{
    return SightlineKlvParser::parseVmtiFields(packet, out);
}

bool SightlineProtocolParser::parseAncillaryText(
    const std::vector<std::uint8_t>& packet, MsgAncillaryTextMetadata& out)
{
    return SightlineKlvParser::parseAncillaryText(packet, out);
}

bool SightlineProtocolParser::parseEthernetDisplay(
    const std::vector<std::uint8_t>& packet, MsgSetEthernetDisplayParameters& out)
{
    return SightlineNetworkParser::parseEthernetDisplay(packet, out);
}

bool SightlineProtocolParser::parseEthernetVideo(
    const std::vector<std::uint8_t>& packet, MsgSetEthernetVideoParameters& out)
{
    return SightlineNetworkParser::parseEthernetVideo(packet, out);
}

bool SightlineProtocolParser::parseNetworkParameters(
    const std::vector<std::uint8_t>& packet, MsgSetNetworkParameters& out)
{
    return SightlineNetworkParser::parseNetworkParameters(packet, out);
}

bool SightlineProtocolParser::parseNetworkList(const std::vector<std::uint8_t>& packet, MsgCurrentNetworkList& out)
{
    return SightlineNetworkParser::parseNetworkList(packet, out);
}

bool SightlineProtocolParser::parseSnapShot(const std::vector<std::uint8_t>& packet, MsgCurrentSnapShot& out)
{
    return SightlineRecordingParser::parseSnapShot(packet, out);
}

bool SightlineProtocolParser::parseCmdAck(const std::vector<std::uint8_t>& packet, MsgCommandAck& out)
{
    return SightlineRecordingParser::parseCmdAck(packet, out);
}

bool SightlineProtocolParser::parseSetFileRecordingV2(const std::vector<std::uint8_t>& packet, MsgSetFileRecordingParamsV2& out)
{
    return SightlineRecordingParser::parseSetFileRecordingV2(packet, out);
}

bool SightlineProtocolParser::parseDoSnapShotV2(const std::vector<std::uint8_t>& packet, MsgDoSnapShotV2& out)
{
    return SightlineRecordingParser::parseDoSnapShotV2(packet, out);
}

bool SightlineProtocolParser::parseRecordingEvent(const std::vector<std::uint8_t>& packet, MsgFileRecordingEvent& out)
{
    return SightlineRecordingParser::parseRecordingEvent(packet, out);
}

bool SightlineProtocolParser::parseRecordingStatusV2(const std::vector<std::uint8_t>& packet, MsgCurrentRecordingStatusV2& out)
{
    return SightlineRecordingParser::parseRecordingStatusV2(packet, out);
}

bool SightlineProtocolParser::parseGetDirListing(const std::vector<std::uint8_t>& packet, MsgGetDirectoryListing& out)
{
    return SightlineRecordingParser::parseGetDirListing(packet, out);
}

bool SightlineProtocolParser::parseDirListingReply(const std::vector<std::uint8_t>& packet, MsgDirectoryListingReply& out)
{
    return SightlineRecordingParser::parseDirListingReply(packet, out);
}

bool SightlineProtocolParser::parseFileStorageMgmt(const std::vector<std::uint8_t>& packet, MsgFileStorageManagement& out)
{
    return SightlineRecordingParser::parseFileStorageMgmt(packet, out);
}

// ==============================================================================
// 4. Thermal NUC & Sensor Calibration Deserializers (Nuc)
// ==============================================================================

bool SightlineProtocolParser::parseNucParameters(ByteView packet, MsgNucParameters& out)
{
    return SightlineNucParser::parseNucParameters(packet, out);
}

bool SightlineProtocolParser::parseDeadPixel(ByteView packet, MsgDeadPixel& out)
{
    return SightlineNucParser::parseDeadPixel(packet, out);
}

bool SightlineProtocolParser::parseReadWriteNuc(ByteView packet, MsgReadWriteNuc& out)
{
    return SightlineNucParser::parseReadWriteNuc(packet, out);
}

bool SightlineProtocolParser::parseUserPalette(ByteView packet, MsgUserPalette& out)
{
    return SightlineNucParser::parseUserPalette(packet, out);
}

bool SightlineProtocolParser::parseDeadPixelStats(ByteView packet, MsgDeadPixelStats& out)
{
    return SightlineNucParser::parseDeadPixelStats(packet, out);
}

bool SightlineProtocolParser::parseCameraCalibration(ByteView packet, MsgCameraCalibration& out)
{
    return SightlineNucParser::parseCameraCalibration(packet, out);
}

bool SightlineProtocolParser::parseCameraParameterFile(ByteView packet, MsgCameraParameterFile& out)
{
    return SightlineNucParser::parseCameraParameterFile(packet, out);
}

// ==============================================================================
// 5. VMTI & Tactical Video Analytics Deserializers (Detection & Classification)
// ==============================================================================

bool SightlineProtocolParser::parseDetectionParams(ByteView packet, MsgSetDetectionParameters& out)
{
    return SightlineDetectionParser::parseDetectionParams(packet, out);
}

bool SightlineProtocolParser::parseVMTI(ByteView packet, MsgSetVMTI& out)
{
    return SightlineDetectionParser::parseVMTI(packet, out);
}

bool SightlineProtocolParser::parseDetectionROI(ByteView packet, MsgDetectionROI& out)
{
    return SightlineDetectionParser::parseDetectionROI(packet, out);
}

bool SightlineProtocolParser::parseAdvDetectionParams(ByteView packet, MsgAdvancedDetectionParameters& out)
{
    return SightlineDetectionParser::parseAdvDetectionParams(packet, out);
}

bool SightlineProtocolParser::parseTrackingPixelStats(ByteView packet, MsgTrackingBoxPixelStats& out)
{
    return SightlineDetectionParser::parseTrackingPixelStats(packet, out);
}

bool SightlineProtocolParser::parseCustomAIDetect(ByteView packet, MsgCustomAIDetect& out)
{
    return SightlineClassificationParser::parseCustomAIDetect(packet, out);
}

bool SightlineProtocolParser::parseVMTIChips(ByteView packet, MsgVMTIChips& out)
{
    return SightlineClassificationParser::parseVMTIChips(packet, out);
}

bool SightlineProtocolParser::parseVMTIFields(ByteView packet, MsgVMTIFields& out)
{
    return SightlineClassificationParser::parseVMTIFields(packet, out);
}

bool SightlineProtocolParser::parseKlvClassFilters(ByteView packet, MsgKlvClassFilters& out)
{
    return SightlineClassificationParser::parseKlvClassFilters(packet, out);
}

bool SightlineProtocolParser::parseTrackingMultiClass(ByteView packet, MsgTrackingMultiClass& out)
{
    return SightlineClassificationParser::parseTrackingMultiClass(packet, out);
}

bool SightlineProtocolParser::parseCustomClassifier(ByteView packet, MsgCustomClassifier& out)
{
    return SightlineClassificationParser::parseCustomClassifier(packet, out);
}

bool SightlineProtocolParser::parseClassifierParams(ByteView packet, MsgClassifierParameters& out)
{
    return SightlineClassificationParser::parseClassifierParams(packet, out);
}

bool SightlineProtocolParser::parseKlvMetricFilters(ByteView packet, MsgKlvMetricFilters& out)
{
    return SightlineClassificationParser::parseKlvMetricFilters(packet, out);
}

bool SightlineProtocolParser::parseClassifierConfig(ByteView packet, MsgClassifierConfig& out)
{
    return SightlineClassificationParser::parseClassifierConfig(packet, out);
}

// ==============================================================================
// 6. Multi-Sensor Alignment & Digital Video Pipeline Deserializers (Phase 3)
// ==============================================================================

bool SightlineProtocolParser::parseBlendParameters(ByteView packet, MsgSetBlendParameters& out)
{
    return SightlineBlendingParser::parseBlendParameters(packet, out);
}

bool SightlineProtocolParser::parseCurrentBlendParameters(ByteView packet, MsgCurrentBlendParameters& out)
{
    return SightlineBlendingParser::parseCurrentBlendParameters(packet, out);
}

bool SightlineProtocolParser::parseFourAlignPoints(ByteView packet, MsgFourAlignPoints& out)
{
    return SightlineBlendingParser::parseFourAlignPoints(packet, out);
}

bool SightlineProtocolParser::parseBlendAlign(ByteView packet, MsgBlendAlign& out)
{
    return SightlineBlendingParser::parseBlendAlign(packet, out);
}

bool SightlineProtocolParser::parseMultipleAlignment(ByteView packet, MsgSetMultipleAlignment& out)
{
    return SightlineBlendingParser::parseMultipleAlignment(packet, out);
}

bool SightlineProtocolParser::parseCameraSwitch(ByteView packet, MsgCameraSwitch& out)
{
    return SightlineCaptureParser::parseCameraSwitch(packet, out);
}

bool SightlineProtocolParser::parseAdvCaptureParams(ByteView packet, MsgAdvancedCaptureParameters& out)
{
    return SightlineCaptureParser::parseAdvCaptureParams(packet, out);
}

bool SightlineProtocolParser::parseDigiVideoParser(ByteView packet, MsgDigitalVideoParserParameters& out)
{
    return SightlineCaptureParser::parseDigiVideoParser(packet, out);
}

bool SightlineProtocolParser::parseCameraCapabilities(ByteView packet, MsgCameraCapabilities& out)
{
    return SightlineCaptureParser::parseCameraCapabilities(packet, out);
}

bool SightlineProtocolParser::parseVideoDisplay(ByteView packet, MsgVideoDisplay& out)
{
    return SightlineDisplayParser::parseVideoDisplay(packet, out);
}

bool SightlineProtocolParser::parseMultiDisplay(ByteView packet, MsgMultiDisplay& out)
{
    return SightlineDisplayParser::parseMultiDisplay(packet, out);
}

// ==============================================================================
// 7. Low-Level Bus, I2C, & Telemetry Tags Deserializers (Phase 4)
// ==============================================================================

bool SightlineProtocolParser::parseI2CCommand(ByteView packet, MsgI2CCommand& out)
{
    return SightlineSerialParser::parseI2CCommand(packet, out);
}

bool SightlineProtocolParser::parseSystemValue(ByteView packet, MsgSystemValue& out)
{
    return SightlineGeneralParser::parseSystemValue(packet, out);
}

bool SightlineProtocolParser::parseTagData(ByteView packet, MsgTagData& out)
{
    return SightlineGeneralParser::parseTagData(packet, out);
}

bool SightlineProtocolParser::parseTagDataRate(ByteView packet, MsgTagDataRate& out)
{
    return SightlineGeneralParser::parseTagDataRate(packet, out);
}

bool SightlineProtocolParser::parseTagSourceSelector(ByteView packet, MsgTagSourceSelector& out)
{
    return SightlineGeneralParser::parseTagSourceSelector(packet, out);
}

bool SightlineProtocolParser::parseDetailedTiming(ByteView packet, MsgDetailedTiming& out)
{
    return SightlineGeneralParser::parseDetailedTiming(packet, out);
}

bool SightlineProtocolParser::parseAppendedMetadata(ByteView packet, MsgAppendedMetadata& out)
{
    return SightlineGeneralParser::parseAppendedMetadata(packet, out);
}

bool SightlineProtocolParser::parseFrameIndex(ByteView packet, MsgFrameIndex& out)
{
    return SightlineGeneralParser::parseFrameIndex(packet, out);
}

// ==============================================================================
// 8. Autonomous Landing Aid & Graphics Deserializers (Phase 5)
// ==============================================================================

bool SightlineProtocolParser::parseLandingAid(ByteView packet, MsgLandingAid& out)
{
    return SightlineLandingParser::parseLandingAid(packet, out);
}

bool SightlineProtocolParser::parseLandingPosition(ByteView packet, MsgLandingPosition& out)
{
    return SightlineLandingParser::parseLandingPosition(packet, out);
}

bool SightlineProtocolParser::parseOverlayMode(ByteView packet, MsgSetOverlayMode& out)
{
    return SightlineOverlayParser::parseOverlayMode(packet, out);
}

bool SightlineProtocolParser::parseDrawObject(ByteView packet, MsgDrawObject& out)
{
    return SightlineOverlayParser::parseDrawObject(packet, out);
}

bool SightlineProtocolParser::parseDrawOverlay(ByteView packet, MsgDrawOverlay& out)
{
    return SightlineOverlayParser::parseDrawOverlay(packet, out);
}

bool SightlineProtocolParser::parseLogoParameters(ByteView packet, MsgLogoParameters& out)
{
    return SightlineOverlayParser::parseLogoParameters(packet, out);
}

bool SightlineProtocolParser::parseAncillaryTextMetadata(ByteView packet, MsgAncillaryTextMetadata& out)
{
    return SightlineOverlayParser::parseAncillaryTextMetadata(packet, out);
}

bool SightlineProtocolParser::parseUserFont(ByteView packet, MsgUserFont& out)
{
    return SightlineOverlayParser::parseUserFont(packet, out);
}

bool SightlineProtocolParser::parseOverlayObjectsIds(ByteView packet, MsgCurrentOverlayObjectsIds& out)
{
    return SightlineOverlayParser::parseOverlayObjectsIds(packet, out);
}

bool SightlineProtocolParser::parseOverlayObjectParams(ByteView packet, MsgCurrentOverlayObjectParameters& out)
{
    return SightlineOverlayParser::parseOverlayObjectParams(packet, out);
}

bool SightlineProtocolParser::parseDecoderParameters(ByteView packet, MsgDecoderParameters& out)
{
    return SightlineCompressionParser::parseDecoderParameters(packet, out);
}

bool SightlineProtocolParser::parseSendToBTS(ByteView packet, MsgSendToBTS& out)
{
    return SightlineSerialParser::parseSendToBTS(packet, out);
}

} // namespace Sightline
