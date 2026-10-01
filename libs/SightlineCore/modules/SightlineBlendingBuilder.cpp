/// @file SightlineBlendingBuilder.cpp
/// @brief Implementation of Sightline blending command serializers.

#include "SightlineBlendingBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineBlendingBuilder::buildSetBlendParameters(const MsgSetBlendParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.absOffZoom, static_cast<std::uint8_t>(msg.vertical),
        static_cast<std::uint8_t>(msg.horizontal), msg.rotation, msg.zoom, msg.mode, msg.amt, msg.hue, msg.flags,
        msg.reset, msg.reserved, msg.warpIndex, msg.fixedIndex, msg.usePresetAlign, msg.presetAlignIndex, msg.hzoom,
        msg.hotStart, msg.coldEnd };
    return SightlineFraming::buildPacket(MessageId::SetBlendParameters, payload);
}

std::vector<std::uint8_t> SightlineBlendingBuilder::buildGetBlendParameters()
{
    return SightlineFraming::buildPacket(MessageId::GetBlendParameters, {});
}

std::vector<std::uint8_t> SightlineBlendingBuilder::buildFourAlignPoints(const MsgFourAlignPoints& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U + 32U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.warpIndex);
    for (std::size_t i { 0U }; i < 4U; ++i) {
        SightlineFraming::appendU16Le(payload, msg.points[i].warpCol);
        SightlineFraming::appendU16Le(payload, msg.points[i].warpRow);
        SightlineFraming::appendU16Le(payload, msg.points[i].fixedCol);
        SightlineFraming::appendU16Le(payload, msg.points[i].fixedRow);
    }
    return SightlineFraming::buildPacket(MessageId::FourAlignPoints, payload);
}

std::vector<std::uint8_t> SightlineBlendingBuilder::buildGetFourAlignPoints(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::FourAlignPoints), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineBlendingBuilder::buildSetBlendAlign(const MsgBlendAlign& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(10U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.mode);
    SightlineFraming::appendS16Le(payload, msg.offsetX);
    SightlineFraming::appendS16Le(payload, msg.offsetY);
    SightlineFraming::appendS16Le(payload, msg.rotation);
    SightlineFraming::appendU16Le(payload, msg.scale);
    return SightlineFraming::buildPacket(MessageId::BlendAlign, payload);
}

std::vector<std::uint8_t> SightlineBlendingBuilder::buildGetBlendAlign(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::BlendAlign), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
