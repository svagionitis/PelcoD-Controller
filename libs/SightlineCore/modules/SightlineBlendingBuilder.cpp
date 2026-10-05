/// @file SightlineBlendingBuilder.cpp
/// @brief Implementation of Sightline blending command serializers.

#include "SightlineBlendingBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineBlendingBuilder::buildSetBlendParameters(const MsgSetBlendParameters& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.absOffZoom,
        static_cast<std::uint8_t>(msg.vertical),
        static_cast<std::uint8_t>(msg.horizontal),
        msg.rotation,
        msg.zoom,
        static_cast<std::uint8_t>(msg.mode),
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

std::vector<std::uint8_t> SightlineBlendingBuilder::buildFourAlignPoints(const MsgFourAlignPoints& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(1U + 32U);
    payload.push_back(msg.index);
    for (std::size_t i { 0U }; i < 4U; ++i) {
        SightlineFraming::appendS16Le(payload, msg.points[i].leftCol);
        SightlineFraming::appendS16Le(payload, msg.points[i].leftRow);
        SightlineFraming::appendS16Le(payload, msg.points[i].rightCol);
        SightlineFraming::appendS16Le(payload, msg.points[i].rightRow);
    }
    return SightlineFraming::buildPacket(MessageId::FourAlignPoints, payload);
}

std::vector<std::uint8_t> SightlineBlendingBuilder::buildGetFourAlignPoints(std::uint8_t index)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::FourAlignPoints), index };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineBlendingBuilder::buildSetBlendAlign(const MsgBlendAlign& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(11U);
    payload.push_back(msg.index);
    SightlineFraming::appendS16Le(payload, msg.vertical);
    SightlineFraming::appendS16Le(payload, msg.horizontal);
    SightlineFraming::appendU16Le(payload, msg.rotate);
    SightlineFraming::appendU16Le(payload, msg.zoom);
    SightlineFraming::appendU16Le(payload, msg.hzoom);
    return SightlineFraming::buildPacket(MessageId::BlendAlign, payload);
}

std::vector<std::uint8_t> SightlineBlendingBuilder::buildGetBlendAlign(std::uint8_t index)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::BlendAlign), index };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineBlendingBuilder::buildSetMultipleAlignment(const MsgSetMultipleAlignment& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(1U + 25U);
    payload.push_back(msg.nAlignments);
    for (std::size_t i { 0U }; i < 5U; ++i) {
        payload.push_back(msg.alignment[i].vertical);
        payload.push_back(msg.alignment[i].horizontal);
        payload.push_back(msg.alignment[i].rotate);
        payload.push_back(msg.alignment[i].zoom);
        payload.push_back(msg.alignment[i].hzoom);
    }
    return SightlineFraming::buildPacket(MessageId::SetMultipleAlignment, payload);
}

std::vector<std::uint8_t> SightlineBlendingBuilder::buildGetMultipleAlignment()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SetMultipleAlignment) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
