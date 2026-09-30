/// @file SightlineEnhancementParser.cpp
/// @brief Implementation of Sightline enhancement frame deserializers.

#include "SightlineEnhancementParser.h"

namespace Sightline {

bool SightlineEnhancementParser::parseVideoEnhance(
    const std::vector<std::uint8_t>& packet, MsgSetVideoEnhancement& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentVideoEnhancementParameters && id != MessageId::SetVideoEnhancementParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 5U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.contrast = payload[1U];
    out.brightness = payload[2U];
    out.sharpening = payload[3U];
    out.claheEnable = payload[4U];
    return true;
}

bool SightlineEnhancementParser::parseNoise3D(
    const std::vector<std::uint8_t>& packet, MsgNoise3D& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::Noise3D) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.enable = payload[1U];
    out.temporalStrength = payload[2U];
    out.spatialStrength = payload[3U];
    return true;
}

} // namespace Sightline
