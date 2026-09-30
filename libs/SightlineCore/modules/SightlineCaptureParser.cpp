/// @file SightlineCaptureParser.cpp
/// @brief Implementation of Sightline capture frame deserializers.

#include "SightlineCaptureParser.h"

namespace Sightline {

bool SightlineCaptureParser::parseVideoParameters(
    const std::vector<std::uint8_t>& packet, MsgSetVideoParameters& out)
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

bool SightlineCaptureParser::parseVideoMode(
    const std::vector<std::uint8_t>& packet, MsgSetVideoMode& out)
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

} // namespace Sightline
