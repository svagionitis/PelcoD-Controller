/// @file SightlineNucParser.cpp
/// @brief Implementation of Sightline SLA NUC and dead pixel frame deserializers.

#include "SightlineNucParser.h"

namespace Sightline {

bool SightlineNucParser::parseNucParameters(
    const std::vector<std::uint8_t>& packet, MsgNucParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::NucParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 3U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.nucAction = payload[1U];
    out.shutterMode = payload[2U];
    return true;
}

bool SightlineNucParser::parseDeadPixel(
    const std::vector<std::uint8_t>& packet, MsgDeadPixel& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DeadPixel) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.mode = payload[1U];
    out.deadPixelCount = SightlineFraming::readU16Le(payload.data() + 2U);
    return true;
}

} // namespace Sightline
