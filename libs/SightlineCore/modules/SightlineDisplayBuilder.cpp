/// @file SightlineDisplayBuilder.cpp
/// @brief Implementation of Sightline display layout command serializers.

#include "SightlineDisplayBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineDisplayBuilder::buildSetDisplayParams(
    const MsgSetDisplayParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(10U);
    payload.push_back(msg.displayIndex);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendU16Le(payload, msg.xOffset);
    SightlineFraming::appendU16Le(payload, msg.yOffset);
    SightlineFraming::appendU16Le(payload, msg.displayWidth);
    SightlineFraming::appendU16Le(payload, msg.displayHeight);
    return SightlineFraming::buildPacket(MessageId::SetDisplayParameters, payload);
}

std::vector<std::uint8_t> SightlineDisplayBuilder::buildGetDisplayParams(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetDisplayParameters, payload);
}

} // namespace Sightline
