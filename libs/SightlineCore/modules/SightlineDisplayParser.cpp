/// @file SightlineDisplayParser.cpp
/// @brief Implementation of Sightline display layout frame deserializers.

#include "SightlineDisplayParser.h"

namespace Sightline {

bool SightlineDisplayParser::parseDisplayParameters(
    const std::vector<std::uint8_t>& packet, MsgSetDisplayParameters& out)
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

} // namespace Sightline
