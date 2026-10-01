/// @file SightlineBlendingParser.cpp
/// @brief Implementation of Sightline blending frame deserializers.

#include "SightlineBlendingParser.h"

namespace Sightline {

bool SightlineBlendingParser::parseBlendParameters(ByteView packet, MsgSetBlendParameters& out)
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

bool SightlineBlendingParser::parseFourAlignPoints(ByteView packet, MsgFourAlignPoints& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::FourAlignPoints) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 34U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.warpIndex = payload[1U];
    for (std::size_t i { 0U }; i < 4U; ++i) {
        const std::size_t offset { 2U + (i * 8U) };
        out.points[i].warpCol = SightlineFraming::readU16Le(payload.data() + offset);
        out.points[i].warpRow = SightlineFraming::readU16Le(payload.data() + offset + 2U);
        out.points[i].fixedCol = SightlineFraming::readU16Le(payload.data() + offset + 4U);
        out.points[i].fixedRow = SightlineFraming::readU16Le(payload.data() + offset + 6U);
    }
    return true;
}

bool SightlineBlendingParser::parseBlendAlign(ByteView packet, MsgBlendAlign& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::BlendAlign) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 10U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.mode = payload[1U];
    out.offsetX = SightlineFraming::readS16Le(payload.data() + 2U);
    out.offsetY = SightlineFraming::readS16Le(payload.data() + 4U);
    out.rotation = SightlineFraming::readS16Le(payload.data() + 6U);
    out.scale = SightlineFraming::readU16Le(payload.data() + 8U);
    return true;
}

} // namespace Sightline
