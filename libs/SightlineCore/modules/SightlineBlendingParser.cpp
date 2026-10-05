/// @file SightlineBlendingParser.cpp
/// @brief Implementation of Sightline blending frame deserializers.

#include "SightlineBlendingParser.h"

namespace Sightline {

bool SightlineBlendingParser::parseBlendParameters(ByteView packet, MsgSetBlendParameters& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    const auto payload { SightlineFraming::extractPayload(packet) };

    if (id == MessageId::SetBlendParameters) {
        if (payload.size() < 18U) {
            return false;
        }
        out.absOffZoom = payload[0U];
        out.vertical = static_cast<std::int8_t>(payload[1U]);
        out.horizontal = static_cast<std::int8_t>(payload[2U]);
        out.rotation = payload[3U];
        out.zoom = payload[4U];
        out.mode = static_cast<BlendMode>(payload[5U]);
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

    if (id == MessageId::CurrentBlendParameters) {
        if (payload.size() < 19U) {
            return false;
        }
        out.absOffZoom = payload[0U];
        const int up { payload[1U] };
        const int right { payload[2U] };
        const int down { payload[3U] };
        const int left { payload[4U] };
        out.vertical = static_cast<std::int8_t>(up - down);
        out.horizontal = static_cast<std::int8_t>(right - left);
        out.rotation = payload[5U];
        out.zoom = payload[6U];
        out.mode = static_cast<BlendMode>(payload[7U]);
        out.amt = payload[8U];
        out.hue = payload[9U];
        out.flags = payload[10U];
        out.reset = 0U;
        out.reserved = payload[11U];
        out.warpIndex = payload[12U];
        out.fixedIndex = payload[13U];
        out.usePresetAlign = payload[14U];
        out.presetAlignIndex = payload[15U];
        out.hzoom = payload[16U];
        out.hotStart = payload[17U];
        out.coldEnd = payload[18U];
        return true;
    }

    return false;
}

bool SightlineBlendingParser::parseCurrentBlendParameters(ByteView packet, MsgCurrentBlendParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CurrentBlendParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 19U) {
        return false;
    }

    out.absOffZoom = payload[0U];
    out.up = payload[1U];
    out.right = payload[2U];
    out.down = payload[3U];
    out.left = payload[4U];
    out.rotation = payload[5U];
    out.zoom = payload[6U];
    out.mode = static_cast<BlendMode>(payload[7U]);
    out.amt = payload[8U];
    out.hue = payload[9U];
    out.flags = payload[10U];
    out.reserved = payload[11U];
    out.warpIndex = payload[12U];
    out.fixedIndex = payload[13U];
    out.usePresetAlign = payload[14U];
    out.presetAlignIndex = payload[15U];
    out.hzoom = payload[16U];
    out.hotStart = payload[17U];
    out.coldEnd = payload[18U];
    return true;
}

bool SightlineBlendingParser::parseFourAlignPoints(ByteView packet, MsgFourAlignPoints& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::FourAlignPoints) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 33U) {
        return false;
    }

    out.index = payload[0U];
    for (std::size_t i { 0U }; i < 4U; ++i) {
        const std::size_t offset { 1U + (i * 8U) };
        out.points[i].leftCol = SightlineFraming::readS16Le(payload.data() + offset);
        out.points[i].leftRow = SightlineFraming::readS16Le(payload.data() + offset + 2U);
        out.points[i].rightCol = SightlineFraming::readS16Le(payload.data() + offset + 4U);
        out.points[i].rightRow = SightlineFraming::readS16Le(payload.data() + offset + 6U);
    }
    return true;
}

bool SightlineBlendingParser::parseBlendAlign(ByteView packet, MsgBlendAlign& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::BlendAlign) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 11U) {
        return false;
    }

    out.index = payload[0U];
    out.vertical = SightlineFraming::readS16Le(payload.data() + 1U);
    out.horizontal = SightlineFraming::readS16Le(payload.data() + 3U);
    out.rotate = SightlineFraming::readU16Le(payload.data() + 5U);
    out.zoom = SightlineFraming::readU16Le(payload.data() + 7U);
    out.hzoom = SightlineFraming::readU16Le(payload.data() + 9U);
    return true;
}

bool SightlineBlendingParser::parseMultipleAlignment(ByteView packet, MsgSetMultipleAlignment& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::SetMultipleAlignment && id != MessageId::CurrentMultipleAlignment) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 26U) {
        return false;
    }

    out.nAlignments = payload[0U];
    for (std::size_t i { 0U }; i < 5U; ++i) {
        const std::size_t offset { 1U + (i * 5U) };
        out.alignment[i].vertical = payload[offset];
        out.alignment[i].horizontal = payload[offset + 1U];
        out.alignment[i].rotate = payload[offset + 2U];
        out.alignment[i].zoom = payload[offset + 3U];
        out.alignment[i].hzoom = payload[offset + 4U];
    }
    return true;
}

} // namespace Sightline
