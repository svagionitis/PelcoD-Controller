/// @file SightlineProtocolBuilder.cpp
/// @brief Facade implementation delegating to modular domain builders.

#include "SightlineProtocolBuilder.h"
#include "SightlineFraming.h"
#include "modules/SightlineBlendingBuilder.h"
#include "modules/SightlineCaptureBuilder.h"
#include "modules/SightlineClassificationBuilder.h"
#include "modules/SightlineCompressionBuilder.h"
#include "modules/SightlineDetectionBuilder.h"
#include "modules/SightlineDisplayBuilder.h"
#include "modules/SightlineEnhancementBuilder.h"
#include "modules/SightlineFocusBuilder.h"
#include "modules/SightlineGeneralBuilder.h"
#include "modules/SightlineKlvBuilder.h"
#include "modules/SightlineLandingBuilder.h"
#include "modules/SightlineNetworkBuilder.h"
#include "modules/SightlineNucBuilder.h"
#include "modules/SightlineOverlayBuilder.h"
#include "modules/SightlineRecordingBuilder.h"
#include "modules/SightlineSerialBuilder.h"
#include "modules/SightlineStabilizationBuilder.h"
#include "modules/SightlineTelemetryBuilder.h"
#include "modules/SightlineTrackingBuilder.h"

namespace Sightline {

// ==============================================================================
// 1. System, Configuration & Diagnostic Messages (General, Network, Serial)
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetVersionNumber()
{
    return SightlineGeneralBuilder::buildGetVersionNumber();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetParameters(std::uint8_t queryId)
{
    return SightlineGeneralBuilder::buildGetParameters(queryId);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildResetAllParameters(const MsgResetAllParameters& msg)
{
    return SightlineGeneralBuilder::buildResetAllParameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSaveParameters(const MsgSaveParameters& msg)
{
    return SightlineGeneralBuilder::buildSaveParameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSystemStatusMode(const MsgSystemStatusMode& msg)
{
    return SightlineGeneralBuilder::buildSystemStatusMode(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetNetworkParameters(const MsgSetNetworkParameters& msg)
{
    return SightlineNetworkBuilder::buildSetNetworkParameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetPortConfiguration(const MsgSetPortConfiguration& msg)
{
    return SightlineSerialBuilder::buildSetPortConfiguration(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildCommandPassThrough(const MsgCommandPassThrough& msg)
{
    return SightlineSerialBuilder::buildCommandPassThrough(msg);
}

// ==============================================================================
// 2. Video Tracking, Acquisition & Motion Analysis Messages (Tracking, Detection, Classification)
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildStartTracking(const MsgStartTracking& msg)
{
    return SightlineTrackingBuilder::buildStartTracking(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildStopTracking(const MsgStopTracking& msg)
{
    return SightlineTrackingBuilder::buildStopTracking(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildModifyTracking(const MsgModifyTracking& msg)
{
    return SightlineTrackingBuilder::buildModifyTracking(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildNudgeTracking(const MsgNudgeTrackingCoordinate& msg)
{
    return SightlineTrackingBuilder::buildNudgeTracking(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetReportingMode(const MsgCoordinateReportingMode& msg)
{
    return SightlineTelemetryBuilder::buildSetReportingMode(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetTrackingParameters(const MsgSetTrackingParameters& msg)
{
    return SightlineTrackingBuilder::buildSetTrackingParameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildModifyTrackIndex(const MsgModifyTrackIndex& msg)
{
    return SightlineTrackingBuilder::buildModifyTrackIndex(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildTrackTrails(const MsgTrackTrails& msg)
{
    return SightlineTrackingBuilder::buildTrackTrails(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildDesignatePrimary(const MsgDesignateSelectedTrackPrimary& msg)
{
    return SightlineTrackingBuilder::buildDesignatePrimary(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildShiftSelectedTrack(const MsgShiftSelectedTrack& msg)
{
    return SightlineTrackingBuilder::buildShiftSelectedTrack(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildStopSelectedTrack(const MsgStopSelectedTrack& msg)
{
    return SightlineTrackingBuilder::buildStopSelectedTrack(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetDetectionParams(const MsgSetDetectionParameters& msg)
{
    return SightlineDetectionBuilder::buildSetDetectionParams(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildCustomAIDetect(const MsgCustomAIDetect& msg)
{
    return SightlineClassificationBuilder::buildCustomAIDetect(msg);
}

// ==============================================================================
// 2b. VMTI & Tactical Video Analytics (Phase 2)
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetVMTI(const MsgSetVMTI& msg)
{
    return SightlineDetectionBuilder::buildSetVMTI(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetVMTI(std::uint8_t cameraIndex)
{
    return SightlineDetectionBuilder::buildGetVMTI(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetDetectionROI(const MsgDetectionROI& msg)
{
    return SightlineDetectionBuilder::buildSetDetectionROI(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetDetectionROI(
    std::uint8_t cameraIndex, std::uint8_t roiIndex)
{
    return SightlineDetectionBuilder::buildGetDetectionROI(cameraIndex, roiIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetAdvDetectionParams(
    const MsgAdvancedDetectionParameters& msg)
{
    return SightlineDetectionBuilder::buildSetAdvDetectionParams(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetAdvDetectionParams(std::uint8_t cameraIndex)
{
    return SightlineDetectionBuilder::buildGetAdvDetectionParams(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetTrackingPixelStats(
    std::uint8_t cameraIndex, std::uint8_t trackId)
{
    return SightlineDetectionBuilder::buildGetTrackingPixelStats(cameraIndex, trackId);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildDoDetectSnapShot(const MsgDoDetectSnapShot& msg)
{
    return SightlineDetectionBuilder::buildDoDetectSnapShot(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildVMTIChips(const MsgVMTIChips& msg)
{
    return SightlineClassificationBuilder::buildVMTIChips(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetVMTIFields(const MsgVMTIFields& msg)
{
    return SightlineClassificationBuilder::buildSetVMTIFields(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetVMTIFields(std::uint8_t cameraIndex)
{
    return SightlineClassificationBuilder::buildGetVMTIFields(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetKlvClassFilters(const MsgKlvClassFilters& msg)
{
    return SightlineClassificationBuilder::buildSetKlvClassFilters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetKlvClassFilters(std::uint8_t cameraIndex)
{
    return SightlineClassificationBuilder::buildGetKlvClassFilters(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildTrackingMultiClass(const MsgTrackingMultiClass& msg)
{
    return SightlineClassificationBuilder::buildTrackingMultiClass(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetCustomClassifier(const MsgCustomClassifier& msg)
{
    return SightlineClassificationBuilder::buildSetCustomClassifier(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetCustomClassifier(std::uint8_t cameraIndex)
{
    return SightlineClassificationBuilder::buildGetCustomClassifier(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetClassifierParams(const MsgClassifierParameters& msg)
{
    return SightlineClassificationBuilder::buildSetClassifierParams(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetClassifierParams(std::uint8_t cameraIndex)
{
    return SightlineClassificationBuilder::buildGetClassifierParams(cameraIndex);
}

// ==============================================================================
// 3. Video Stabilization, Alignment & Enhancement Messages (Stabilization, Blending, Enhancement)
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetStabilization(const MsgSetStabilizationParameters& msg)
{
    return SightlineStabilizationBuilder::buildSetStabilization(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildResetStabilization(const MsgResetStabilizationParameters& msg)
{
    return SightlineStabilizationBuilder::buildResetStabilization(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetStabilizationBias(const MsgSetStabilizationBias& msg)
{
    return SightlineStabilizationBuilder::buildSetStabilizationBias(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetRegistration(const MsgSetRegistrationParameters& msg)
{
    return SightlineStabilizationBuilder::buildSetRegistration(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetBlendParameters(const MsgSetBlendParameters& msg)
{
    return SightlineBlendingBuilder::buildSetBlendParameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetNoise3D(const MsgNoise3D& msg)
{
    return SightlineEnhancementBuilder::buildSetNoise3D(msg);
}

// ==============================================================================
// 3b. Multi-Sensor Alignment & Digital Video Pipeline (Phase 3)
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildFourAlignPoints(const MsgFourAlignPoints& msg)
{
    return SightlineBlendingBuilder::buildFourAlignPoints(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetFourAlignPoints(std::uint8_t cameraIndex)
{
    return SightlineBlendingBuilder::buildGetFourAlignPoints(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetBlendAlign(const MsgBlendAlign& msg)
{
    return SightlineBlendingBuilder::buildSetBlendAlign(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetBlendAlign(std::uint8_t cameraIndex)
{
    return SightlineBlendingBuilder::buildGetBlendAlign(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildCameraSwitch(const MsgCameraSwitch& msg)
{
    return SightlineCaptureBuilder::buildCameraSwitch(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetAdvCaptureParams(const MsgAdvancedCaptureParameters& msg)
{
    return SightlineCaptureBuilder::buildSetAdvCaptureParams(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetAdvCaptureParams(std::uint8_t cameraIndex)
{
    return SightlineCaptureBuilder::buildGetAdvCaptureParams(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetDigiVideoParser(const MsgDigitalVideoParserParameters& msg)
{
    return SightlineCaptureBuilder::buildSetDigiVideoParser(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetDigiVideoParser(std::uint8_t cameraIndex)
{
    return SightlineCaptureBuilder::buildGetDigiVideoParser(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetCameraCapabilities(std::uint8_t cameraIndex)
{
    return SightlineCaptureBuilder::buildGetCameraCapabilities(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetVideoDisplay(const MsgVideoDisplay& msg)
{
    return SightlineDisplayBuilder::buildSetVideoDisplay(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetVideoDisplay(std::uint8_t displayIndex)
{
    return SightlineDisplayBuilder::buildGetVideoDisplay(displayIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetMultiDisplay(const MsgMultiDisplay& msg)
{
    return SightlineDisplayBuilder::buildSetMultiDisplay(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetMultiDisplay(std::uint8_t displayIndex)
{
    return SightlineDisplayBuilder::buildGetMultiDisplay(displayIndex);
}

// ==============================================================================
// 4. Video Pipeline, Display & Streaming Messages (Capture, Display, Network, Compression, Recording)
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetVideoParameters(const MsgSetVideoParameters& msg)
{
    return SightlineCaptureBuilder::buildSetVideoParameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetVideoMode(const MsgSetVideoMode& msg)
{
    return SightlineCaptureBuilder::buildSetVideoMode(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetVideoEnhance(const MsgSetVideoEnhancement& msg)
{
    return SightlineEnhancementBuilder::buildSetVideoEnhance(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetVideoEnhanceFull(const MsgSetVideoEnhancementFull& msg)
{
    return SightlineEnhancementBuilder::buildSetVideoEnhanceFull(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetFalseColor(
    std::uint8_t cameraIndex, FalseColorPalette palette)
{
    return SightlineEnhancementBuilder::buildSetFalseColor(cameraIndex, palette);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetDisplayParams(const MsgSetDisplayParameters& msg)
{
    return SightlineDisplayBuilder::buildSetDisplayParams(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetEthernetVideo(const MsgSetEthernetVideoParameters& msg)
{
    return SightlineNetworkBuilder::buildSetEthernetVideo(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetEthernetDisplay(const MsgSetEthernetDisplayParameters& msg)
{
    return SightlineNetworkBuilder::buildSetEthernetDisplay(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetH264Parameters(const MsgSetH264Parameters& msg)
{
    return SightlineCompressionBuilder::buildSetH264Parameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetSDRecording(const MsgSetSDRecordingParameters& msg)
{
    return SightlineRecordingBuilder::buildSetSDRecording(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildStreamingControl(const MsgStreamingControl& msg)
{
    return SightlineCompressionBuilder::buildStreamingControl(msg);
}

// ==============================================================================
// 5. Metadata, KLV & Telemetry Messages (KLV, Telemetry)
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetMetadataValues(const MsgSetMetadataValues& msg)
{
    return SightlineKlvBuilder::buildSetMetadataValues(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildMetadataStaticValues(const MsgMetadataStaticValues& msg)
{
    return SightlineKlvBuilder::buildMetadataStaticValues(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetMetadataRate(const MsgSetMetadataRate& msg)
{
    return SightlineKlvBuilder::buildSetMetadataRate(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetTelemetryDest(const MsgSetTelemetryDestination& msg)
{
    return SightlineTelemetryBuilder::buildSetTelemetryDest(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildCursorOnTarget(const MsgCursorOnTarget& msg)
{
    return SightlineKlvBuilder::buildCursorOnTarget(msg);
}

// ==============================================================================
// 6. Optics, Lens & Hardware Messages (Focus, Serial GPIO)
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildLensCommand(const MsgLensCommand& msg)
{
    return SightlineFocusBuilder::buildLensCommand(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildFocusParameters(const MsgFocusParameters& msg)
{
    return SightlineFocusBuilder::buildFocusParameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetLensParameters(const MsgSetLensParameters& msg)
{
    return SightlineFocusBuilder::buildSetLensParameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGPIO(const MsgGPIO& msg)
{
    return SightlineSerialBuilder::buildGPIO(msg);
}

// ==============================================================================
// 7. Thermal NUC & Sensor Calibration Messages (Nuc)
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildNucParameters(const MsgNucParameters& msg)
{
    return SightlineNucBuilder::buildNucParameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildDeadPixel(const MsgDeadPixel& msg)
{
    return SightlineNucBuilder::buildDeadPixel(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildReadWriteNuc(const MsgReadWriteNuc& msg)
{
    return SightlineNucBuilder::buildReadWriteNuc(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetUserPalette(const MsgUserPalette& msg)
{
    return SightlineNucBuilder::buildSetUserPalette(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildCameraCalibration(const MsgCameraCalibration& msg)
{
    return SightlineNucBuilder::buildCameraCalibration(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildCameraParameterFile(const MsgCameraParameterFile& msg)
{
    return SightlineNucBuilder::buildCameraParameterFile(msg);
}

// ==============================================================================
// 8. Reticles, Overlays & Graphics Messages (Overlays)
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetOverlayMode(const MsgSetOverlayMode& msg)
{
    return SightlineOverlayBuilder::buildSetOverlayMode(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildDrawObject(const MsgDrawObject& msg)
{
    return SightlineOverlayBuilder::buildDrawObject(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildDrawOverlay(const MsgDrawOverlay& msg)
{
    return SightlineOverlayBuilder::buildDrawOverlay(msg);
}

// ==============================================================================
// Parameter Query Getters
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetBlendParameters()
{
    return SightlineBlendingBuilder::buildGetBlendParameters();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetVideoParameters(std::uint8_t cameraIndex)
{
    return SightlineCaptureBuilder::buildGetVideoParameters(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetVideoMode()
{
    return SightlineCaptureBuilder::buildGetVideoMode();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetCustomAIDetect()
{
    return SightlineClassificationBuilder::buildGetCustomAIDetect();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetH264Parameters(std::uint16_t displayId)
{
    return SightlineCompressionBuilder::buildGetH264Parameters(displayId);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetDetectionParams(
    std::uint8_t cameraIndex, std::uint8_t detIdx)
{
    return SightlineDetectionBuilder::buildGetDetectionParams(cameraIndex, detIdx);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetDisplayParams(std::uint8_t cameraIndex)
{
    return SightlineDisplayBuilder::buildGetDisplayParams(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetVideoEnhance(std::uint8_t cameraIndex)
{
    return SightlineEnhancementBuilder::buildGetVideoEnhance(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetFocusParameters(std::uint8_t cameraIndex)
{
    return SightlineFocusBuilder::buildGetFocusParameters(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetLensParameters(std::uint8_t cameraIndex)
{
    return SightlineFocusBuilder::buildGetLensParameters(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetHardwareId()
{
    return SightlineGeneralBuilder::buildGetHardwareId();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetMetadataValues()
{
    return SightlineKlvBuilder::buildGetMetadataValues();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetLandingAid()
{
    return SightlineLandingBuilder::buildGetLandingAid();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetNetworkParameters(std::uint8_t index)
{
    return SightlineNetworkBuilder::buildGetNetworkParameters(index);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetEthernetVideo(std::uint16_t displayId)
{
    return SightlineNetworkBuilder::buildGetEthernetVideo(displayId);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetEthernetDisplay(std::uint16_t displayId)
{
    return SightlineNetworkBuilder::buildGetEthernetDisplay(displayId);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetNetworkList()
{
    return SightlineNetworkBuilder::buildGetNetworkList();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetNucParameters(std::uint8_t cameraIndex)
{
    return SightlineNucBuilder::buildGetNucParameters(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetUserPalette(std::uint8_t paletteIndex)
{
    return SightlineNucBuilder::buildGetUserPalette(paletteIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetDeadPixelStats(std::uint8_t cameraIndex)
{
    return SightlineNucBuilder::buildGetDeadPixelStats(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetCameraCalibration(std::uint8_t cameraIndex)
{
    return SightlineNucBuilder::buildGetCameraCalibration(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetOverlayMode(std::uint8_t cameraIndex)
{
    return SightlineOverlayBuilder::buildGetOverlayMode(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetSnapShot(std::uint8_t cameraIndex)
{
    return SightlineRecordingBuilder::buildGetSnapShot(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetSDRecording()
{
    return SightlineRecordingBuilder::buildGetSDRecording();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetPortConfiguration(std::uint8_t port)
{
    return SightlineSerialBuilder::buildGetPortConfiguration(port);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetStabilization(std::uint8_t cameraIndex)
{
    return SightlineStabilizationBuilder::buildGetStabilization(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetRegistration(std::uint8_t cameraIndex)
{
    return SightlineStabilizationBuilder::buildGetRegistration(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetCoordReportingMode(std::uint8_t cameraIndex)
{
    return SightlineTelemetryBuilder::buildGetCoordReportingMode(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetTrackingParameters(std::uint8_t cameraIndex)
{
    return SightlineTrackingBuilder::buildGetTrackingParameters(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetStreamingControl(std::uint8_t streamIndex)
{
    return SightlineCompressionBuilder::buildGetStreamingControl(streamIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetNoise3D(std::uint8_t cameraIndex)
{
    return SightlineEnhancementBuilder::buildGetNoise3D(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetSystemStatusMode()
{
    return SightlineGeneralBuilder::buildGetSystemStatusMode();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetCurrentConfig()
{
    return SightlineGeneralBuilder::buildGetCurrentConfig();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetMetadataStaticValues()
{
    return SightlineKlvBuilder::buildGetMetadataStaticValues();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetMetadataRate()
{
    return SightlineKlvBuilder::buildGetMetadataRate();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetDeadPixel(std::uint8_t cameraIndex)
{
    return SightlineNucBuilder::buildGetDeadPixel(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetGPIO()
{
    return SightlineSerialBuilder::buildGetGPIO();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetStabilizationBias(std::uint8_t cameraIndex)
{
    return SightlineStabilizationBuilder::buildGetStabilizationBias(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetTelemetryDest()
{
    return SightlineTelemetryBuilder::buildGetTelemetryDest();
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetTrackTrails(std::uint8_t cameraIndex)
{
    return SightlineTrackingBuilder::buildGetTrackTrails(cameraIndex);
}

// --- Low-Level Bus, I2C, & Telemetry Tags (Phase 4) ---

std::vector<std::uint8_t> SightlineProtocolBuilder::buildI2CCommand(const MsgI2CCommand& msg)
{
    return SightlineSerialBuilder::buildI2CCommand(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetSystemValue(const MsgSystemValue& msg)
{
    return SightlineGeneralBuilder::buildSetSystemValue(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetSystemValue(std::uint8_t systemValueId)
{
    return SightlineGeneralBuilder::buildGetSystemValue(systemValueId);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildTagData(const MsgTagData& msg)
{
    return SightlineGeneralBuilder::buildTagData(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetTagDataRate(const MsgTagDataRate& msg)
{
    return SightlineGeneralBuilder::buildSetTagDataRate(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetTagDataRate(std::uint16_t tagId)
{
    return SightlineGeneralBuilder::buildGetTagDataRate(tagId);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetTagSourceSelector(const MsgTagSourceSelector& msg)
{
    return SightlineGeneralBuilder::buildSetTagSourceSelector(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetTagSourceSelector(std::uint16_t tagId)
{
    return SightlineGeneralBuilder::buildGetTagSourceSelector(tagId);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildDetailedTiming(const MsgDetailedTiming& msg)
{
    return SightlineGeneralBuilder::buildDetailedTiming(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetAppendedMetadata(const MsgAppendedMetadata& msg)
{
    return SightlineGeneralBuilder::buildSetAppendedMetadata(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetAppendedMetadata(std::uint8_t cameraIndex)
{
    return SightlineGeneralBuilder::buildGetAppendedMetadata(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildFrameIndex(const MsgFrameIndex& msg)
{
    return SightlineGeneralBuilder::buildFrameIndex(msg);
}

// ==============================================================================
// Autonomous Landing Aid & Graphics (Phase 5)
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildLandingAid(const MsgLandingAid& msg)
{
    return SightlineLandingBuilder::buildLandingAid(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildLandingPosition(const MsgLandingPosition& msg)
{
    return SightlineLandingBuilder::buildLandingPosition(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetLogoParameters(const MsgLogoParameters& msg)
{
    return SightlineOverlayBuilder::buildSetLogoParameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetLogoParameters(std::uint8_t cameraIndex)
{
    return SightlineOverlayBuilder::buildGetLogoParameters(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildAncillaryTextMetadata(const MsgAncillaryTextMetadata& msg)
{
    return SightlineOverlayBuilder::buildAncillaryTextMetadata(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildUserFont(const MsgUserFont& msg)
{
    return SightlineOverlayBuilder::buildUserFont(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetOverlayObjectsIds(std::uint8_t cameraIndex)
{
    return SightlineOverlayBuilder::buildGetOverlayObjectsIds(cameraIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetOverlayObjectParams(std::uint8_t objectId)
{
    return SightlineOverlayBuilder::buildGetOverlayObjectParams(objectId);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetDecoderParameters(const MsgDecoderParameters& msg)
{
    return SightlineCompressionBuilder::buildSetDecoderParameters(msg);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetDecoderParameters(std::uint8_t decoderIndex)
{
    return SightlineCompressionBuilder::buildGetDecoderParameters(decoderIndex);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSendToBTS(const MsgSendToBTS& msg)
{
    return SightlineSerialBuilder::buildSendToBTS(msg);
}

// ==============================================================================
// Raw Packet Assembly Engine
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildRawPacket(
    MessageId id, const std::vector<std::uint8_t>& payload)
{
    return SightlineFraming::buildPacket(id, payload);
}

} // namespace Sightline
