/// @file SightlineBlendingParser.cpp
/// @brief Implementation of Sightline blending frame deserializers.

#include "SightlineBlendingParser.h"

namespace Sightline {

bool SightlineBlendingParser::parseBlendParameters(
    const std::vector<std::uint8_t>& packet, MsgSetBlendParameters& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentBlendParameters && id != MessageId::SetBlendParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 18U) {
        return false;
    }

    out.absOffZoom = payload[0U];
    out.vertical = static_cast<std::int8_t>(payload[1U]);
    out.horizontal = static_cast<std::int8_t>(payload[2U]);
    out.rotation = payload[3U];
    out.zoom = payload[4U];
    out.mode = payload[5U];
    out.amt = payload[6U];
    out.hue = payload[7U];
    out.flags = payload[8U];
    out.reset = payload[9U];
    out.reserved = payload[10U];
    out.warpIndex = payload[11U];
    out.fixedIndex = payload[12U];
    out.usePresetAlign = payload[13U];
    out.presetAlignIndex = payload[14U];
    out.hzoom = payload[15U];
    out.hotStart = payload[16U];
    out.coldEnd = payload[17U];

    return true;
}

} // namespace Sightline
