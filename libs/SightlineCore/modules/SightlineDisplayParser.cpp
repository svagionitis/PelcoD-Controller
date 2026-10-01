/// @file SightlineDisplayParser.cpp
/// @brief Implementation of Sightline display layout frame deserializers.

#include "SightlineDisplayParser.h"

namespace Sightline {

bool SightlineDisplayParser::parseDisplayParameters(ByteView packet, MsgSetDisplayParameters& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentDisplayParameters && id != MessageId::SetDisplayParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 10U) {
        return false;
    }

    out.displayIndex = payload[0U];
    out.cameraIndex = payload[1U];
    out.xOffset = SightlineFraming::readU16Le(payload.data() + 2U);
    out.yOffset = SightlineFraming::readU16Le(payload.data() + 4U);
    out.displayWidth = SightlineFraming::readU16Le(payload.data() + 6U);
    out.displayHeight = SightlineFraming::readU16Le(payload.data() + 8U);
    return true;
}

bool SightlineDisplayParser::parseVideoDisplay(ByteView packet, MsgVideoDisplay& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::VideoDisplay) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 5U) {
        return false;
    }

    out.displayIndex = payload[0U];
    out.cameraIndex = payload[1U];
    out.aspectRatio = payload[2U];
    out.rotation = payload[3U];
    out.mirror = payload[4U];
    return true;
}

bool SightlineDisplayParser::parseMultiDisplay(ByteView packet, MsgMultiDisplay& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::MultiDisplay) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 11U) {
        return false;
    }

    out.displayIndex = payload[0U];
    out.layout = payload[1U];
    out.pipCameraIndex = payload[2U];
    out.pipX = SightlineFraming::readU16Le(payload.data() + 3U);
    out.pipY = SightlineFraming::readU16Le(payload.data() + 5U);
    out.pipWidth = SightlineFraming::readU16Le(payload.data() + 7U);
    out.pipHeight = SightlineFraming::readU16Le(payload.data() + 9U);
    return true;
}

} // namespace Sightline
