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

    /// @brief Encodes network video stream destination (Message ID 0x1A).
    /// @param[in] msg Ethernet video parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetEthernetVideo(const MsgSetEthernetVideoParameters& msg);

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

    // --- Raw Transport Helper ---

    /// @brief Encodes arbitrary payload into a framed, CRC-checked SLA packet.
    /// @param[in] id SLA Message ID.
    /// @param[in] payload Raw message payload bytes.
    /// @return Complete framed packet bytes.
    [[nodiscard]] static std::vector<std::uint8_t> buildRawPacket(
        MessageId id, const std::vector<std::uint8_t>& payload);
};

} // namespace Sightline
