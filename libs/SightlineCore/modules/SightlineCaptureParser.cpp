/// @file SightlineCaptureParser.cpp
/// @brief Implementation of Sightline capture frame deserializers.

#include "SightlineCaptureParser.h"

namespace Sightline {

bool SightlineCaptureParser::parseVideoParameters(ByteView packet, MsgSetVideoParameters& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentVideoParameters && id != MessageId::SetVideoParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 8U) {
        return false;
    }

    out.autoChop = payload[0U];
    out.chopTop = payload[1U];
    out.chopBottom = payload[2U];
    out.chopLeft = payload[3U];
    out.chopRight = payload[4U];
    out.deinterlace = payload[5U];
    out.autoReset = payload[6U];
    out.cameraIndex = payload[7U];
    return true;
}

bool SightlineCaptureParser::parseVideoMode(ByteView packet, MsgSetVideoMode& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentVideoModeParameters && id != MessageId::SetVideoMode) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 5U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.freeze = payload[1U];
    out.digitalZoom = payload[2U];
    out.mirror = payload[3U];
    out.flip = payload[4U];
    return true;
}

bool SightlineCaptureParser::parseCameraSwitch(ByteView packet, MsgCameraSwitch& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CameraSwitch) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 3U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.switchType = payload[1U];
    out.flags = payload[2U];
    return true;
}

bool SightlineCaptureParser::parseAdvCaptureParams(ByteView packet, MsgAdvancedCaptureParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::AdvancedCaptureParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 8U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.bitDepth = payload[1U];
    out.laneCount = payload[2U];
    out.pixelClockHz = SightlineFraming::readU32Le(payload.data() + 3U);
    out.syncFlags = payload[7U];
    return true;
}

bool SightlineCaptureParser::parseDigiVideoParser(ByteView packet, MsgDigitalVideoParserParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DigitalVideoParserParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 5U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.videoStandard = payload[1U];
    out.embeddedSync = payload[2U];
    out.clockEdge = payload[3U];
    out.flags = payload[4U];
    return true;
}

bool SightlineCaptureParser::parseCameraCapabilities(ByteView packet, MsgCameraCapabilities& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CameraCapabilities) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 8U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.maxWidth = SightlineFraming::readU16Le(payload.data() + 1U);
    out.maxHeight = SightlineFraming::readU16Le(payload.data() + 3U);
    out.maxFrameRate = payload[5U];
    out.supportsZoom = payload[6U];
    out.flags = payload[7U];
    return true;
}

bool SightlineCaptureParser::parseDigitalCameraParams(ByteView packet, MsgDigitalCameraParameters& out)
{
    const auto id = SightlineFraming::identifyMessage(packet);
    if (id != MessageId::SetDigitalCameraParameters && id != MessageId::CurrentDigitalCameraParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 6U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.mode = static_cast<AutoGainMode>(payload[1U]);
    out.agHoldmax = SightlineFraming::readU16Le(payload.data() + 2U);
    out.agHoldmin = SightlineFraming::readU16Le(payload.data() + 4U);

    if (payload.size() >= 10U) {
        out.rowROIPct = payload[6U];
        out.colROIPct = payload[7U];
        out.highROIPct = payload[8U];
        out.wideROIPct = payload[9U];
    }
    if (payload.size() >= 12U) {
        out.minAGRange = SightlineFraming::readU16Le(payload.data() + 10U);
    }
    if (payload.size() >= 13U) {
        out.agRate8 = payload[12U];
    }
    if (payload.size() >= 15U) {
        out.minExp = payload[13U];
        out.maxExp = payload[14U];
    }
    if (payload.size() >= 17U) {
        out.rejectDarkTail = payload[15U];
        out.rejectBrightTail = payload[16U];
    }
    if (payload.size() >= 18U) {
        out.midpoint = payload[17U];
    }
    if (payload.size() >= 22U) {
        out.highBitDepthFlags = SightlineFraming::readU32Le(payload.data() + 18U);
    }

    return true;
}

} // namespace Sightline
