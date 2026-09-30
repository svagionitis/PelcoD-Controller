/// @file SightlineProtocolBuilder.cpp
/// @brief Implementation of Sightline SLA protocol command serializer.

#include "SightlineProtocolBuilder.h"

#include <cstring>

namespace Sightline {

namespace {

    void appendU16Le(std::vector<std::uint8_t>& buf, std::uint16_t val)
    {
        buf.push_back(static_cast<std::uint8_t>(val & 0xFFU));
        buf.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
    }

    void appendS16Le(std::vector<std::uint8_t>& buf, std::int16_t val)
    {
        appendU16Le(buf, static_cast<std::uint16_t>(val));
    }

    void appendU32Le(std::vector<std::uint8_t>& buf, std::uint32_t val)
    {
        buf.push_back(static_cast<std::uint8_t>(val & 0xFFU));
        buf.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
        buf.push_back(static_cast<std::uint8_t>((val >> 16U) & 0xFFU));
        buf.push_back(static_cast<std::uint8_t>((val >> 24U) & 0xFFU));
    }

    [[maybe_unused]] void appendS32Le(std::vector<std::uint8_t>& buf, std::int32_t val)
    {
        appendU32Le(buf, static_cast<std::uint32_t>(val));
    }

    void appendU64Le(std::vector<std::uint8_t>& buf, std::uint64_t val)
    {
        appendU32Le(buf, static_cast<std::uint32_t>(val & 0xFFFFFFFFU));
        appendU32Le(buf, static_cast<std::uint32_t>((val >> 32U) & 0xFFFFFFFFU));
    }

    [[maybe_unused]] void appendFloat32Le(std::vector<std::uint8_t>& buf, float val)
    {
        std::uint32_t raw { 0U };
        std::memcpy(&raw, &val, sizeof(float));
        appendU32Le(buf, raw);
    }

    void appendDouble64Le(std::vector<std::uint8_t>& buf, double val)
    {
        std::uint64_t raw { 0U };
        std::memcpy(&raw, &val, sizeof(double));
        appendU64Le(buf, raw);
    }

    void appendString(std::vector<std::uint8_t>& buf, const std::string& str)
    {
        buf.insert(buf.end(), str.begin(), str.end());
        buf.push_back(0x00U); // Null terminator
    }

} // namespace

// ==============================================================================
// 1. System, Configuration & Diagnostic Messages
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGetParameters(std::uint8_t queryId)
{
    const std::vector<std::uint8_t> payload { queryId };
    return buildRawPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildResetAllParameters(const MsgResetAllParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.resetType };
    return buildRawPacket(MessageId::ResetAllParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSaveParameters(const MsgSaveParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.commitType };
    return buildRawPacket(MessageId::SaveParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetNetworkParameters(const MsgSetNetworkParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(17U);
    appendU32Le(payload, msg.ipAddress);
    appendU32Le(payload, msg.subnetMask);
    appendU32Le(payload, msg.gateway);
    payload.push_back(msg.dhcpEnable);
    appendU16Le(payload, msg.commandPort);
    appendU16Le(payload, msg.replyPort);
    return buildRawPacket(MessageId::SetNetworkParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetPortConfiguration(const MsgSetPortConfiguration& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(6U);
    payload.push_back(msg.portIndex);
    appendU32Le(payload, msg.baudRate);
    payload.push_back(msg.mode);
    return buildRawPacket(MessageId::SetPortConfiguration, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildCommandPassThrough(const MsgCommandPassThrough& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(1U + msg.data.size());
    payload.push_back(msg.destPort);
    payload.insert(payload.end(), msg.data.begin(), msg.data.end());
    return buildRawPacket(MessageId::CommandPassThrough, payload);
}

// ==============================================================================
// 2. Video Tracking, Acquisition & Motion Analysis Messages
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildStartTracking(const MsgStartTracking& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(10U);
    payload.push_back(msg.cameraIndex);
    appendU16Le(payload, msg.centerCol);
    appendU16Le(payload, msg.centerRow);
    appendU16Le(payload, msg.width);
    appendU16Le(payload, msg.height);
    payload.push_back(msg.flags);

    return buildRawPacket(MessageId::StartTracking, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildStopTracking(const MsgStopTracking& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.trackId };
    return buildRawPacket(MessageId::StopTracking, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildModifyTracking(const MsgModifyTracking& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.trackId, msg.mode, msg.flags };
    return buildRawPacket(MessageId::ModifyTracking, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildNudgeTracking(const MsgNudgeTrackingCoordinate& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(5U);
    payload.push_back(msg.cameraIndex);
    appendS16Le(payload, msg.deltaCol);
    appendS16Le(payload, msg.deltaRow);

    return buildRawPacket(MessageId::NudgeTrackingCoordinate, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetReportingMode(const MsgCoordinateReportingMode& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.framePeriod, msg.reportingFlags };
    return buildRawPacket(MessageId::CoordinateReportingMode, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetTrackingParameters(const MsgSetTrackingParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(7U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.mode);
    appendU16Le(payload, msg.acquisitionSearchCol);
    appendU16Le(payload, msg.acquisitionSearchRow);
    payload.push_back(msg.flags);
    return buildRawPacket(MessageId::SetTrackingParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildDesignatePrimary(const MsgDesignateSelectedTrackPrimary& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.trackId };
    return buildRawPacket(MessageId::DesignateSelectedTrackPrimary, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildShiftSelectedTrack(const MsgShiftSelectedTrack& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(6U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.trackId);
    appendS16Le(payload, msg.shiftCol);
    appendS16Le(payload, msg.shiftRow);
    return buildRawPacket(MessageId::ShiftSelectedTrack, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildStopSelectedTrack(const MsgStopSelectedTrack& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.trackId };
    return buildRawPacket(MessageId::StopSelectedTrack, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetDetectionParams(const MsgSetDetectionParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(7U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.mode);
    payload.push_back(msg.threshold);
    appendU16Le(payload, msg.minTargetSize);
    appendU16Le(payload, msg.maxTargetSize);
    return buildRawPacket(MessageId::SetDetectionParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildCustomAIDetect(const MsgCustomAIDetect& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.modelId, msg.confidenceThreshold, msg.nmsThreshold };
    return buildRawPacket(MessageId::CustomAIDetect, payload);
}

// ==============================================================================
// 3. Video Stabilization, Alignment & Enhancement Messages
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetStabilization(const MsgSetStabilizationParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.mode, msg.autoBias, msg.maxShift, msg.flags };
    return buildRawPacket(MessageId::SetStabilizationParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildResetStabilization(const MsgResetStabilizationParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex };
    return buildRawPacket(MessageId::ResetStabilizationParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetStabilizationBias(const MsgSetStabilizationBias& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(7U);
    payload.push_back(msg.cameraIndex);
    appendS16Le(payload, msg.biasCol);
    appendS16Le(payload, msg.biasRow);
    appendS16Le(payload, msg.biasRotation);
    return buildRawPacket(MessageId::StabilizationBias, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetRegistration(const MsgSetRegistrationParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.searchRange, msg.pyramidLevels, msg.flags };
    return buildRawPacket(MessageId::SetRegistrationParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetBlendParameters(const MsgSetBlendParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.primaryCamera, msg.secondaryCamera, msg.blendMode, msg.alphaPercent };
    return buildRawPacket(MessageId::SetBlendParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetNoise3D(const MsgNoise3D& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.enable, msg.temporalStrength, msg.spatialStrength };
    return buildRawPacket(MessageId::Noise3D, payload);
}

// ==============================================================================
// 4. Video Pipeline, Display & Streaming Messages
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetVideoParameters(const MsgSetVideoParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(7U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.inputFormat);
    appendU16Le(payload, msg.width);
    appendU16Le(payload, msg.height);
    payload.push_back(msg.frameRate);
    return buildRawPacket(MessageId::SetVideoParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetVideoMode(const MsgSetVideoMode& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.freeze, msg.digitalZoom, msg.mirror, msg.flip };
    return buildRawPacket(MessageId::SetVideoMode, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetVideoEnhance(const MsgSetVideoEnhancement& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.contrast, msg.brightness, msg.sharpening,
        msg.claheEnable };
    return buildRawPacket(MessageId::SetVideoEnhancementParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetDisplayParams(const MsgSetDisplayParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(10U);
    payload.push_back(msg.displayIndex);
    payload.push_back(msg.cameraIndex);
    appendU16Le(payload, msg.xOffset);
    appendU16Le(payload, msg.yOffset);
    appendU16Le(payload, msg.displayWidth);
    appendU16Le(payload, msg.displayHeight);
    return buildRawPacket(MessageId::SetDisplayParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetEthernetVideo(const MsgSetEthernetVideoParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(10U);
    payload.push_back(msg.streamIndex);
    appendU32Le(payload, msg.destIpAddress);
    appendU16Le(payload, msg.destPort);
    payload.push_back(msg.protocol);
    appendU16Le(payload, msg.ttl);
    return buildRawPacket(MessageId::SetEthernetVideoParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetH264Parameters(const MsgSetH264Parameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(9U);
    payload.push_back(msg.streamIndex);
    appendU32Le(payload, msg.targetBitrateBps);
    appendU16Le(payload, msg.gopLength);
    payload.push_back(msg.qualityLevel);
    payload.push_back(msg.rateControl);
    return buildRawPacket(MessageId::SetH264Parameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetSDRecording(const MsgSetSDRecordingParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U + msg.filenamePrefix.size() + 1U);
    payload.push_back(msg.recordingState);
    payload.push_back(msg.cameraIndex);
    appendString(payload, msg.filenamePrefix);
    return buildRawPacket(MessageId::SetSDRecordingParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildStreamingControl(const MsgStreamingControl& msg)
{
    const std::vector<std::uint8_t> payload { msg.streamIndex, msg.action };
    return buildRawPacket(MessageId::StreamingControl, payload);
}

// ==============================================================================
// 5. Metadata, KLV & Telemetry Messages
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetMetadataValues(const MsgSetMetadataValues& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(64U);
    appendDouble64Le(payload, msg.platformLatitudeDeg);
    appendDouble64Le(payload, msg.platformLongitudeDeg);
    appendDouble64Le(payload, msg.platformAltitudeMeters);
    appendDouble64Le(payload, msg.platformHeadingDeg);
    appendDouble64Le(payload, msg.platformPitchDeg);
    appendDouble64Le(payload, msg.platformRollDeg);
    appendDouble64Le(payload, msg.sensorHorizontalFovDeg);
    appendDouble64Le(payload, msg.sensorVerticalFovDeg);
    return buildRawPacket(MessageId::SetMetadataValues, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildMetadataStaticValues(const MsgMetadataStaticValues& msg)
{
    std::vector<std::uint8_t> payload {};
    appendString(payload, msg.missionId);
    appendString(payload, msg.platformTailNumber);
    appendString(payload, msg.securityClassification);
    return buildRawPacket(MessageId::MetadataStaticValues, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetMetadataRate(const MsgSetMetadataRate& msg)
{
    const std::vector<std::uint8_t> payload { msg.metadataType, msg.ratePeriod };
    return buildRawPacket(MessageId::SetMetadataRate, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetTelemetryDest(const MsgSetTelemetryDestination& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(8U);
    payload.push_back(msg.clientIndex);
    appendU32Le(payload, msg.clientIpAddress);
    appendU16Le(payload, msg.clientPort);
    payload.push_back(msg.flags);
    return buildRawPacket(MessageId::SetTelemetryDestination, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildCursorOnTarget(const MsgCursorOnTarget& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.push_back(msg.enable);
    appendU16Le(payload, msg.broadcastPort);
    appendString(payload, msg.uid);
    appendString(payload, msg.cotType);
    return buildRawPacket(MessageId::CursorOnTarget, payload);
}

// ==============================================================================
// 6. Optics, Lens & Hardware Messages
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildLensCommand(const MsgLensCommand& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(4U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.commandType);
    appendS16Le(payload, msg.rateOrPosition);

    return buildRawPacket(MessageId::LensCommand, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildFocusParameters(const MsgFocusParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(10U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.focusMode);
    appendU16Le(payload, msg.roiX);
    appendU16Le(payload, msg.roiY);
    appendU16Le(payload, msg.roiWidth);
    appendU16Le(payload, msg.roiHeight);
    return buildRawPacket(MessageId::FocusParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetLensParameters(const MsgSetLensParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(33U);
    payload.push_back(msg.cameraIndex);
    appendDouble64Le(payload, msg.minFocalLengthMm);
    appendDouble64Le(payload, msg.maxFocalLengthMm);
    appendDouble64Le(payload, msg.horizontalFovWideDeg);
    appendDouble64Le(payload, msg.horizontalFovTeleDeg);
    return buildRawPacket(MessageId::SetLensParameters, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildGPIO(const MsgGPIO& msg)
{
    const std::vector<std::uint8_t> payload { msg.pinMask, msg.pinValues, msg.directionMask };
    return buildRawPacket(MessageId::GPIO, payload);
}

// ==============================================================================
// 7. Reticles, Overlays & Graphics Messages
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildSetOverlayMode(const MsgSetOverlayMode& msg)
{
    const std::vector<std::uint8_t> payload { msg.displayIndex, msg.reticleMode, msg.trackingBoxMode,
        msg.telemetryTextMode };
    return buildRawPacket(MessageId::SetOverlayMode, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildDrawObject(const MsgDrawObject& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(15U + msg.text.size() + 1U);
    payload.push_back(msg.displayIndex);
    payload.push_back(msg.objectId);
    payload.push_back(msg.shapeType);
    appendU16Le(payload, msg.x);
    appendU16Le(payload, msg.y);
    appendU16Le(payload, msg.width);
    appendU16Le(payload, msg.height);
    appendU32Le(payload, msg.colorRgba);
    appendString(payload, msg.text);
    return buildRawPacket(MessageId::DrawObject, payload);
}

std::vector<std::uint8_t> SightlineProtocolBuilder::buildDrawOverlay(const MsgDrawOverlay& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U + (msg.objects.size() * 16U));
    payload.push_back(msg.displayIndex);
    payload.push_back(msg.clearDisplay);
    payload.push_back(static_cast<std::uint8_t>(msg.objects.size()));

    for (const auto& obj : msg.objects) {
        payload.push_back(obj.objectId);
        payload.push_back(obj.shapeType);
        appendU16Le(payload, obj.x);
        appendU16Le(payload, obj.y);
        appendU16Le(payload, obj.width);
        appendU16Le(payload, obj.height);
        appendU32Le(payload, obj.colorRgba);
        appendString(payload, obj.text);
    }

    return buildRawPacket(MessageId::DrawOverlay, payload);
}

// ==============================================================================
// Raw Packet Assembly Engine
// ==============================================================================

std::vector<std::uint8_t> SightlineProtocolBuilder::buildRawPacket(
    MessageId id, const std::vector<std::uint8_t>& payload)
{
    // Length covers Message ID + Payload + Checksum
    const std::size_t payloadAndCsLen = 1U + payload.size() + 1U;

    std::vector<std::uint8_t> packet {};
    packet.reserve(payloadAndCsLen + 4U);

    packet.push_back(HeaderByte1);
    packet.push_back(HeaderByte2);

    if (payloadAndCsLen < 128U) {
        // Normal 1-byte length
        packet.push_back(static_cast<std::uint8_t>(payloadAndCsLen));
    } else {
        // Extended 2-byte length: bit 7 set on low byte
        const auto lenLow = static_cast<std::uint8_t>((payloadAndCsLen & 0x7FU) | 0x80U);
        const auto lenHigh = static_cast<std::uint8_t>((payloadAndCsLen >> 7U) & 0xFFU);
        packet.push_back(lenLow);
        packet.push_back(lenHigh);
    }

    const std::size_t crcStartIdx = packet.size();
    packet.push_back(static_cast<std::uint8_t>(id));
    packet.insert(packet.end(), payload.begin(), payload.end());

    const std::uint8_t crc = SightlineCrc8::compute(packet.data() + crcStartIdx, packet.size() - crcStartIdx);
    packet.push_back(crc);

    return packet;
}

} // namespace Sightline
