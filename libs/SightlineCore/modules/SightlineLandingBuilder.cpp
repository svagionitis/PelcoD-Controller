/// @file SightlineLandingBuilder.cpp
/// @brief Implementation of Sightline visual landing aid command serializers.

#include "SightlineLandingBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineLandingBuilder::buildLandingAid(
    const MsgLandingAid& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.mode, msg.patternType };
    return SightlineFraming::buildPacket(MessageId::LandingAid, payload);
}

std::vector<std::uint8_t> SightlineLandingBuilder::buildLandingPosition(
    const MsgLandingPosition& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(50U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendDouble64Le(payload, msg.relativeX);
    SightlineFraming::appendDouble64Le(payload, msg.relativeY);
    SightlineFraming::appendDouble64Le(payload, msg.relativeZ);
    SightlineFraming::appendDouble64Le(payload, msg.yawDeg);
    SightlineFraming::appendDouble64Le(payload, msg.pitchDeg);
    SightlineFraming::appendDouble64Le(payload, msg.rollDeg);
    payload.push_back(msg.confidence);
    return SightlineFraming::buildPacket(MessageId::LandingPosition, payload);
}

std::vector<std::uint8_t> SightlineLandingBuilder::buildGetLandingAid()
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::LandingAid)
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
