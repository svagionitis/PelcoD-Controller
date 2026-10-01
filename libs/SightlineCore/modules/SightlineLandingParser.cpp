/// @file SightlineLandingParser.cpp
/// @brief Implementation of Sightline SLA visual landing aid frame deserializers.

#include "SightlineLandingParser.h"

namespace Sightline {

bool SightlineLandingParser::parseLandingAid(ByteView packet, MsgLandingAid& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::LandingAid) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 3U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.mode = payload[1U];
    out.patternType = payload[2U];
    return true;
}

bool SightlineLandingParser::parseLandingPosition(ByteView packet, MsgLandingPosition& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::LandingPosition) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 50U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.relativeX = SightlineFraming::readDouble64Le(payload.data() + 1U);
    out.relativeY = SightlineFraming::readDouble64Le(payload.data() + 9U);
    out.relativeZ = SightlineFraming::readDouble64Le(payload.data() + 17U);
    out.yawDeg = SightlineFraming::readDouble64Le(payload.data() + 25U);
    out.pitchDeg = SightlineFraming::readDouble64Le(payload.data() + 33U);
    out.rollDeg = SightlineFraming::readDouble64Le(payload.data() + 41U);
    out.confidence = payload[49U];
    return true;
}

} // namespace Sightline
