/// @file SightlineBlendingBuilder.cpp
/// @brief Implementation of Sightline blending command serializers.

#include "SightlineBlendingBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineBlendingBuilder::buildSetBlendParameters(
    const MsgSetBlendParameters& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.absOffZoom,
        static_cast<std::uint8_t>(msg.vertical),
        static_cast<std::uint8_t>(msg.horizontal),
        msg.rotation,
        msg.zoom,
        msg.mode,
        msg.amt,
        msg.hue,
        msg.flags,
        msg.reset,
        msg.reserved,
        msg.warpIndex,
        msg.fixedIndex,
        msg.usePresetAlign,
        msg.presetAlignIndex,
        msg.hzoom,
        msg.hotStart,
        msg.coldEnd
    };
    return SightlineFraming::buildPacket(MessageId::SetBlendParameters, payload);
}

std::vector<std::uint8_t> SightlineBlendingBuilder::buildGetBlendParameters()
{
    return SightlineFraming::buildPacket(MessageId::GetBlendParameters, {});
}

} // namespace Sightline
