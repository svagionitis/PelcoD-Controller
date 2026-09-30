/// @file SightlineEnhancementBuilder.cpp
/// @brief Implementation of Sightline enhancement command serializers.

#include "SightlineEnhancementBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineEnhancementBuilder::buildSetVideoEnhance(
    const MsgSetVideoEnhancement& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.cameraIndex,
        msg.contrast,
        msg.brightness,
        msg.sharpening,
        msg.claheEnable
    };
    return SightlineFraming::buildPacket(MessageId::SetVideoEnhancementParameters, payload);
}

std::vector<std::uint8_t> SightlineEnhancementBuilder::buildSetNoise3D(
    const MsgNoise3D& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.cameraIndex, msg.enable, msg.temporalStrength, msg.spatialStrength
    };
    return SightlineFraming::buildPacket(MessageId::Noise3D, payload);
}

std::vector<std::uint8_t> SightlineEnhancementBuilder::buildGetVideoEnhance(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetVideoEnhancementParameters, payload);
}

std::vector<std::uint8_t> SightlineEnhancementBuilder::buildGetNoise3D(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::Noise3D), cameraIndex
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
