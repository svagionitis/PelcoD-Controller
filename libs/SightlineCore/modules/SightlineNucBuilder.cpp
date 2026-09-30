/// @file SightlineNucBuilder.cpp
/// @brief Implementation of Sightline NUC and dead pixel command serializers.

#include "SightlineNucBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineNucBuilder::buildNucParameters(
    const MsgNucParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.nucAction, msg.shutterMode };
    return SightlineFraming::buildPacket(MessageId::NucParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildDeadPixel(
    const MsgDeadPixel& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(4U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.mode);
    SightlineFraming::appendU16Le(payload, msg.deadPixelCount);
    return SightlineFraming::buildPacket(MessageId::DeadPixel, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetNucParameters(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::NucParameters), cameraIndex
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetDeadPixel(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::DeadPixel), cameraIndex
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
