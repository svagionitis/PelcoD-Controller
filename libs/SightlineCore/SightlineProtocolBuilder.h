#pragma once

/// @file SightlineProtocolBuilder.h
/// @brief Packet serializer encoding high-level commands into framed SLA binary packets.

#include "SightlineCrc8.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineProtocolBuilder
/// @brief Assembles framed, CRC-valid binary packets conforming to IDD-SLA-Protocol_3_11_6.
class SightlineProtocolBuilder {
public:
    // --- System & Control ---

    /// @brief Encodes get version number query (Message ID 0x00).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVersionNumber();

    /// @brief Encodes a generic parameter query (Message ID 0x28).
    /// @param[in] queryId Target setter Message ID to query.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetParameters(std::uint8_t queryId);

    /// @brief Encodes system reset command (Message ID 0x01).
    /// @param[in] msg Reset parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildResetAllParameters(const MsgResetAllParameters& msg);

    /// @brief Encodes save to flash command (Message ID 0x25).
    /// @param[in] msg Save parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSaveParameters(const MsgSaveParameters& msg);

    /// @brief Encodes system status mode configuration (Message ID 0x80).
    /// @param[in] msg Status mode settings.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSystemStatusMode(const MsgSystemStatusMode& msg);

    /// @brief Encodes network parameters configuration (Message ID 0x1C).
    /// @param[in] msg Network parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetNetworkParameters(const MsgSetNetworkParameters& msg);

    /// @brief Encodes serial port configuration (Message ID 0x3E).
    /// @param[in] msg Port parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetPortConfiguration(const MsgSetPortConfiguration& msg);

    /// @brief Encodes transparent pass-through data packet (Message ID 0x3D).
    /// @param[in] msg Passthrough payload.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCommandPassThrough(const MsgCommandPassThrough& msg);

    // --- Video Tracking & Motion ---

    /// @brief Encodes target tracking acquisition command (Message ID 0x08).
    /// @param[in] msg Start tracking parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildStartTracking(const MsgStartTracking& msg);

    /// @brief Encodes target tracking termination command (Message ID 0x09).
    /// @param[in] msg Stop tracking parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildStopTracking(const MsgStopTracking& msg);

    /// @brief Encodes active track modification command (Message ID 0x05).
    /// @param[in] msg Modify tracking parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildModifyTracking(const MsgModifyTracking& msg);

    /// @brief Encodes sub-pixel tracking nudge command (Message ID 0x0A).
    /// @param[in] msg Coordinate trim nudge parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildNudgeTracking(const MsgNudgeTrackingCoordinate& msg);

    /// @brief Encodes coordinate reporting mode configuration (Message ID 0x0B).
    /// @param[in] msg Reporting parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetReportingMode(const MsgCoordinateReportingMode& msg);

    /// @brief Encodes tracking algorithm tuning parameters (Message ID 0x0C).
    /// @param[in] msg Tracking parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTrackingParameters(const MsgSetTrackingParameters& msg);

    /// @brief Encodes track index modification (stop/primary) (Message ID 0x17).
    /// @param[in] msg Track index modify parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildModifyTrackIndex(const MsgModifyTrackIndex& msg);

    /// @brief Encodes tracking trail parameters (Message ID 0x9D).
    /// @param[in] msg Track trail parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildTrackTrails(const MsgTrackTrails& msg);

    /// @brief Encodes track promotion to primary (Message ID 0x32).
    /// @param[in] msg Designate primary parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDesignatePrimary(const MsgDesignateSelectedTrackPrimary& msg);

    /// @brief Encodes track gate shift relative to centroid (Message ID 0x33).
    /// @param[in] msg Shift parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildShiftSelectedTrack(const MsgShiftSelectedTrack& msg);

    /// @brief Encodes termination of single track (Message ID 0x3C).
    /// @param[in] msg Stop track parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildStopSelectedTrack(const MsgStopSelectedTrack& msg);

    /// @brief Encodes MTI moving target detection configuration (Message ID 0x2D).
    /// @param[in] msg Detection parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDetectionParams(const MsgSetDetectionParameters& msg);

    /// @brief Encodes custom AI inference model execution (Message ID 0xBA).
    /// @param[in] msg AI detect parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCustomAIDetect(const MsgCustomAIDetect& msg);

    // --- VMTI & Tactical Video Analytics (Phase 2) ---

    /// @brief Encodes Video Moving Target Indication (VMTI) configuration (Message ID 0x84).
    /// @param[in] msg VMTI parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVMTI(const MsgSetVMTI& msg);

    /// @brief Encodes query for active VMTI configuration (Message ID 0x28 query 0x84).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVMTI(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes detection region of interest (ROI) bounding box (Message ID 0x7C).
    /// @param[in] msg Detection ROI parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDetectionROI(const MsgDetectionROI& msg);

    /// @brief Encodes query for active detection ROI (Message ID 0x28 query 0x7C).
    /// @param[in] cameraIndex Target camera index.
    /// @param[in] roiIndex ROI slot index (0..3).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDetectionROI(
        std::uint8_t cameraIndex = 0U, std::uint8_t roiIndex = 0U);

    /// @brief Encodes advanced detection parameters (Message ID 0x76).
    /// @param[in] msg Advanced detection parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetAdvDetectionParams(
        const MsgAdvancedDetectionParameters& msg);

    /// @brief Encodes query for advanced detection parameters (Message ID 0x28 query 0x76).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetAdvDetectionParams(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for tracking box pixel luminance statistics (Message ID 0x28 query 0x78).
    /// @param[in] cameraIndex Target camera index.
    /// @param[in] trackId Target track ID.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTrackingPixelStats(
        std::uint8_t cameraIndex = 0U, std::uint8_t trackId = 0U);

    /// @brief Encodes automated detection snapshot trigger (Message ID 0xAB).
    /// @param[in] msg Snapshot trigger parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDoDetectSnapShot(const MsgDoDetectSnapShot& msg);

    /// @brief Encodes extracted target thumbnail image chip packet (Message ID 0xAD).
    /// @param[in] msg VMTI chip parameters and raw payload buffer.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildVMTIChips(const MsgVMTIChips& msg);

    /// @brief Encodes MISB ST 0903 KLV stream VMTI field insertion mask (Message ID 0xBF).
    /// @param[in] msg VMTI fields configuration.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVMTIFields(const MsgVMTIFields& msg);

    /// @brief Encodes query for active VMTI fields configuration (Message ID 0x28 query 0xBF).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVMTIFields(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes STANAG 4609 KLV target class filtering rules (Message ID 0xC1).
    /// @param[in] msg KLV class filter parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetKlvClassFilters(const MsgKlvClassFilters& msg);

    /// @brief Encodes query for active KLV class filter rules (Message ID 0x28 query 0xC1).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetKlvClassFilters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes deep learning multi-class categorization report (Message ID 0xBD).
    /// @param[in] msg Multi-class telemetry data.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildTrackingMultiClass(const MsgTrackingMultiClass& msg);

    /// @brief Encodes custom neural network classifier pipeline activation (Message ID 0xA7).
    /// @param[in] msg Custom classifier settings.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetCustomClassifier(const MsgCustomClassifier& msg);

    /// @brief Encodes query for custom classifier configuration (Message ID 0x28 query 0xA7).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCustomClassifier(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes classifier execution bounds and threshold parameters (Message ID 0xA9).
    /// @param[in] msg Classifier parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetClassifierParams(const MsgClassifierParameters& msg);

    /// @brief Encodes query for classifier execution parameters (Message ID 0x28 query 0xA9).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetClassifierParams(std::uint8_t cameraIndex = 0U);

    // --- Stabilization & Enhancement ---

    /// @brief Encodes video stabilization configuration (Message ID 0x02).
    /// @param[in] msg Stabilization parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetStabilization(const MsgSetStabilizationParameters& msg);

    /// @brief Encodes stabilization filter reset (Message ID 0x04).
    /// @param[in] msg Reset parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildResetStabilization(const MsgResetStabilizationParameters& msg);

    /// @brief Encodes stabilization bias correction (Message ID 0x12).
    /// @param[in] msg Bias parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetStabilizationBias(const MsgSetStabilizationBias& msg);

    /// @brief Encodes frame registration parameters (Message ID 0x0E).
    /// @param[in] msg Registration parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetRegistration(const MsgSetRegistrationParameters& msg);

    /// @brief Encodes multi-sensor blending fusion (Message ID 0x2F).
    /// @param[in] msg Blend parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetBlendParameters(const MsgSetBlendParameters& msg);

    /// @brief Encodes 3D spatio-temporal noise reduction (Message ID 0xAF).
    /// @param[in] msg Noise reduction parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetNoise3D(const MsgNoise3D& msg);

    // --- Multi-Sensor Alignment & Digital Video Pipeline (Phase 3) ---

    /// @brief Encodes 4-point projective homography calibration (Message ID 0x95).
    /// @param[in] msg Four align points parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildFourAlignPoints(const MsgFourAlignPoints& msg);

    /// @brief Encodes query for 4-point projective calibration (Message ID 0x28 query 0x95).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetFourAlignPoints(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes fine-tune alignment offsets and automated registration (Message ID 0xB9).
    /// @param[in] msg Blend align parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetBlendAlign(const MsgBlendAlign& msg);

    /// @brief Encodes query for blend alignment parameters (Message ID 0x28 query 0xB9).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetBlendAlign(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes camera input channel switch (Message ID 0x82).
    /// @param[in] msg Camera switch parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCameraSwitch(const MsgCameraSwitch& msg);

    /// @brief Encodes advanced capture deserializer hardware registers (Message ID 0x7B).
    /// @param[in] msg Advanced capture parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetAdvCaptureParams(const MsgAdvancedCaptureParameters& msg);

    /// @brief Encodes query for advanced capture parameters (Message ID 0x28 query 0x7B).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetAdvCaptureParams(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes digital video framing decoder parameters (Message ID 0x91).
    /// @param[in] msg Digital video parser parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDigiVideoParser(const MsgDigitalVideoParserParameters& msg);

    /// @brief Encodes query for digital video parser parameters (Message ID 0x28 query 0x91).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDigiVideoParser(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for camera sensor capabilities (Message ID 0x28 query 0xBB).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCameraCapabilities(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes multi-channel video display routing and aspect ratio (Message ID 0xA4).
    /// @param[in] msg Video display parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVideoDisplay(const MsgVideoDisplay& msg);

    /// @brief Encodes query for video display routing (Message ID 0x28 query 0xA4).
    /// @param[in] displayIndex Target display index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVideoDisplay(std::uint8_t displayIndex = 0U);

    /// @brief Encodes multi-display split screen and PiP window routing (Message ID 0xA5).
    /// @param[in] msg Multi-display parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMultiDisplay(const MsgMultiDisplay& msg);

    /// @brief Encodes query for multi-display configuration (Message ID 0x28 query 0xA5).
    /// @param[in] displayIndex Target display index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMultiDisplay(std::uint8_t displayIndex = 0U);

    // --- Video & Display Pipeline ---

    /// @brief Encodes video capture format (Message ID 0x10).
    /// @param[in] msg Video parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVideoParameters(const MsgSetVideoParameters& msg);

    /// @brief Encodes video freeze/zoom/flip mode (Message ID 0x1F).
    /// @param[in] msg Mode parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVideoMode(const MsgSetVideoMode& msg);

    /// @brief Encodes contrast/brightness/CLAHE enhancement (Message ID 0x21).
    /// @param[in] msg Enhancement parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVideoEnhance(const MsgSetVideoEnhancement& msg);

    /// @brief Encodes display layout and scaling (Message ID 0x16).
    /// @param[in] msg Display parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDisplayParams(const MsgSetDisplayParameters& msg);

    /// @brief Encodes video frame rate, downsample, and quality over Ethernet (Message ID 0x1A).
    /// @param[in] msg Ethernet video parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetEthernetVideo(const MsgSetEthernetVideoParameters& msg);

    /// @brief Encodes network video stream destination and port (Message ID 0x29).
    /// @param[in] msg Ethernet display parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetEthernetDisplay(const MsgSetEthernetDisplayParameters& msg);

    /// @brief Encodes H.264 compression bitrate and GOP (Message ID 0x23).
    /// @param[in] msg Encoder parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetH264Parameters(const MsgSetH264Parameters& msg);

    /// @brief Encodes SD card recording control (Message ID 0x1E).
    /// @param[in] msg Recording parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetSDRecording(const MsgSetSDRecordingParameters& msg);

    /// @brief Encodes streaming pipeline play/pause/stop (Message ID 0x90).
    /// @param[in] msg Streaming control parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildStreamingControl(const MsgStreamingControl& msg);

    // --- Metadata & KLV ---

    /// @brief Encodes platform position telemetry for KLV insertion (Message ID 0x13).
    /// @param[in] msg Metadata values.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMetadataValues(const MsgSetMetadataValues& msg);

    /// @brief Encodes static mission and classification metadata (Message ID 0x14).
    /// @param[in] msg Static metadata values.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildMetadataStaticValues(const MsgMetadataStaticValues& msg);

    /// @brief Encodes KLV transmission rate (Message ID 0x62).
    /// @param[in] msg Metadata rate parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMetadataRate(const MsgSetMetadataRate& msg);

    /// @brief Encodes external telemetry destination IP/port (Message ID 0x64).
    /// @param[in] msg Destination parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTelemetryDest(const MsgSetTelemetryDestination& msg);

    /// @brief Encodes Cursor-on-Target XML tactical broadcast (Message ID 0xB0).
    /// @param[in] msg CoT parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCursorOnTarget(const MsgCursorOnTarget& msg);

    // --- Lens & Optics ---

    /// @brief Encodes motorized lens command (Message ID 0xB2).
    /// @param[in] msg Lens command parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildLensCommand(const MsgLensCommand& msg);

    /// @brief Encodes focus mode and ROI (Message ID 0xB3).
    /// @param[in] msg Focus parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildFocusParameters(const MsgFocusParameters& msg);

    /// @brief Encodes optical calibration parameters (Message ID 0x6E / 0xB1).
    /// @param[in] msg Lens calibration parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetLensParameters(const MsgSetLensParameters& msg);

    /// @brief Encodes GPIO pin state and direction (Message ID 0xB6).
    /// @param[in] msg GPIO parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGPIO(const MsgGPIO& msg);

    // --- Thermal NUC & Sensor Calibration ---

    /// @brief Encodes NUC calibration and shutter mode command (Message ID 0x35).
    /// @param[in] msg NUC parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildNucParameters(const MsgNucParameters& msg);

    /// @brief Encodes dead pixel replacement configuration (Message ID 0xA8).
    /// @param[in] msg Dead pixel parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDeadPixel(const MsgDeadPixel& msg);

    /// @brief Encodes NUC flash read/write/restore command (Message ID 0x36).
    /// @param[in] msg Read/write NUC parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildReadWriteNuc(const MsgReadWriteNuc& msg);

    /// @brief Encodes custom pseudo-color thermal palette table (Message ID 0x72).
    /// @param[in] msg User palette table.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetUserPalette(const MsgUserPalette& msg);

    /// @brief Encodes geometric camera intrinsic calibration parameters (Message ID 0xC0).
    /// @param[in] msg Camera calibration parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCameraCalibration(const MsgCameraCalibration& msg);

    /// @brief Encodes camera parameter file load/save command (Message ID 0xC2).
    /// @param[in] msg Camera parameter file parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCameraParameterFile(const MsgCameraParameterFile& msg);

    // --- Overlays & Reticles ---

    /// @brief Encodes overlay reticle and telemetry mode (Message ID 0x06).
    /// @param[in] msg Overlay mode parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetOverlayMode(const MsgSetOverlayMode& msg);

    /// @brief Encodes single dynamic overlay graphic primitive (Message ID 0x3B).
    /// @param[in] msg Object parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDrawObject(const MsgDrawObject& msg);

    /// @brief Encodes multi-primitive overlay update (Message ID 0x9C).
    /// @param[in] msg Draw overlay parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDrawOverlay(const MsgDrawOverlay& msg);

    // --- Parameter Query Getters ---

    /// @brief Encodes query for active blend parameters (Message ID 0x30).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetBlendParameters();

    /// @brief Encodes query for active video parameters (Message ID 0x11).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVideoParameters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active video mode (Message ID 0x20).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVideoMode();

    /// @brief Encodes query for custom AI detect parameters (Message ID 0x28 query 0xBA).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCustomAIDetect();

    /// @brief Encodes query for active H.264 parameters (Message ID 0x24).
    /// @param[in] displayId Network display mask ID.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetH264Parameters(std::uint16_t displayId = 0x0002U);

    /// @brief Encodes query for active detection parameters (Message ID 0x2E).
    /// @param[in] cameraIndex Target camera index.
    /// @param[in] detIdx Detection index (0 or 1).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDetectionParams(
        std::uint8_t cameraIndex = 0U, std::uint8_t detIdx = 0U);

    /// @brief Encodes query for active display parameters (Message ID 0x3A).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDisplayParams(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active video enhancement parameters (Message ID 0x22).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVideoEnhance(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for focus parameters (Message ID 0x28 query 0xB3).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetFocusParameters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for lens parameters (Message ID 0x28 query 0xB1).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetLensParameters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes hardware ID query (Message ID 0x50).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetHardwareId();

    /// @brief Encodes query for active metadata values (Message ID 0x28 query 0x13).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMetadataValues();

    /// @brief Encodes query for landing aid parameters (Message ID 0x28 query 0x81).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetLandingAid();

    /// @brief Encodes query for active network parameters (Message ID 0x1D).
    /// @param[in] index Network interface index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetNetworkParameters(std::uint8_t index = 0U);

    /// @brief Encodes query for active Ethernet video parameters (Message ID 0x1B).
    /// @param[in] displayId Network display mask ID.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetEthernetVideo(std::uint16_t displayId = 0x0002U);

    /// @brief Encodes query for active Ethernet display parameters (Message ID 0x39).
    /// @param[in] displayId Network display mask ID.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetEthernetDisplay(std::uint16_t displayId = 0x0002U);

    /// @brief Encodes query for network interfaces list (Message ID 0x66).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetNetworkList();

    /// @brief Encodes query for active NUC parameters (Message ID 0x28 query 0x35).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetNucParameters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active user palette (Message ID 0x28 query 0x72).
    /// @param[in] paletteIndex Palette slot index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetUserPalette(std::uint8_t paletteIndex = 0U);

    /// @brief Encodes query for dead pixel detection statistics (Message ID 0x28 query 0xA1).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDeadPixelStats(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for camera calibration parameters (Message ID 0x28 query 0xC0).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCameraCalibration(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active overlay mode (Message ID 0x07).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetOverlayMode(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active snapshot state (Message ID 0x5F).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetSnapShot(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active SD recording parameters (Message ID 0x28 query 0x1E).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetSDRecording();

    /// @brief Encodes query for active port configuration (Message ID 0x3F).
    /// @param[in] port Port index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetPortConfiguration(std::uint8_t port = 0U);

    /// @brief Encodes query for active stabilization parameters (Message ID 0x03).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetStabilization(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active registration parameters (Message ID 0x0F).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetRegistration(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for coordinate reporting mode (Message ID 0x28 query 0x0B).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCoordReportingMode(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active tracking parameters (Message ID 0x0D).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTrackingParameters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active streaming control (Message ID 0x28 query 0x90).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetStreamingControl(std::uint8_t streamIndex = 0U);

    /// @brief Encodes query for 3D noise reduction (Message ID 0x28 query 0xAF).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetNoise3D(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for system status mode (Message ID 0x28 query 0x80).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetSystemStatusMode();

    /// @brief Encodes query for hardware configuration (Message ID 0x28 query 0x8E).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCurrentConfig();

    /// @brief Encodes query for static metadata values (Message ID 0x28 query 0x14).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMetadataStaticValues();

    /// @brief Encodes query for metadata rate (Message ID 0x28 query 0x62).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMetadataRate();

    /// @brief Encodes query for dead pixel replacement (Message ID 0x28 query 0xA8).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDeadPixel(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for GPIO pin states (Message ID 0x28 query 0xB6).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetGPIO();

    /// @brief Encodes query for active stabilization bias (Message ID 0x28 query 0x12).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetStabilizationBias(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for telemetry destination (Message ID 0x28 query 0x64).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTelemetryDest();

    /// @brief Encodes query for tracking trails (Message ID 0x28 query 0x9D).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTrackTrails(std::uint8_t cameraIndex = 0U);

    // --- Low-Level Bus, I2C, & Telemetry Tags (Phase 4) ---

    /// @brief Encodes master I2C read/write transaction (Message ID 0x94).
    [[nodiscard]] static std::vector<std::uint8_t> buildI2CCommand(const MsgI2CCommand& msg);

    /// @brief Encodes set system value register (Message ID 0x92).
    [[nodiscard]] static std::vector<std::uint8_t> buildSetSystemValue(const MsgSystemValue& msg);

    /// @brief Encodes query for system value register (Message ID 0x28 query 0x92).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetSystemValue(std::uint8_t systemValueId);

    /// @brief Encodes custom binary tag data frame (Message ID 0x96).
    [[nodiscard]] static std::vector<std::uint8_t> buildTagData(const MsgTagData& msg);

    /// @brief Encodes tag data broadcast decimation rate (Message ID 0x97).
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTagDataRate(const MsgTagDataRate& msg);

    /// @brief Encodes query for tag data rate (Message ID 0x28 query 0x97).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTagDataRate(std::uint16_t tagId);

    /// @brief Encodes tag data source selector (Message ID 0x98).
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTagSourceSelector(const MsgTagSourceSelector& msg);

    /// @brief Encodes query for tag source selector (Message ID 0x28 query 0x98).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTagSourceSelector(std::uint16_t tagId);

    /// @brief Encodes latency timing profiler packet (Message ID 0x88).
    [[nodiscard]] static std::vector<std::uint8_t> buildDetailedTiming(const MsgDetailedTiming& msg);

    /// @brief Encodes appended metadata stream configuration (Message ID 0x89).
    [[nodiscard]] static std::vector<std::uint8_t> buildSetAppendedMetadata(const MsgAppendedMetadata& msg);

    /// @brief Encodes query for appended metadata configuration (Message ID 0x28 query 0x89).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetAppendedMetadata(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes frame index and timestamp packet (Message ID 0x8A).
    [[nodiscard]] static std::vector<std::uint8_t> buildFrameIndex(const MsgFrameIndex& msg);

    // --- Autonomous Landing Aid & Graphics (Phase 5) ---

    /// @brief Encodes landing aid search and tracking command (Message ID 0x81).
    [[nodiscard]] static std::vector<std::uint8_t> buildLandingAid(const MsgLandingAid& msg);

    /// @brief Encodes landing target relative position and orientation (Message ID 0x83).
    [[nodiscard]] static std::vector<std::uint8_t> buildLandingPosition(const MsgLandingPosition& msg);

    /// @brief Encodes logo watermark configuration (Message ID 0x9B).
    [[nodiscard]] static std::vector<std::uint8_t> buildSetLogoParameters(const MsgLogoParameters& msg);

    /// @brief Encodes query for logo watermark configuration (Message ID 0x28 query 0x9B).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetLogoParameters(std::uint8_t displayIndex = 0U);

    /// @brief Encodes dynamic ancillary text metadata / subtitle overlay (Message ID 0xAC).
    [[nodiscard]] static std::vector<std::uint8_t> buildAncillaryTextMetadata(const MsgAncillaryTextMetadata& msg);

    /// @brief Encodes custom raster font glyph table upload (Message ID 0xAE).
    [[nodiscard]] static std::vector<std::uint8_t> buildUserFont(const MsgUserFont& msg);

    /// @brief Encodes hardware video decoder configuration (Message ID 0x99).
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDecoderParameters(const MsgDecoderParameters& msg);

    /// @brief Encodes query for decoder configuration (Message ID 0x28 query 0x99).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDecoderParameters(std::uint8_t decoderIndex = 0U);

    /// @brief Encodes transparent packet forwarding to Base Transceiver Station (Message ID 0xBE).
    [[nodiscard]] static std::vector<std::uint8_t> buildSendToBTS(const MsgSendToBTS& msg);

    // --- Raw Transport Helper ---

    /// @brief Encodes arbitrary payload into a framed, CRC-checked SLA packet.
    /// @param[in] id SLA Message ID.
    /// @param[in] payload Raw message payload bytes.
    /// @return Complete framed packet bytes.
    [[nodiscard]] static std::vector<std::uint8_t> buildRawPacket(
        MessageId id, const std::vector<std::uint8_t>& payload);
};

} // namespace Sightline
